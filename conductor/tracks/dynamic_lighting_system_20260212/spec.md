# 动态 2D 光照系统 技术规格书 (Spec)

> **Track ID**: `dynamic_lighting_system_20260212`
> **Parent Phase**: Phase 2 (GPU 渲染系统 2.0)
> **设计参考**: [GPU_Rendering_System_2.md §7](../../设计文档/特效和UI/GPU_Rendering_System_2.md)
> **前置依赖**: Phase 0 (RenderGraph) ✅ / Phase 1 (HDR + PostProcess) 🔵 验收中
> **状态**: 📝 设计中

---

## 1. 概述 (Overview)

为 NoMoreDay 引入动态 2D 光照系统，通过点光源衰减、锥形光束和区域化环境光来营造场景氛围。光照计算在 HDR 空间内完成，自然配合后处理管线中的 Bloom 效果——高强度光源自动产生发光溢出。

### 1.1 核心特性

| 特性 | 规格 |
|------|------|
| **最大光源数** | 256（Ultra Tier），按 QualityTier 分档 |
| **光源类型** | PointLight / SpotLight / AmbientZone |
| **衰减模型** | 平方反比 + 软边界 (smooth falloff) |
| **计算方式** | 全屏 Fragment Shader，逐像素累加光源贡献 |
| **HDR 集成** | 光照在 RGBA16F 空间计算，intensity 可 >1.0 |
| **性能预算** | 常规场景 ≤ 0.5ms / 高强度 ≤ 0.8ms / 极限 ≤ 1.0ms |
| **降级路径** | Low Tier 完全禁用，使用固定环境光 |

### 1.2 设计目标

1. **氛围营造**: 洞穴幽暗、森林斑驳、火焰温暖、技能冷光——通过光照区分场景情绪。
2. **自然集成**: 光源自动挂载到游戏中已有的火焰、技能特效、爆炸等实体。
3. **性能可控**: 严格遵循性能预算，超预算时自动裁剪低优先级光源。
4. **零退步**: Low Tier 完全跳过 LightingPass，视觉效果与 Phase 1 完全一致。

### 1.3 非目标

- 不实现 3D 光照或 PBR
- 不实现硬阴影（High/Ultra 的软阴影 SDF 为后续扩展预留接口，本期不实现）
- 不改动现有粒子/实体渲染 Shader（光照在后置 Pass 中统一施加）

### 1.4 核心约束

- **OpenGL 4.3+**: 使用 SSBO 传递光源数据（通过 `GPUUtils` 封装）
- **GPU ABI 契约**: GPULight 结构体 C++/GLSL 双端一致，`static_assert(sizeof == 32)`
- **Binding 注册**: 使用 `Binding::SSBO_RESERVED_9` → 重命名为 `SSBO_LIGHT_DATA`
- **GL 状态契约**: Pass 边界强制 `ScopedGLState`
- **RAII**: 所有 GPU 资源由 RAII 管理，主循环零堆分配

---

## 2. 系统架构

### 2.1 Phase 2 渲染管线流程

```
┌────────────────────────────────────────────────────────────────────────┐
│                            RenderGraph                                │
│                                                                        │
│  ScenePass ──→ LightingPass ──→ VFXPass ──→ UIWorldPass               │
│  (HDRScene)    (HDRScene→     (继续在      (继续在                     │
│                 LitHDR→        HDRScene     HDRScene                   │
│                 Blit回         上叠加)      上叠加)                     │
│                 HDRScene)                                              │
│                      │                                                 │
│                      ↓                                                 │
│               PostProcessPass ──→ CompositePass                       │
│               (Bloom/Tonemap/     (LDR + UI                           │
│                FXAA/Vignette)      → Screen)                          │
└────────────────────────────────────────────────────────────────────────┘
```

### 2.2 LightingPass 数据流

