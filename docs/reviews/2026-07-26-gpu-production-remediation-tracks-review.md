# GPU Production Remediation Tracks Review

## 审查目标

对照 `conductor/specs/rendering_engine_v5_master_spec.md` §12，整体审查 M0-A、M0-B、M0-C、M1-D 和 M2-E 五条生产整改 Track。报告合并了既有整体静态审查、`review1.log` 和 `review2.log`；后两份日志中的新增意见均已回到源码、Track 规格和计划核验，未以日志本身作为结论证据。

## 结论

修改

`M0-C` 不能提供主规格要求的唯一 production `GO` 证据，且 M0-A、M0-B、M1-D 与 M2-E 仍分别留有必备行为、验证或状态一致性缺口。不得宣称五条 Track 全部完成、Gameplay production `GO` 已达成，也不得据此启用 SPH、DRS/auto exposure 或新的高风险视觉特性。

## 审查轮次

首次整合审查。

## 输入

- 设计：`conductor/specs/rendering_engine_v5_master_spec.md` §12。
- Track 规格与计划：`conductor/tracks/gpu_*_20260726/{spec,plan,index,metadata,validation}.md`。
- 审查标准：`docs/workflows/review.md`。
- 代码标准：`conductor/code_standard.md` §2、§5、§9。
- 上游审查：`docs/reviews/2026-07-26-gpu-*-review.md` 和 `docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md`。
- 外部审查输入：仓库根目录 `review1.log`、`review2.log`。
- 变更范围：`d044908^..HEAD`，88 个文件、7,576 additions、268 deletions；涵盖 RenderSystem、RenderGraph、JFA、GPU validation gate、DRS、测试和 Track 文档。
- 工作区：审查开始时 `git status --short` 无输出；本报告是本轮唯一新增文件。
- 验证：仅静态源码、规格、计划和已版本化记录审查。本轮未运行 build、CTest、Python gate 或目标 GPU nightly；不存在可以支持 runtime `GO` 的新执行证据。

## 范围对齐

主规格要求 M0-A → M0-B → M0-C，并将 M0-C 规定为 production `GO` 的唯一证据；M1-D 和 M2-E 不得绕过该门禁。当前代码已改善离屏路径、SDF 负 epsilon 和 SPH 默认关闭，但不足以满足该交付链。

- M0-A：Track 声称 completed 20/20，但 `plan.md:5,103-108` 仍为 Planned 且最终验证未勾选，`spec.md:72-78` 的验收项未勾选。
- M0-B：metadata 声称 completed 25/25，`index.md:4,16-17` 和 `conductor/rendering_system_progress.md:162` 仍为 Planned；核心 RenderGraph 合同也未完成。
- M0-C：Track 声称 `GO`，但硬件 artifact 位于被忽略且未版本化的 `bin/`；门禁实现本身还能把不合格结果报为 `GO`。
- M1-D：`index.md:4,16-17` 声称 completed，`metadata.json:5,15-16` 仍为 planned 0/17；必需的 GPU 精度、fallback 和性能证据不存在。
- M2-E：`index.md:4,16-17`、`metadata.json:5,12-13`、`conductor/tracks.md:859-866` 和 `conductor/rendering_system_progress.md:166` 均为 In Progress、11/18；auto exposure 与最终验证未完成。

这类状态和验收记录冲突使 `docs/workflows/review.md:18-26,41-45` 所需输入不可靠，单独即可否决“全部完成”结论。

## 发现项

### Blocker — M0-C 可将失败或未执行的门禁误报为 `GO`

`scripts/gpu_hardware_validation_gate.py:144-145` 将包含 `Status: SUCCESS!` 的 doctest 成功解释为 `GO`，而 `tests/integration/GPUHardwareValidationGateTest.cpp:80-97` 明确接受 `NO_GO` 和 `NOT_RUN`。此外，`GPUHardwareValidationGate.cpp:395-399,415-417,541-545` 不把 timing 失败纳入 `overallPassed`。这违反 M0-C “所有 MUST PASS 才可 GO”的合同，并触发 `docs/workflows/review.md:47-53` 对伪造、隐藏或旁路验证的硬否决。

修复：Python runner 必须解析并原样传递 `GateReport.status`，只允许实际 `GO` 返回成功；测试必须断言 `NO_GO`、`NOT_RUN` 和任一 timing 失败均使 runner 失败；`overallPassed` 必须 AND 所有 fixture、pass timing、readback、diagnostic 和 stability 结果。

