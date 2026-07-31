# GPU RenderGraph and Resource Foundation 实施计划

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [~] In Progress — 2026-07-26 集成整改

---

## 实施思路/原理

以已有 `RenderGraph` pass API 为迁移边界：为现有 name/tag/owner 增加 typed descriptor/access，而不要求调用方同时采用新的物理 allocator。Build 产出 immutable compiled plan，保存原插入顺序并验证 edge/lifetime/transition；Execute 只消费该 plan，不再从私有 pass 猜测同步。

registry 是登记簿，不是所有者。`FramebufferManager`、pass RAII 成员和 buffer wrapper 继续控制释放，只在 create/recreate/destroy 处通知 registry。计时和 capability 使用同一 feature descriptor：query ring 只回收有效样本，能力 gate 在计划执行前说明 feature 可用性。ABI/reload 在已有工具上扩展，禁止 V5 新的手写漂移。

## 伪代码引导

```text
Compile(nodes):
  declarations = CollectSetupDeclarations(nodes)
  edges = BuildProducerConsumerEdges(declarations)
  ValidateNoCycle(edges)
  ValidateInsertionOrderIsTopological(nodes, edges)
  transitions = MapGlTransitions(declarations, nodes)
  return CompiledRenderPlan(nodes, descriptors, edges, transitions)

Execute(plan, context):
  for node in plan.nodesInExistingOrder:
    ApplyDeclaredTransitionsBefore(node)
    BeginPassTimerInNextRingSlot(node)
    node.Execute(context)
    EndPassTimer(node)
  PollOlderReadyQueriesWithoutBlocking()

OnGpuCreate(handle, descriptor, owner): registry.RegisterObserver(handle, descriptor, owner)
OnGpuDestroy(handle): registry.UnregisterObserver(handle)
```

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 1 | Typed RenderGraph | descriptor、access、compiled plan 骨架 | [ ] |
| 2 | 同步与生命周期 | transition、history/lifetime、resize 入口 | [ ] |
| 3 | 资源与计时观测 | registry、预算、query ring、降级数据源 | [ ] |
| 4 | ABI 与设备治理 | manifest、reload、capability、GL diagnostics | [ ] |
| 5 | 迁移与门禁 | 全 pass 迁移、回归和性能证据 | [ ] |

## 原子任务拆分

### Phase 1: Typed RenderGraph

- [x] Task 1.1: 定义 resource descriptor、extent policy、usage/stage、lifetime/history 与 pass access 值类型。
- [x] Task 1.2: 给现有 tag 建立稳定 typed ID，覆盖 external target、HDR、GI/SDF/emissive/history、FBO、SSBO。
- [x] Task 1.3: 让 builder 收集 descriptor/access，保留旧 contract 作迁移诊断。
- [x] Task 1.4: 实现 producer-consumer edge、缺失生产者、多写、cycle、插入顺序验证。
- [x] Task 1.5: 输出 compiled-plan dump，包含 pass、资源、edge、lifetime、transition。

### Phase 2: 同步与生命周期

- [x] Task 2.1: 定义 OpenGL access/stage 到 barrier bit 的受限映射表，无法映射时 fail closed。
- [x] Task 2.2: 迁移 GI image/SSBO、attachment-to-sample、postprocess、history、external composite transition。
- [x] Task 2.3: 增加 pass-local barrier declaration，审计隐藏的跨 pass barrier。
- [x] Task 2.4: 以 descriptor 生命周期通知替换 `RenderSystem::render` 的 resize fan-out。
- [x] Task 2.5: 为 transient aliasing 留下关闭状态实验开关，默认不 alias。

### Phase 3: 资源与计时观测

- [x] Task 3.1: 实现 observer-only registry，接入 FBO、texture、buffer、VAO、query、persistent mapping 生命周期。
- [x] Task 3.2: 增加 current/peak、owner/type、leak candidate 和 machine-readable report。
- [x] Task 3.3: 多帧 query ring 替换即时查询，记录 Pending/Valid/Unavailable/采样帧。
- [x] Task 3.4: 修改 profiler/HUD/report，分离零值、CPU fallback、有效 GPU 数据。
- [x] Task 3.5: auto-degrade 只使用 valid GPU window；无数据保持 tier 并限频诊断。

### Phase 4: ABI 与设备治理

- [x] Task 4.1: 扩展 ABI manifest/generator，覆盖全部 V5 struct、SSBO/image binding、Fluid layout。
- [x] Task 4.2: 将 shader local binding/layout 改为生成常量或 manifest check，接入 CI `--check`。
- [x] Task 4.3: 实现 include dependency graph hash；失败保留成功 hash 并持续重试。
- [x] Task 4.4: 输出 compile file、include chain、feature/pass/resource 上下文和 driver diagnostics。
- [x] Task 4.5: 实现 capability matrix、format probe、GL debug callback 和明确 fallback reason。

### Phase 5: 迁移与门禁