```
输入:
├── HDR SceneBuffer (RGBA16F, colorTexture) — 已渲染的实体/地形
├── SSBO<GPULight>[activeLightCount] — 活跃光源数组
└── Uniforms:
    ├── ambientColor (vec3) — 全局环境光颜色
    ├── ambientIntensity (float) — 全局环境光强度
    ├── activeLightCount (int) — 本帧活跃光源数
    ├── cameraOffset (vec2) — 世界坐标→屏幕坐标偏移
    └── screenSize (vec2) — 屏幕尺寸

处理:
├── 全屏 Fragment Shader
├── 逐像素: litColor = sceneColor * (ambient + Σ lightContribution_i)
└── 光源贡献 = attenuation(distance, radius) * intensity * color

输出:
├── LitHDR Buffer (临时, RGBA16F) — 光照后的场景
└── Blit 回 HDR SceneBuffer (替换原始内容)
```

### 2.3 Low Tier (dynamicLightingEnabled=false) 回退路径

```
ScenePass → VFXPass → UIWorldPass → PostProcessPass → CompositePass
(与 Phase 1 完全一致, LightingPass 整体跳过)
```

**关键原则**: `LightingPass` 在 `dynamicLightingEnabled == false` 时不注册到 RenderGraph，管线回退到 Phase 1 行为，确保零退步。

### 2.4 与现有系统的边界

```
自定义 GPU 管线 (完全控制):
├── LightManager — 从 ECS 收集光源, 上传 SSBO
├── LightingPass — 全屏光照计算
└── light_accumulation.frag — 光照 Shader

Raylib 层 (不改动):
├── UI 面板渲染
├── 字体/窗口/输入
└── 不参与光照计算
```

---

## 3. 数据模型 (Data Model)

### 3.1 GPULight (GPU 端, SSBO)

```glsl
// assets/shaders/lighting/gpu_light.glslinc
// 对应 C++ GPUData.hpp 中的 GPULight
// 32 bytes, std430 layout
struct GPULight {
    vec2  position;      // 8  世界坐标
    float radius;        // 4  衰减半径
    float intensity;     // 4  强度 (可 >1.0 for HDR Bloom)
    vec4  color;         // 16 RGBA (alpha 作为额外参数)
};                       // Total: 32 bytes
```

```cpp
// ============================================================
// src/engine/render/GPUData.hpp — 新增 GPULight
// ============================================================
namespace NoMoreDay::components {

/// GPU 光源数据结构 (std430, 32 bytes)
/// 严格与 GLSL GPULight 保持一致
struct GPULight {
    float posX = 0.0f;      // 4
    float posY = 0.0f;      // 4
    float radius = 100.0f;  // 4  衰减半径 (世界坐标单位)
    float intensity = 1.0f; // 4  光照强度
    float colorR = 1.0f;    // 4
    float colorG = 1.0f;    // 4
    float colorB = 1.0f;    // 4
    float colorA = 1.0f;    // 4  Alpha 用作 spotAngle (PointLight 时为 1.0)
};

static_assert(sizeof(GPULight) == 32,
              "GPULight struct must be exactly 32 bytes for SSBO alignment");

} // namespace NoMoreDay::components
```

### 3.2 LightType 枚举

```cpp
// src/engine/render/GPUData.hpp — 新增
namespace NoMoreDay::components {

enum class LightType : uint8_t {
    PointLight = 0,   // 径向衰减 (平方反比)
    SpotLight = 1,    // 扇形区域光
    AmbientZone = 2,  // 区域化环境光
};

} // namespace NoMoreDay::components
```

### 3.3 LightComponent (ECS 组件)

```cpp
// ============================================================
// src/game/components/LightComponent.hpp — 新增文件
// ============================================================
#pragma once

#include "engine/render/GPUData.hpp"
#include <cstdint>

namespace NoMoreDay {

/// 挂载到 ECS 实体的光源组件
struct LightComponent {
    components::LightType type = components::LightType::PointLight;
    float radius = 100.0f;      // 衰减半径
    float intensity = 1.0f;     // 光照强度 (>1.0 = HDR 发光)
    float colorR = 1.0f;
    float colorG = 0.9f;
    float colorB = 0.7f;        // 默认暖色调
    float spotAngle = 360.0f;   // SpotLight: 扇形角度 (PointLight: 360)
    float spotDirection = 0.0f; // SpotLight: 朝向角度 (度)
    uint8_t priority = 128;     // 裁剪优先级 (0=最低, 255=不可裁剪)
    bool enabled = true;        // 运行时开关
    bool flicker = false;       // 火焰闪烁效果
    float flickerSpeed = 5.0f;  // 闪烁频率
    float flickerAmplitude = 0.2f; // 闪烁幅度 (0~1)
};

} // namespace NoMoreDay
```