### Blocker — M0-C 未执行真实 Gameplay fixture，GI、SDF 和 GL 诊断判据均不可信

`GPUHardwareValidationGate.cpp:185-188,234-281` 使用空 `entt::registry`/context、独立构造 graph 和通用 FBO，而非规格要求的已初始化 `GameplayState::BeginTextureMode -> RenderSystem -> Composite` 场景。`GPUHardwareValidationGate.cpp:191-216,240-244,477` 只记录 `giOn`，没有在每帧改变真实 `RenderConfig.giEnabled`；`GPUHardwareValidationGate.cpp:304-351` 读取合成画面亮度来代替 SDF 正负号、ray-stop 和 GI on/off 差分验证；`GPUHardwareValidationGate.hpp:103-104` 与 `GPUHardwareValidationGate.cpp:588-589` 只序列化初始化为零的 GL diagnostic 计数。

修复：由真实 GameplayState 驱动三个有遮挡物、emissive、相机轨迹和拥有离屏目标的固定场景；每帧应用受支持的 GI feature override，并比较真实 trace、资源状态和 paired captures；直接读取 SDF/occupancy 纹理的内外坐标并执行 ray-stop probes；安装 GL debug callback，持久化消息并将高严重度消息判为失败。

### Blocker — M0-A 缺少 Radiance 前的 VFX 发光生产者

`src/engine/render/RenderSystem.cpp:2114-2117,2129-2134` 在 `VFXPass` 前调度 Radiance；`src/engine/render/passes/RadianceCascadesPass.cpp:494-532` 以 HDR brightness 推导“particle emission”。这不是 M0-A 所要求的当前帧、冻结且版本化的 `EmissiveVfx` GI 输入。

修复：在 Radiance 前创建显式 VFX-emission snapshot pass，记录并版本化该资源，将其作为独立 GI 输入；增加遮挡、VFX emission 和 history invalidation 的成对 fixture/readback。

### Blocker — M0-B compiled plan 未编译或执行跨 pass transition

`src/engine/render/graph/RenderGraph.cpp:450-455` 仅映射当前 access；`src/engine/render/graph/RenderResourceDescriptor.hpp:223-225` 的双参数 overload 将 stage 映射为自身；`src/engine/render/graph/RenderGraph.hpp:268-275` 中的 `CompiledRenderPlan` 没有 transition 列表。这不满足 M0-B 对先前 access/stage 到下一 access/stage barrier 的要求。

修复：按稳定资源 ID 跟踪前序状态，在 immutable compiled plan 中生成 transition records，并在消费者前仅执行计划声明的 barrier；为同一资源的 read/write、write/read、跨 stage 与条件 pass 顺序添加合同测试。

### Blocker — M1-D 不验证增量结果，也不会在同帧回退 full JFA

`src/engine/render/passes/JFAPass.cpp:640-735` 仅进行部分 dispatch 并关注 overflow；`JFAPass.hpp:82-83,158` 中声明的 verification readback 没有接入生产执行。M1 规格要求把增量输出与 EDT/full-JFA 比较，在越过误差阈值时同帧重跑 full JFA。

修复：在生产帧上调度确定性的 GPU/CPU reference comparison，按规格比较误差，并在任何失败当帧执行 full JFA；将结果、fallback 原因和执行模式写入硬件 artifact，增加移动遮挡物和视图变化场景。

### Blocker — Track 状态、验收和验证证据互相冲突，M2-E 明确尚未完成

M0-A、M0-B、M1-D 的 index、metadata、计划与验收复选框互相冲突；M0-C 将未版本化 artifact 记为 `GO`；M2-E 的 auto exposure 和 Phase 4 验证仍未交付，却存在 `2c51a37` “complete M2-E”提交。这使完成声明不具备审计性，并违反审查流程的必要输入和缺失证据规则。

修复：以实际完成的规格验收为唯一状态来源，同一提交中同步 `index.md`、`metadata.json`、`tracks.md`、进度表、plan 和 spec；对 M0-B 显式登记 deferred pass/ABI debt；保留 M2-E 为 In Progress，或拆分未完成的 auto exposure 与验证范围；只在真实目标硬件 artifact 已版本化或可复现归档后写入 `GO`。

### High — M0-A GI history 在 resize、缩放和 occupancy 变化时可能被错误复用

