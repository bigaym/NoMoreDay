# GPU RenderGraph and Resource Foundation 规格说明书

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **类型**: P0 refactor/foundation
> **依赖**: `gpu_production_hdr_gi_closure_20260726`
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
> **状态**: 🚧 In Progress — 2026-07-26 集成整改

---

## 1. 问题与目标

当前 `RenderGraph::Build` 只收集 pass 声明和 name/tag/owner 合同，`Execute` 按插入顺序执行。资源没有 format、extent、stage、usage、物理句柄、history 或 lifetime 描述，因此 RenderSystem 的手工条件链还要维护 barrier、resize、owner 与资源状态。GPU timing、显存、ABI、shader reload、capability 诊断也各自绕过统一合同。

本 Track 建立**保持既有视觉顺序的 compiled-plan 基础**：资源/访问先编译为显式 edge 和 transition，再执行必要 barrier；资源生命周期、计时、ABI 与设备能力共享描述。它不是 Vulkan 抽象，也不承诺 OpenGL async compute。

## 2. 范围

### 必须交付

- typed resource descriptor、typed pass access、producer-consumer edge、history/lifetime 声明。
- compiled plan：检测缺失生产者、非法多写、循环、未声明读写和顺序违反；以现有插入顺序为拓扑 tie-breaker，不自动重排。
- 按 OpenGL 4.3 usage/stage 映射 image、SSBO、texture fetch、framebuffer attachment transition 的 barrier，并在开发/测试验证。
- observer-only GPU resource registry，覆盖纹理、FBO、buffer、VAO、query、persistent mapping 的 owner、descriptor、字节、生命周期、峰值和预算。
- 跨帧 GPU timer-query ring，区分 `Pending`、`Valid`、`Unavailable`、CPU fallback；自动降级只消费 Valid GPU 样本。
- 单一 manifest 覆盖 V5 ABI/binding，包括 Fluid 残留手写 layout/local binding；递归 include hash、reload 失败重试、compile diagnostics、capability gate 和 GL debug callback。

### 非目标

- 不改变前序 Track 固化的视觉/GI 顺序与质量策略。
- 不实现跨队列、async compute、Vulkan 后端或通用 renderer abstraction。
- registry 不成为 global raw-GL owner；不以观测名义绕过 RAII。
- 不实施 DRS、曝光或 JFA dirty-region，只为它们提供可信基础。

## 3. 架构合同

### 描述与计划

每个逻辑资源必须声明稳定 ID、kind、format、extent policy、mip/layer/sample、usage、lifetime、history relation、预算类别。每个 pass access 声明读写模式、pipeline stage、binding/attachment 与期望 usage。external composite target 标记 external；HDR/history persistent；当前帧中间资源 transient。

`CompiledRenderPlan` 建立 producer-consumer edge 并验证原插入顺序是合法拓扑序。消费者位于生产者之前时编译失败并报告 pass/resource；本阶段不重排。transient aliasing 只保留关闭状态的未来开关，必须以兼容 descriptor 和无重叠 lifetime 证明后才可开启。

### 同步、生命周期与观测

每一跨 pass transition 由 `(previous access/stage, next access/stage, resource kind)` 映射 GL barrier。无法映射的计划在 debug/test 失败，Release 关闭相关 feature 并说明原因。pass-local 特殊同步也要声明，禁止隐藏跨 pass barrier。

registry 只接收 create/recreate/destroy observer 事件，实际释放仍由现有 RAII wrapper/pass owner 执行。报告必须含 current/peak bytes、owner/type 分类、创建/释放计数和预算；驱动 VRAM extension 数据与引擎估算严格分列。

### 计时、ABI、设备

每个 pass 使用多槽 query ring，在 N 帧后只读取 ready query。未就绪保持 Pending，绝不写 `0ms`。HUD、统计和 auto-degrade 分别显示有效 GPU、CPU 提交和无样本。

manifest 生成或验证 C++ layout、GLSL layout、binding 常量。shader watch fingerprint 递归包含 `#include`；编译失败保留上次成功 fingerprint，持续重试并输出文件/include chain/driver log。capability gate 检查 GL 4.3、SSBO、compute/image、barrier、required format、timer 与 debug callback，缺失时使依赖 feature fail-closed 至明确回退。

## 4. 验收标准

- [ ] 所有生产 pass 的资源使用 typed descriptor/access 声明，GI/HDR/history/SSBO/external target 可导出 compiled plan。
- [ ] compiled plan 对生产者、读写、cycle、顺序、history 与 transition 给出确定性诊断，正常计划保持批准顺序。
- [ ] compute image/SSBO、attachment-to-sample、history ping-pong、external composite transition 有声明式同步。
- [ ] registry 覆盖引擎创建的 texture/FBO/SSBO/VBO/VAO/query/persistent mapping，无双重所有权。
- [ ] profiler 不将未就绪 query 记为零，自动降级不混用 CPU fallback 与 GPU 数据。
- [ ] ABI/binding check、include 修改、失败重试、capability fallback 与 GL diagnostics 有自动化测试。
- [ ] 构建、unit、integration、CI、performance 和目标 GPU smoke 通过，旧 V3/V5 contract 入口持续兼容。

## 5. 风险

- 迁移会揭露现有未声明依赖；先诊断、逐 pass 迁移，禁止一次性替换渲染路径。
- 过度保守 barrier 可能回归性能；每种映射用功能和有效 GPU timing 对照，不能为性能删除正确性 barrier。
- registry 只精确统计已登记引擎资源，驱动总 VRAM 需要可选扩展/外部工具，报告必须披露范围。
