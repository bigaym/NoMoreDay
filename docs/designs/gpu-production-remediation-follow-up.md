# GPU Production Remediation Follow-up

## 文档目的

本文记录 2026-07 GPU 生产整改当前仍未完成的内容，以及后续实现时应遵循的工程方案。本文不改变 Track 的验收标准，也不把局部实现或测试通过解释为 production `GO`。

权威依据为：

- `conductor/specs/rendering_engine_v5_master_spec.md` §12
- `docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md`
- 各 Track 的 `spec.md`、`plan.md` 和 `validation.md`

## 当前结论

当前 revision 仍为 Gameplay production `NO-GO`。Python runner 已经 fail-closed，C++ gate 也会保留 SPH、timing、诊断和资源稳定性失败；但真实目标硬件上仍没有满足所有 MUST PASS 条件的、可审计且可复现的 `GO` artifact。

已经落地但不等于完成的基础能力包括：

- runner 精确解析 `GPU_HARDWARE_GATE_RESULT`，只有 C++ 返回码为零且 verdict 为 `GO` 才成功。
- GPU gate 可使用固定输入的 Gameplay runtime harness 和 production-order composite target。
- ROI 读回使用显式 framebuffer 与 `x/y/width/height`。
- GI runtime override、GL debug callback、五秒资源快照已接入 gate 的验证合同。
- RenderGraph 已开始使用 stable resource ID、compiled transition records 和 stable pass identity。
- Radiance 前增加了当前帧、版本化的 VFX emission snapshot。
- JFA 默认走 full 路径；显式增量实验有校验、artifact 记录和同帧 full fallback。

这些能力目前只能证明“门禁更诚实”或“局部合同已接入”。本地 gate 输出 `NO_GO` 是正确结果，不得用 doctest 的 `Status: SUCCESS!`、单测通过或旧 artifact 替代 production `GO`。

## 未完成内容

### M0-C：Production GO 证据仍不完整

1. **真实 fixture 的判据未闭环**

   三个 Gameplay recipe 已能驱动真实渲染链，但 gate 仍需在每个 fixture、quality tier 和 GI paired mode 中稳定采集并比较：

   - compiled pass trace 与 stable pass ID
   - 当前帧资源版本和资源状态
   - SDF 与 occupancy 的真实纹理读回
   - 遮挡物内部/外部 sign probe
   - ray-stop probe 的命中深度和停止条件
   - GI-on/GI-off 的同输入 paired capture
   - emissive、遮挡物移动和相机轨迹的结果

   合成画面亮度只能作为 non-black 检查，不能替代这些判据。

2. **GI differential 仍未达到规格阈值**

   当前真实硬件记录中 Cave paired GI delta 低于阈值，Ultra 结果也受该失败影响。必须先确认 fixture 确实包含可观测的 emissive indirect contribution，再修复输入、资源版本或 composite 路径；不能只降低阈值。

3. **artifact 和 CI 证据未闭环**

   artifact 必须离开被忽略的 `bin/` 临时输出，至少包含 revision、GPU/driver、fixture seed、camera、ROI、extent、tier、GI mode、pass trace、distinct timer samples、probe 结果、GL diagnostics、resource snapshots、fallback 与失败原因。artifact 应归档到可追踪位置，并能由相同 revision、配置和硬件重新生成。

4. **共享 CI 失败缺少正式处理**

   `UITests` 和 `HeavenlySwordClosureTests` 必须记录完整 test identifier、失败断言、baseline revision、owner、批准范围、waiver 到期条件和复测命令。没有这些字段时不能用“文件未改动”作为豁免。

### M0-A：GI history 和资源治理仍需完成

- 保存上一帧 extent、zoom 以及所有 GI input version；任何变化都必须将 history weight 置零。
- 保存 occupancy/depth history，重投影后执行 disocclusion rejection。
- 为 resize、zoom、emissive 变化、occupancy 变化和相机移动增加 paired fixture。
- 继续验证 VFX snapshot 的版本与当前帧一致，并补充遮挡、VFX、history invalidation readback。
- 完整 depth reprojection 是规格允许的残余风险，但 occupancy/disocclusion rejection 不是可跳过项。

### M0-B：RenderGraph 基础设施尚未全部闭环

- 将所有 descriptor/access 从名称解析迁移到 typed tag 或 stable ID，并拒绝一个 access 解析到零个或多个逻辑资源。
- 将 FBO、texture、buffer、VAO、query 和 mapping 纳入明确 owner 与 RAII 生命周期；禁止生产 pass 绕过 registry 创建未登记资源。
- 在每个 rendered frame 调用 `GPUResourceRegistry::AdvanceFrame`，并让 created/destroyed/live 统计可用于五秒边界检查。
- 将 production shader reload 和 capability matrix 接入唯一的 `ShaderReloadGovernance` 路径，删除或登记独立 manager 的 bridge debt。
- 在 `RenderSyncContracts` 中明确 `flush -> ScopedGLState -> execute -> flush` 的 executor 依赖、线程边界和失败行为。
- 对 deferred pass、旧 ABI 和 bridge API 建立 debt register，逐项指定 owner、退出条件和删除版本。

