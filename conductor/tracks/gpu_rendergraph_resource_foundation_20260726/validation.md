# GPU RenderGraph and Resource Foundation 验证记录

> **状态**: 部分完成，production NO-GO
> **依据**: [集成整改 Track 审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md)

## 2026-07-28：资源生命周期与帧同步

- 变更：registry 重复登记 accounting、`GetCurrentFrame()`、RenderGraph 帧边界 `AdvanceFrame()`、统一 pass flush entry/exit、PersistentBuffer/Distortion/JFA buffer observer、timer query observer。
- 构建：`build.bat` 通过；证据日志 `%TEMP%\\NoMoreDay_m0b_registry_buffers_build.log`。
- 合同测试：`bin\\NoMoreDayTests.exe --test-case=*RenderGraph*` 通过 23 cases/173 assertions。
- 解释：测试覆盖 registry accounting、stable pass ID/history、typed identity drift 和 compiled transitions。ShaderReloadGovernance 测试故意注入一次失败以验证保留成功 hash，最终 doctest 为 SUCCESS。
- 未关闭：compiled transition 已成为当前 RenderGraph 执行器的 barrier 来源，但未声明 access 与其余 legacy access 仍需收敛；VAO/全部 buffer owner、统一 reload/capability governance、硬件 resource snapshot 仍未满足 Track 退出标准。因此不能恢复 M0-B 完成状态，也不能改变生产 NO-GO。

## 2026-07-28：单一 GPU timer owner 与 GL 状态回归

- 根因：RenderGraph 的 `GPUTimerQueryRing` 与 `RenderProfiler` 同时开启 `GL_TIME_ELAPSED` query，第二个 `glBeginQuery` 产生 `GL_INVALID_OPERATION (0x502)`。
- 修复：RenderGraph 内 RenderProfiler 改为 CPU-only pass 采样，GPU query 只由 `GPUTimerQueryRing` 持有；RenderProfiler 独立 benchmark API 不变。
- 构建：`build.bat` 通过，证据日志 `%TEMP%\\NoMoreDay_single_timer_owner_final_build.log`。
- `bin\\NoMoreDayTests.exe --test-case=*RenderGraph*`：23 cases/173 assertions 通过；真实 Target Capture：1 case/3 assertions 通过。
- 完整 RTX 4070 门禁中不再出现 `0x502` 或 `GPUEntitySystem::Get()`，但 C++ verdict 仍为 `NO_GO`，剩余 Cave paired GI differential 与 M0-C/R6 条件未闭合。
- 该项关闭 GL query ownership 污染，但不等于 M0-B Track 完成；VAO/其余 buffer observer、reload/capability governance 和硬件 smoke 仍未完成。

## 2026-07-28：VertexArray observer 起步

- `ResourceKind` 新增 typed `VertexArray`；`FullscreenQuad` 在 VAO 创建/销毁处登记和注销，保留真实 owner 的 RAII 生命周期。
- 构建：`build.bat` 通过，证据日志 `%TEMP%\\NoMoreDay_vertex_array_registry_build.log`。
- 合同测试：`bin\\NoMoreDayTests.exe --test-case=*RenderGraph*` 通过 23 cases/173 assertions；真实 Target Capture 通过 1 case/3 assertions。
- 仅公共 fullscreen VAO 已覆盖，GPUText/GPULoot/Particle/Skill/MDI/Popup/Fluid/Trail/Entity 等专用 VAO/VBO 仍待登记，R3 继续保持部分完成，生产 NO-GO。
