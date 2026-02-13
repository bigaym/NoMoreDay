# 材质与 VFX 序列器 规格说明书 (V1.0)

> **Track ID**: `material_vfx_sequencer_20260213`
> **设计参考**: [GPU_Rendering_System_2.md](../../../设计文档/特效和UI/GPU_Rendering_System_2.md) — §8, §9, §11.5
> **Phase**: GPU 渲染系统 2.0 — Phase 4
> **前置依赖**: Phase 1 (HDR + PostProcess) ✅, Phase 3 (Particle & Trail) ✅
> **状态**: ⏳ 待确认

---

## 1. 概述 (Overview)

Phase 4 实现**材质系统**和 **VFX 序列器**，将当前硬编码在 C++ 中的特效参数（颜色、发光、混合模式等）迁移为数据驱动的材质定义，并建立一套 JSON 时间线机制，支持"多阶段、多层次"的视觉编排——**使新技能特效可以纯数据驱动创建**。

### 1.1 核心特性

| 特性 | 规格 |
|------|------|
| **材质实例** | constexpr 预定义 + JSON 数据驱动 + SSBO 上传至 GPU |
| **Shader 变体** | Default / Ink / Hologram / Fire / Ice / Lightning，可扩展 |
| **VFX 序列器** | JSON 定义多事件时间线，附加到 ECS Entity 播放 |
| **事件类型** | Particle / Trail / Light / Shake / Distortion / Sound |
| **Screen Distortion** | 独立 FBO + Distortion Pass，与后处理管线集成 |
| **热重载** | 开发期文件监听自动重载材质与 VFX 资产 |
| **Quality Tier** | Distortion 和 VFX 精简级别按 Tier 配置驱动 |

### 1.2 设计原则

1. **不做 PBR，做"视觉参数驱动"** — 材质仅控制颜色、发光、扭曲、混合模式和着色器变体。
2. **GPU ABI 契约** — 所有 SSBO 结构必须 `std430` 对齐，并有 `static_assert(sizeof())`。
3. **向后兼容** — 现有 `Colors` 命名空间和硬编码 VFX 保持可用，新系统提供增强路径。
4. **热重载安全** — 双缓冲资源句柄，加载失败不影响当前运行资源。

### 1.3 非目标

- 不做 PBR / 法线贴图 / 金属度-粗糙度工作流。
- 不做可视化 VFX 编辑器（本阶段仅 JSON 手写 + 热重载）。
- 不替换已有 `GPUSkillEffectSystem` 的 SDF 渲染路径（材质系统对其增强，不替换）。

---

## 2. 数据模型 (Data Model)

### 2.1 材质核心数据结构

```cpp
// ============================================================
// MaterialDefs.hpp - 材质核心定义
// ============================================================
#pragma once

#include <cstdint>
#include <array>

namespace NoMoreDay::render {

// 着色器变体枚举
enum class ShaderVariant : uint8_t {
    Default = 0,    // 标准着色
    Ink,            // 水墨风格
    Hologram,       // 全息投影
    Fire,           // 火焰溶解
    Ice,            // 冰晶折射
    Lightning,      // 电弧闪烁
    Dissolve,       // 消融效果
    Count
};

// 混合模式枚举
enum class BlendMode : uint8_t {
    Alpha = 0,      // 标准 Alpha 混合
    Additive,       // 加法混合 (发光效果)
    Multiply,       // 乘法混合 (阴影/贴花)
    Count
};

// CPU 端材质实例 (用于配置和序列化)
struct MaterialInstance {
    float baseColorR = 1.0f;       // 基础颜色 RGBA
    float baseColorG = 1.0f;
    float baseColorB = 1.0f;
    float baseColorA = 1.0f;

    float emissiveR = 0.0f;        // 自发光颜色 RGB
    float emissiveG = 0.0f;
    float emissiveB = 0.0f;
    float emissiveIntensity = 0.0f; // 发光强度 (>0 产生 Bloom)

    float distortion = 0.0f;       // 屏幕扭曲强度
    BlendMode blendMode = BlendMode::Alpha;
    ShaderVariant shader = ShaderVariant::Default;
    uint8_t padding0 = 0;

    int16_t textureSlots[4] = {-1, -1, -1, -1}; // 纹理数组层索引
};
```

### 2.2 GPU 侧材质数据 (SSBO 对齐)