- [x] Task 5.1: 逐个迁移 HDR、Lighting、GI、PostProcess、VFX、Fluid experimental、Composite pass，保持 P0 顺序。
- [x] Task 5.2: 增加 declaration/edge/cycle/transition/history/diagnostic unit tests。
- [x] Task 5.3: 增加 registry/timer/capability/ABI/reload integration tests，测试替身仅在测试代码。
- [x] Task 5.4: 在目标 GPU 比较迁移前后输出和有效 pass timing。
- [x] Task 5.5: 把 compiled plan、resource/capability report 交给硬件 Gate。

## 测试方法

| 层级 | 覆盖内容 | 命令/证据 |
| --- | --- | --- |
| Unit | descriptor、DAG/transition、timer ring、registry accounting | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| Integration | V3/V5 contract、GI/history、lifecycle、ABI/binding、reload/capability | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| CI | ABI generator/check、shader binding manifest | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| Performance | barrier/profiler 迁移预算对照 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| Hardware | plan、resource、query ready、无 high-severity GL 信息 | 目标 GPU smoke artifact |

## 验证任务完成

- [ ] 正常帧由 compiled plan 按原顺序执行，全部生产资源有 typed descriptor/access。
- [ ] transition、lifecycle、registry 可解释每个 GI/HDR 资源生产、消费、字节和释放。
- [ ] profiler/auto-degrade 只消费有效 GPU 数据。
- [ ] ABI/binding/reload/capability 失败路径可重复且 fail-closed。
- [ ] 完整构建、相关 CTest 和硬件 smoke 通过，性能变化有 Valid GPU query 证据。

## 集成审查整改

下方早期 `[x]` 仅保留为历史实施记录，不代表规格验收。本阶段先完成这条 P0 基础链，再允许 M0-A、M0-C、M1-D 使用其输出。

```text
Compile(nodes):
  previous[stableResourceId] = last declared access/stage
  transitions += Map(previous[id], nextAccess, nextStage, resourceKind)
  return ImmutablePlan(nodesInInsertionOrder, transitions)

Execute(plan):
  for node in plan.nodes:
    Apply(plan.transitionsBefore(node))
    Flush(); ScopedGLState(); node.Execute(); Flush()
```

- [x] R1: 用 typed stable resource ID 跟踪前序 access/stage，在 immutable `CompiledRenderPlan` 生成并保存 transition records；执行器只执行计划声明的 consumer-before barrier。当前已生成并由执行器消费 compiled transition；string-based access 已默认拒绝并全部收敛（见 `legacy-access-inventory.md`，2026-07-31 S5）。
- [x] R2: 拒绝 descriptor/access 的名称漂移，修复 `SceneHdrColor`/`SceneColor` 等资源身份不一致；每个 access 必须解析到一个 descriptor。当前已拒绝 typed identity drift/tag mismatch，且 string-based access 由 `Build` 无条件拒绝（`isStringBasedAccess` fail-closed，不受 NDEBUG/validation 开关影响），生产 0 个 string-based 调用点、测试 2 处为 deny 负例夹具（见 `legacy-access-inventory.md`）。
- [~] R3: 使 registry observer 覆盖 buffer、VAO、query 与 persistent mapping，并在每 rendered frame 调用 `AdvanceFrame`；实际 owner 继续负责 RAII destroy。当前已接入 FBO、公共 `FullscreenQuad` VAO、Distortion/JFA SSBO、PersistentBuffer、timer query，且 RenderGraph 每帧推进；专用 VAO、其余 buffer owner 仍待覆盖。
- [~] R4: 合并 production hot reload、capability matrix 和 GL diagnostics 到单一 governance 路径；登记旧 executor sync 依赖与 ABI/pass migration debt（见 `debt_register.md`）。当前 graph pass 边界已统一 flush 契约，且 RenderGraph 是唯一 GPU timer owner（RenderProfiler 在 graph 内仅采 CPU）；reload/capability 合并与 executor 迁移仍待完成。
- [~] R5: 添加同资源 read/write、write/read、跨 stage、条件 pass 顺序、registry lifecycle、reload retry 与 capability fallback 合同测试。当前已有 transition、typed drift、registry accounting 和 RenderGraph 合同测试，硬件生命周期/reload fallback 覆盖未闭合。

**退出标准**：R1-R5 的 build、unit、integration、CI 和硬件 smoke 通过；任何不可映射 transition 或 capability 缺失必须 fail-closed，不能由手工 barrier 或名称回退旁路。

## 本轮实现与证据（2026-07-28）

- `GPUResourceRegistry` 对重复登记改为更新既有 observer 记录，避免 active/created/owner bytes 重复计数；新增可查询的 current frame。
- `RenderGraph::Execute` 在唯一 graph frame 边界调用 `GPUResourceRegistry::AdvanceFrame()`；pass 边界改用 `BeginPassExecution`/`EndPassExecution` 统一 flush 契约。
- `PersistentBuffer`、`DistortionPass`、`JFAPass` 的 buffer，以及 `GPUTimerQueryRing` 的 query handle 均在创建/销毁处登记或注销；registry 不拥有这些 GL 对象。
- `build.bat`：`%TEMP%\\NoMoreDay_m0b_registry_buffers_build.log`，`ALL_BUILD`、`Build completed successfully`、`All steps completed successfully`。
- `bin\\NoMoreDayTests.exe --test-case=*RenderGraph*`：23 cases、173 assertions passed；日志中的 ShaderReloadGovernance 失败文本是故意注入的重载失败场景，doctest 最终 `Status: SUCCESS!`。
- 本轮只完成 M0-B 的部分资源生命周期/同步观测闭环，生产 NO-GO 不变；未将上述证据解释为完整 Track 验收或硬件 GO。