`src/engine/render/passes/GICompositePass.cpp:108-114,139-143` 在 `EnsureResources` 前更新 cached extent，resize 不一定失效 history；`GICompositePass.cpp:216-235` 只对 light/occluder reset。`assets/shaders/lighting/v5_gi_composite.comp:5-9,35-46` 与 `GICompositePass.cpp:197-235,273-292` 没有存储或比对 current/previous occupancy/depth，不能满足 `gpu_production_hdr_gi_closure_20260726/spec.md:62` 的 occupancy mismatch rejection。

修复：保留上帧 extent、zoom 和所有 GI-input version，变化时将 history weight 置零；持久化 occupancy/depth history，在重投影 UV 比较后拒绝 disocclusion；添加 resize、zoom、emissive、occupancy 和相机移动 fixture。完整 depth reprojection 不是本项问题，它是规格中已接受的残余风险。

### High — M0-B resource identity、registry 和 capability governance 仍未闭环

`src/engine/render/passes/ScenePass.cpp:12-24` 声明 `SceneHdrColor`，实际 access 却为 canonical `SceneColor`；`RenderGraph.cpp:626-640` 以名称建 descriptor，导致 descriptor 和逻辑资源可脱离。`FramebufferManager.cpp:157-164` 仅注册 FBO/颜色纹理，而 `FluidSimulationPass.cpp:90-99` 等仍绕过 registry 创建 VAO/VBO；`GPUResourceRegistry.cpp:91-94` 的 `AdvanceFrame` 没有生产调用。`RenderSystem.cpp:1591-1656` 构建了独立 hot-reload manager，`ShaderReloadGovernance.cpp:27-94` 未接到渲染路径。

修复：descriptor 使用 typed tag/stable ID，要求每个 access 精确解析一个资源；纳入 buffer、VAO、query、mapping 的 RAII 生命周期和真实 owner，并每 rendered frame 调用 `AdvanceFrame`；让 production reload 和 capability matrix 走唯一 governance 路径。补充 M0-B 对 `RenderSyncContracts`/`ScopedGLState` 的 executor 依赖与 `flush -> state guard -> execute -> flush` 合同说明，当前该依赖未在 Track 规格声明。

### High — M0-C timer 名称/所有权和 stability 检查不能证明规格的性能门槛

Gate 在 `GPUHardwareValidationGate.cpp:289-301,356-400` 另行 begin/end global timer frame 并把固定名称绑定到 `0..10`，而 `RenderGraph.cpp:441-479` 在 `Execute` 内自行管理 timer，pass ID 为条件节点的运行时 index；ID 会随 shadow、GI、GPU text/loot 等条件 pass 改变。`GPUTimerQueryRing.cpp:235-243` 还允许 gate 多次读取同一个 retained latest query。`GPUHardwareValidationGate.cpp:433-467` 在每次循环采样并允许 2 MiB 增长，不是规格要求的五秒窗口、无单调增长检查。`GPUHardwareValidationGate.hpp:42-45` 定义 fixture ROI origin，但 `GPUHardwareValidationGate.cpp:305-310` 未使用 `roiX/roiY`，故每个 fixture 都读 lower-left rectangle。

修复：移除 gate 外层 timer frame，暴露由 compiled-plan stable pass ID/name 和 frame sequence 标识的实际 timer records，只统计不同的 Valid query；用五秒边界采集 registry snapshots，任何单调 net growth 均失败，容差必须先修订规格；按 `roiX/roiY/roiW/roiH` 通过显式 FBO helper 读取并测试 ROI origin。离屏 FBO 绑定本身不是问题：Raylib 的 `rlReadScreenPixels` 会对当前绑定 FBO 调用 `glReadPixels`。

### High — M1-D 的默认 full-JFA 安全策略和 1080p P95 证据均未达标

`JFAPass.hpp:74-76,157` 将 `m_incrementalExperimentEnabled` 默认/reset 为 false，但 `JFAPass.cpp:573-581` 的分支只重算等价条件，不能在默认状态强制 full JFA；生产路径仍可在 `JFAPass.cpp:593-615,639-645` 选择 incremental dispatch。`RenderGraphV5ContractsIntegrationTest.cpp:273-305` 的“1080p”测试只推导 texel-area ratio，调用空 timer frame 并 `CHECK_NOTHROW`，没有提交 JFA GPU work 或比较 Valid GPU P95。这违反 `gpu_jfa_incremental_update_20260726/spec.md:23-24,36` 的默认 full 与同 fixture 1080p P95 至少改善 20% 要求。

修复：配置一个真正控制 execution mode 的 production gate，默认强制 full JFA，只有显式 opt-in 且通过正确性/稳定性/性能门槛后才启用 incremental；在同一 1920x1080 目标 GPU fixture 上采集 full/incremental 的 Valid timestamp P95，断言 incremental P95 不高于 full 的 80%，并归档 revision/GPU/config artifact。