### 3.4 RenderConfig 扩展

```cpp
// src/engine/render/core/RenderConstants.hpp — RenderConfig 扩展
struct RenderConfig {
    // --- Phase 0/1 (已有) ---
    bool bloomEnabled = false;
    bool dynamicLightingEnabled = false;  // ← 已存在
    int maxParticles = 20000;
    int shadowResolution = 0;
    int bloomMipLevels = 0;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.8f;
    float bloomKnee = 0.1f;
    bool fxaaEnabled = false;
    bool vignetteEnabled = false;
    float vignetteIntensity = 0.3f;
    float vignetteRadius = 0.75f;

    // --- Phase 2 (新增) ---
    int maxLights = 0;                    // 最大动态光源数
    float ambientIntensity = 0.3f;        // 全局环境光强度
    float ambientColorR = 0.15f;          // 全局环境光颜色
    float ambientColorG = 0.15f;
    float ambientColorB = 0.2f;
};
```

### 3.5 Quality Tier 参数表

| 参数 | Low | Medium | High | Ultra |
|------|-----|--------|------|-------|
| `dynamicLightingEnabled` | `false` | `true` | `true` | `true` |
| `maxLights` | `0` | `32` | `128` | `256` |
| `ambientIntensity` | `0.5` | `0.3` | `0.25` | `0.2` |

### 3.6 RenderConstants Binding 扩展

```cpp
// src/engine/render/RenderConstants.hpp — Binding 枚举修改
enum class Binding : uint32_t {
    // ... (0-8 已有) ...

    // === Lighting System (Phase 2) ===
    SSBO_LIGHT_DATA = 9,        // GPULight 数组 (LightingPass)

    // === Reserved ===
    SSBO_RESERVED_10 = 10,
    // ...
};
```

### 3.7 光源预设模板 (constexpr)

```cpp
// src/engine/render/GPUData.hpp — 光源预设常量
namespace NoMoreDay::Constants::Lighting {

// 火焰光源 (暖色, 中等范围, 闪烁)
constexpr float FIRE_RADIUS = 120.0f;
constexpr float FIRE_INTENSITY = 1.5f;
constexpr float FIRE_COLOR_R = 1.0f;
constexpr float FIRE_COLOR_G = 0.7f;
constexpr float FIRE_COLOR_B = 0.3f;

// 技能冷光 (冷色, 短范围, 高强度)
constexpr float SKILL_ICE_RADIUS = 80.0f;
constexpr float SKILL_ICE_INTENSITY = 2.0f;
constexpr float SKILL_ICE_COLOR_R = 0.5f;
constexpr float SKILL_ICE_COLOR_G = 0.8f;
constexpr float SKILL_ICE_COLOR_B = 1.0f;

// 爆炸闪光 (白色, 大范围, 极高强度, 短暂)
constexpr float EXPLOSION_RADIUS = 300.0f;
constexpr float EXPLOSION_INTENSITY = 5.0f;

// 环境萤火 (微弱, 小范围)
constexpr float AMBIENT_FIREFLY_RADIUS = 40.0f;
constexpr float AMBIENT_FIREFLY_INTENSITY = 0.5f;

// 最大光源数量上限 (GPU Buffer 预分配)
constexpr int MAX_LIGHTS = 256;

} // namespace NoMoreDay::Constants::Lighting
```

---

## 4. LightManager (光源管理器)

### 4.1 职责