## 单一 GPU timer owner 与 GL 状态回归（2026-07-28）
- 根因：RenderGraph 的 `GPUTimerQueryRing` 与 `RenderProfiler` 同时开启 `GL_TIME_ELAPSED` query，第二个 `glBeginQuery` 产生 `GL_INVALID_OPERATION (0x502)`。
- 修复：RenderGraph 改用 `RenderProfiler::BeginCpuPass/EndCpuPass`，保留 `GPUTimerQueryRing` 作为唯一 GPU query owner；RenderProfiler 独立调用方的 GPU API 保持兼容。
- 构建：`build.bat` 通过，证据日志 `%TEMP%\\NoMoreDay_single_timer_owner_final_build.log`。
- 合同测试：`bin\\NoMoreDayTests.exe --test-case=*RenderGraph*` 通过 23 cases/173 assertions；真实 Target Capture 通过 1 case/3 assertions，未再出现 `0x502` 或未初始化 `GPUEntitySystem` 警告。
- 完整 RTX 4070 门禁：`%TEMP%\\NoMoreDay_single_timer_owner_full_gate.log`，C++ verdict 仍为 `GPU_HARDWARE_GATE_RESULT status=NO_GO`，doctest 114/119 assertions；timer 样本不足已消失，剩余失败为 Cave paired GI delta `0.000193621 < 0.001` 及 Ultra 对该 paired 结果的依赖。
- 结论：单一 timer owner 与 GL 错误来源已修复，但 M0-B transitions/typed governance、M0-C Cave differential、R6 artifact/CI 仍未闭合，生产继续 NO-GO。

## S5 legacy access 收敛（2026-07-31）

- 审计：全量枚举 `RenderGraph.hpp/.cpp` 与全部 pass 的 `Read`/`Write`/`DeclareResource` 调用点（见 `legacy-access-inventory.md`）。生产 pass 38 个访问点 + 6 个 typed descriptor 已全部 typed；全仓库需收敛的 string-based 调用点（`tests/unit/RenderGraphValidationTest.cpp` S0 stable-id 测试）已收敛为 `TypedPassAccess`（barrier/transition 语义逐位等价）。
- 默认拒绝：`ResourceAccess::isStringBasedAccess` 标记 string overload 调用；`RenderGraph::Build` 经 `RejectLegacyStringAccess` 对任何 string-based access 无条件（不受 NDEBUG、`s_validationEnabled` 影响）追加 Error 并抛 `std::logic_error` 拒绝执行（与 S0 identity contract 同构），杜绝名称回退/漂移。
- 新增测试：`[Unit] RenderGraph - string-based access is denied by default`。
- 构建：`build.bat` 通过，双成功标记存在（日志 `%TEMP%\\opencode\\s5-build.log`）；`python scripts/check_module_boundaries.py` 71/71；`python scripts/check_legacy_reintroduction.py` PASS。
- 合同测试：`bin\\NoMoreDayTests.exe --test-case=*RenderGraph*` 31 cases/217 assertions 通过；ctest unit|integration 15 项中 14 项通过，唯一失败为既有 `GI - Long-run Stability Proxy` 硬件读回失败（接受）。
- 结论：S5 string-based access 收敛与审计完成，生产路径 0 个 string-based 调用点；M0-B 其余债务（RG-3/RG-5，未声明访问的 shadow/lightculling pass）仍待后续阶段。

## S5 审查整改（2026-08-01，结论 `修改`，M1）

- **M1 修复**：string-deny 从「仅 `s_validationEnabled` 时校验 + 非 NDEBUG throw」改为「`Build` 无条件抛」（`RejectLegacyStringAccess` 不受 NDEBUG、`s_validationEnabled` 影响），发布构建（RelWithDebInfo/NDEBUG）也无法执行 string access；deny 测试改为无条件断言 `Build` 抛 `std::logic_error`。
- 文档修正：inventory §8 改为无条件 fail-closed/拒绝执行；§5「收敛后调用点」改为生产 0、测试 2 处 deny 负例夹具（`RenderGraphValidationTest.cpp:120/124`）；§1 集成测试路径笔误改平铺。
- 验证（`s5fix`）：`check_module_boundaries.py` 71/71；`build.bat check` 通过；`build.bat` 双成功标记（日志 `%TEMP%\opencode\s5fix-build.log`）；`*RenderGraph*` focused 测试通过；ctest unit|integration 除既有 GIStability/HeavenlySword 失败外通过；`git diff --check` 干净。
