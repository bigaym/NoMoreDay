# GPU 渲染卸载 - 技术规格书
**Track:** `gpu-sprite-offload`
**Date:** 2026-01-25
**Source:** `conductor/analyzer/2026-01-25_gpu_offload_analysis.md`

---

## 1. 概述

### 1.1 问题陈述

| ID | 问题描述 | 影响 | 风险等级 |
|----|---------|------|---------|
| P1 | MDI 渲染管线不支持纹理，所有带 `SpriteComponent` 的实体被强制标记为 `GPU_ENTITY_FLAG_NO_RENDER`，回退至 CPU 逐个 Draw Call | 超过 50% 的实体无法利用 GPU Instancing，万级实体时 CPU 成为瓶颈 | 🔴 Critical |
| P2 | 稀有度光晕、状态效果 (冰冻、燃烧) 作为 Overlay 单独渲染，产生大量额外 Draw Call 和 Overdraw | 增加 GPU 带宽压力，降低渲染效率 | 🟡 High |
| P3 | 通用 VFX (拾取闪烁、命中火星等) 由 CPU `switch-case` 逻辑处理 | 高频纯视觉元素产生 CPU 开销 | 🟡 Medium |
| P4 | `DamagePopup` 系统存在遗留 CPU 渲染路径 | 代码冗余，维护成本高 | 🟢 Low |

### 1.2 目标指标

| 指标 | 当前基线 | 优化目标 | 测量方法 |
|-----|---------|---------|---------|
| Draw Calls (10k 实体) | ~1000+ (CPU Loop) | < 10 (MDI + VFX) | RenderDoc 帧分析 |
| GPU-Rendered Entity Rate | < 30% | > 95% | `GPU_ENTITY_FLAG_NO_RENDER` 比例统计 |
| `RenderSystem::Render` 耗时 | ~4ms | < 1.5ms | ScopedTimer 日志 |
| Overdraw (Status Overlays) | > 1.5x | ~ 1.0x | RenderDoc Overdraw 视图 |

### 1.3 设计目标

| 目标 | 描述 |
|-----|-----|
| **纹理数组 (Texture Array)** | 启用 `GL_TEXTURE_2D_ARRAY` 以支持 MDI 渲染多纹理精灵，消除 CPU Draw Loop |
| **统一状态渲染** | 将稀有度/状态视觉效果整合到 MDI Fragment Shader 中，利用 `GPUVisualStats` SSBO |
| **VFX GPU 迁移** | 将通用特效 (拾取、命中) 迁移到 Compute Shader 模拟，`glDrawArraysIndirect` 渲染 |
| **遗留清理** | 移除 `DamagePopup` 的 CPU 渲染路径 |

---

## 2. 数据结构变更 (DOD)

### 2.1 GPUEntity 扩展

`type` 字段的语义需要从"抽象类型ID"变为"纹理层索引 (Texture Layer Index)"。

```cpp
// src/engine/render/GPUData.hpp - GPUEntity.type 语义文档化
struct GPUEntity {
  // ... 其他字段 ...
  int32_t type = 0; // 4 bytes - **Texture Layer Index** in GL_TEXTURE_2D_ARRAY
                    // -1: Use SDF Circle (Legacy fallback)
                    // 0+: Index into bound Texture Array Layers
  // ...
};
```

### 2.2 新增 Constants::GPU 命名空间

```cpp
// src/engine/render/GPUData.hpp
namespace NoMoreDay::Constants::GPU {
  // Texture Array Constraints
  constexpr int TEXTURE_LAYER_SIZE = 128;       // Standardized sprite size (px)
  constexpr int MAX_TEXTURE_LAYERS = 256;       // Max sprites in primary array
  constexpr int SDF_CIRCLE_TYPE = -1;           // Special type for SDF rendering
  
  // Status Visual Indices (packed into GPUVisualStats or flags)
  // These are indices for status indicator sprites/SDFs
  constexpr int STATUS_NONE = 0;
  constexpr int STATUS_FROZEN = 1;
  constexpr int STATUS_BURNING = 2;
  constexpr int STATUS_POISONED = 3;
  constexpr int STATUS_SHOCKED = 4;
}
```

### 2.3 GPUVisualStats 扩展

为支持状态视觉效果，扩展 `GPUVisualStats` 结构体。

