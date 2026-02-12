# HDR + 后处理管线（执行计划）

> **Track ID**: `hdr_postprocess_pipeline_20260212`  
> **关联规格**: `spec.md`（V1.0）  
> **状态**: `COMPLETED`  
> **最后更新**: `2026-02-12`

---

## 总览

| 阶段 | 主题 | 主要交付 | 预估工时 | 状态 |
|------|------|----------|----------|------|
| 1A | GPU 基础设施补齐 | FBO/RBO API、FramebufferManager、FullscreenQuad | 4h | 已完成 |
| 1B | RenderConfig 与质量档位扩展 | Phase 1 渲染配置项 + Tier 预设 | 1h | 已完成 |
| 1C | HDR SceneBuffer 接入 | Scene/VFX/UIWorld 输出到 RGBA16F FBO | 3h | 已完成 |
| 1D | Bloom 管线 | BrightExtract + Kawase Down/Up + Mip Chain | 6h | 已完成 |
| 1E | Tonemapping | ACES Filmic + Exposure + Gamma | 2h | 已完成 |
| 1F | FXAA 与 Vignette | FXAA 3.11 Quality + 暗角 | 3h | 已完成 |
| 1G | RenderGraph 集成与合成 | PostProcessPass + CompositePass 兼容回退 | 3h | 已完成 |
| 1H | 测试、基准与验收 | 单测、性能基准、游戏内验证 | 3h | 已完成 |

---

## Phase 1A: GPU 基础设施（4h）

### Task 1A.1: GPUUtils FBO 能力扩展
- [x] 在 `GPUUtils.hpp` 增加 FBO/RBO/TexStorage2D/Viewport/DrawArrays/Enable/Disable/BlendFunc 接口。
- [x] 在 `GPUUtils.cpp` 完成对应函数指针加载与封装调用。
- [x] 在 `GPUUtils::Initialize()` 完成能力检测与初始化顺序校验。
- [x] 完成编译与运行验证。

### Task 1A.2: FramebufferHandle + FramebufferManager
- [x] 新增 `FramebufferHandle.hpp`。
- [x] 新增 `FramebufferManager.hpp/.cpp`，支持 `Create/Destroy/Resize`。
- [x] `Create` 增加 `CheckFramebufferStatus == GL_FRAMEBUFFER_COMPLETE` 校验。
- [x] 完成编译与运行验证。

### Task 1A.3: FullscreenQuad
- [x] 新增 `FullscreenQuad.hpp/.cpp`。
- [x] 建立全屏三角形 VAO，使用 `DrawArrays(GL_TRIANGLES, 0, 3)`。
- [x] 提供 `Shutdown()` 释放 GPU 资源。
- [x] 完成编译与运行验证。

---

## Phase 1B: RenderConfig & QualityTier（1h）

### Task 1B.1: RenderConfig 扩展
- [x] 在 `RenderConstants.hpp` 新增 Bloom/FXAA/Vignette 配置字段。
- [x] 完成编译与运行验证。

### Task 1B.2: QualityTierManager 预设
- [x] 按规格 3.3 完成 Low/Medium/High/Ultra Phase 1 参数映射。
- [x] 完成编译与运行验证。

---

## Phase 1C: HDR SceneBuffer 接入（3h）

### Task 1C.1: RenderContext 扩展
- [x] 在 `graph/RenderContext.hpp` 增加 `hdrSceneBuffer`。
- [x] 完成编译与运行验证。

### Task 1C.2: HDR FBO 生命周期
- [x] 在 `RenderSystem` 新增 `s_hdrSceneBuffer`。
- [x] 在 `Initialize()` 创建 RGBA16F FBO。
- [x] 在 `Shutdown()` 释放 FBO。
- [x] 在 `render()` 中支持屏幕尺寸变化时重建。

### Task 1C.3: Scene/VFX/UIWorld 输出路径
- [x] `bloomEnabled == true` 时，Scene/VFX/UIWorld 输出到 HDR FBO。
- [x] `bloomEnabled == false` 时，保持 Phase 0 兼容路径。
- [x] 通过 `RenderContext` 传递 `hdrSceneBuffer`。
- [x] 完成编译与运行验证。

---

## Phase 1D: Bloom（6h）

### Task 1D.1: 后处理 Shader 资源
- [x] `assets/shaders/postprocess/fullscreen.vert`
- [x] `assets/shaders/postprocess/bright_extract.frag`
- [x] `assets/shaders/postprocess/kawase_down.frag`
- [x] `assets/shaders/postprocess/kawase_up.frag`

### Task 1D.2: PostProcessPass 与 Mip Chain
- [x] 新增 `PostProcessPass.hpp/.cpp`。
- [x] `Initialize()` 完成 shader 加载与 uniform location 缓存。
- [x] `RebuildBloomMips()` 完成分级 FBO 创建。
- [x] `DestroyBloomMips()` 与 `Shutdown()` 完成资源回收。
- [x] 完成编译与运行验证。

