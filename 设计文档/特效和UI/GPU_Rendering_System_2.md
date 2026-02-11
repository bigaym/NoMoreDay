# GPU 渲染系统 2.0 — 完整设计规格书

> **文档版本**: 2.0  
> **创建日期**: 2026-02-12  
> **定位**: 后续所有渲染子模块设计和开发的**根本参考文档**  
> **来源**: GPU Rendering System Audit 审计会话  

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [当前系统现状审计](#2-当前系统现状审计)
3. [核心设计决策](#3-核心设计决策)
4. [总体架构：RenderGraph](#4-总体架构rendergraph)
5. [FBO 管理与后处理管线](#5-fbo-管理与后处理管线)
6. [增强粒子系统 & 轨迹渲染器](#6-增强粒子系统--轨迹渲染器)
7. [动态 2D 光照系统](#7-动态-2d-光照系统)
8. [材质系统](#8-材质系统)
9. [VFX 序列器](#9-vfx-序列器)
10. [Quality Tier 配置系统](#10-quality-tier-配置系统)
11. [分阶段实施路线](#11-分阶段实施路线)
12. [附录：关键数据结构汇总](#12-附录关键数据结构汇总)
13. [GPU ABI 契约（CPU/Shader 一致性）](#13-gpu-abi-契约cpushader-一致性)
14. [Binding Registry 与 Pass 资源命名域](#14-binding-registry-与-pass-资源命名域)
15. [Frame Ownership 与渲染目标生命周期](#15-frame-ownership-与渲染目标生命周期)
16. [GL 状态契约（含 rlgl 互操作）](#16-gl-状态契约含-rlgl-互操作)
17. [性能预算与硬性门槛](#17-性能预算与硬性门槛)
18. [能力探测与 Tier 回退矩阵](#18-能力探测与-tier-回退矩阵)
19. [资产 Schema 版本化与热重载规则](#19-资产-schema-版本化与热重载规则)
20. [验证与发布门禁（DoD）](#20-验证与发布门禁dod)

---

## 1. 项目背景与目标

### 1.1 审计结论

当前 GPU 渲染系统在**性能**方面已达生产级水平（20万实体/20万粒子），但**特效表现力严重不足**——缺乏后处理管线、光照系统、纹理粒子、轨迹渲染、材质系统以及 VFX 编排能力，无法呈现"绚烂、优雅"的视觉效果。

### 1.2 目标定义

- **视觉标准**：**暗黑破坏神 IV / 流放之路** 级别
  - 全屏 Bloom、HDR、动态光源、复杂粒子链、物理化轨迹、完整后处理管线
- **技术基线**：OpenGL 4.3，支持现代集显（Intel Iris Xe）和独显
- **兼容策略**：通过 **Quality Tier 配置系统**实现从集显到独显的自适应，所有高级特性均有降级路径
- **架构策略**：**混合模式** — GPU 管线完全自建负责主战场渲染和后处理，Raylib 层保留负责 UI 面板和基础工具

### 1.3 非目标

- 不更换底层框架（保留 Raylib/GLFW）
- 不迁移到 Vulkan/Metal
- 不推翻已有成熟子系统（GPUEntitySystem、GPUParticleSystem、MDIRenderer 等）

---

## 2. 当前系统现状审计

| 子系统 | 状态 | 性能 | 特效自由度 |
|--------|------|------|-----------|
| **GPUEntitySystem** (MDI + Compute Physics) | ✅ 成熟 | 20万实体 | 低 (仅纹理切换) |
| **GPUParticleSystem** (Compute + Indirect) | ✅ 成熟 | 20万粒子 | **低** (无纹理/轨迹) |
| **GPUSkillEffectSystem** (SDF Instanced) | ⚠️ 基础 | 1024效果 | **极低** (仅3种SDF形状) |
| **PopupRenderer** (Instanced) | ✅ 成熟 | 2048飘字 | 低 |
| **GPUFlowFieldSystem** (Compute) | ✅ 成熟 | N/A | N/A |
| **RenderSystem** (主管线编排) | ⚠️ 杂糅 | - | - |
| **后处理 (Bloom/HDR/Distortion)** | ❌ **不存在** | - | - |
| **场景光照** | ❌ **不存在** | - | - |
| **材质系统** | ❌ **不存在** | - | - |
| **RenderGraph / Pass 管理** | ❌ **不存在** | - | - |
| **VFX Timeline/Sequence** | ❌ **不存在** | - | - |

### 关键问题

- `RenderSystem::render()` 是一个 **579 行的巨型函数**，所有渲染逻辑杂糅在一起
- Raylib 的 `rlgl` batch renderer 会**偷偷修改 GL 状态**，每次自定义 GPU 渲染前后都需要 flush
- 所有渲染参数（颜色、发光强度等）硬编码在 C++ 或着色器中，无数据驱动能力

---

## 3. 核心设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| **管线架构** | Render Graph（方案 A） | D4/PoE 级别特效需要灵活的 Pass 组合能力，长期可持续性最佳 |
| **Raylib 依赖** | 混合模式（方案 B） | 保留 Raylib 在 UI/字体/音频/窗口管理的便利，主战场渲染完全 GPU 化 |
| **HDR 管线** | RGBA16F FBO | 发光效果（Bloom）的物理基础，颜色值允许超过 1.0 |
| **Bloom 算法** | Dual Kawase Blur | 性能远优于高斯模糊，Quality Tier 通过 Mip 层级数控制 |
| **光照模型** | 2D 点光源衰减 + 环境光 | 不做 PBR，足够营造氛围 |
| **材质设计** | 视觉参数驱动 | 不做物理材质，通过 emissive/distortion/blendMode 控制视觉风格 |

---

## 4. 总体架构：RenderGraph

### 4.1 分层架构

```
┌────────────────────────────────────────────────────────────┐
│                    RenderGraph (核心调度)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ ScenePass │→│ VFXPass  │→│ PostProc  │→│ Composite │  │
│  │(MDI/Sprite│  │(Particle │  │(Bloom/HDR │  │(Scene+UI  │  │
│  │ /Light)   │  │ /Trail)  │  │ /Tonemap) │  │ → Screen) │  │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘  │
│         ↕              ↕             ↕                      │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         TransientResourcePool (FBO/纹理管理)         │   │
│  └─────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────┤
│  QualityTierManager (Low/Med/High/Ultra 配置驱动)          │
├────────────────────────────────────────────────────────────┤
│  Raylib 层 (窗口/输入/音频/UI面板绘制)                      │
└────────────────────────────────────────────────────────────┘
```

### 4.2 GPU 管线与 Raylib 职责分离

```
┌─────────────────────────────────────────┐
│  自定义 GPU 渲染管线 (完全控制 GL 状态)  │
│  - FBO 管理 / Render Graph              │
│  - HDR Scene Buffer                      │
│  - Entity MDI Render                     │
│  - Particle Render                       │
│  - VFX / Skill Effects                   │
│  - Post-Processing (Bloom/Tonemap/etc)   │
│  - 最终输出到默认 Framebuffer            │
├─────────────────────────────────────────┤
│  Raylib 层 (仅 UI + 辅助)               │
│  - 菜单/面板/tooltip 绘制               │
│  - 字体光栅化                            │
│  - 图片/纹理加载                         │
│  - 窗口/输入/音频                        │
└─────────────────────────────────────────┘
```

### 4.3 RenderGraph 核心职责

- 管理 RenderPass 的**注册、排序和执行**
- 自动分配/回收临时 FBO（TransientResourcePool）
- 在 Pass 之间插入 **GL Memory Barrier**
- 根据 QualityTier 决定哪些 Pass **启用/跳过**

### 4.4 RenderPass 接口

每个 RenderPass 实现以下接口：

| 方法 | 职责 |
|------|------|
| `Setup()` | 声明输入/输出资源（FBO、SSBO） |
| `Execute(RenderContext&)` | 执行实际渲染 |
| `GetName()` | 调试标识 |

### 4.5 与现有系统的关系

- 现有 `GPUEntitySystem`、`GPUParticleSystem` 等系统**不拆除**，而是被封装为一个个 RenderPass 节点接入图中
- `RenderSystem::render()` 的 579 行巨型函数将被**拆解为多个独立 Pass**：
  - `ScenePass` — MDI Entity + CPU Sprite
  - `VFXPass` — Particle + SkillEffect + HoloBlade
  - `UIWorldPass` — Loot Label + Popup + Beam
  - `CompositePass` — 最终合成输出

---

## 5. FBO 管理与后处理管线

### 5.1 TransientResourcePool

```
资源类型：
├── RenderTarget (FBO + Color Attachment + 可选 Depth)
│   ├── HDR SceneBuffer  — RGBA16F, 全分辨率, 持久
│   ├── Bloom Mip Chain  — RGBA16F, 1/2 → 1/16 分辨率, 临时
│   ├── VFX Buffer       — RGBA16F, 全分辨率, 临时
│   └── UI Buffer        — RGBA8, 全分辨率, 临时(可选)
└── 管理策略：
    ├── 持久资源 — 随窗口尺寸变化重建（Scene, Final）
    └── 临时资源 — 按帧申请/回收，Pool 缓存同规格 FBO 避免每帧创建
```

### 5.2 后处理 Pass 链（Quality Tier 驱动）

| Pass | 输入 | 输出 | Low | Med | High | Ultra |
|------|------|------|-----|-----|------|-------|
| **Bloom** | HDR Scene | Bloom Texture | ❌ | 3 Mip | 5 Mip | 7 Mip + 高斯 |
| **Tonemapping** | HDR + Bloom | LDR Buffer | ACES 简版 | ACES | ACES | AgX |
| **Vignette** | LDR | LDR | ❌ | ✅ | ✅ | ✅ |
| **Color Grading** | LDR | LDR | ❌ | ❌ | LUT 16 | LUT 32 |
| **Screen Distortion** | LDR | LDR | ❌ | ❌ | ✅ | ✅ |
| **FXAA** | LDR | Final | ❌ | ✅ | ✅ | ✅ |

### 5.3 Bloom 实现方案

采用 **Dual Kawase Blur**（性能远优于高斯模糊）：

```
HDR Scene → 亮度提取(阈值) → Downsample Chain → Upsample Chain → 混合回场景
              threshold         1/2 → 1/4 → 1/8    1/8 → 1/4 → 1/2
```

- **Low 档**：完全跳过 Bloom Pass
- **Ultra 档**：增加亮度权重曲线，支持不同颜色通道独立 Bloom 强度

### 5.4 HDR 管线要点

- 场景渲染全部输出到 **RGBA16F** FBO（不再直接写默认 Framebuffer）
- 粒子、技能特效的颜色可以**超过 1.0**（发光效果的物理基础）
- Tonemapping 是最后一步将 HDR → LDR，保留高光细节

---

## 6. 增强粒子系统 & 轨迹渲染器

### 6.1 粒子系统升级

#### A. 纹理粒子

```
GPUParticle 结构扩展：
├── textureIndex (int16)  — 纹理数组层索引，-1 = SDF 圆形
├── subUV (uint16)        — 纹理内子区域（序列帧动画）
└── 渲染改造：
    ├── 粒子着色器 Fragment 中采样 Texture2DArray
    ├── 支持序列帧动画（根据 lifetime 自动播放）
    └── 支持 Additive / Alpha Blend 两种混合模式（flags 控制）
```

#### B. 子发射器 (Sub-Emitter)

```
粒子死亡/碰撞时触发新粒子：
├── 死亡 → 爆裂（火花碎片）
├── 碰撞 → 溅射（落地水花）
└── 实现：Compute Shader 中检测条件 → 写入 Emission Buffer
```

#### C. 力场 (Affector)

```
全局力场，影响所有粒子：
├── 径向力（吸引/排斥）— 黑洞效果
├── 涡旋力（旋转）— 龙卷风效果
├── 噪声力（湍流）— 自然飘散
└── 实现：Compute Buffer 存储力场参数，physics.compute 中采样
```

### 6.2 GPU 轨迹渲染器 (TrailRenderer)

当前 `TrailSystem` 是 CPU 线段渲染，需要完全 GPU 化。

#### 架构

```
┌─────────────┐    ┌──────────────┐    ┌───────────┐
│ CPU：每帧追加 │ →  │ PersistentBuf │ →  │ Trail     │
│ 头部控制点   │    │ (环形写入)    │    │ Shader    │
└─────────────┘    └──────────────┘    └───────────┘
```

#### 特性

- 宽度沿长度渐变（头宽尾尖）
- 颜色/透明度沿长度渐变
- 支持纹理映射（水墨效果沿轨迹展开）
- UV 滚动动画
- 最大控制点数 per trail：**64**
- 最大同时 trail 数：**512**

#### GPU 数据结构

```glsl
// 32 bytes per point
struct GPUTrailPoint {
    vec2  position;     // 8  世界坐标
    vec2  direction;    // 8  用于计算法线展宽
    float width;        // 4
    float lifetime;     // 4
    uint  colorPacked;  // 4
    uint  flags;        // 4  textureIdx, blendMode, etc.
};
```

### 6.3 与后处理的配合

- 粒子和轨迹渲染到 **HDR Scene Buffer**
- 颜色值可以 >1.0（自发光粒子自动产生 Bloom）
- 特殊粒子可以写入 **Screen Distortion Buffer**（热浪/冲击波效果）

---

## 7. 动态 2D 光照系统

### 7.1 架构

2D ARPG 不需要完整 3D 光照，但需要**点光源衰减 + 环境光**来营造氛围。

```
┌─────────────────────────────────────────────┐
│  LightAccumulationPass (在 Scene 之后)       │
│                                              │
│  输入: Scene HDR Buffer (已渲染的实体/地形)    │
│  输出: Lit Scene Buffer (= Scene × Light)    │
│                                              │
│  光源数据: SSBO<GPULight> 最多 256 个         │
│  计算方式: 全屏 Fragment Shader               │
│           对每像素累加所有光源贡献             │
└─────────────────────────────────────────────┘
```

### 7.2 GPULight 数据结构

```glsl
// 32 bytes
struct GPULight {
    vec2  position;      // 8  世界坐标
    float radius;        // 4  衰减半径
    float intensity;     // 4  强度 (可 >1.0 for HDR)
    vec4  color;         // 16 RGBA
};
```

### 7.3 光源类型

通过 flags 区分：

| 类型 | 描述 | 用途 |
|------|------|------|
| **PointLight** | 径向衰减 (平方反比) | 火焰、光球、爆炸 |
| **SpotLight** | 扇形区域 | 技能照明、探照灯 |
| **AmbientZone** | 区域化环境光 | 生物群落氛围 |

### 7.4 Quality Tier 适配

| Tier | 最大光源数 | 阴影 | 附加 |
|------|-----------|------|------|
| Low | 0（禁用，固定环境光） | ❌ | — |
| Med | 32 | ❌ | — |
| High | 128 | ✅ 软阴影 (SDF) | — |
| Ultra | 256 | ✅ 软阴影 | 体积光散射 |

---

## 8. 材质系统

### 8.1 设计原则

**不做 PBR，做"视觉参数驱动"**。

### 8.2 MaterialInstance 定义

| 字段 | 类型 | 描述 |
|------|------|------|
| `baseColor` | `vec4` | 基础颜色 |
| `emissive` | `vec3 + intensity` | 控制 Bloom 强度 |
| `distortion` | `float` | 控制屏幕扭曲 |
| `blendMode` | `enum` | Alpha / Additive / Multiply |
| `shaderVariant` | `enum` | Default / Ink / Hologram / Fire / ... |
| `textureSlots[]` | `TextureHandle[4]` | 最多 4 张纹理 |

### 8.3 存储方式

```
├── 预定义材质 — C++ constexpr 定义在 MaterialDefs.hpp
├── 数据驱动  — JSON 文件定义，支持热重载
└── GPU 端    — SSBO<MaterialData> 数组，着色器按 materialId 索引
```

### 8.4 与现有系统的集成

- `GPUVisualStats` 中已有 `glowIntensity` 和 `glowColorPacked`，这些将映射到材质的 `emissive` 参数
- `GPUEntity.type` 可以映射到 `materialId`
- `Colors` 命名空间中的颜色常量将逐步迁移为材质预设

### 8.5 与 VFX 的关系

- 粒子发射时指定 `materialId` → 自动获得正确的颜色/混合/发光参数
- 技能特效通过 `materialId` 控制视觉风格
- 不同生物群落可以预设不同的环境材质（色调/氛围）

---

## 9. VFX 序列器

### 9.1 设计动机

D4/PoE 级别的技能特效不是单一粒子爆发，而是**多阶段、多层次的视觉编排**。

### 9.2 示例："剑气斩击" 技能

```
t=0.00s  → 蓄力光环（PointLight 渐亮 + 粒子内聚）
t=0.15s  → 斩击轨迹（TrailRenderer 扇形展开）
t=0.15s  → 屏幕震动（ScreenShake 0.3 强度）
t=0.20s  → 冲击波（Distortion 环形扩散）
t=0.20s  → 命中粒子爆发（Additive 火花 + 墨迹碎片）
t=0.30s  → 伤害飘字
t=0.50s  → 残留光效淡出
```

### 9.3 数据结构

```
VFXSequence:
├── name: string
├── duration: float
├── events[]:
│   ├── time: float               // 触发时刻
│   ├── type: enum                // Particle/Trail/Light/Shake/Distortion/Sound
│   ├── params: variant<...>      // 类型特定参数
│   └── position: RelativeAnchor  // 相对于施法者/目标/世界坐标
└── qualityTier: QualityTier      // 最低品质要求
```

### 9.4 存储与加载

| 层级 | 说明 |
|------|------|
| **JSON 定义** | `assets/vfx/*.json`，定义每个 VFX 的完整时间线 |
| **运行时解析** | 解析为 `VFXSequenceAsset`，内存驻留 |
| **热重载** | 开发期支持文件监听自动重载，快速迭代 |
| **播放器** | `VFXPlayer` 实例附加到 ECS Entity 上 |

---

## 10. Quality Tier 配置系统

### 10.1 QualityTierManager 架构

```
┌─────────────────────────────────────┐
│  QualityTierManager (单例)          │
│                                      │
│  ├── currentTier: enum {Low, Med,    │
│  │                      High, Ultra} │
│  ├── autoDetect: bool               │
│  │   (启动时基于 GPU 型号/显存自动)    │
│  ├── overrides: map<string, int>    │
│  │   (用户手动覆盖单项设置)            │
│  └── getConfig(key) → value         │
│      (各子系统查询自己关心的配置)       │
└─────────────────────────────────────┘
```

### 10.2 配置项一览

| 配置项 | Low | Med | High | Ultra |
|--------|-----|-----|------|-------|
| `render.bloom.enabled` | 0 | 1 | 1 | 1 |
| `render.bloom.mipLevels` | 0 | 3 | 5 | 7 |
| `render.lighting.maxLights` | 0 | 32 | 128 | 256 |
| `render.lighting.shadows` | 0 | 0 | 1 | 1 |
| `render.particle.maxCount` | 5k | 50k | 100k | 200k |
| `render.particle.textures` | 0 | 1 | 1 | 1 |
| `render.trail.enabled` | 0 | 1 | 1 | 1 |
| `render.trail.maxPoints` | 0 | 32 | 48 | 64 |
| `render.postprocess.fxaa` | 0 | 1 | 1 | 1 |
| `render.postprocess.distort` | 0 | 0 | 1 | 1 |
| `render.vfx.sequenceDetail` | minimal | reduced | full | full |

### 10.3 自动检测逻辑

1. 查询 `GL_RENDERER` 字符串
2. 查询可用显存（`GL_NVX_gpu_memory_info` / `GL_ATI_meminfo` 扩展）
3. 跑一个 **2 秒的基准测试帧**
4. 根据帧率结果选择初始 Tier（`RTX 4070 SUPER + 2560x1440` 标定阈值）：
   - `>= 270 FPS` → `Ultra`
   - `180 - 269 FPS` → `High`
   - `144 - 179 FPS` → `Med`
   - `< 144 FPS` → `Low`
5. 非 4070S 机型先走“能力探测分档”，再用基准帧结果在相邻档位内微调（最多上下 1 档）

---

## 11. 分阶段实施路线

### 11.1 Phase 0：基础设施 (Foundation)

| 任务 | 描述 |
|------|------|
| RenderGraph 核心框架 | Pass 注册/执行/依赖解析 |
| TransientResourcePool | FBO 池化管理 |
| QualityTierManager | 配置读取/自动检测 |
| RenderSystem 拆解 | 将 `render()` 巨型函数拆解为独立 Pass |

**拆解后的 Pass 清单**：
- `ScenePass` — MDI Entity + CPU Sprite
- `VFXPass` — Particle + SkillEffect + HoloBlade
- `UIWorldPass` — Loot Label + Popup + Beam
- `CompositePass` — 直接输出（暂无后处理）

**验收标准**：画面与当前完全一致，性能无退步。

---

### 11.2 Phase 1：HDR + 后处理管线

| 任务 | 描述 |
|------|------|
| HDR Scene Buffer | RGBA16F FBO |
| Bloom Pass | Dual Kawase Blur |
| Tonemapping Pass | ACES 色调映射 |
| FXAA Pass | 抗锯齿 |
| Vignette Pass | 暗角效果 |

**验收标准**：开启 Bloom 后，光效/粒子自然发光。

---

### 11.3 Phase 2：动态光照

| 任务 | 描述 |
|------|------|
| GPULight SSBO | LightAccumulationPass |
| 光源管理器 | 自动挂载到火焰/技能/环境 |
| 环境光区域 | 生物群落氛围 |
| Low Tier 回退 | 固定环境光回退路径 |

**验收标准**：洞穴/森林等场景有明显光影氛围差异。

---

### 11.4 Phase 3：粒子 & 轨迹增强

| 任务 | 描述 |
|------|------|
| 纹理粒子 | Texture2DArray 采样 |
| 序列帧动画 | 基于 lifetime 自动播放 |
| 子发射器 | Compute Shader 链式触发 |
| 力场系统 | 径向力/涡旋力/噪声力 |
| GPU TrailRenderer | 环形 PersistentBuffer |

**验收标准**：剑气/火焰/冰霜等技能有丰富粒子层次。

---

### 11.5 Phase 4：材质 & VFX 序列器

| 任务 | 描述 |
|------|------|
| MaterialDefs.hpp | C++ constexpr 预定义材质 |
| JSON 材质定义 | 数据驱动 + 热重载 |
| MaterialData SSBO | GPU 端材质数据 |
| VFXSequencer | JSON → Timeline 播放 |
| 技能特效模板库 | 至少 10 个预制序列 |
| Screen Distortion Pass | Distortion Buffer |

**验收标准**：新技能特效可以纯数据驱动创建。

---

### 11.6 Phase 5：打磨 & 高级特性

| 任务 | 描述 |
|------|------|
| Color Grading | LUT 色彩校正 |
| 体积光散射 | Ultra Tier 专属 |
| 性能 Profiler HUD | 按 Pass 显示耗时 |
| 着色器热重载 | 开发期热重载支持 |

**验收标准**：Ultra 画质达到 D4/PoE 视觉标准。

---

### 11.7 依赖关系图

```
Phase 0 ──→ Phase 1 ──→ Phase 2
                │
                └──→ Phase 3 ──→ Phase 4 ──→ Phase 5
```

- **Phase 2 和 Phase 3 可以并行**
- Phase 4/5 依赖 Phase 1 + Phase 3 完成

---

## 12. 附录：关键数据结构汇总

### GPUTrailPoint (32 bytes)

```glsl
struct GPUTrailPoint {
    vec2  position;     // 8  世界坐标
    vec2  direction;    // 8  用于计算法线展宽
    float width;        // 4
    float lifetime;     // 4
    uint  colorPacked;  // 4
    uint  flags;        // 4  textureIdx, blendMode, etc.
};
```

### GPULight (32 bytes)

```glsl
struct GPULight {
    vec2  position;      // 8  世界坐标
    float radius;        // 4  衰减半径
    float intensity;     // 4  强度 (可 >1.0 for HDR)
    vec4  color;         // 16 RGBA
};
```

### MaterialInstance

```cpp
struct MaterialInstance {
    glm::vec4 baseColor;
    glm::vec3 emissive;
    float     emissiveIntensity;
    float     distortion;
    BlendMode blendMode;      // Alpha / Additive / Multiply
    ShaderVariant shader;     // Default / Ink / Hologram / Fire / ...
    TextureHandle textures[4];
};
```

### VFXSequence

```cpp
struct VFXEvent {
    float          time;
    VFXEventType   type;        // Particle/Trail/Light/Shake/Distortion/Sound
    VFXParams      params;      // variant<ParticleParams, TrailParams, ...>
    RelativeAnchor position;    // Caster/Target/World
};

struct VFXSequence {
    std::string           name;
    float                 duration;
    std::vector<VFXEvent> events;
    QualityTier           minTier;
};
```

---

---

## 13. GPU ABI 契约（CPU/Shader 一致性）

> 目标：彻底杜绝“CPU 结构体与 GLSL 结构体字节错位”导致的静默渲染错误。  
> 本章为**强制执行**条款，违背即视为阻断合入。

### 13.1 基本规则

1. 所有 SSBO/UBO 结构采用 `std430`（UBO 如需 `std140` 必须单独声明并说明理由）。
2. C++ 端结构必须有明确对齐策略（`alignas(16)` 或已验证的紧凑布局）与 `static_assert(sizeof(...))`。
3. 禁止在 Shader 中“手写近似结构”而不与 C++ 源结构建立映射。
4. 禁止不同 Shader 对同一逻辑结构定义不一致字段（字段顺序/数量/含义都必须一致）。

### 13.2 唯一数据源与生成策略

- 以 `src/engine/render/GPUData.hpp` 为 CPU 侧权威定义。
- 新增 `tools/render_abi/` 生成流程（可由 Python 脚本实现）：
  - 输入：结构元描述（字段名、类型、offset、size）。
  - 输出：`assets/shaders/generated/gpu_abi.glslinc`（结构定义 + offset 常量 + 版本号）。
- 所有相关 Shader 通过 `#include`（或构建期拼接）引用同一 ABI 定义文件。

### 13.3 ABI 版本号

- 定义 `GPU_ABI_VERSION`（整数）。
- C++ 与 Shader 同时声明该版本；程序启动时记录并校验版本一致性。
- ABI 破坏性修改必须：
  1. 递增版本号。
  2. 在变更日志注明升级影响。
  3. 更新相关测试基线。

### 13.4 CI 校验（必须通过）

1. `sizeof/offsetof` 静态断言测试（C++）。
2. 结构体布局快照测试（导出 JSON 与基线比对）。
3. Shader 编译与反射检查（字段名与 binding 一致）。

---

## 14. Binding Registry 与 Pass 资源命名域

> 目标：避免 Binding 冲突、避免魔法数字、支持 RenderGraph 扩展而不返工。

### 14.1 全局 Binding Registry（长期资源）

- 全局保留区由 `RenderConstants.hpp` 统一管理，仅用于跨 Pass 共享且长期驻留资源。
- 任何全局 binding 变更必须更新“绑定点总表”并附迁移说明。

### 14.2 Pass 局部绑定域（临时资源）

- RenderGraph 为每个 Pass 分配局部 binding（编译期或构建期确定）。
- 局部 binding 仅在 Pass 执行窗口内有效，禁止跨 Pass 假设其值不变。
- 禁止在系统中写 `BindBase(4)` 等字面量；统一改为命名常量。

### 14.3 绑定点冲突门禁

CI 新增检查：
1. 扫描 C++/GLSL 的 `binding = N` 与 `BindBase(N)` 字面量。
2. 非白名单字面量直接报错。
3. 输出冲突图（资源名 → binding → 所属 Pass）。

---

## 15. Frame Ownership 与渲染目标生命周期

> 目标：定义“谁写、谁读、何时销毁”，彻底消除双路径合成与状态漂移。

### 15.1 单帧所有权规则

同一帧内渲染所有权固定为：

1. `ScenePass`：写 `HDRScene`（RGBA16F）
2. `LightingPass`：读 `HDRScene`，写 `LitHDR`
3. `VFXPass`：写入 `LitHDR` 或独立 `VFXHDR` 后合成（按配置）
4. `PostProcessPass`：`Bloom -> Tonemap -> LDR`
5. `CompositePass`：`LDR + UI` 输出到默认 Framebuffer

### 15.2 与 Raylib 的边界

- Raylib 负责窗口/输入/音频/UI 绘制入口，不拥有主战场 HDR 目标。
- 禁止并存“旧 BeginTextureMode 主场景链路”与“新 RenderGraph 主场景链路”。
- 迁移期允许 Feature Flag 二选一，不允许双写同屏。

### 15.3 生命周期策略

- 持久资源：窗口尺寸变化时重建（`HDRScene`、`FinalLDR`）。
- 临时资源：每帧从 `TransientResourcePool` 申请并回收。
- 资源申请必须声明：格式、分辨率比例、读写阶段、是否可复用。

---

## 16. GL 状态契约（含 rlgl 互操作）

> 目标：消除“rlgl 偷改状态”造成的偶发渲染错误。

### 16.1 Pass 入口/出口契约

每个 Pass 必须声明并遵守：

- 输入状态：`Framebuffer`、`Viewport`、`Blend`、`DepthTest`、`Cull`、`Shader Program`、`Texture Units`
- 输出状态：恢复到 RenderGraph 规定基线，不泄漏临时状态。

### 16.2 基线状态（RenderGraph 统一）

- `DepthTest = OFF`（2D 主战场）
- `DepthMask = OFF`
- `Cull = OFF`
- `Blend = ALPHA`（特殊 Pass 可覆写）
- `ActiveTexture = 0`
- `Program = 0`（Pass 结束后）

### 16.3 rlgl 互操作规范

1. 进入自定义 GL Pass 前，统一执行一次 batch flush。
2. Raylib UI Pass 与自定义 Pass 之间必须有状态屏障函数（统一入口，不允许散落调用）。
3. 新增 `ScopedGLState`（或等价机制）用于调试构建下自动校验状态泄漏。

---

## 17. 性能预算与硬性门槛

> 目标：把“看起来快”改为“可度量、可验收、可回退”。

### 17.1 目标机型与帧预算

- 标定机型：`RTX 4070 SUPER`（独显）+ `2560x1440` 原生分辨率
- 逻辑帧率：CPU 固定 `60Hz`（逻辑步进与渲染解耦）
- 渲染帧率硬阈值（Native Rendering）：
  - 常规战斗场景：`>= 270 FPS`（`<= 3.70ms/frame`）
  - 高强度战斗场景：`>= 180 FPS`（`<= 5.56ms/frame`）
  - 极限压力场景：`>= 144 FPS`（`<= 6.94ms/frame`）
- 测试口径：关闭 VSync，记录 30 秒窗口内平均帧率与 P1 低帧（1% Low）

### 17.2 Pass 级预算（按场景压力）

| Pass | 常规场景（270） | 高强度场景（180） | 极限场景（144） | 备注 |
|------|------------------|-------------------|------------------|------|
| ScenePass | 1.1ms | 1.6ms | 2.0ms | 含 MDI 主体 |
| VFXPass | 0.8ms | 1.4ms | 1.8ms | 含粒子与轨迹 |
| LightingPass | 0.5ms | 0.8ms | 1.0ms | 超预算优先降灯数 |
| Bloom | 0.3ms | 0.5ms | 0.8ms | Kawase Mip 可降级 |
| Tonemap + FXAA + Vignette | 0.3ms | 0.4ms | 0.5ms | 后处理尾段 |
| Composite + UI 合成 | 0.2ms | 0.3ms | 0.4ms | 不含复杂 UI 逻辑 |
| **渲染总计上限** | **3.2ms** | **5.0ms** | **6.5ms** | 需低于对应帧预算 |

### 17.3 超预算自动降级顺序

1. 降低 Bloom Mip 层数
2. 关闭 Distortion
3. 降低动态光源上限
4. 降低粒子上限与子发射器频率
5. 降级阴影/体积光（Ultra → High）

---

## 18. 能力探测与 Tier 回退矩阵

> 目标：避免“仅按 OpenGL 版本号分档”导致的误判。

### 18.1 启动时能力探测（必须）

除 `GL_RENDERER` 外，必须采集：

- `GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS`
- `GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS`
- `GL_MAX_COMPUTE_WORK_GROUP_SIZE`
- `GL_MAX_TEXTURE_SIZE`
- `GL_MAX_ARRAY_TEXTURE_LAYERS`
- `GL_MAX_IMAGE_UNITS`
- 可选显存扩展：`GL_NVX_gpu_memory_info` / `GL_ATI_meminfo`

### 18.2 分档策略

1. 先按硬能力上限打分（硬门槛）。
2. 再跑 2 秒基准帧（软门槛）。
3. 取两者较低档位作为初始 Tier。
4. 游戏内允许手动覆盖单项配置并持久化。

### 18.3 功能硬门槛示例

| 功能 | 最低条件 | 不满足时 |
|------|----------|----------|
| RenderGraph 全功能 | OpenGL 4.3 + SSBO/Compute | 回退兼容渲染路径 |
| HDR + Bloom | RGBA16F + 多级 FBO | 关闭 HDR，走 LDR |
| Distortion | image load/store + 额外 RT | 关闭 Distortion |
| 高级粒子链 | 足够 Compute 吞吐 | 降级为基础粒子 |

---

## 19. 资产 Schema 版本化与热重载规则

> 目标：避免内容迭代后“旧 JSON/材质 silently broken”。

### 19.1 Schema 字段（必须）

以下资产均需版本号字段：

- `materials.json` → `material_schema_version`
- `assets/vfx/*.json` → `vfx_schema_version`
- 纹理数组清单 → `texture_array_schema_version`

### 19.2 兼容策略

1. 仅允许“向后兼容读”一代（N 兼容 N-1）。
2. 破坏性变更必须提供迁移脚本。
3. 解析失败时回退到安全默认材质/默认 VFX，并打印结构化日志。

### 19.3 热重载安全策略

- 热重载采用“双缓冲资源句柄”：
  1. 新资源加载并校验通过后原子替换；
  2. 失败不影响当前运行资源；
  3. 记录失败原因与文件版本。

---

## 20. 验证与发布门禁（DoD）

> 目标：保证每个 Phase 合入后都可长期稳定，不靠人工“体感验收”。

### 20.1 必测项

1. **图形一致性**：关键场景截图回归（多分辨率、多 Tier）。
2. **性能回归**：Pass 级 GPU 耗时基线对比。
3. **稳定性**：30 分钟压力战斗无崩溃、无显存持续增长。
4. **ABI 校验**：CPU/Shader 结构一致性测试通过。
5. **状态泄漏**：调试构建下 GL 状态守卫无报警。

### 20.2 发布阻断条件（任一触发即禁止合入）

- 关键 Pass 超预算超过 10% 且无降级兜底。
- 在标定机（4070S@2K）下，性能场景未达到阈值：
  - 常规场景 `< 270 FPS`
  - 高强度场景 `< 180 FPS`
  - 极限场景 `< 144 FPS`
- Tier 自动检测结果与手工能力探测矛盾。
- 出现 binding 冲突或 ABI 版本不一致。
- 热重载导致渲染资源丢失或黑屏。

### 20.3 Phase 验收升级

在第 11 章原有验收标准基础上，新增统一验收模板：

- 功能验收（视觉目标）
- 性能验收（预算达标）
- 稳定性验收（长时运行）
- 可回退验收（降级路径可用）

### 20.4 三档性能场景（可复现脚本标准）

> 目标：保证 `270/180/144` 阈值可自动化复测，避免“人工手感测试”。

#### A. 常规战斗场景（目标 `>= 270 FPS`）

- 分辨率：`2560x1440`，全屏或无边框全屏
- VSync：关闭
- 画质档位：`Ultra`
- 场景要素（稳定刷怪 30 秒）：
  - 活跃实体：`8,000 - 12,000`
  - 活跃粒子：`20,000 - 40,000`
  - 动态光源：`32 - 64`
  - 后处理：Bloom/Tonemap/FXAA 全开，Distortion 低频触发
- 通过条件：
  - 平均 FPS `>= 270`
  - 1% Low FPS `>= 220`
  - 渲染总耗时均值 `<= 3.7ms`

#### B. 高强度战斗场景（目标 `>= 180 FPS`）

- 分辨率：`2560x1440`
- VSync：关闭
- 画质档位：`Ultra`（允许按自动降级策略动态关停非关键特效）
- 场景要素（持续 AoE 与群怪 30 秒）：
  - 活跃实体：`15,000 - 20,000`
  - 活跃粒子：`60,000 - 100,000`
  - 动态光源：`96 - 128`
  - 后处理：Bloom 高档 + Distortion 中频触发
- 通过条件：
  - 平均 FPS `>= 180`
  - 1% Low FPS `>= 150`
  - 渲染总耗时均值 `<= 5.56ms`

#### C. 极限压力场景（目标 `>= 144 FPS`）

- 分辨率：`2560x1440`
- VSync：关闭
- 画质档位：`Ultra`
- 场景要素（弹幕峰值 30 秒）：
  - 活跃实体：`20,000+`
  - 活跃粒子：`120,000 - 200,000`
  - 动态光源：`192 - 256`
  - 后处理：Bloom/Distortion/ColorGrading 全开
- 通过条件：
  - 平均 FPS `>= 144`
  - 1% Low FPS `>= 120`
  - 渲染总耗时均值 `<= 6.94ms`

#### D. 统一采样与统计口径（强制）

1. 预热 `10s`（不计入统计）
2. 采样窗口 `30s`
3. 输出指标：
   - 平均 FPS
   - 1% Low FPS
   - 各 Pass 平均/峰值耗时（Scene/VFX/Lighting/Post/Composite）
4. 数据格式：`JSON + CSV` 双份落盘，纳入 CI artifacts
5. 结果判定：三档场景均通过才允许标记“性能验收通过”

#### E. 自动化执行入口（建议）

- 新增基准入口：`tests/performance/RenderingBenchmark.cpp` 扩展三档 profile
- 运行命令建议：
  - `--profile=baseline_270`
  - `--profile=combat_180`
  - `--profile=stress_144`
- 失败时输出“首个超预算 Pass + 超预算比例 + 建议降级路径”

---

> **修订说明（2026-02-11）**  
> 本次修订补齐了“最终形态不再重构”所需的工程约束层：GPU ABI 契约、Binding 管理、Frame Ownership、GL 状态契约、性能预算、能力探测、资产版本化、发布门禁。  
> 从本版本起，渲染系统的后续实现应以“先满足契约与预算，再增加视觉特性”为执行顺序。