```cpp
// src/engine/render/GPUData.hpp - GPUVisualStats 扩展
struct GPUVisualStats {
  // --- Existing ---
  float weaponDamage = 0.0f;
  float attackSpeed = 1.0f;
  float critChance = 0.0f;
  float critDamage = 0.0f;
  float defenseRating = 0.0f;
  float statusStrength = 0.0f;
  float glowIntensity = 0.0f;
  uint32_t glowColorPacked = 0xFFFFFFFF;
  // --- New (Use existing padding) ---
  uint32_t activeStatusMask = 0;  // 4 bytes - Bitmask of active status effects
  float statusTimer = 0.0f;       // 4 bytes - Animation phase [0, 1]
  float padding[6] = {0.0f};      // 24 bytes - Remaining padding
  
  GPUVisualStats() = default;
};
// Total: 64 bytes (unchanged)
// activeStatusMask: bit 0 = Frozen, bit 1 = Burning, etc.
```

---

## 3. 系统修改清单

### 3.1 资源管线

| 文件 | 函数/位置 | 修改内容描述 |
|-----|----------|------------|
| `ResourceManager.hpp/cpp` | 新增 `TextureArrayHandle` | 管理 `GL_TEXTURE_2D_ARRAY` 的创建与销毁 |
| `ResourceManager.cpp` | `LoadTextureArray()` | 新函数：接受纹理路径列表，创建 128x128 纹理数组 |
| `ResourceManager.cpp` | `Init()` | 调用 `LoadTextureArray()` 加载游戏精灵图集 |

### 3.2 渲染管线

| 文件 | 函数/位置 | 修改内容描述 |
|-----|----------|------------|
| `MDIRenderer.cpp` | `Init()` | 绑定纹理数组至 Shader 的 `sampler2DArray` uniform |
| `MDIRenderer.cpp` | `Render()` | 在 Draw 前绑定 `glBindTexture(GL_TEXTURE_2D_ARRAY, ...)` |
| `entity_mdi.frag` | `main()` | **核心修改**: 根据 `vTextureIndex` 采样纹理数组或回退到 SDF |
| `entity_mdi.frag` | 新增 | 添加状态效果叠加逻辑 (Frozen glow, Burn flicker) |
| `entity_mdi.vert` | 新增输出 | 传递 `vStatusMask`、`vStatusTimer` 至 Fragment Shader |

### 3.3 同步管线

| 文件 | 函数/位置 | 修改内容描述 |
|-----|----------|------------|
| `GPUEntitySystem.cpp` | `Update()` | **移除** 对带 `SpriteComponent` 实体的 `GPU_ENTITY_FLAG_NO_RENDER` 自动标记 |
| `GPUEntitySystem.cpp` | `Update()` | 新增逻辑：从 `SpriteComponent` 读取纹理层索引，写入 `GPUEntity.type` |
| `GPUEntitySystem.cpp` | `Update()` | 新增逻辑：从 `StatusEffectComponent` 读取状态并写入 `GPUVisualStats` |

### 3.4 遗留清理

| 文件 | 函数/位置 | 修改内容描述 |
|-----|----------|------------|
| `RenderSystem.cpp` | CPU Sprite 渲染循环 | 移除或条件化 (仅用于不规则尺寸资源) |
| `PopupRenderer.cpp` | Legacy Path | 移除旧的 CPU 绘制代码 |

---

## 4. API 契约

### 4.1 ResourceManager

```cpp
// ResourceManager.hpp
class ResourceManager {
public:
  // 加载多个 128x128 纹理到一个 GL_TEXTURE_2D_ARRAY
  // paths: 纹理文件路径列表 (按顺序对应 layer index 0, 1, 2...)
  // Returns: 纹理数组的 OpenGL ID
  unsigned int LoadTextureArray(const std::vector<std::string>& paths);
  
  // 获取已加载的实体纹理数组
  unsigned int GetEntityTextureArray() const { return m_entityTextureArray; }
  
  // 获取指定纹理名称对应的 Layer Index (-1 if not found)
  int GetTextureLayerIndex(const std::string& name) const;
  
private:
  unsigned int m_entityTextureArray = 0;
  std::unordered_map<std::string, int> m_textureLayerMap; // name -> layer index
};
```

### 4.2 GPUEntitySystem 接口无变化

无需暴露新的公共 API，内部逻辑变更在 `Update()` 中完成。

---

## 5. Shader 逻辑 (entity_mdi.frag)

