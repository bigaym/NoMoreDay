# GPU 渲染引擎架构审查

## 审查目标

审查 NoMoreDay 当前 GPU 渲染引擎是否具备可继续扩展的生产级基础，识别 V5 核心 GI 发布后仍缺失的引擎级能力、生产路径风险与验证缺口，并给出后续完善顺序。

## 结论

`修改`

当前引擎已具备 MDI、GPU Text/Loot、2D PBR、Clustered Lighting、Shadow Atlas、HDR 后处理、JFA、Radiance Cascades GI、质量分级与基础 ABI 治理等重要能力；但主 Gameplay 渲染路径、GI 正确性、资源/同步模型与长期验证未形成生产闭环。不得优先继续堆叠新视觉特性、SPH 或 Vulkan 后端迁移。

## 审查轮次

首次审查。

## 输入

- 设计基线：[V5 主控技术规格书](../../conductor/specs/rendering_engine_v5_master_spec.md)。
- 进度基线：[GPU 渲染系统实施进度](../../conductor/rendering_system_progress.md)。
- 历史 V5 回顾：[V5 GPU Rendering 全面回顾与重规划](../../conductor/tracks/v5_rendering_review_20260219.md)。
- 历史发布证据：[V5 Release Posture](../../conductor/archive/v5_validation_release_gate_20260219/release_posture.md)、[V5 Validation](../../conductor/archive/v5_validation_release_gate_20260219/validation.md)。
- 审查标准：[审查流程](../workflows/review.md)。
- 实施计划：不适用。本报告是当前渲染引擎基线的架构与生产能力审查，不审批单一功能包。
- 验证证据：代码图谱定义/调用检索、源码直接检视、历史 V5 测试记录；本轮未重跑构建或测试。

## 变更文件边界

- 审查开始前的 `git status --short` 无输出，工作区干净。
- 审查期间未修改渲染源码、资源或测试。
- 本报告是本轮唯一新增文件：`docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md`。

## 范围对齐

V5 规格的核心目标是以 JFA 和 Radiance Cascades 形成分档实时 GI，并保留 SPH 为不阻断发布的探索分支。历史发布姿态也明确将 SPH 排除在 V5 Core 外。

源码中 JFA、Radiance Cascades、GI Composite 与 V5 合同均已存在，但当前实现并未保证主 Gameplay 离屏路径执行这些 pass，也未形成由资源依赖驱动的调度、屏障、生命周期与硬件级验收。因此，文档中的“V5 core released”不能等同于“生产 Gameplay 图已完整闭环”。

## 已验证能力

- `RenderGraph` 已提供 pass 顺序、资源 owner 与读写合同校验：`src/engine/render/graph/RenderGraph.cpp:261-476`。
- HDR、Bloom、Tonemap、FXAA、LUT、Volumetric、PBR、Clustered Lighting、Shadow Atlas、HeightShadow 与 MDI 路径均有实现基础。
- JFA、Radiance Cascades、GI Composite、Tier 路由、JFA+2 fallback 与基础 profiler 已实现：`src/engine/render/passes/`。
- GPU ABI 生成、CPU layout 断言与启动时 ABI 检查已存在：`src/engine/render/GPUData.hpp:559-611`、`src/engine/render/GPUABIContract.cpp:42-107`。

## 质量与风险评估

渲染编排集中在 `RenderSystem::render` 的 475 行手工条件链中，图谱复杂度为 cyclomatic 57、cognitive 114：`src/engine/render/RenderSystem.cpp:1603-2077`。新增 pass、资源或平台回退时，需要同时维护主函数中的添加顺序、pass 私有状态、手工 barrier、resize fan-out 与质量开关，已超过可安全扩展的复杂度边界。

## 发现项

### Blocker — 主 Gameplay 离屏路径跳过高级渲染链

`GameplayState::OnRender` 在 `BeginTextureMode(m_sceneRT)` 中调用 `RenderSystem::render`：`src/game/states/GameplayState.cpp:971-984`。`RenderSystem` 对任何离屏 composite target 设置 `offscreenV3SafeMode`：`src/engine/render/RenderSystem.cpp:1672-1711`；该模式跳过 Lighting、HeightShadow、GI、Fluid 与 PostProcess：`src/engine/render/RenderSystem.cpp:1896-1960`。

这使高画质配置的主游戏画面无法确认实际执行 V3-V5 色彩重写与 GI 链路，且该规避逻辑以“避免 black-frame regression”为目的，不能作为长期生产路径。

### High — GI 遮挡、符号距离场与时域历史尚未形成正确性闭环

