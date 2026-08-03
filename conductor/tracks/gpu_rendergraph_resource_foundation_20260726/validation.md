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

## 2026-08-02：W5 RG-3 生命周期修复（MS-8）

- 变更：registry 记账安全语义（duplicate registration reject + diagnostic，绝不计入计数器；`UnregisterResource`/`UpdateResourceSize` missing-record no-op + 诊断；尺寸更新防下溢）；`ResourceKind` 新增 `VertexArray` 与 `ShaderProgram`；`ComputeBuffer` 与 `PersistentBuffer`（含 Persistent mapping 记录）接入 observer-only registry；`GPUEntitySystem` 显式幂等 `Shutdown`（释放 physics output/block-sum buffer、raw render shader、quad VAO/VBO，先注销后释放，部分初始化回滚，状态清零，不触碰 ResourceManager 所有的 5 个 compute shader）；exactly-one 帧推进（`RenderSystem::render` 中 `graph.Execute` 成功后一次 `AdvanceFrame`，删除 `GPUHardwareValidationGate` 手动 advance）；FullscreenQuad VAO 登记恢复（S5 重构丢失，本轮重新登记）。
- 构建：`build.bat` 通过（RelWithDebInfo, ALL_BUILD j=7，legacy marker/模块边界/MS-1/ABI 治理/资产校验全部 OK），stderr 为空。
- 合同测试：
  - `bin\NoMoreDayTests.exe --test-case=*W5*`：3 cases / 18 assertions 通过（GPUEntitySystem 生命周期 registry 平衡、exact-one advance、未初始化 Shutdown no-op）。
  - `bin\NoMoreDayTests.exe --test-case=*GPUResourceRegistry*`：7 cases / 53 assertions 通过（含 4 个新记账测试）。
  - `bin\NoMoreDayTests.exe --test-case=*RenderGraph*`：31 cases / 217 assertions 通过（V5 契约回归）。
  - `ctest -L unit`：8/9 通过；`nmd.tests.skill.unit` 首次运行失败、重跑通过（已知 flaky HeavenlySwordClosureTests，与 W5 无关）。
- 证据：W5 集成测试断言内容如下（如实）——`GPUEntitySystem` 专属记录（`GPUEntityRenderShader`/`GPUEntityQuadVAO`/`GPUEntityQuadVBO`/`ComputeBuffer`/`PersistentBuffer`/`PersistentBufferMapping`）在 `Shutdown` 后全部注销（具体 handle 逐一验证不存在）；registry `Reset()` 后的 active count 与 current bytes 基线在生命周期测试中恢复为零；`totalDestroyedCount - baseline >= Init 创建数`（created/destroyed 平衡，且只多不少——render 全局记录若被注销会计入 destroyed）；正常帧 `frameIndex` 精确 +1；GL 诊断清空。**注**：全局 active/bytes 归零断言只适用于未触碰全局渲染路径的生命周期用例；全局渲染路径（`RenderSystem::render`）可能残留引擎静态资源记录，故不作为 balance 断言。
- 未关闭：引擎其余专用 VAO/VBO owner（GPUText/GPULoot/Particle/Skill/MDI/Popup/Fluid/Trail）与 5 个 ResourceManager compute shader 仍未登记（compute shader 属 ResourceManager `unloadAll` 唯一所有，registry 不观察，避免双重所有权）；外部 target 合同因 5c257e22 不完整（遗留，非 W5 范围）；M0-C 硬件证据未产生。生产保持 NO-GO。

## 2026-08-02（复测）：W5 修正轮（reviewer 结论「修改」的 6 项处置）