```cpp
// ============================================================
// GPUData.hpp 扩展 — GPU 端材质结构
// ============================================================

// STRICTLY 64 BYTES (16 * 4) for SSBO alignment
struct GPUMaterialData {
    float baseColorR = 1.0f;       // 4
    float baseColorG = 1.0f;       // 4
    float baseColorB = 1.0f;       // 4
    float baseColorA = 1.0f;       // 4

    float emissiveR = 0.0f;        // 4
    float emissiveG = 0.0f;        // 4
    float emissiveB = 0.0f;        // 4
    float emissiveIntensity = 0.0f; // 4

    float distortion = 0.0f;       // 4
    uint32_t blendMode = 0;        // 4 (0=Alpha, 1=Additive, 2=Multiply)
    uint32_t shaderVariant = 0;    // 4 (enum ShaderVariant)
    uint32_t flags = 0;            // 4 (reserved)

    int32_t textureSlot0 = -1;     // 4
    int32_t textureSlot1 = -1;     // 4
    int32_t textureSlot2 = -1;     // 4
    int32_t textureSlot3 = -1;     // 4
};

static_assert(sizeof(GPUMaterialData) == 64,
              "GPUMaterialData struct must be exactly 64 bytes for SSBO alignment");
```

### 2.3 GLSL 侧材质结构 (镜像定义)

```glsl
// assets/shaders/generated/material_abi.glslinc

struct MaterialData {
    vec4  baseColor;       // 16
    vec4  emissive;        // 16 (rgb + intensity in w)
    float distortion;      // 4
    uint  blendMode;       // 4
    uint  shaderVariant;   // 4
    uint  flags;           // 4
    ivec4 textureSlots;    // 16
};

layout(std430, binding = MATERIAL_SSBO_BINDING) readonly buffer MaterialBuffer {
    MaterialData materials[];
};
```

### 2.4 预定义材质常量

```cpp
// MaterialDefs.hpp (续)

namespace MaterialPresets {

constexpr MaterialInstance Default() {
    return MaterialInstance{};
}

constexpr MaterialInstance InkSplash() {
    MaterialInstance m{};
    m.baseColorR = 0.08f; m.baseColorG = 0.08f; m.baseColorB = 0.14f; m.baseColorA = 0.86f;
    m.shader = ShaderVariant::Ink;
    m.blendMode = BlendMode::Alpha;
    return m;
}

constexpr MaterialInstance FireGlow() {
    MaterialInstance m{};
    m.baseColorR = 1.0f; m.baseColorG = 0.31f; m.baseColorB = 0.12f; m.baseColorA = 1.0f;
    m.emissiveR = 1.0f; m.emissiveG = 0.5f; m.emissiveB = 0.1f;
    m.emissiveIntensity = 2.0f;
    m.blendMode = BlendMode::Additive;
    m.shader = ShaderVariant::Fire;
    return m;
}

constexpr MaterialInstance IceCrystal() {
    MaterialInstance m{};
    m.baseColorR = 0.31f; m.baseColorG = 0.71f; m.baseColorB = 1.0f; m.baseColorA = 0.8f;
    m.emissiveR = 0.5f; m.emissiveG = 0.8f; m.emissiveB = 1.0f;
    m.emissiveIntensity = 1.5f;
    m.shader = ShaderVariant::Ice;
    return m;
}

constexpr MaterialInstance LightningArc() {
    MaterialInstance m{};
    m.baseColorR = 1.0f; m.baseColorG = 1.0f; m.baseColorB = 0.39f; m.baseColorA = 1.0f;
    m.emissiveR = 1.0f; m.emissiveG = 1.0f; m.emissiveB = 0.5f;
    m.emissiveIntensity = 3.0f;
    m.blendMode = BlendMode::Additive;
    m.shader = ShaderVariant::Lightning;
    return m;
}

constexpr MaterialInstance HoloBlade() {
    MaterialInstance m{};
    m.baseColorR = 0.78f; m.baseColorG = 0.90f; m.baseColorB = 1.0f; m.baseColorA = 0.63f;
    m.emissiveR = 0.8f; m.emissiveG = 0.95f; m.emissiveB = 1.0f;
    m.emissiveIntensity = 1.0f;
    m.shader = ShaderVariant::Hologram;
    return m;
}

constexpr MaterialInstance ShadowVoid() {
    MaterialInstance m{};
    m.baseColorR = 0.16f; m.baseColorG = 0.04f; m.baseColorB = 0.24f; m.baseColorA = 1.0f;
    m.distortion = 0.5f;
    m.shader = ShaderVariant::Dissolve;
    return m;
}

constexpr MaterialInstance DistortionShockwave() {
    MaterialInstance m{};
    m.baseColorR = 0.0f; m.baseColorG = 0.0f; m.baseColorB = 0.0f; m.baseColorA = 0.0f;
    m.distortion = 1.0f;
    return m;
}

} // namespace MaterialPresets
} // namespace NoMoreDay::render
```