`OccluderExtractPass` 的重建条件只比较静态/动态遮挡签名：`src/engine/render/passes/OccluderExtractPass.cpp:395-434`；但实际提取使用相机 target、offset、zoom 和屏幕尺寸：`src/engine/render/passes/OccluderExtractPass.cpp:247-299`。相机移动而遮挡实体不变时，可能复用旧的屏幕空间掩码。

遮挡像素在 SeedInit 写入自身坐标：`assets/shaders/lighting/v5_seed_init.comp:24-29`。距离 resolve 对遮挡像素仅翻转距离符号：`assets/shaders/lighting/v5_distance_field.comp:33-44`。因此内部像素可能产生 `-0.0`；RC 仅在 `sdf < 0.0` 时停止射线：`assets/shaders/lighting/v5_radiance_cascade.comp:49-51`。这是源码可推导的高风险，仍需 GPU readback 作为最终实机证据。

GI Composite 仅在同像素读取 history，并只按相机位移调整权重；没有重投影、disocclusion 或遮挡/发光体版本拒绝：`src/engine/render/passes/GICompositePass.cpp:191-289`。

### High — VFX/Fluid 对 GI 的生产者语义与 pass 顺序不完整

V5 规格要求材质 Emission、场景光与 VFX 共同构建 Emissive Buffer：`conductor/specs/rendering_engine_v5_master_spec.md:113-123`。当前 `RenderSystem` 在 VFX 之前调度 GI，在 Fluid 之后才执行：`src/engine/render/RenderSystem.cpp:1908-1931`。Fluid 仍在执行时尝试注入 emissive 与 occluder，这无法影响当帧已经完成的 Radiance Cascades。

应建立显式的 VFX/Fluid emissive 与 occluder 生产资源，并在 JFA/Radiance 前按合同合并，而不是通过 HDR 亮度代理或后置写入间接参与。

### High — SPH 已为 NO-GO，却仍在 Ultra 默认路由中启用

发布姿态明确 V5 Core 为 GO、SPH 为 NO-GO 且排除在发布范围外：`conductor/archive/v5_validation_release_gate_20260219/release_posture.md:5-15`。然而 Ultra 默认设置 `fluidEnabled=true` 与 `fluidMaxParticles=10000`：`src/engine/render/core/QualityTierManager.cpp:1399-1407`。

SPH 当前还存在未达标的性能与实现风险：10K 基准超过 `0.80ms` 目标：`conductor/archive/v5_sph_fluid_exploration_20260219/validation.md:22-41`；邻居搜索实际上扫描全部粒子，属于 O(N^2)：`assets/shaders/lighting/v5_fluid_neighbor_search.comp:46-64`；GLSL `std430` 的 `vec4 color` 对齐也与 48B C++ payload 需要进一步统一验证。

### High — RenderGraph 是线性合同校验器，不是资源编译器

`RenderGraph::Build` 收集 pass 声明并验证，`Execute` 以插入顺序串行执行：`src/engine/render/graph/RenderGraph.cpp:261-337`。资源声明仅包含名称、读写、tag 与 owner：`src/engine/render/graph/RenderGraph.hpp:139-149`，没有 format、extent、pipeline stage、attachment/load-store、物理句柄、history 或 lifetime。

因此，资源依赖不能自动生成 barrier、拓扑计划或瞬态 aliasing；Compute/SSBO/image 的正确性仍依赖每个 pass 手工维护。`TransientResourcePool` 也只管理帧级 FBO 复用：`src/engine/render/resources/TransientResourcePool.cpp:12-75`。

### High — GPU timing、显存与长稳验证不能证明完整生产链

Profiler 在 `EndPass` 后立即查询 GPU timer，未就绪时记录 `0ms`：`src/engine/render/debug/RenderProfiler.cpp:115-153`；统计时又忽略零样本：`src/engine/render/debug/RenderProfiler.cpp:155-179`。自动降级可能退回 CPU 提交时间，而不是有效 GPU 开销。

GI 长稳测试只实例化并执行 `RadianceCascadesPass`，未覆盖完整 Occluder→JFA→Radiance→Composite 路径：`tests/integration/GIStabilityIntegrationTest.cpp:97-188`。发布门禁的“VRAM”只验证 `FramebufferManager` 的 FBO 字节代理：`tests/integration/ReleaseGateIntegrationTest.cpp:75-113`，未覆盖 SSBO、纹理数组、persistent mapping、query、VAO/VBO 和驱动级显存。

### Medium — Shader、ABI、binding 与设备能力治理仍存在绕过路径

基础 ABI 生成已存在，但 V5 Fluid shader 手写 `FluidParticle`：`assets/shaders/lighting/v5_fluid_density.comp:5-43`，C++ payload 为 48B：`src/engine/render/GPUData.hpp:579-587`。BindingRegistry 与生成链尚未覆盖所有 V5 local binding。

