# Track 1: Render Constants 规格说明书 (V1.0)

> **Track ID**: `T1_render_constants`
> **依赖**: 无
> **状态**: 🔵 待开始
> **预计工时**: 4h

---

## 1. 概述 (Overview)

本 Track 旨在消除 GPU 渲染管线中所有 SSBO/Buffer 绑定点使用的"魔法数字"，建立类型安全的中央常量命名空间。

### 1.1 问题陈述

当前代码中存在大量硬编码的绑定索引：

```cpp
// MDIRenderer.cpp:212
entities.BindPreviousNoSync(0);  // 魔法数字 0

// MDIRenderer.cpp:215
m_visibleBuffer.BindPreviousNoSync(1);  // 魔法数字 1

// Compute Shader (cull.compute)
layout(std430, binding = 0) readonly buffer EntityData { ... };
layout(std430, binding = 1) writeonly buffer VisibleIndices { ... };
```

这种模式存在以下风险：
1. **静默损坏**: C++ 端与 Shader 端绑定索引不匹配时，GPU 读取错误数据。
2. **重构困难**: 添加新 Buffer 时需手动追踪所有引用点。
3. **可读性差**: 数字本身不携带语义信息。

### 1.2 设计目标

1. 定义 `RenderConstants` 命名空间，包含所有渲染相关常量。
2. 使用 `enum class Binding : uint32_t` 确保类型安全和自动补全。
3. 统一 C++ 端与 Shader 端的绑定索引命名约定。

---

## 2. 数据模型 (Data Model)

### 2.1 核心常量定义

```cpp
// ============================================================
// src/engine/render/RenderConstants.hpp
// ============================================================
#pragma once
#include <cstdint>

namespace NoMoreDay::RenderConstants {

/**
 * @brief SSBO Binding Point 索引。
 * 
 * 所有 GPU Buffer 绑定必须使用此枚举，严禁使用字面量。
 * 对应 GLSL: layout(std430, binding = X) buffer ...
 */
enum class Binding : uint32_t {
    // === Entity Rendering Pipeline ===
    SSBO_ENTITY_DATA    = 0,  // GPUEntity 物理/变换数据 (GPUEntitySystem)
    SSBO_VISIBLE_ID     = 1,  // 视锥剔除后的可见实体索引 (MDIRenderer)
    SSBO_COMMAND        = 2,  // Indirect Draw Command (MDIRenderer)
    SSBO_VISUAL_STATS   = 3,  // 视觉属性 (Glow, Status Effects)
    
    // === Particle System ===
    SSBO_PARTICLES      = 4,  // GPUParticle 数组
    SSBO_PARTICLE_FREE  = 5,  // 空闲粒子槽索引
    SSBO_PARTICLE_COUNT = 6,  // 原子计数器
    
    // === Flow Field ===
    SSBO_FLOW_COST      = 7,  // 流场代价场
    SSBO_FLOW_INTEGRATE = 8,  // 流场积分场
    SSBO_FLOW_VECTOR    = 9,  // 流场向量场
    
    // === Skill Effects ===
    SSBO_SKILL_EFFECTS  = 10, // GPUSkillEffect 数组
    
    // === Popup Rendering ===
    SSBO_POPUP_DATA     = 11, // 伤害数字实例数据
    SSBO_POPUP_COMMAND  = 12, // Popup Indirect Draw Command
    
    // === Reserved ===
    SSBO_RESERVED_13    = 13,
    SSBO_RESERVED_14    = 14,
    SSBO_RESERVED_15    = 15, // OpenGL 4.3 最低保证 16 个 Binding
};

/**
 * @brief Uniform Block Binding Point 索引 (UBO)。
 */
enum class UBOBinding : uint32_t {
    UBO_GLOBAL_PARAMS = 0,  // 全局参数 (时间、相机等)
    UBO_LIGHTING      = 1,  // 光照参数 (Reserved)
};

/**
 * @brief Texture Unit 索引。
 */
enum class TextureUnit : uint32_t {
    TEX_ENTITY_ARRAY    = 0,  // Entity Texture2DArray
    TEX_PARTICLE_ATLAS  = 1,  // Particle Atlas
    TEX_SKILL_SDF       = 2,  // Skill SDF Texture
    TEX_FONT_ATLAS      = 3,  // Font Atlas for Popups
};

/**
 * @brief Memory Barrier 类型。
 * 
 * 可用 `|` 组合。
 */
enum class Barrier : uint32_t {
    None    = 0,
    SSBO    = 0x00002000,  // GL_SHADER_STORAGE_BARRIER_BIT
    Command = 0x00000040,  // GL_COMMAND_BARRIER_BIT
    Buffer  = 0x00000200,  // GL_BUFFER_UPDATE_BARRIER_BIT
    Image   = 0x00000020,  // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
    Client  = 0x00004000,  // GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT
    
    // 常用组合
    All     = SSBO | Command | Buffer,
};

inline Barrier operator|(Barrier a, Barrier b) {
    return static_cast<Barrier>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline uint32_t ToGL(Barrier b) {
    return static_cast<uint32_t>(b);
}

// === Convenience Constants ===
namespace GPU {
    constexpr int MAX_ENTITIES = 200000;
    constexpr int MAX_PARTICLES = 200000;
    constexpr int MAX_SKILL_EFFECTS = 1024;
    constexpr int MAX_POPUPS = 2048;
}

} // namespace NoMoreDay::RenderConstants
```

