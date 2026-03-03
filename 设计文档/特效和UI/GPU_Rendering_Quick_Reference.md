# GPU 渲染系统 — AI Agent 快速访问手册

> **版本**: 1.1 | **日期**: 2026-03-03  
> **定位**: AI Agent 快速查阅渲染系统架构、数据结构、管线流程的**操作手册**  
> **ABI 版本**: `GPU_ABI_VERSION = 5` | **RenderGraph 契约版本**: `RENDERGRAPH_CONTRACT_VERSION = 3`  
> **图形 API**: OpenGL 4.3+ (MSVC-only, Windows)  
> **框架**: Raylib (底层) + 自研 RenderGraph / MDI / Compute 管线

---

## 目录

1. [目录结构速查](#1-目录结构速查) L29
2. [渲染管线总览](#2-渲染管线总览) L152
3. [RenderGraph 架构](#3-rendergraph-架构) L204
4. [SSBO Binding 分配表 ](#4-ssbo-binding-分配表) L255
5. [GPU 数据结构 (ABI)](#5-gpu-数据结构-abi) L296
6. [RenderConfig 与 Feature Flag](#6-renderconfig-与-feature-flag) L330
7. [Quality Tier 系统](#7-quality-tier-系统) L368
8. [Shader 系统](#8-shader-系统) L404
9. [核心子系统速查](#9-核心子系统速查) L432
10. [ABI 治理规则](#10-abi-治理规则) L492
11. [常用操作模板](#11-常用操作模板) L515
12. [性能预算表](#12-性能预算表) L562
13. [约束与已知陷阱](#13-约束与已知陷阱) L593

---

## 1. 目录结构速查

```
src/engine/render/
├── RenderSystem.hpp/cpp          # 顶层入口：组装 RenderGraph、调度所有子系统
├── RenderConstants.hpp           # 全局 SSBO Binding / Barrier / TextureUnit 枚举
├── GPUData.hpp                   # 所有 GPU ABI 结构体定义 (GPUEntity, GPULight, GPUParticle...)
├── GPUUtils.hpp/cpp              # OpenGL 底层工具函数
├── ComputeBuffer.hpp             # 简单 SSBO 封装
├── PersistentBuffer.hpp/cpp      # 持久映射双缓冲 SSBO
├── MDIRenderer.hpp/cpp           # Multi-Draw Indirect 渲染器 (实体主渲染)
├── GPUEntitySystem.hpp/cpp       # 实体 GPU 同步 (Slot管理/物理/视觉同步)
├── GPUEntitySync.hpp/cpp         # SlotManager / PhysicsSync / VisualSync
├── GPUTextSystem.hpp/cpp         # V4 MSDF GPU 文字渲染
├── GPULootSystem.hpp/cpp         # V4 GPU 战利品渲染 (力导向避让)
├── GPUParticleSystem.hpp/cpp     # GPU 粒子系统 (Compute)
├── GPUSkillEffectSystem.hpp/cpp  # 技能特效 GPU 系统
├── GPUFlowFieldSystem.hpp/cpp    # GPU 流场寻路
├── MaterialManager.hpp/cpp       # 材质系统 (Schema V3, SSBO 同步)
├── MaterialDefs.hpp              # 材质定义与 JSON Schema
├── LootTextBatcher.hpp/cpp       # 战利品文字批处理
├── PopupRenderer.hpp/cpp         # [已废弃] CPU 伤害数字渲染 (Low/Med 回退)
├── UIRenderer.hpp/cpp            # UI 渲染
│
├── core/
│   ├── RenderConstants.hpp       # QualityTier 枚举 / RenderConfig 结构体 / 预算常量
│   ├── QualityTierManager.hpp/cpp# 画质分级管理 (自动检测/降级/Feature Flag路由)
│   ├── BindingRegistry.hpp/cpp   # SSBO Binding 注册与冲突检查
│   ├── RenderSyncContracts.hpp   # 帧同步契约
│   └── ScopedGLState.hpp         # RAII GL 状态管理
│
├── graph/
│   ├── RenderGraph.hpp/cpp       # RenderGraph 核心 (AddPass/Build/Execute/Validate)
│   ├── RenderPass.hpp            # RenderPass 基类 (Setup/Execute/GetName)
│   └── RenderContext.hpp         # Pass 间共享上下文
│
├── passes/                       # === 所有渲染 Pass 实现 ===
│   ├── ScenePass                 # HDR 场景绘制 (MDI 实体 + PBR 材质)
│   ├── LightCullingPass          # Clustered Forward+ 光源剔除 (Compute)
│   ├── LightingPass              # 光照累积 (BRDF-Lite)
│   ├── ShadowPreparePass         # 阴影准备 (遮挡体收集)
│   ├── ShadowBuildPass           # 阴影构建 (SDF Compute)
│   ├── ShadowResolvePass         # 阴影合成
│   ├── HeightShadowPass          # V4 高度图光线步进阴影
│   ├── OccluderExtractPass       # V5 遮挡体提取 → OccluderMask
│   ├── JFAPass                   # V5 Jump Flood 距离场
│   ├── RadianceCascadesPass      # V5 Radiance Cascades GI
│   ├── GICompositePass           # V5 GI 合成到场景
│   ├── FluidSimulationPass       # V5 SPH 流体 (NO-GO)
│   ├── VFXPass                   # 粒子/轨迹/特效
│   ├── GPUTextPass               # V4 GPU 文字 MDI 绘制
│   ├── GPULootPass               # V4 GPU 战利品 MDI 绘制
│   ├── UIWorldPass               # 世界空间 UI
│   ├── DistortionPass            # 扭曲后处理
│   ├── VolumetricLightPass       # 体积光 (Ultra)
│   ├── PostProcessPass           # Bloom/Tonemap/FXAA/Vignette/ColorGrading
│   └── CompositePass             # 最终合成到 BackBuffer
│
├── lighting/
│   ├── LightManager.hpp/cpp      # 光源管理/SSBO 同步
│   ├── ClusteredLightingState.hpp/cpp # Clustered 状态 (Cluster SSBO)
│   └── GlobalHeightField.hpp/cpp # V4 全局高度场管理
│
├── shadow/
│   ├── ShadowAtlasAllocator      # 阴影图集分配 (确定性淘汰)
│   ├── ShadowSDFMath             # SDF 阴影数学
│   └── OccluderCollector         # 遮挡体收集器
│
├── gi/
│   └── JFADistanceFieldEvaluator # V5 JFA 距离场评估器
│
├── resource/
│   ├── MSDFAtlasLoader           # V4 MSDF 字体图集加载
│   └── TextureArrayManager       # V4 Texture2DArray 管理
│
├── resources/
│   └── FramebufferHandle.hpp     # FBO 安全句柄
│
├── particle/                     # 粒子子系统细节
├── trail/                        # 轨迹子系统细节
├── fluid/                        # SPH 参考实现
├── debug/
│   ├── RenderProfiler.hpp/cpp    # GPU/CPU 性能采样
│   └── ProfilerHudRenderer.cpp   # Profiler HUD 绘制
│
└── dev/
    └── ShaderHotReloadManager    # Shader 热重载 (0.5s 轮询)

assets/shaders/
├── entity_mdi.vert/frag          # MDI 实体渲染 Shader (含 PBR 分支)
├── entity.vert/frag              # 传统实体 Shader
├── cull.compute                  # 视锥剔除 Compute
├── generated/
│   ├── gpu_abi.glslinc           # [自动生成] GPU ABI GLSL 结构体
│   └── material_abi.glslinc      # [自动生成] 材质 ABI
├── lighting/
│   ├── light_culling.comp        # Clustered 光源剔除
│   ├── light_accumulation.frag   # 光照累积
│   ├── shadow_sdf.comp           # SDF 阴影 Compute
│   ├── height_shadow_apply.frag  # 高度阴影应用
│   ├── v5_*.comp                 # V5 JFA/GI/Fluid 全部 Compute
│   └── volumetric_light.frag     # 体积光
├── loot/
│   ├── loot_frustum_cull.compute # 战利品视锥剔除
│   ├── loot_indirect_args.compute# 间接绘制参数生成
│   ├── loot_grid_hash.compute    # 空间网格散列
│   ├── loot_repulsion.compute    # 斥力计算
│   └── loot_position_update.compute # 位置更新
├── text/
│   ├── text_layout.compute       # MSDF 文字排版 Compute
│   └── text_quad.vert/frag       # 文字 Quad 渲染
├── postprocess/                  # Bloom/Tonemap/FXAA/等
├── trail/                        # 轨迹 Shader
└── vfx/                          # VFX 特效 Shader

tools/render_abi/
├── abi_manifest.json             # ABI 结构体清单 (唯一真相源)
├── generate_gpu_abi.py           # GLSL ABI 生成脚本
└── check_no_manual_abi_structs.py# ABI 手写检测脚本
```

---

## 2. 渲染管线总览

### 2.1 帧执行顺序

```
GameLoop::Update()
  ├── Input → Player Movement → AI → Combat → Spatial Grid → Physics
  └── RenderSystem::render()
        ├── GPUEntitySystem::UploadGPU()      // 实体数据 CPU→GPU
        ├── GPUTextSystem::Update()           // 文字命令上传
        ├── GPULootSystem::Update()           // 战利品数据同步
        ├── LightManager::Sync()              // 光源 SSBO 同步
        ├── MaterialManager::Sync()           // 材质 SSBO 同步
        ├── RenderGraph::Build()              // 构建 Pass 拓扑
        └── RenderGraph::Execute()            // 按序执行所有 Pass
              ├── ScenePass          (Write: SceneColor)
              ├── ShadowPreparePass  (Read: SceneColor)
              ├── ShadowBuildPass    (Write: ShadowAtlas)
              ├── ShadowResolvePass  (Read: ShadowAtlas → Write: SceneColor)
              ├── LightCullingPass   (Compute → Write: ClusterSSBO)
              ├── LightingPass       (Read: Clusters+Lights → Write: SceneColor)
              ├── HeightShadowPass   (Read: HeightMap → Write: SceneColor)
              ├── OccluderExtractPass (Write: OccluderMask)
              ├── JFAPass            (Read: OccluderMask → Write: DistanceField)
              ├── RadianceCascadesPass(Read: DF+Emissive → Write: RadianceMap)
              ├── GICompositePass    (Read: Radiance → Write: SceneColor)
              ├── VFXPass            (Write: SceneColor)
              ├── GPUTextPass        (Read: TextQuadSSBO → Write: SceneColor)
              ├── GPULootPass        (Read: LootSSBO → Write: SceneColor)
              ├── UIWorldPass        (Write: SceneColor)
              ├── VolumetricLightPass (Write: SceneColor)
              ├── PostProcessPass    (Read: SceneColor → Write: PostProcessColor)
              ├── DistortionPass     (Read: PostProcess → Write: DistortionColor)
              └── CompositePass      (Read: Distortion → Write: BackBuffer)
```

### 2.2 FBO 资源流

| 资源 Tag | 格式 | 用途 |
|----------|------|------|
| `SceneHdrColor` | RGBA16F | HDR 场景缓冲 (主渲染目标) |
| `SceneDepth` | DEPTH24 | 深度缓冲 |
| `PostProcessLdrColor` | RGBA8 | Bloom/Tonemap 后 LDR |
| `DistortionLdrColor` | RGBA8 | 扭曲结果 |
| `FinalOutputColor` | BackBuffer | 最终输出 (FBO 0) |
| `OccluderMask` | R8 | V5 遮挡体掩码 |
| `DistanceField` | R16F | V5 JFA 距离场 |
| `EmissiveBuffer` | RGBA16F | V5 发光体缓冲 |
| `RadianceMap` | RGBA16F | V5 GI 辐射图 |

---

## 3. RenderGraph 架构

### 3.1 核心接口

```cpp
// 所有 Pass 继承此基类
class RenderPass {
public:
  virtual void Setup(RenderGraphBuilder &builder) = 0;  // 声明资源依赖
  virtual void Execute(RenderContext &context) = 0;      // 执行渲染
  virtual const char *GetName() const = 0;               // Pass 名称
};

// RenderContext — Pass 间共享的上下文
struct RenderContext {
  entt::registry *registry;
  const SharedContext *shared;
  const Camera2D *camera;
  TransientResourcePool *transientPool;
  QualityTierManager *qualityManager;
  RenderProfiler *renderProfiler;
  FramebufferHandle hdrSceneBuffer;
  // V5 GI 纹理句柄
  uint32_t giDistanceFieldTexture, giEmissiveTexture, giRadianceTexture;
  int giDistanceFieldWidth, giDistanceFieldHeight;
  // ...
};
```

### 3.2 新建 RenderPass 模板

```cpp
// 1. 创建 passes/MyPass.hpp
class MyPass : public RenderPass {
public:
  void Setup(RenderGraphBuilder &builder) override {
    builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
    builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::MyOwner);
  }
  void Execute(RenderContext &ctx) override { /* 渲染逻辑 */ }
  const char *GetName() const override { return "MyPass"; }
};

// 2. 在 RenderSystem::render() 中注册
renderGraph.AddPass(std::make_shared<MyPass>());

// 3. 如需新 OwnerTag，在 RenderGraph.hpp 的 RenderOwnerTag 枚举中添加
```

---

## 4. SSBO Binding 分配表

### 4.1 全局共享 Binding (持久)

| Binding | 枚举名 | 数据结构 | 拥有者 |
|:---:|---|---|---|
| 0 | `SSBO_ENTITY_DATA` | `GPUEntity` (64B) | GPUEntitySystem |
| 1 | `SSBO_VISIBLE_ID` | `uint32_t[]` | MDIRenderer (剔除输出) |
| 2 | `SSBO_COMMAND` | `DrawArraysIndirectCommand` | MDIRenderer |
| 3 | `SSBO_VISUAL_STATS` | `GPUVisualStats` (16B) | MDIRenderer |
| 4 | `SSBO_LABEL_INSTANCE` | `GPULabelInstance` | RenderSystem |
| 5 | `SSBO_BEAM_INSTANCE` | `GPUBeamInstance` | RenderSystem |
| 6 | `SSBO_SKILL_EFFECTS` | `GPUSkillEffect` | GPUSkillEffectSystem |
| 7 | `SSBO_POPUP_DATA` | `GPUPopup` | PopupRenderer |
| 8 | `SSBO_GLYPH_INSTANCE` / `SSBO_TEXT_QUAD` | `GPUTextQuad` (40B) | GPUTextPass |
| 9 | `SSBO_LIGHT_DATA` | `GPULight` (64B) | LightManager |
| 10 | `SSBO_TRAIL_HEADERS` | `GPUTrailHeader` (32B) | TrailSystem |
| 11 | `SSBO_TRAIL_POINTS` | `GPUTrailPoint` (32B) | TrailSystem |
| 12 | `SSBO_MATERIAL_DATA` | `GPUMaterialDataV3` (64B) | MaterialManager |
| 13 | `SSBO_DISTORTION_DATA` | `GPUDistortionSource` (16B) | DistortionPass |
| 14 | `SSBO_HOLOBLADE_INSTANCE` | HoloBlade 数据 | HoloBladeSystem |
| 15 | `SSBO_LOOT_INSTANCE` | `GPULootInstance` (32B) | GPULootSystem |

> ⚠️ **OpenGL 4.3 最低保证 16 个 SSBO Binding**，目前已用满 0-15。

### 4.2 Compute Shader 本地 Binding (临时)

Compute Shader 独占执行，其内部 binding 不与全局冲突：

| 命名空间 | 绑定分配 |
|----------|----------|
| `ParticleCS` | 0-6 (粒子输入/输出/间接/原子/力场/子发射) |
| `TextLayoutCS` | 0-5 (命令/度量/索引/Quad/计数/字串元数据) |
| `FlowFieldCS` | 0-4 (代价/积分读写/流向/密度) |
| `EntityCS` | 0-3 (实体/可见/间接/视觉) |
| `StatsScatterCS` | 0-1 (更新/主统计) |
| `FogOfWarCS` | 0 (可见性) |
| `V5GI` | Image 0-5 (遮挡/种子IO/距离场/发光/辐射) |

---

## 5. GPU 数据结构 (ABI)

> 所有结构体定义在 `src/engine/render/GPUData.hpp`  
> 对应 GLSL 由 `tools/render_abi/generate_gpu_abi.py` 从 `abi_manifest.json` 自动生成  
> 输出到 `assets/shaders/generated/gpu_abi.glslinc`

| 结构体 | 大小 | 用途 | Binding |
|--------|:---:|------|:---:|
| `GPUEntity` | 64B | 实体物理/变换/标志 | 0 |
| `GPUVisualStats` | 16B | 发光/状态效果 | 3 |
| `GPUParticle` | 64B | 粒子 (位置/速度/生命/颜色/动画) | ParticleCS:0 |
| `GPUMaterialDataV3` | 64B | 材质 Schema V3 (颜色/法线/粗糙/金属/纹理槽) | 12 |
| `GPULight` | 64B | 光源 (位置/半径/颜色/类型/阴影/优先级) | 9 |
| `GPUDistortionSource` | 16B | 屏幕扭曲源 | 13 |
| `GPUTrailHeader` | 32B | 轨迹头 (索引/计数/宽度/颜色) | 10 |
| `GPUTrailPoint` | 32B | 轨迹采样点 | 11 |
| `GPUForceField` | 32B | 力场 (径向/涡旋/噪声) | ParticleCS:4 |
| `GPUTextCommand` | 16B | V4 文字渲染指令 | TextLayoutCS:0 |
| `GPUGlyphMetrics` | 40B | V4 字形度量 (UV/偏移/进给) | TextLayoutCS:1 |
| `GPUTextQuad` | 40B | V4 排版输出 Quad | 8 |
| `GPULootInstance` | 32B | V4 战利品实例 | 15 |
| `GPULabelInstance` | — | 标签实例 | 4 |

### 光源类型枚举

```cpp
enum class LightType : uint8_t {
  PointLight = 0, SpotLight = 1, AmbientZone = 2,
  AreaLight = 3,  LineLight = 4,
};
```

---

## 6. RenderConfig 与 Feature Flag

`RenderConfig` 定义在 `src/engine/render/core/RenderConstants.hpp`，是所有渲染功能的**运行时开关**。

### 6.1 核心 Feature Flag 速查

| Flag | 类型 | 默认 | 控制范围 |
|------|:---:|:---:|----------|
| `bloomEnabled` | bool | false | Bloom 后处理 |
| `dynamicLightingEnabled` | bool | false | 动态光照全局开关 |
| `shadowEnabled` | bool | false | 阴影系统 |
| `shadowMode` | ShadowMode | Off | Off/SDF/Hybrid |
| `clusteredLightingEnabled` | bool | false | V3 Clustered Lighting |
| `clusteredLightingV4Enabled` | bool | false | 兼容标记（LightCulling 不再走 V4 256 光源回退分支） |
| `normalLightingEnabled` | bool | false | 法线光照 |
| `specularEnabled` | bool | false | 高光 |
| `heightShadowEnabled` | bool | false | V4 高度阴影 |
| `pomEnabled` | bool | false | V4 视差遮挡 |
| `gpuTextEnabled` | bool | false | V4 GPU 文字 (否则 CPU 回退) |
| `gpuLootEnabled` | bool | false | V4 GPU 战利品 (否则 CPU) |
| `giEnabled` | bool | false | V5 全局光照 |
| `fluidEnabled` | bool | false | V5 流体 (NO-GO) |
| `v3Enabled` | bool | false | V3 总开关 |
| `distortionEnabled` | bool | false | 扭曲后处理 |
| `volumetricLightEnabled` | bool | false | 体积光 (Ultra) |
| `colorGradingEnabled` | bool | false | 色彩分级 |
| `materialSystemEnabled` | bool | false | 材质系统 |
| `profilerHudEnabled` | bool | false | 性能 HUD |
| `shaderHotReloadEnabled` | bool | false | Shader 热重载 |

### 6.2 持久化路径

`settings.json` → `render.v3.*` / `render.gpuText.enabled` / `render.gpuLoot.enabled` / `render.gi.enabled`

`QualityTierManager::Initialize()` 读取 settings.json → 按 Tier 填充 RenderConfig → 应用 override。

---

## 7. Quality Tier 系统

### 7.1 四档画质

| Tier | 典型场景 | 关键特性 |
|:---:|---|---|
| **Low** | 集显/低端 | Bloom关/阴影关/Clustered关/CPU文字/CPU战利品 |
| **Medium** | 入门独显 | 基础Bloom/SDF阴影/256光源/CPU文字/CPU战利品 |
| **High** | 中端独显 | 全Bloom/Hybrid阴影/1024光源/GPU文字/GPU战利品/GI基础/16步高度阴影 |
| **Ultra** | 高端独显 | 全特效/4096光源/全GI/64步高度阴影+自投影/POM/体积光/色彩分级 |

### 7.2 自动降级顺序 (超预算时)

```
1. ReduceBloom        → Bloom Mip 减少
2. DisableDistortion  → 扭曲关闭
3. LimitDynamicLights → 光源上限: 4096→1024→256
4. ReduceClusteredPressure → Clustered 降压
5. HybridShadowToSDF  → 混合阴影→纯SDF
6. DisableHighMaterialBranch → 材质高分支关闭
```

### 7.3 API

```cpp
auto &mgr = QualityTierManager::Get();
mgr.Initialize("settings.json");          // 初始化 (自动检测+基准)
mgr.GetTier();                             // 当前 Tier
mgr.GetConfig();                           // 当前 RenderConfig
mgr.ForceTier(QualityTier::Ultra);         // 强制 Tier
mgr.SetV3Enabled(true);                    // V3 开关
mgr.IncreaseAutoDegradeLevel("budget", ms, budget); // 触发降级
```

---

## 8. Shader 系统

### 8.1 关键 Shader 文件

| 文件 | 类型 | 用途 |
|------|:---:|------|
| `entity_mdi.vert/frag` | VS/FS | MDI 实体主渲染 (含 PBR 材质分支) |
| `cull.compute` | CS | MDI 视锥剔除 |
| `lighting/light_culling.comp` | CS | Clustered 光源剔除 (4096) |
| `lighting/light_accumulation.frag` | FS | BRDF-Lite 光照累积 |
| `lighting/shadow_sdf.comp` | CS | SDF 阴影计算 |
| `lighting/height_shadow_apply.frag` | FS | 高度阴影应用 |
| `text/text_layout.compute` | CS | MSDF 文字排版 (前缀和) |
| `text/text_quad.vert/frag` | VS/FS | MSDF 文字 Quad 绘制 |
| `loot/loot_*.compute` | CS | 战利品 5 阶段 Compute |
| `lighting/v5_*.comp` | CS | V5 所有 Compute (JFA/GI/Fluid) |
| `generated/gpu_abi.glslinc` | Include | [自动生成] 勿手编 |

### 8.2 Shader Include 预处理

`ResourceManager` 在加载 Shader 时自动预处理 `#include "..."` 指令（vertex/fragment/compute 均支持），编译使用内存源码。

### 8.3 热重载

`ShaderHotReloadManager` 以 **0.5s** 轮询监控 `assets/shaders/` 目录变更，双缓冲句柄 + 验证后替换，失败自动回退。

---

## 9. 核心子系统速查

### 9.1 MDIRenderer — 实体主渲染

```
Init(rm, maxEntities) → Update(rm, entityBuf, alpha) → Cull(rm, entities, viewBounds) → Render(rm, entities, alpha)
```

- 使用 `PersistentBuffer` 双缓冲 (SSBO 0/1/2/3)
- Compute Shader 视锥剔除 → 写入 `SSBO_VISIBLE_ID` + `SSBO_COMMAND`
- `glMultiDrawArraysIndirect` 单次绘制所有可见实体

### 9.2 GPUEntitySystem — 实体 GPU 同步

```
Init() → Update(ctx, dt) [物理CS] → UploadGPU(ctx) [增量上传] → SyncBack(registry) [位置回读]
```

- `GPUSlotManager`: 管理实体→GPU槽位映射 (O(1) 分配/回收)
- `GPUPhysicsSync`: 网格化 Compute 物理
- `GPUVisualSync`: 增量更新视觉状态
- 脏块追踪: 1024 实体一块，仅上传脏块

### 9.3 GPUTextSystem — V4 MSDF 文字

```
Initialize(atlasTexture, glyphMetrics) → SubmitText(cmd) → Update(dt) → [TextLayoutCS] → GPUTextPass::Execute()
```

- MSDF Atlas: 4096² RGB8
- Compute 排版: warp-level 前缀和
- MDI Quad 绘制: `GPUTextQuad` SSBO → instanced draw
- Feature Flag: `gpuTextEnabled` / 未就绪自动回退 CPU

### 9.4 GPULootSystem — V4 战利品渲染

```
Initialize() → SyncFromECS(registry) → [FrustumCullCS → IndirectArgsCS → GridHashCS → RepulsionCS → PositionUpdateCS] → GPULootPass::Execute()
```

- 5 阶段 Compute 管线
- 力导向标签避让 (32px Cell, 阻尼+锁定)
- Feature Flag: `gpuLootEnabled`

### 9.5 LightManager — 光源管理

- 管理所有 `GPULight` 数据
- 每帧同步到 `SSBO_LIGHT_DATA` (Binding 9)
- 支持 5 种光源类型 (Point/Spot/Ambient/Area/Line)
- 最大光源数: 4096 (V4, Ultra)

### 9.6 MaterialManager — 材质系统

- Schema V3: `GPUMaterialData` 64B
- `Texture2DArray` 分层管理 (Albedo/Normal/Mask/Detail)
- JSON 资产解析 + 热重载
- SSBO Binding 12

---

## 10. ABI 治理规则

### 强制规则

1. **唯一真相源**: `tools/render_abi/abi_manifest.json`
2. **GLSL 自动生成**: 运行 `python tools/render_abi/generate_gpu_abi.py` → 输出 `gpu_abi.glslinc`
3. **禁止手写 GLSL struct**: 必须通过生成链路
4. **`static_assert(sizeof(...))`**: 所有 ABI 结构体必须有大小断言
5. **C++ 与 GLSL 的 `GPU_ABI_VERSION` 必须匹配** (当前=5)
6. **变更必须递增版本号**
7. **CI 检查**: `build.bat analyze` 调用 `check_no_manual_abi_structs.py`

### 新增 ABI 结构体流程

```
1. GPUData.hpp 中定义 C++ struct + static_assert
2. abi_manifest.json 中注册
3. python tools/render_abi/generate_gpu_abi.py 重新生成
4. 验证: build.bat → ctest -L ci
```

---

## 11. 常用操作模板

### 11.1 新增 SSBO 数据

```cpp
// 1. GPUData.hpp — 定义结构体
struct GPUMyData { float x, y; uint32_t flags; float pad; }; // 16B
static_assert(sizeof(GPUMyData) == 16);

// 2. RenderConstants.hpp — 如需全局 binding (目前已满 0-15!)
//    → 优先使用 Compute 本地 binding 或复用时间片

// 3. 创建 ComputeBuffer 或 PersistentBuffer
ComputeBuffer myBuffer;
myBuffer.Create(maxCount * sizeof(GPUMyData), GL_DYNAMIC_DRAW);
```

### 11.2 新增 Feature Flag

```cpp
// 1. core/RenderConstants.hpp → RenderConfig 添加字段
bool myFeatureEnabled = false;

// 2. core/QualityTierManager.cpp → UpdateConfigForTier() 按 Tier 设置
// 3. settings.json 添加 render.myFeature.enabled 持久化
// 4. 渲染代码中通过 ctx.qualityManager->GetConfig().myFeatureEnabled 判断
```

### 11.3 新增 Compute Shader

```glsl
// assets/shaders/my_compute.comp
#version 430 core
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer MyInput { ... };
layout(std430, binding = 1) buffer MyOutput { ... };
void main() { /* ... */ }
```

```cpp
// C++ 端
Shader cs = ResourceManager::LoadComputeShader("assets/shaders/my_compute.comp");
// 绑定 SSBO → glDispatchCompute → glMemoryBarrier
```

---

## 12. 性能预算表

| Pass | 常规 (≥270FPS) | 高压 (≥180FPS) | 极限 (≥144FPS) |
|------|:---:|:---:|:---:|
| ScenePass (PBR) | 1.3ms | 1.8ms | 2.2ms |
| LightCullingPass | 0.20ms | 0.40ms | 0.60ms |
| ShadowPass | 0.40ms | 0.90ms | 1.30ms |
| LightingPass (BRDF) | 0.70ms | 1.10ms | 1.40ms |
| HeightShadowPass | 0.30ms | 0.60ms | 0.90ms |
| VFXPass | 0.50ms | 0.80ms | 1.00ms |
| GPUTextPass | 0.05ms | 0.10ms | 0.15ms |
| GPULootPass | 0.05ms | 0.10ms | 0.20ms |
| PostProcess | 0.30ms | 0.50ms | 0.80ms |
| Composite | 0.10ms | 0.15ms | 0.20ms |
| **渲染总计** | **3.90ms** | **6.45ms** | **8.75ms** |

### GPU 容量常量 (`RenderConstants.hpp`)

| 常量 | 值 | 说明 |
|------|:---:|------|
| `GPU::MAX_ENTITIES` | 200,000 | 最大实体数 |
| `GPU::MAX_PARTICLES` | 200,000 | 最大粒子数 |
| `GPU::MAX_SKILL_EFFECTS` | 1,024 | 技能特效上限 |
| `GPU::MAX_POPUPS` | 2,048 | 弹出文字上限 |
| `GPU::MAX_GLYPHS` | 4,096 | 文字字形上限 |
| `kMaxTotalClusteredLights` | 4,096 | Clustered 光源上限 |
| `kMaxLightsPerCluster` | 64 | 每 Cluster 光源上限 |
| `Shadow::kMaxShadowCasters` | 8,192 | 阴影投射体上限 |

---

## 13. 约束与已知陷阱

### 13.1 硬性约束

| 约束 | 说明 |
|------|------|
| **SSBO Binding 已满** | 全局 0-15 全部占用，新增数据必须用 Compute 本地 binding 或时间片复用 |
| **FBO 0 禁止硬编码** | 除 CompositePass 最终输出外，禁止写入 FBO 0 |
| **帧序不可变** | Input→Movement→AI→Combat→SpatialGrid→Physics→Render |
| **MSVC Only** | 仅支持 MSVC 编译器 |
| **WIN32_LEAN_AND_MEAN + NOMINMAX** | 必须定义，避免 Windows 头文件冲突 |
| **DrawText 冲突** | WinAPI vs Raylib 的 `DrawText` 必须消歧 |

### 13.2 常见陷阱

| 陷阱 | 解决方案 |
|------|----------|
| Shader `#include` 不工作 | ResourceManager 自动预处理，不要依赖驱动 include |
| GPU 功能初始化失败无回退 | **必须检查 IsInitialized()**，失败回退 CPU 路径 |
| ABI struct 手写 GLSL | 被 CI 拒绝，必须用生成链路 |
| rlgl 状态污染 | Pass 边界强制 `rlDrawRenderBatchActive()` + ScopedGLState |
| Resize 后 FBO 失效 | FramebufferHandle 自动重建/重设 |
| `GPUUtils` 状态丢失 | 已修复 (BUG-20260213-001)，注意 Pass 间状态隔离 |
| PersistentBuffer 跨帧读写 | 必须用 Fence 同步，参考 PersistentBuffer::WaitSync() |

### 13.3 版本演进关系

```
V2 (已完成) → RenderGraph/MDI/粒子/轨迹/材质/VFX/后处理
V3 (已完成) → ABI治理/Clustered/阴影/Material2.0/VFX联动
V4 (已完成) → GPU Text/Loot/PBR材质/4096光源/高度阴影/POM
V5 (已完成) → JFA距离场/Radiance Cascades GI/SPH(NO-GO)
```

---

> **修订记录**  
> - 2026-03-03: 更新 Material ABI 命名（`GPUMaterialDataV3`），补充 `clusteredLightingV4Enabled` 在 LightCulling 中不再控制 256 光源回退分支
> - 2026-02-21: 首版，基于 V2-V5 全部已完成状态编写