### Task 1D.3: Bloom 执行流程
- [x] 实现 `ExecuteBloom()`：BrightExtract → Downsample → Upsample。
- [x] 处理 FBO、Viewport、纹理绑定状态切换。
- [x] 完成编译与运行验证。

---

## Phase 1E: Tonemapping（2h）

### Task 1E.1: Tonemap Shader
- [x] 新增 `assets/shaders/postprocess/tonemap.frag`。
- [x] 实现 ACES Filmic + Gamma 2.2。
- [x] 暴露 `uHDRScene/uBloomTexture/uBloomIntensity/uExposure`。

### Task 1E.2: Tonemap 执行
- [x] 实现 `ExecuteTonemap()` 输出到 LDR FBO。
- [x] 合并 HDR + Bloom 输入。
- [x] 默认曝光 `uExposure = 1.0`。
- [x] 完成编译与运行验证。

---

## Phase 1F: FXAA & Vignette（3h）

### Task 1F.1: FXAA Shader
- [x] 新增 `assets/shaders/postprocess/fxaa.frag`。
- [x] 采用 FXAA 3.11 Quality（3x3 邻域）。
- [x] 暴露 `uSource` 与 `uTexelSize`。

### Task 1F.2: Vignette Shader
- [x] 新增 `assets/shaders/postprocess/vignette.frag`。
- [x] 实现 `smoothstep(radius, radius - 0.45, dist)`。
- [x] 暴露 `uSource/uIntensity/uRadius`。

### Task 1F.3: 执行链路
- [x] 实现 `ExecuteFXAA()`（LDR → ping-pong）。
- [x] 实现 `ExecuteVignette()`（FXAA 输出再处理）。
- [x] 完成编译与运行验证。

---

## Phase 1G: RenderGraph 集成与 Composite（3h）

### Task 1G.1: PostProcessPass 接入 RenderGraph
- [x] 在 `RenderSystem::render()` 中于 `UIWorldPass` 后插入 `PostProcessPass`。
- [x] `bloomEnabled == false` 时跳过 `PostProcessPass`。
- [x] `PostProcessPass::Execute()` 执行 Bloom/Tonemap/FXAA/Vignette。

### Task 1G.2: CompositePass 回写逻辑
- [x] HDR 启用时：优先合成后处理结果。
- [x] HDR 禁用时：保持 Phase 0 透传行为。
- [x] 修复离屏渲染路径：仅在默认 framebuffer 下启用内部 HDR 合成，避免覆盖 Gameplay `m_sceneRT`。

### Task 1G.3: 初始化与销毁
- [x] `RenderSystem::Initialize()` 调用 `PostProcessPass::Initialize()`。
- [x] `RenderSystem::Shutdown()` 调用 `PostProcessPass::Shutdown()` 与 `FullscreenQuad::Shutdown()`。

---

## Phase 1H: 测试、基准与验收（3h）

### Task 1H.1: 单元测试
- [x] 新增 `tests/unit/PostProcessTest.cpp`。
- [x] `FramebufferManager_CreateDestroy`
- [x] `FramebufferManager_Resize`
- [x] `BloomMipChain_Levels`
- [x] `QualityTier_Phase1Config`
- [x] `PostProcess_LowTierBypass`

### Task 1H.2: 性能基准
- [x] 新增 `tests/performance/PostProcessBenchmark.cpp`。
- [x] 接入 GPU Timer Query（`GL_TIME_ELAPSED`）。
- [x] 输出 Low / Ultra / Delta(Ultra-Low) 结果。
- [x] 分别统计 Bloom 与 Tonemap+FXAA+Vignette 耗时。

### Task 1H.3: 游戏内验收
- [x] Low Tier 路径回退行为正确（与 Phase 0 一致）。
- [x] Ultra Tier 后处理链路正确（Bloom/Tonemap/FXAA/Vignette）。
- [x] 窗口 Resize 稳定（多次切换无异常）。
- [x] 长时间运行稳定（无崩溃/明显泄漏）。
- [x] 2026-02-12 进游戏验证通过：实体、资源贴图、传送门特效渲染路径恢复正常。

---

## Definition of Done

- [x] 全部后处理 shader 编译成功并接入运行时。
- [x] FBO 创建/重建流程完整，`CheckFramebufferStatus` 通过。
- [x] Low Tier 完整回退到 Phase 0 行为。
- [x] Ultra Tier 功能链路正确。
- [x] Bloom 与 Tonemap/FXAA/Vignette 基准项可输出。
- [x] Resize 与长时间运行验证通过。
- [x] `build.bat` 构建通过。
- [x] 游戏内人工验收通过。

---

*版本: 1.2（UTF-8 重写）*  
*更新时间: 2026-02-12*