### 2.2 Shader 端约定

为保持 C++/GLSL 一致性，Shader 中使用注释标记绑定来源：

```glsl
// cull.compute
// Binding 来源: RenderConstants::Binding::SSBO_ENTITY_DATA
layout(std430, binding = 0) readonly buffer EntityData {
    GPUEntity entities[];
};

// Binding 来源: RenderConstants::Binding::SSBO_VISIBLE_ID
layout(std430, binding = 1) writeonly buffer VisibleIndices {
    uint visibleIds[];
};
```

---

## 3. 影响范围 (Affected Files)

### 3.1 C++ 文件

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `RenderConstants.hpp` | **新增** | 中央常量定义 |
| `GPUEntitySystem.cpp` | 修改 | 替换 `BindBase(0)` → `BindBase((uint32_t)Binding::SSBO_ENTITY_DATA)` |
| `MDIRenderer.cpp` | 修改 | 替换所有绑定点字面量 |
| `GPUParticleSystem.cpp` | 修改 | 替换粒子相关绑定点 |
| `GPUFlowFieldSystem.cpp` | 修改 | 替换流场相关绑定点 |
| `GPUSkillEffectSystem.cpp` | 修改 | 替换特效相关绑定点 |
| `PopupRenderer.cpp` | 修改 | 替换弹出数字绑定点 |

### 3.2 Shader 文件

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `cull.compute` | 修改 | 添加绑定来源注释 |
| `physics.compute` | 修改 | 添加绑定来源注释 |
| `entity_mdi.vert` | 修改 | 添加绑定来源注释 |
| `particle_*.compute` | 修改 | 添加绑定来源注释 |
| `flow_*.compute` | 修改 | 添加绑定来源注释 |

---

## 4. 使用示例

### 4.1 C++ 端绑定

```cpp
#include "engine/render/RenderConstants.hpp"

using namespace NoMoreDay::RenderConstants;

void MDIRenderer::Render(...) {
    // 类型安全的绑定
    entities.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA));
    m_visibleBuffer.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
    m_statsBuffer.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_VISUAL_STATS));
    
    // Memory Barrier
    GPUUtils::MemoryBarrier(ToGL(Barrier::Command | Barrier::SSBO));
}
```

### 4.2 便捷宏 (可选)

```cpp
// 可选: 定义便捷宏减少冗余
#define BIND(buf, binding) (buf).BindBase(static_cast<uint32_t>(Binding::binding))

// 使用
BIND(m_entityBuffer, SSBO_ENTITY_DATA);
```

---

## 5. 验收标准 (Acceptance Criteria)

- [ ] `RenderConstants.hpp` 已创建，包含所有当前使用的绑定索引。
- [ ] 所有 C++ 文件中的 `BindBase(X)` / `BindPreviousNoSync(X)` 字面量已替换。
- [ ] `Select-String -Path 'src/engine/render/*.cpp' -Pattern 'Bind.*\([0-9]+\)'` 返回空。
- [ ] 所有 Shader `.compute` 文件添加了绑定来源注释。
- [ ] 项目编译通过，所有测试通过。
- [ ] 视觉验收：游戏运行画面与重构前无差异。

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| 遗漏绑定点替换 | GPU 渲染异常 | 使用 `Select-String` 全局搜索验证 |
| Shader 未同步更新 | 静默损坏 | 运行视觉测试、检查 RenderDoc 捕获 |

---

*规格版本: 1.0*
*最后更新: 2026-01-26*
