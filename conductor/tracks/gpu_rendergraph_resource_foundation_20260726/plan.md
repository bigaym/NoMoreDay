# GPU RenderGraph and Resource Foundation 实施计划

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [ ] Planned

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