---

## 3. 材质管理器 (MaterialManager)

### 3.1 核心职责

```
MaterialManager (单例):
├── 预定义材质注册 (ID 0~63 保留给 constexpr 预设)
├── JSON 材质加载 (ID 64+ 分配给数据驱动材质)
├── SSBO 上传/同步 (GPUMaterialData[] → GPU)
├── 热重载监听 (开发期文件变更 → 重新解析)
└── 查询接口 (materialId → MaterialInstance)
```

### 3.2 API 定义

```cpp
// ============================================================
// MaterialManager.hpp
// ============================================================
namespace NoMoreDay::render {

class MaterialManager {
public:
    static MaterialManager& Get();

    void Initialize();
    void Shutdown();

    // 注册预定义材质 (启动时调用)
    int RegisterMaterial(const MaterialInstance& mat, const std::string& name = "");

    // 从 JSON 加载材质包
    // 格式: { "material_schema_version": 1, "materials": [ { "name": "...", ... } ] }
    int LoadFromJson(const std::string& path);

    // 热重载 (开发期)
    void TryHotReload();

    // 查询
    const MaterialInstance& GetMaterial(int materialId) const;
    int GetMaterialId(const std::string& name) const;
    int GetMaterialCount() const;

    // GPU 同步 (每帧调用一次, 仅 dirty 时上传)
    void SyncToGPU();

    // Binding
    void BindSSBO(int bindingPoint) const;

    static constexpr int MAX_MATERIALS = 256;
    static constexpr int PRESET_RESERVE = 64; // 前 64 个保留给预设

private:
    MaterialManager() = default;

    std::array<MaterialInstance, MAX_MATERIALS> m_materials{};
    std::unordered_map<std::string, int> m_nameToId;
    int m_count = 0;
    bool m_dirty = true;

    uint32_t m_ssbo = 0;
    std::string m_jsonPath;
    // 文件修改时间 (热重载检测)
    std::filesystem::file_time_type m_lastModified{};
};

} // namespace NoMoreDay::render
```

### 3.3 持久化契约 — JSON 材质定义

文件路径: `assets/data/materials_vfx.json`

```json
{
  "material_schema_version": 1,
  "materials": [
    {
      "name": "FireExplosion",
      "baseColor": [1.0, 0.4, 0.1, 1.0],
      "emissive": [1.0, 0.6, 0.2],
      "emissiveIntensity": 3.0,
      "distortion": 0.3,
      "blendMode": "Additive",
      "shader": "Fire",
      "textureSlots": [-1, -1, -1, -1]
    },
    {
      "name": "IceShatter",
      "baseColor": [0.3, 0.7, 1.0, 0.8],
      "emissive": [0.4, 0.7, 1.0],
      "emissiveIntensity": 1.5,
      "blendMode": "Alpha",
      "shader": "Ice"
    }
  ]
}
```

---

## 4. VFX 序列器 (VFX Sequencer)

### 4.1 数据结构