```glsl
#version 430 core

// ...existing inputs...
flat in int vTextureIndex; // Changed from uint to int (to support -1)
flat in uint vStatusMask;
flat in float vStatusTimer;

uniform sampler2DArray entityTextures; // Texture Array binding

out vec4 fragColor;

void main() {
    // NO_RENDER check
    if ((vFlags & 2u) != 0u) discard;

    vec4 baseColor;
    
    // ---- TEXTURE OR SDF ----
    if (vTextureIndex >= 0) {
        // Sample from Texture Array
        baseColor = texture(entityTextures, vec3(vTexCoord, float(vTextureIndex)));
        if (baseColor.a < 0.1) discard; // Alpha cutout
    } else {
        // Fallback: Circle SDF (Legacy path for non-sprite entities)
        float distSq = dot(vLocalPos, vLocalPos);
        if (distSq > 1.0) discard;
        float delta = fwidth(distSq);
        float alpha = 1.0 - smoothstep(1.0 - delta, 1.0, distSq);
        baseColor = vec4(1.0, 0.3, 0.3, alpha); // Default red for SDF entities
    }
    
    // ---- STATUS OVERLAY ----
    vec3 statusGlow = vec3(0.0);
    // Frozen (Bit 0)
    if ((vStatusMask & 1u) != 0u) {
        statusGlow += vec3(0.2, 0.6, 1.0) * (0.3 + 0.2 * sin(vStatusTimer * 6.28));
    }
    // Burning (Bit 1)
    if ((vStatusMask & 2u) != 0u) {
        statusGlow += vec3(1.0, 0.5, 0.1) * (0.4 + 0.2 * sin(vStatusTimer * 12.56));
    }
    // ... other statuses ...
    
    // ---- RARITY GLOW (from glowIntensity/glowColorPacked) ----
    if (vGlow > 0.0) {
        // Unpack glowColor and apply
        // ...existing glow logic...
    }
    
    fragColor = vec4(baseColor.rgb + statusGlow, baseColor.a);
}
```

---

## 6. 测试矩阵

| 测试类型 | 覆盖内容 | 预期结果 |
|---------|---------|---------|
| Unit | `ResourceManager::LoadTextureArray` with 3 textures | 返回有效 OpenGL ID，`glGetTexLevelParameteriv` 报告 3 layers |
| Unit | `ResourceManager::GetTextureLayerIndex` | 返回正确索引，未知名称返回 -1 |
| Integration | 创建带 `SpriteComponent` 的实体 | `GPUEntitySystem` 正确填充 `GPUEntity.type`，MDI 渲染无异常 |
| Integration | 应用 `StatusEffect(Frozen)` | MDI 渲染出冰冻蓝光效果 |
| Performance | 10k 带精灵实体渲染 | Draw Calls < 10，帧率 > 120 FPS |
| Visual | RenderDoc 帧捕获 | 纹理数组正确绑定，Overdraw 无明显增加 |

---

## 7. 风险评估

| 风险 | 描述 | 缓解措施 |
|-----|-----|---------|
| 纹理尺寸不一致 | 部分资源非 128x128 可能导致纹理扭曲或加载失败 | 加载时强制缩放或拒绝非标尺寸资源 (日志警告) |
| Layer 数量上限 | `GL_MAX_ARRAY_TEXTURE_LAYERS` 通常为 256-2048，超出则需分多个数组 | 初版限制 256 层，后续按需扩展 |
| Alpha Sorting | MDI 不保证绘制顺序，半透明精灵可能错误遮挡 | 使用 Alpha Cutout (discard)；极端情况回退 CPU 排序渲染 |
| Shader 编译兼容性 | 低版本 OpenGL 或驱动可能不支持 `sampler2DArray` | 维持 SDF 回退路径，运行时检测支持并降级 |

---

## 8. 验收标准 (Checklist)

- [ ] `GL_TEXTURE_2D_ARRAY` 成功创建并绑定至 MDI Shader
- [ ] 带 `SpriteComponent` 的实体通过 MDI 渲染显示正确纹理
- [ ] `GPU_ENTITY_FLAG_NO_RENDER` 不再对标准精灵实体生效
- [ ] 状态效果 (冰冻/燃烧) 在 Shader 中正确显示叠加光效
- [ ] Draw Calls 在万级实体下保持个位数
- [ ] `RenderSystem::Render` 耗时 < 1.5ms (ScopedTimer 验证)
- [ ] 所有现有单元测试和集成测试通过
- [ ] 代码通过 `code-risk-analyzer` 审计