### High — M2-E DRS 未保持 Gameplay camera/坐标一致性，HUD 也未暴露决策上下文

`src/game/states/GameplayState.cpp:413,823-826,973-986` 以缩放 extent 重建 scene target，却仍以 native-size camera render，破坏世界目标与坐标转换的同步。M2 Task 2.5 要求 scale、sample state、reason 和 target extent，`RenderSystem.cpp:2296-2308` 只显示 scale、开关、GPU state 和 P95；reason 仅存在于 `RenderSystem.cpp:531-551` 的局部 decision/log 路径。

修复：在缩放 target 时同步调整 Gameplay camera/viewport 和 mouse-to-world conversion，并加入输入坐标回归测试；持久化 last `AdaptiveQualityDecision` 与 active `RenderTargetExtent`，在 HUD 展示 reason、sample frame 和 extent。默认 disabled/locked 的安全值应保持到真实硬件验收完成。

### High — 共享 CI 失败没有合格的基线或 waiver 证据

M2 validation 记录 `UITests.cpp` 和 `HeavenlySwordClosureTests.cpp` 失败，但没有失败 test case、断言、基线 revision 或 waiver。已版本化 M0-B review 只证明 `HeavenlySwordClosureTests.cpp:17` 在 M2 前出现过一次失败，不能证明 `UITests` 同样预存。两者都编译进共享测试可执行文件，且 CI/unit gate 会执行相关用例：`tests/CMakeLists.txt:3-11,70-78,90-96`。

修复：版本化记录失败命令、完整 test identifiers、基线 revision、owner、批准范围、到期条件与复测条件；否则修复失败后重新提供 CI 证据。不得将“文件未改动”视为可忽略测试失败的 waiver。

## 最佳实践建议

- 在 M0-C artifact 中记录 gate JSON、每个 fixture 的 scene seed/camera/ROI、GI mode、SDF probes、pass trace、distinct timer samples、GL diagnostics、registry snapshots 与硬件信息；使用可复现的归档位置而不是被忽略的 `bin/` 输出。
- 将其余 production pass 的 typed migration 和旧 bridge API 退役路径列成 M0-B debt register，避免“completed”掩盖实际支持矩阵。
- 将 M2-E auto exposure 作为明确的未完成阶段或独立 Track；在 HDR histogram、adaptation、Gameplay integration 和目标硬件验证完成前保持功能关闭。
- 使所有 Track 评审结论只使用 `提交` 或 `修改`，不使用 `Approved`、`通过` 等非工作流术语。

## 已核验但不纳入缺陷的意见

- `rlReadScreenPixels` 不会自动切回默认 framebuffer；绑定的 offscreen FBO 是正确 read source。问题是 fixture ROI origin 没有用于 readback。
- M0-A 未实现完整 depth reprojection 是 Track 明示并接受的残余风险；缺陷是未实现规格要求的 occupancy/disocclusion rejection。
- M1 的小网格测试不是全部证据：存在 1080p decision/area 测试。缺口是没有真实 1080p GPU work、Valid P95 和 20% 比较。
- M2 HUD 已显示 scale；缺少的是 decision reason 和 target extent。

## 下一步动作

1. 先停止所有 production `GO`、五 Track 全完成和 M2-E complete 的发布表述，并统一 Track 状态与验收记录。
2. 先完成 M0-C 的真实 fixture、真实 feature toggle/readback/diagnostics、timer ownership、`overallPassed` 和五秒 stability 门禁；在目标硬件重新运行并归档 evidence。
3. 完成 M0-B transition/resource/governance 合同，再以它为基础重新验证 M0-A history/VFX 和 M1-D incremental fallback。
4. 修复 M1-D 的默认 full gate、分辨率变换和真实 1080p P95；验证失败必须当帧回退。
5. 修复 M2-E camera/coordinate 契约，补齐 HUD、auto exposure 和 Phase 4 验证；在此之前保留禁用默认值。
6. 解决或合规豁免共享 CI 失败，随后执行 build、相关 CTest、gate runner 与目标 GPU nightly；仅在全部 MUST PASS 后重新进行最终审查。

## 剩余风险

当前不能接受剩余风险。若强行推进，空场景 gate 假绿、错误的 GI/SDF/计时结果、RenderGraph barrier 漏失、错误 history、JFA 伪增量和缩放坐标错误都可能进入生产而未被门禁拦截。