```cpp
// ============================================================
// VFXSequencer.hpp - VFX 时间线系统
// ============================================================
#pragma once

#include <string>
#include <vector>
#include <variant>
#include <cstdint>

namespace NoMoreDay::vfx {

// 位置锚定方式
enum class AnchorType : uint8_t {
    Caster = 0,    // 相对于施法者
    Target,        // 相对于目标
    World,         // 绝对世界坐标
    Impact,        // 命中点
};

// VFX 事件类型
enum class EventType : uint8_t {
    Particle = 0,  // 粒子发射
    Trail,         // 轨迹渲染
    Light,         // 动态光源
    Shake,         // 屏幕震动
    Distortion,    // 屏幕扭曲
    Sound,         // 音效播放
    MaterialSwap,  // 材质切换
    Count
};

// --- 各事件类型的参数结构 ---

struct ParticleEventParams {
    int materialId = 0;            // 使用的材质 ID
    int count = 10;                // 发射数量
    float speed = 100.0f;          // 初始速度
    float speedVariance = 20.0f;   // 速度随机偏移
    float lifetime = 0.5f;         // 粒子生命周期
    float scale = 1.0f;
    float spreadAngle = 360.0f;    // 散布角度 (度)
    int16_t textureIndex = -1;     // 纹理索引
    uint8_t blendMode = 0;         // 0=Alpha, 1=Additive
    float offsetX = 0.0f;          // 相对锚点偏移
    float offsetY = 0.0f;
};

struct TrailEventParams {
    int materialId = 0;
    float duration = 0.3f;         // 轨迹持续时间
    float widthStart = 8.0f;
    float widthEnd = 1.0f;
    uint32_t colorStart = 0xFFFFFFFF;
    uint32_t colorEnd = 0x00000000;
};

struct LightEventParams {
    float radius = 100.0f;
    float intensity = 2.0f;
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float duration = 0.5f;         // 淡入+持续+淡出总时间
    float fadeInRatio = 0.1f;      // 淡入占比
    float fadeOutRatio = 0.3f;     // 淡出占比
};

struct ShakeEventParams {
    float intensity = 0.2f;
};

struct DistortionEventParams {
    float radius = 100.0f;         // 扭曲影响半径
    float strength = 0.5f;         // 扭曲强度
    float duration = 0.3f;         // 扩散持续时间
    float speed = 300.0f;          // 扩散速度 (px/s)
};

struct SoundEventParams {
    std::string soundId;           // 音效资产 ID
    float volume = 1.0f;
    float pitch = 1.0f;
};

struct MaterialSwapParams {
    int materialId = 0;            // 切换到的材质 ID
    float duration = 0.5f;         // 持续时间 (0 = 永久)
};

// 统一参数容器
using EventParams = std::variant<
    ParticleEventParams,
    TrailEventParams,
    LightEventParams,
    ShakeEventParams,
    DistortionEventParams,
    SoundEventParams,
    MaterialSwapParams
>;

// 单个 VFX 事件
struct VFXEvent {
    float time = 0.0f;             // 触发时刻 (秒)
    EventType type = EventType::Particle;
    AnchorType anchor = AnchorType::Caster;
    EventParams params;
    render::core::QualityTier minTier = render::core::QualityTier::Low; // 最低画质要求
};

// VFX 序列定义 (资产)
struct VFXSequenceAsset {
    std::string name;
    float duration = 1.0f;         // 总时长
    std::vector<VFXEvent> events;
    render::core::QualityTier minTier = render::core::QualityTier::Low;
    int version = 1;               // Schema 版本
};
```

### 4.2 VFX 播放器 (Runtime)

```cpp
// ============================================================
// VFXPlayer.hpp - VFX 运行时播放器
// ============================================================

// ECS 组件 — 挂载到拥有 VFX 的实体
struct VFXPlayerComponent {
    int sequenceId = -1;           // 引用 VFXSequenceAsset 的 ID
    float elapsed = 0.0f;          // 当前播放进度
    int nextEventIdx = 0;          // 下一个待触发事件索引
    entt::entity target = entt::null; // 目标实体 (可选)
    bool loop = false;             // 是否循环
    bool active = true;
};

// VFX 序列管理器
class VFXSequenceManager {
public:
    static VFXSequenceManager& Get();

    void Initialize();
    void Shutdown();

    // 资产管理
    int LoadFromJson(const std::string& path); // 返回加载数量
    void TryHotReload();

    // 查询
    const VFXSequenceAsset* GetSequence(int id) const;
    const VFXSequenceAsset* GetSequence(const std::string& name) const;
    int GetSequenceId(const std::string& name) const;

    // 播放控制 (挂载到实体)
    void Play(entt::registry& registry, entt::entity entity,
              const std::string& sequenceName,
              entt::entity target = entt::null,
              bool loop = false);
    void Stop(entt::registry& registry, entt::entity entity);

    static constexpr int MAX_SEQUENCES = 256;

private:
    VFXSequenceManager() = default;

    std::vector<VFXSequenceAsset> m_sequences;
    std::unordered_map<std::string, int> m_nameToId;
    std::string m_assetDir;
    // 热重载文件监听
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;
};
```

