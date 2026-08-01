# MS-6 GPU Entity Render Boundary Adapter — Review

Date: 2026-08-01

## 审查目标

验证 MS-6 实施包：GPU 实体渲染的 Game ECS/`SharedContext` 暴露替换为 Game adapter + 窄化 Engine DTO 契约，符合 `docs/plans/modular-split-exe-lib-dll-implementation-plan.md` MS-6 节与 MS-0 所有权台账。

## 结论

最终：`提交`

## 轮次

### Round One — 修改

**输入**：设计（受保护，排除）、MS-6 审计方案、实施包（`git status`/`git diff`）、`docs/reports/modular-split-exe-lib-dll/ms-6/evidence.md`。

**变更边界**：符合 MS-6 包范围（GPUEntitySync 移动、GPUEntitySystem 窄化、GPUEntityAdapter 新增、RenderSystem.cpp:714、Game.cpp/hpp、6 个测试文件、ledger、checker REQUIRED_P0_SOURCES）。受保护设计文档不在 diff 且未读取。

**发现**：

- **[HIGH]** `RenderSystem.cpp:714` 无条件解引用 `frame.context.renderContext`。HEAD 原 `GPUEntitySystem::Get().Render(frame.context, frame.camera)` 内部对 `renderContext==nullptr` 有兜底（走 RenderLegacy，且未 Init 时早退）。改动后 `frame.context.renderContext->GPU()...` 在 Render 内 fallback 生效前即解引用。既有集成测试 `GPUHardwareValidationGateTest`（GameplayRuntimeHarness 仅设 registry/settings/renderAlpha，renderContext 恒 nullptr）会命中空指针/UB；`-L unit` 不含集成测试故未发现。修复：加 null 守卫（参考 MDIRenderBenchmark.cpp 写法）。
- **[LOW]** `SetUpdatedStatsIndices` 移动赋值改拷贝赋值（非热路径，可接受）。
- **[LOW]** 三访问器偏差（`ShadowBuffer/VisualStatsBuffer/SetUpdatedStatsIndices`）——评估为可接受：`GPUData.hpp` 纯 DTO 零 Game 依赖、Engine 无 game include、无新反向边；`MAP_BOUNDARY` 字面量 5000.0f 等价 `Constants::World::MAP_BOUNDARY`（技术债，有注释）。

**已复核通过**：文件移动（hpp byte 一致、cpp 仅 line-1）、API 窄化、EntityRenderFrame 取值正确、Game.cpp 三调用点、ledger 精确删 20（实读 JSON 51 条）、checker 实跑 51/51、RG-3 资源生命周期与 HEAD 逐字一致、RenderGraph/passes/graph 构建/ResourceManager/GPUResourceRegistry/pch/CMake 零触碰、热路径零拷贝、MDI/RenderLegacy 行为保留、`frame.mdi==nullptr→Get()` fallback 保留。

### Round Two — 提交（修复复审）

**修复**：`RenderSystem.cpp:714` 加 `if (frame.context.renderContext != nullptr)` 守卫，包裹两处解引用；生产路径（Game.cpp:208/323 注入 renderContext）恒非空不受影响；null 路径跳过 GPU 实体渲染等价原 RenderLegacy 早退。diff 仅此一处。

**重点调查**：gate 测试"missing FixtureRenderDriver" NO_GO 非回归——4 个 TEST_CASE 均为 HEAD（S6 commit `0fdce87`）已提交内容；用例 4 是 S6 新增 "Missing driver fails closed NOT_RUN" 负例（`RunGate("TEST_REV_NODRIVER",...)` 无 driver），"FixtureRenderDriver is required" 唯一来源 `GPUHardwareValidationGate.cpp:616-623`，修正 agent 所见即该负例；用例 2 正常传 harness。4/4 通过。

**验证**：`check_module_boundaries.py` 51/51 PASS；`git diff --check` exit 0；修正 agent 实跑 gate 4/4 无崩溃、build 双标记、定向 2/2。

**已解决发现**：HIGH（null guard）已关闭。
**未解决发现**：无。
**接受的剩余风险**：三访问器为长期公开 API 技术债（后续批建议收敛 `Sync()` 契约）；`MAP_BOUNDARY` 字面量重复；`evidence.md` 已同步修正（Round One 后一行描述更新为带守卫版本）。

## 下一步

按用户授权分批次提交；提交后处理剩余 46 条 MS-6 边（RenderSystem 优先子批次），再 MS-7/8，最后 DOD-2 实机 gate。