Shader hot reload 只哈希顶层 vertex/fragment/compute 文件：`src/engine/render/dev/ShaderHotReloadManager.cpp:88-99`；reload 失败仍更新 hash，源文件不再变化时不会自动重试：`src/engine/render/dev/ShaderHotReloadManager.cpp:52-86`。GPU capability 初始化只显式标记 indirect draw 和 persistent mapping：`src/engine/render/GPUUtils.cpp:53-131`，缺少面向 GI/image/barrier/format 的 fail-fast capability gate 与统一诊断。

### Medium — JFA 增量更新和部分验收证据仍为未闭环项

规格要求动态遮挡 chunk 局部重算：`conductor/archive/v5_jfa_distance_field_20260219/spec.md:63-70`。当前唯一的 incremental 开关仅用于测试，实际仍执行完整 Seed、JumpFlood 与 Distance 过程：`src/engine/render/passes/JFAPass.hpp:57-59`、`src/engine/render/passes/JFAPass.cpp:449-582`。

归档 JFA validation 仍将 half-res 上采样、静态抖动、单独性能、resize 稳定性标为未完成：`conductor/archive/v5_jfa_distance_field_20260219/validation.md:18-30`，与进度表的 V5 完成状态存在证据漂移。

### Best Practice — 自适应画质缺少 render scale 与曝光控制

`RenderConfig` 没有全场景 render scale 或 exposure 字段：`src/engine/render/core/RenderConstants.hpp:42-137`。当前自动降级以关闭/削减功能为主：`src/engine/render/core/QualityTierManager.cpp:1459-1544`；Tonemap 固定使用 `exposure=1.0f`：`src/engine/render/passes/PostProcessPass.cpp:381-414`。

此项不阻断当前修复，但应在 profiler 和资源预算可信后，加入有滞回的动态分辨率、原生分辨率 UI 合成和可配置曝光策略。

## 最佳实践建议

| 发现项 | 可执行修复建议 |
| --- | --- |
| Gameplay 离屏路径 | 建立离屏 HDR input/output contract，完成 HDR 合成回写，新增 `BeginTextureMode` 下的 Lighting/GI/PostProcess 实机集成测试。 |
| GI 正确性 | 将相机、视口和 chunk version 纳入 OccluderMask 失效键；增加 SDF 内外符号 GPU readback、相机平移、动态遮挡、动态 emissive 与 history rejection 测试。 |
| VFX/Fluid GI | 将 emissive、occluder、radiance 建模为独立 typed resource；在 Radiance 前统一 merge 并校验 barrier。 |
| SPH NO-GO | 默认关闭并改为开发者 opt-in；在真正 cell-binning、std430 layout、SSBO barrier 和 10K 性能通过前，不进入生产 tier。 |
| RenderGraph | 先实现不改变既有 pass 顺序的 compiled-plan：typed descriptor、usage/stage、producer-consumer 边、transition validation、history 与 lifetime。 |
| 资源与观测 | 建立全 GPU resource registry，记录 owner、descriptor、字节数、生命周期、峰值和预算；使用跨帧 timer-query ring，并区分无样本与 0ms。 |
| ABI 与后端能力 | 从单一 manifest 生成 C++/GLSL layout 和 binding；加入 include dependency tracking、shader compile diagnostics、feature capability gate 与 GL debug callback。 |
| JFA 与画质扩展 | 将 dirty chunk/rect 做为运行时能力；在基础数据可信后再实施 DRS、曝光与 V6 Vulkan 迁移决策。 |

## 剩余风险

- SDF 的 `-0.0` 风险来自 GLSL 数据流和比较条件的源码推导，尚未以目标 GPU readback 复现。
- 本轮没有重跑构建、单元、集成或性能测试；引用的 PASS 仅是 2026-02-21 的历史记录。
- OpenGL 4.3 的单队列模型不适合作为短期 async-compute 承诺；应先完成资源状态模型，再根据实测决定是否开展 Vulkan V6。

## 下一步动作

1. 先按 P0 创建“生产离屏 HDR/GI 闭环”设计与实施计划，修复 safe mode、GI 失效与 SPH 默认路由。
2. 随后创建“RenderGraph 与 GPU Resource Foundation”工作包，完成 typed resource、同步合同、history、资源预算与 ABI/binding 治理。
3. 建立硬件 nightly 验收：完整 GI 链、tier 切换、resize、动态遮挡、长稳运行、有效 GPU timing、全资源台账与黑帧检测。
4. 仅在前三项完成后评估动态分辨率、曝光和 Vulkan V6；不优先实施 HZB、传统 TAA 或新的实验性流体特性。