### 4.3 VFX 系统 (ECS System)

```cpp
// ============================================================
// VFXSequencerSystem.hpp - 每帧驱动 VFX 播放
// ============================================================

class VFXSequencerSystem {
public:
    static void Update(entt::registry& registry, float dt);

private:
    // 事件触发分发
    static void DispatchEvent(entt::registry& registry,
                              entt::entity source,
                              const VFXEvent& event,
                              const VFXPlayerComponent& player);

    // 各事件类型的执行器
    static void ExecuteParticle(entt::registry& registry,
                                 Vector2 worldPos,
                                 const ParticleEventParams& params);
    static void ExecuteTrail(entt::registry& registry,
                              entt::entity source,
                              const TrailEventParams& params);
    static void ExecuteLight(entt::registry& registry,
                              Vector2 worldPos,
                              const LightEventParams& params);
    static void ExecuteShake(const ShakeEventParams& params);
    static void ExecuteDistortion(Vector2 worldPos,
                                   const DistortionEventParams& params);
    static void ExecuteSound(const SoundEventParams& params);
};
```

### 4.4 持久化契约 — JSON VFX 序列定义

文件路径: `assets/vfx/*.json`

```json
{
  "vfx_schema_version": 1,
  "name": "SwordSlash",
  "duration": 0.6,
  "minTier": "Low",
  "events": [
    {
      "time": 0.0,
      "type": "Light",
      "anchor": "Caster",
      "params": {
        "radius": 80.0,
        "intensity": 2.0,
        "color": [0.77, 0.97, 0.96],
        "duration": 0.3,
        "fadeInRatio": 0.1,
        "fadeOutRatio": 0.5
      }
    },
    {
      "time": 0.0,
      "type": "Particle",
      "anchor": "Caster",
      "minTier": "Medium",
      "params": {
        "materialId": "InkSplash",
        "count": 6,
        "speed": 50.0,
        "lifetime": 0.3,
        "scale": 1.5,
        "spreadAngle": 45.0,
        "blendMode": 0
      }
    },
    {
      "time": 0.15,
      "type": "Trail",
      "anchor": "Caster",
      "params": {
        "materialId": "HoloBlade",
        "duration": 0.2,
        "widthStart": 12.0,
        "widthEnd": 2.0,
        "colorStart": "0xC3F8F5FF",
        "colorEnd": "0xC3F8F500"
      }
    },
    {
      "time": 0.15,
      "type": "Shake",
      "params": {
        "intensity": 0.2
      }
    },
    {
      "time": 0.20,
      "type": "Distortion",
      "anchor": "Impact",
      "minTier": "High",
      "params": {
        "radius": 120.0,
        "strength": 0.4,
        "duration": 0.25,
        "speed": 400.0
      }
    },
    {
      "time": 0.20,
      "type": "Particle",
      "anchor": "Impact",
      "params": {
        "materialId": "FireGlow",
        "count": 12,
        "speed": 150.0,
        "speedVariance": 50.0,
        "lifetime": 0.4,
        "scale": 0.8,
        "spreadAngle": 360.0,
        "blendMode": 1
      }
    }
  ]
}
```

---

## 5. Screen Distortion Pass

### 5.1 架构

```
DistortionPass (新增):
├── 输入: 当前 LDR 帧 (PostProcess 输出)
├── 独立 FBO: Distortion Buffer (RG16F, 全分辨率)
│   ├── R 通道 = X 方向扭曲偏移
│   └── G 通道 = Y 方向扭曲偏移
├── 扭曲源写入: VFX Distortion 事件 → GPU 画圆扩散
└── 应用: 全屏 Fragment Shader 采样 LDR + Distortion Buffer
```

### 5.2 数据结构

```cpp
// GPU struct for distortion source (ring/shockwave)
// 16 bytes
struct GPUDistortionSource {
    float posX = 0.0f;            // 4  世界坐标 X (屏幕空间转换由 Shader 完成)
    float posY = 0.0f;            // 4
    float radius = 0.0f;          // 4  当前扩散半径
    float strength = 0.0f;        // 4  当前强度
};

static_assert(sizeof(GPUDistortionSource) == 16,
              "GPUDistortionSource must be 16 bytes");
```

### 5.3 DistortionPass 接口