```
┌─────────────────────────────────────────────────────────────┐
│  LightManager (每帧调用)                                     │
│                                                              │
│  1. 收集: 遍历 ECS 中所有 LightComponent 实体                │
│  2. 裁剪: 视锥裁剪 + 按 priority 排序 + 截断到 maxLights    │
│  3. 转换: LightComponent → GPULight (世界坐标)              │
│  4. 闪烁: 对 flicker=true 的光源施加时间扰动                 │
│  5. 上传: 批量写入 SSBO (OrphanAndUpload)                   │
│  6. 统计: 记录本帧 activeLightCount                         │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 接口定义

```cpp
// ============================================================
// src/engine/render/lighting/LightManager.hpp — 新增文件
// ============================================================
#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace NoMoreDay::render::lighting {

class LightManager {
public:
    static LightManager& Get();

    /// 初始化 GPU Buffer
    void Initialize();
    void Shutdown();

    /// 每帧调用: 从 ECS 收集光源, 裁剪, 上传 SSBO
    void Update(entt::registry& registry, const Camera2D& camera,
                int maxLights, float gameTime);

    /// 绑定光源 SSBO 到指定 binding point
    void Bind() const;

    /// 本帧活跃光源数
    [[nodiscard]] int GetActiveLightCount() const { return m_activeLightCount; }

    /// 手动添加临时光源 (爆炸/技能闪光, 一帧有效)
    void AddTransientLight(const components::GPULight& light);

private:
    LightManager() = default;

    std::unique_ptr<core::ComputeBuffer> m_lightBuffer;
    std::vector<components::GPULight> m_stagingBuffer;    // CPU 侧暂存
    std::vector<components::GPULight> m_transientLights;  // 临时光源
    int m_activeLightCount = 0;
};

} // namespace NoMoreDay::render::lighting
```

### 4.3 裁剪策略

1. **视锥裁剪**: 光源 `position ± radius` 与屏幕视口做矩形相交测试（世界坐标），不在屏幕范围内的光源直接跳过。
2. **优先级排序**: `priority` 降序排列（高优先级在前）。
3. **距离权重**: 同优先级时，离相机更近的光源优先。
4. **截断**: 超过 `maxLights` 的部分直接丢弃。

---

## 5. LightingPass 详细设计

### 5.1 类定义

```cpp
// ============================================================
// src/engine/render/passes/LightingPass.hpp — 新增文件
// ============================================================
#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"

namespace NoMoreDay::render::passes {

class LightingPass final : public graph::RenderPass {
public:
    LightingPass();
    ~LightingPass() override;

    void Setup(graph::RenderGraphBuilder& builder) override;
    void Execute(graph::RenderContext& context) override;
    const char* GetName() const override { return "LightingPass"; }

    bool Initialize();
    void Shutdown();
    void OnResize(int width, int height);

private:
    void DrawFullscreen(Shader shader, uint32_t sourceTexture);

    Shader m_lightAccumShader = {0};
    resources::FramebufferHandle m_litBuffer = {};  // 临时 LitHDR

    // Uniform locations
    int m_sceneTexLoc = -1;
    int m_ambientColorLoc = -1;
    int m_ambientIntensityLoc = -1;
    int m_lightCountLoc = -1;
    int m_cameraOffsetLoc = -1;
    int m_screenSizeLoc = -1;