### M1-D：增量 JFA 还缺真实性能证据

- production 默认必须是 full JFA；增量只能由显式配置开启。
- 在同一 1920x1080 fixture、同一目标 GPU、同一 revision 上提交真实 full/incremental GPU work。
- 只统计不同 frame 的 Valid GPU timestamp，比较 P95；增量必须满足规格要求的改善比例。
- 同时比较 GPU 输出与 deterministic EDT/Full-JFA reference，覆盖移动遮挡物和视图变化。
- 任何正确性、稳定性或性能失败都必须在当前帧重跑 full JFA，并将 mode、误差、fallback reason 和 artifact revision 写入报告。

### M2-E：DRS、HUD 和 Auto Exposure 仍保持关闭

- 缩放 scene target 时同步 camera、viewport 和 mouse-to-world conversion，并补输入坐标回归测试。
- 持久化最后一次 `AdaptiveQualityDecision` 和 active `RenderTargetExtent`。
- HUD 至少展示 reason、sample frame、target extent、当前 scale 和 fallback state。
- HDR histogram、adaptation、Gameplay integration 和 Phase 4 验证完成前，auto exposure 保持 disabled/locked。
- SPH、DRS、auto exposure 和其他高风险视觉特性在 M0-C GO 前不得改变默认安全值。

## 推荐实施顺序

### 1. 先固定证据模型

先定义一个版本化 `GateArtifact` schema，把 fixture 输入、运行时 trace、GPU timing、probe、diagnostics、resource snapshots 和 verdict 放在同一份报告中。字段缺失、版本不匹配或来源不是 production path 时直接 `NOT_RUN`，不要用默认值填充。

### 2. 以真实 Gameplay harness 为唯一驱动

门禁只允许调用生产初始化顺序：固定 seed -> 初始化 Gameplay runtime -> 创建 owned offscreen target -> `GameplayState::OnEnter` -> `OnUpdate/OnRender` -> 捕获 composite -> 销毁并检查 owner。测试专用替身只能位于测试层，不能在生产 gate 中构造空 registry、空 context 或 synthetic graph。

### 3. 让 RenderGraph 先完成身份和同步合同

所有 pass 在 build 阶段解析 stable resource ID，按资源保存前一状态，并在 immutable compiled plan 中生成 transition。execute 阶段只消费计划，不根据当前 access 临时推导 barrier。timer 也必须绑定 stable pass ID 和 frame sequence，由单一 owner 创建、结束、poll 和消费。

### 4. 再完成 GI 输入和 history 正确性

VFX snapshot、occupancy、SDF、depth 和 GI history 都必须具备当前帧 version。任何 extent、camera、zoom、occluder 或 input version 变化先拒绝 history，再决定是否允许重投影复用。paired capture 必须使用同一 seed、相机轨迹和资源输入，唯一变量是 feature override。

### 5. 最后启用增量优化和质量策略

先以 full JFA、GI 安全配置和固定 render scale 建立基线，再在同一硬件上采集增量 JFA 与 DRS 数据。优化路径必须是 opt-in、可观测、可回退；没有正确性、稳定性和 P95 证据时保持关闭。

## 验收与发布门槛

以下条件必须全部满足，才允许重新进行最终审查：

- `./build.bat` 和相称的 CTest/单测通过。
- Python runner 对 `GO`、`NO_GO`、`NOT_RUN`、缺失、重复和非法 verdict 均有回归测试。
- 三个真实 Gameplay fixture 的所有 paired capture、SDF/occupancy/ray-stop probe 和 pass trace 通过。
- 所有 declared pass 都有足够数量的不同 frame 的 Valid GPU samples，P95 不超过预算。
- GL 高严重度消息为零，resource registry 五秒边界无单调净增长。
- artifact 已包含硬件身份、revision、配置、完整失败原因，并可在目标硬件复现。
- M0-A、M0-B、M0-C、M1-D、M2-E 的 index、metadata、plan、spec、validation 和总进度状态一致。
- 共享 CI 失败已修复，或具备完整、未过期、可复测的正式 waiver。

在上述条件全部满足前，唯一允许的发布状态是 `NO-GO` 或 `NOT_RUN`；局部测试通过只能作为实现证据，不能作为生产验收证据。

## 验证记录模板

每次后续整改至少记录：

```text
revision:
hardware:
driver:
command:
fixture:
seed:
camera:
roi:
extent:
quality_tier:
gi_mode:
pass_trace_revision:
valid_timer_frames:
sdf_occupancy_probes:
ray_stop_probes:
gl_diagnostics:
resource_snapshots:
jfa_mode:
jfa_fallback:
result: GO | NO_GO | NOT_RUN
failure_reasons:
artifact_path:
retest_condition:
```

任何未实际采集的字段必须保持缺失并导致 `NOT_RUN`，不得写入零值后宣称通过。