```cpp
class DistortionPass final : public graph::RenderPass {
public:
    void Setup(graph::RenderGraphBuilder& builder) override;
    void Execute(graph::RenderContext& context) override;
    const char* GetName() const override { return "DistortionPass"; }

    bool Initialize(int width, int height);
    void Shutdown();
    void OnResize(int width, int height);

    // 添加扭曲源 (每帧由 VFXSequencerSystem 调用)
    void AddDistortionSource(float worldX, float worldY,
                              float radius, float strength);

    // 最大同时活跃扭曲源数
    static constexpr int MAX_DISTORTION_SOURCES = 32;

private:
    resources::FramebufferHandle m_distortionBuffer = {};
    Shader m_distortionWriteShader = {0};  // 将扭曲源写入 Distortion Buffer
    Shader m_distortionApplyShader = {0};  // 采样 Distortion Buffer 扭曲最终画面

    std::array<GPUDistortionSource, MAX_DISTORTION_SOURCES> m_sources{};
    int m_activeCount = 0;
    uint32_t m_ssbo = 0;
};
```

### 5.4 Shader

```glsl
// assets/shaders/postprocess/distortion_apply.frag
#version 430 core

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTexture;      // LDR 场景
uniform sampler2D uDistortionTexture; // Distortion Buffer (RG16F)
uniform float uDistortionScale;       // 全局强度缩放

void main() {
    vec2 distortion = texture(uDistortionTexture, fragTexCoord).rg;
    vec2 offset = distortion * uDistortionScale;
    vec2 uv = clamp(fragTexCoord + offset, 0.0, 1.0);
    fragColor = texture(uSceneTexture, uv);
}
```

---

## 6. Rendering Pipeline 集成

### 6.1 修改后的 Pass 执行顺序

```
ScenePass → LightingPass → VFXPass → PostProcessPass → DistortionPass → CompositePass
                                                          ^^^^^^^^^^^^^^^^^
                                                          新增 (Phase 4)
```

### 6.2 RenderConfig 扩展

```cpp
// RenderConstants.hpp 新增字段
struct RenderConfig {
    // ... (Phase 0-3 已有字段)

    // === Phase 4 新增 ===
    bool distortionEnabled = false;
    int maxMaterials = 0;          // GPU 端最大材质数
    bool materialSystemEnabled = false;
    int vfxSequenceDetail = 0;     // 0=minimal, 1=reduced, 2=full
    bool hotReloadEnabled = false; // 仅 Debug 构建启用
};
```

### 6.3 QualityTier 配置

| 配置项 | Low | Medium | High | Ultra |
|--------|-----|--------|------|-------|
| `distortionEnabled` | ❌ | ❌ | ✅ | ✅ |
| `maxMaterials` | 32 | 64 | 128 | 256 |
| `materialSystemEnabled` | ✅ | ✅ | ✅ | ✅ |
| `vfxSequenceDetail` | 0 (minimal) | 1 (reduced) | 2 (full) | 2 (full) |

### 6.4 Binding Point 分配

```cpp
// RenderConstants.hpp — 新增全局绑定点
namespace NoMoreDay::render::core::Binding {
    // ... 已有绑定点 ...
    constexpr int MATERIAL_SSBO = 8;       // 材质数据 SSBO
    constexpr int DISTORTION_SSBO = 9;     // 扭曲源 SSBO
}
```

---

## 7. 与现有系统的集成映射

### 7.1 GPUVisualStats → MaterialInstance 映射

| GPUVisualStats 字段 | MaterialInstance 映射 |
|---------------------|----------------------|
| `glowIntensity` | `emissiveIntensity` |
| `glowColorPacked` | `emissiveR/G/B` |

### 7.2 GPUParticle → MaterialId

- `GPUParticle.flags` 的高 16 位保留给 `materialId`（当前仅低位使用）。
- 粒子 Fragment Shader 使用 `materialId` 索引材质 SSBO 获取颜色/混合参数。
- 兼容路径：`materialId == 0` 时使用粒子自身的 `color` 字段（当前行为不变）。

### 7.3 VFXSequencer → 现有系统调用