    int m_cachedWidth = 0;
    int m_cachedHeight = 0;
    bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
```

### 5.2 Execute 流程

```
LightingPass::Execute(context):
  1. 检查 dynamicLightingEnabled → false 则 return (不执行)
  2. 获取 hdrSceneBuffer → 若无效则 return
  3. 确保 m_litBuffer 尺寸与 hdrSceneBuffer 匹配
  4. 绑定 m_litBuffer 为渲染目标
  5. 绑定 hdrSceneBuffer.colorTexture 为输入纹理
  6. LightManager::Get().Bind()  // 绑定 SSBO<GPULight>
  7. 设置 uniforms (ambient, lightCount, cameraOffset, screenSize)
  8. DrawFullscreen(m_lightAccumShader, hdrSceneBuffer.colorTexture)
  9. Blit m_litBuffer → hdrSceneBuffer (全屏拷贝)
```

### 5.3 RenderGraph 集成

```cpp
// RenderSystem::render() 中 — LightingPass 插入位置
// 在 ScenePass 之后, VFXPass 之前

const auto& renderConfig = QualityTierManager::Get().GetConfig();

graph.AddPass(/* ScenePass */);

if (renderConfig.dynamicLightingEnabled && useHdrSceneBuffer) {
    // LightManager 更新
    LightManager::Get().Update(registry, camera,
                                renderConfig.maxLights,
                                static_cast<float>(GetTime()));
    graph.AddPass(g_lightingPass);
}

graph.AddPass(/* VFXPass */);
// ...
```

---

## 6. Shader 设计

### 6.1 光照累积 Fragment Shader

```glsl
// ============================================================
// assets/shaders/lighting/light_accumulation.frag
// ============================================================
#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;       // HDR Scene Buffer
uniform vec3  uAmbientColor;       // 全局环境光颜色
uniform float uAmbientIntensity;   // 全局环境光强度
uniform int   uLightCount;         // 活跃光源数
uniform vec2  uCameraOffset;       // camera.target - screen/2
uniform vec2  uScreenSize;         // 屏幕分辨率

struct GPULight {
    vec2  position;      // 世界坐标
    float radius;
    float intensity;
    vec4  color;         // RGB + alpha(spotAngle/360)
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

/// 平方反比衰减 + 软边界
float calcAttenuation(float dist, float radius) {
    float normalizedDist = dist / radius;
    if (normalizedDist >= 1.0) return 0.0;
    // Smooth falloff: (1 - d²)²
    float d2 = normalizedDist * normalizedDist;
    float atten = (1.0 - d2);
    return atten * atten;
}

void main() {
    vec4 sceneColor = texture(uSceneTex, vTexCoord);

    // 当前像素的世界坐标
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    // 环境光基底
    vec3 totalLight = uAmbientColor * uAmbientIntensity;

    // 累加所有动态光源
    for (int i = 0; i < uLightCount; i++) {
        vec2 lightPos = lights[i].position;
        float radius = lights[i].radius;
        float intensity = lights[i].intensity;
        vec3 lightColor = lights[i].color.rgb;

        float dist = distance(worldPos, lightPos);
        float atten = calcAttenuation(dist, radius);

        totalLight += lightColor * intensity * atten;
    }

    // Scene × Lighting
    fragColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
```

### 6.2 复用 fullscreen.vert

```glsl
// assets/shaders/postprocess/fullscreen.vert (已存在)
// LightingPass 复用此顶点着色器
```

### 6.3 衰减模型说明

采用 `(1 - d²)²` 平方衰减模型，相比物理精确的 `1/d²`：
- 在 `d=0` 处自然达到最大值 1.0（无奇点）
- 在 `d=radius` 处平滑衰减到 0（无硬边界）
- 计算量极小，适合逐像素循环

---

## 7. 游戏集成

### 7.1 自动光源挂载

| 游戏实体 | 光源类型 | 参数模板 | 触发条件 |
|----------|----------|----------|----------|
| 火焰粒子区域 | PointLight | FIRE_* | 火焰类粒子发射时 |
| 冰系技能 | PointLight | SKILL_ICE_* | 技能释放期间 |
| 爆炸 | PointLight | EXPLOSION_* | 命中/爆炸瞬间，短暂闪光 |
| 玩家光环 | PointLight | 自定义 | 永久 (低强度) |
| 生物群落光源 | AmbientZone | 由 Biome 定义 | 地图加载时 |
| 掉落物光柱 | PointLight | 按稀有度调色 | 物品在地时 |

### 7.2 手动 API (VFX/特殊效果)

```cpp
// 技能爆炸闪光 (一帧有效)
NoMoreDay::components::GPULight flash;
flash.posX = hitPos.x;
flash.posY = hitPos.y;
flash.radius = Constants::Lighting::EXPLOSION_RADIUS;
flash.intensity = Constants::Lighting::EXPLOSION_INTENSITY;
flash.colorR = 1.0f; flash.colorG = 1.0f; flash.colorB = 1.0f;
LightManager::Get().AddTransientLight(flash);
```

---

## 8. 性能预算与约束

### 8.1 Pass 级预算 (标定机: RTX 4070S @ 2560x1440)

| 场景 | LightingPass 预算 | 预期光源数 | 备注 |
|------|-------------------|-----------|------|
| 常规战斗 | ≤ 0.5ms | 32-64 | 基础火焰/玩家光环 |
| 高强度战斗 | ≤ 0.8ms | 96-128 | 多 AOE + 群怪火焰 |
| 极限压力 | ≤ 1.0ms | 192-256 | 全屏弹幕 |

### 8.2 超预算自动降级

1. 减少活跃光源数（裁剪低优先级光源）
2. 降低光照计算精度（跳过 SpotLight 角度判定，全部按 PointLight）
3. 最终回退：禁用动态光照，使用固定环境光

### 8.3 GPU 内存预算

```
GPULight SSBO: 256 * 32 bytes = 8 KiB  (可忽略)
LitHDR Buffer: 2560 * 1440 * 8 bytes (RGBA16F) ≈ 29.5 MiB (临时, 每帧回收)
```

---

## 9. 风险与缓解

| ID | 描述 | 影响 | 缓解措施 |
|----|------|------|----------|
| R-L01 | 集显平台全屏 Shader 逐光源循环性能差 | 帧率波动 | maxLights 上限 + 早期跳出 + Low 回退 |
| R-L02 | Blit LitHDR → HDRScene 额外开销 | 0.1-0.2ms | 若超预算可考虑 imageLoad/imageStore 原地修改 |
| R-L03 | 世界坐标→屏幕坐标转换精度 | 光源位置偏移 | 统一使用 camera.target + offset 算法 |
| R-L04 | 与 Phase 1 PostProcess 的 HDR Buffer 生命周期冲突 | 画面异常 | 严格遵循 §15 Frame Ownership |
| R-L05 | 光源数量爆炸 (大量粒子自带光源) | 性能问题 | 禁止粒子级光源，仅粒子区域级光源 |

---

## 10. 验收标准 (Acceptance Criteria)

### 功能验收
- [ ] Medium 及以上 Tier 可看到动态光源效果 (移动中光源实时更新)
- [ ] PointLight 在 radius 边界平滑衰减至零
- [ ] 高强度光源 (intensity >1.0) 配合 Bloom 自动产生发光溢出
- [ ] Low Tier 完全跳过 LightingPass，视觉效果与 Phase 1 完全一致
- [ ] 火焰类光源有自然的闪烁效果
- [ ] 支持手动添加一帧有效的临时光源 (爆炸/技能闪光)

### 性能验收
- [ ] 常规场景 (64 光源): LightingPass ≤ 0.5ms
- [ ] 极限场景 (256 光源): LightingPass ≤ 1.0ms
- [ ] GPU 内存: SSBO ≤ 8 KiB, LitHDR Buffer ≤ 30 MiB

### 稳定性验收
- [ ] 30 分钟压力战斗无崩溃、无显存泄漏
- [ ] Resize 窗口 20 次后光照效果正常
- [ ] Low→High→Low 切换 Quality Tier 后无残留光源

### ABI 验收
- [ ] `static_assert(sizeof(GPULight) == 32)` 通过
- [ ] C++ 端 GPULight 字段顺序/偏移与 GLSL 完全一致

---

## 11. 已确认设计决策 (Confirmed Decisions)

| 问题 | 决策 |
|------|------|
| **光照计算空间** | 在 HDR SceneBuffer (RGBA16F) 上计算, 而非 LDR |
| **LightingPass 输出** | Ping-pong: 读 HDRScene → 写 LitHDR → Blit 回 HDRScene |
| **光源数据传递** | SSBO (std430), binding = 9 |
| **衰减模型** | `(1 - d²)²` 平方平滑衰减 |
| **阴影** | 本期不实现，接口预留 |
| **光源级别** | 实体级（不做粒子级光源，避免数量爆炸） |
| **环境光** | 统一 uniform，不做逐区域差异（AmbientZone 预留接口） |

---

*规格版本: 1.0*
*最后更新: 2026-02-12*