- High 1 `PersistentBuffer` 状态复位：新增不执行 GL 操作的私有 `ResetState()`（恢复默认 `Compat` 状态，清零 bufferId/尺寸/slot/count/mode/fence/staging），在 `Destroy()` 末尾与 move 源对象上调用；已销毁/被移动对象不再能以 Persistent 模式访问清空的 `m_fences`。
- High 2 `ComputeBuffer::OrphanAndUpload`：`BufferData` 重分配后若 `size != m_size` 则更新 `m_size` 并调 `GPUResourceRegistry::UpdateResourceSize(id, StorageBuffer, size)`（等尺寸为 no-op，防下溢饱和），registry bytes 与 GL 容量一致。
- High 3 生命周期测试断言强化：`Init` 前捕获 baseline snapshot；`Shutdown` ×2 后断言 active count 与 current bytes 恢复 baseline（Reset 后为零）、具体 handle（shader/VAO/VBO）逐一验证已注销、`totalDestroyedCount - baseline >= Init 创建数`；`unloadAll()` 之后再 drain GL errors。上方 2026-08-02 小节的「证据」已同步修正为如实表述（全局渲染路径可能残留引擎静态记录，故 balance 断言仅限生命周期用例）。
- High 4 `GPUEntitySystem` move 语义：move 构造/赋值改为 `= delete`（原为 default，会复制裸 Shader/VAO/VBO 句柄且源不清零）。类持有裸 GL 句柄且无任何调用点需要 move（Game 持成员），删除使误用成为编译错误。
- Medium 5 partial-init 回滚：`Init` 每步创建后检查 `GetId() != 0`（2 个 PersistentBuffer + 5 个 ComputeBuffer），并新增 grid compute shader 依赖集完整性检查（`ResourceManager::loadComputeShader` 文件缺失返回 `Shader{0}`）；任何失败统一走幂等 `Shutdown()`。新增集成测试 `[Integration] W5 - GPUEntitySystem partial-init failure rolls back to baseline`：用独立 `ResourceManager` 实例（缓存为空）+ RAII 隐藏 `grid_scan.compute` 注入失败，断言 `GetMaxEntities()==0`、registry 无残留、基线恢复、GL 诊断清空（文件由 guard 还原）。
- Medium 6 pending age 边界：`TakeSnapshot` 判定由 `age <= 9` 改为 `age < 9`（9 帧窗口 = age 0..8，age 9 即 quiesced），注释同步；新增 `[Unit] GPUResourceRegistry - pending window boundary is age 8 pending / age 9 quiesced` 锁定边界。
- 复测结果（2026-08-02 修正轮）：
  - `build.bat` 通过（legacy marker/模块边界/MS-1/ABI/资产校验全部 OK），stderr 为空。
  - `bin\NoMoreDayTests.exe --test-case=*W5*`：4 cases / 28 assertions 通过（新增 partial-init rollback 用例）。
  - `--test-case=*GPUResourceRegistry*`：8 cases / 57 assertions 通过（新增 age 8/9 边界用例）。
  - `--test-case=*RenderGraph*`：31 cases / 217 assertions 通过（V5 契约回归）。
  - `ctest -L unit` 全量：`nmd.tests.unit` 与 `nmd.tests.ai.unit` 首跑失败、各自单独重跑通过（已知 flaky `HeavenlySwordClosureTests.cpp:97`，与 W5 无关）；`nmd.tests.skill.unit` 等其余通过。
- 结论：6 项修正全部落地并复测通过；W5 范围与未关闭项不变（同上小节），生产保持 NO-GO。

## 2026-08-02（M0-B）：外部 target 合同补齐（MS-8 剩余风险第 4 项）

- 变更（gate 层）：`GPUHardwareValidationGate.hpp/.cpp` 新增 `TargetAttachmentState` 与静态 `CaptureTargetState()`——经 `glfwGetProcAddress` 解析合法 GL 4.3 入口（`glGetFramebufferAttachmentParameteriv`/`glGetTexLevelParameteriv`/`glGetRenderbufferParameteriv`/`glGetIntegerv`/`glIsEnabled`），记录 bind/viewport/scissor 快照 + COLOR_ATTACHMENT0 的 OBJECT_TYPE/OBJECT_NAME/COLOR_ENCODING/COMPONENT_TYPE/各分量 size + texture-level/renderbuffer 参数（extent、internal format）。未复用 `5c257e22` 曾用的非法 pname（0x8D24/0x8D25/0x825D）。矩阵 cell 在 `CompositeFramebuffer()` 校验后调用；非 `passed`（missing entry→`unavailable`、fbo==0/GL_NONE/extent-format 不符→`failed`）判 cell 失败（fail-closed）；fbo==0 分支如实记录。`ToJsonString` 仅在采集过的 cell 输出 `matrix_results[*].target_state`。
- 测试新增：`[Integration] GPU Hardware Validation Gate - Target state capture verifies composite attachment`（harness 真实 RGBA16F texture attachment 断言 passed/1280x720/0x881A/GL_TEXTURE(0x1702)/16-bit RGBA/GL_FLOAT(0x1406)/GL_LINEAR(0x2601)/bind 恢复为 0；fbo==0 → failed 且 reason 非空）；`[GPU-Diagnostic]` RunGate 测试逐 matrix cell 断言 `target_state` schema 全键、status=="passed"、internal format 0x881A、object type 0x1702、extent>0。
- 构建/测试待补（被并发代理 blocker）：`build.bat` 全量构建当前被 `src/engine/render/passes/GICompositePass.hpp`（M0-A R3 代理独占、未完成状态：引用了未声明的 `m_readHistoryA` 等成员，非 M0-B 改动）阻塞；`GPUHardwareValidationGate.cpp` 编译 0 错误。聚焦测试与 ctest 需等 M0-A R3 文件修复后执行。
- 未关闭：外部 target 合同代码与文档已补齐，但完整构建/聚焦测试因并发代理未完成而待验；M0-C 硬件证据未产生；生产保持 NO-GO。