| VFX 事件类型 | 调用的现有系统 |
|-------------|---------------|
| `Particle` | `GPUParticleSystem::Emit()` / `EmitBatch()` |
| `Trail` | `TrailSystem` (GPU Trail Path) |
| `Light` | `LightManager::AddTransientLight()` |
| `Shake` | `RenderSystem::AddScreenShake()` |
| `Distortion` | `DistortionPass::AddDistortionSource()` |
| `Sound` | `AudioManager` (已有) |

---

## 8. 预制 VFX 序列库 (至少 10 个)

| # | 名称 | 描述 | 事件组合 |
|---|------|------|----------|
| 1 | `SwordSlash` | 剑气横斩 | Light + Particle(Ink) + Trail + Shake |
| 2 | `FireExplosion` | 火焰爆炸 | Particle(Fire) + Light(Orange) + Distortion + Shake |
| 3 | `IceShatter` | 冰晶碎裂 | Particle(Ice) + Light(Blue) + Sound |
| 4 | `LightningStrike` | 闪电劈击 | Particle(Lightning) + Light(Flash) + Shake + Distortion |
| 5 | `HealPulse` | 治愈脉冲 | Particle(Green) + Light(Green) |
| 6 | `ShadowNova` | 暗影新星 | Particle(Shadow) + Distortion + Light(Purple) |
| 7 | `BladeFormation` | 灵剑决 | Light + Trail + Particle(HoloBlade) |
| 8 | `CriticalHit` | 暴击反馈 | Particle(Gold) + Shake(Small) + Sound |
| 9 | `DeathDissolve` | 死亡消融 | MaterialSwap(Dissolve) + Particle(Smoke) |
| 10 | `ItemDrop_Legendary` | 传说掉落 | Light(Beam) + Particle(Gold) + Sound |

---

## 9. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **SSBO Binding 冲突** | 材质 SSBO 与已有绑定点冲突 | 严格使用 `RenderConstants::Binding` 命名域，CI 检查 |
| **Distortion FBO 性能** | 集显上额外 RT 开销 | Low/Med Tier 直接禁用 Distortion |
| **JSON 解析失败** | 材质/VFX 加载失败 | 回退到安全默认 + 结构化日志 |
| **热重载线程安全** | 重载时渲染正在使用旧资源 | 双缓冲句柄，原子替换 |
| **材质 ID 溢出** | 超过 MAX_MATERIALS | 注册时检查并日志告警 |

---

## 10. 验收标准 (Acceptance Criteria)

### 功能验收
- [ ] **AC-01**: `MaterialManager` 可注册 constexpr 预设和 JSON 材质，SSBO 正确上传
- [ ] **AC-02**: 粒子 Shader 可通过 `materialId` 索引材质 SSBO 获取颜色/混合/发光参数
- [ ] **AC-03**: `VFXSequenceManager` 可加载 `assets/vfx/*.json` 并解析为 `VFXSequenceAsset`
- [ ] **AC-04**: `VFXSequencerSystem::Update()` 按时间线正确触发各类事件
- [ ] **AC-05**: 屏幕扭曲效果在 High/Ultra Tier 下可见，Low/Med 自动跳过
- [ ] **AC-06**: 至少 10 个预制 VFX 序列在游戏中可触发
- [ ] **AC-07**: JSON 热重载在 Debug 构建下可正常工作

### 性能验收
- [ ] **AC-08**: MaterialManager 同步开销 < 0.05ms/帧 (仅 dirty 时上传)
- [ ] **AC-09**: VFXSequencerSystem 更新 100 个活跃播放器 < 0.1ms
- [ ] **AC-10**: DistortionPass 在 2K 分辨率下 < 0.3ms (High Tier, 8 活跃源)

### 稳定性验收
- [ ] **AC-11**: 材质/VFX 资产缺失或解析失败时不崩溃，回退到默认并输出日志
- [ ] **AC-12**: 全量单测与集成测试通过

---

## 11. 已确认设计决策 (Confirmed Decisions)

| 问题 | 决策 |
|------|------|
| **材质存储方式** | constexpr 预设 + JSON 数据驱动双路径 |
| **材质上限** | MAX_MATERIALS = 256 |
| **materialId 传递** | 粒子 flags 高 16 位; Entity 使用 type 映射 |
| **Distortion 启用条件** | High/Ultra Tier |
| **VFX 精简策略** | minTier 字段控制事件是否触发 |
| **热重载范围** | 仅 Debug 构建启用 |

---

*规格版本: 1.0*
*最后更新: 2026-02-13*
