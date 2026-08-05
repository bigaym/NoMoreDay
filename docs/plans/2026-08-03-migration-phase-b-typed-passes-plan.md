# Phase B: 5 个空 Setup pass typed 迁移

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.1
> **关闭债务:** RG-3/RG-5（最大残留组）
> **依赖:** 无（B 是 C/D 的前置地基）
> **状态:** [~] 进行中（B1/B5/B6/B7/B9-B12 完成；B8 合同级真实 GL binding fixture 已通过，但生产视觉回归证据仍缺失；B2/B3 真实 ShadowBuild 逐 pass GL gate 已新增并通过（合同级，2026-08-04，见 B2/B3 gate 明细）；B2/B3 gate 暴露的 shader 双重释放 owner blocker 已修复（2026-08-04，见 Owner blocker 明细）；B4 真实 LightCulling 逐 pass GL gate 已新增并通过（合同级，2026-08-05，见 B4 gate 明细）；B4 生产 Setup observer 接线已完成（2026-08-05：真实 `LightCullingPass::Setup` 声明 6 条 observer-only `BindBufferBase`，gate 现在能直接解析真实 pass，见 B4 gate 明细）；B4 仍不标完成，删手工绑定阻塞于真实生产视觉/资源快照证据）

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.1；M0-B spec §3/§4。
- **范围**: ShadowPreparePass、ShadowBuildPass、ShadowResolvePass、LightCullingPass、VFXEmissionSnapshotPass 五个 pass 的 Setup 声明 typed 资源/访问，Execute 内移除手工 BindBufferBase/BindImageTexture/MemoryBarrier/flush 依赖。
- **边界**: 渲染行为（tile FBO bind/viewport/clear/FullscreenQuad、dispatch 顺序）保留在 Execute；仅访问声明与同步声明化。不改变 pass 顺序与视觉输出。

## 2. Verified Baseline

- 空 Setup：LightCullingPass.cpp:41-43、ShadowPreparePass.cpp:23-25、ShadowBuildPass.cpp:38-40、ShadowResolvePass.cpp:32-34、VFXEmissionSnapshotPass.cpp:12。
- 手工同步：LightCullingPass :260-290（BindBufferBase 6 SSBO、DispatchComputeNoBarrier、MemoryBarrier 0x2000、ApplyRlglFlushTemplate）；ShadowBuildPass :359-380（BindBufferBase/BindImageTexture/Dispatch/MemoryBarrier(Image|Buffer)）+:210-276（tile 渲染+手工恢复 hdr FBO）；ShadowResolvePass :164-200（ApplyComputeToFragmentBarrierTemplate/BindFramebuffer/恢复）；ShadowPreparePass :219（flush）。
- 资源宿主：ShadowBuildPass `m_sdfField/m_shadowAtlas/m_occluderBuffer`（FramebufferHandle/ComputeBuffer）；ShadowResolvePass `m_shadowMask`；ClusteredLightingState 5 个 ComputeBuffer。
- `RenderResourceTag`/`RenderOwnerTag` 现无 shadow/cluster 条目；switch 映射（ToResourceName:56/ToResourceTag:96/ToOwnerName:148）须同步补 case。

## 3. Implementation Rationale

1. 先扩展枚举（`ShadowAtlas`/`ShadowDistanceField`/`ShadowMask`/`ShadowOccluderSSBO`/`ClusterHeaderSSBO`/`ClusterLightIndexSSBO`/`ClusterPackedLightSSBO`/`ClusterCounterSSBO`/`LightBoundsSSBO` + owner `Shadow`/`LightCulling`），让 shadow/cluster 资源进入 graph 视图，graph 才能生成 transition、检测缺失生产者/多写。
2. 每个 pass 的 Setup 用 `DeclareResource`（持久资源一次声明）+ typed `Read/Write(Tag,Owner,Stage,Usage)`。compute 写 SSBO→后续 fragment 读，graph 经 `MapGlBarrierBits` 自动生成 compute→fragment barrier；ShadowBuild 的真实 SDF backing 为 `RG16F`。
3. SDF 写→读在同 pass 内跨 shader 阶段（compute 写 image 后 fragment 采样）使用已实现的 `AddPhaseBarrier(Compute, Fragment, bits)` 声明，并由 `RenderContext::EmitPhaseBarrier` 在 dispatch 后、tile draw 前发出；不能用 pass-entry `AddPassLocalBarrier` 替代。
4. typed binding 目前是 observer-only：`BindBufferBase`/`BindImageUnit` 只进入 compiled plan 做一致性诊断，不分配 backing、不发 GL bind、不改变 owner；真实 GL 绑定继续由 Execute 保持。
5. ShadowPrepare 纯 CPU：声明对 `ShadowOccluderSSBO` 的 Host 写依赖（Transfer/Host 段），graph 生成 Host→Compute transition，替代 flush 依赖。

## 4. Pseudocode Guidance

```text
// 枚举扩展（RenderGraph.hpp + switch 映射）
RenderResourceTag += ShadowAtlas, ShadowDistanceField, ShadowMask, ShadowOccluderSSBO,
                    ClusterHeaderSSBO, ClusterLightIndexSSBO, ClusterPackedLightSSBO,
                    ClusterCounterSSBO, LightBoundsSSBO
RenderOwnerTag     += Shadow, LightCulling

// ShadowBuildPass::Setup
DeclareResource(ShadowAtlas,          Texture2D, RGBA16F, Persistent, owner=Shadow)
DeclareResource(ShadowDistanceField,  Texture2D, RG16F,   Persistent, owner=Shadow)
DeclareResource(ShadowOccluderSSBO,   StorageBuffer,      Persistent, owner=Shadow)
Read(SceneHdrColor, Shadow, FramebufferAttachment, ShaderRead)   // 尺寸/边界
Write(ShadowAtlas,         Shadow, Fragment, ColorAttachment)
Write(ShadowDistanceField, Shadow, Compute,  StorageWrite)
AddPhaseBarrier(Compute, Fragment, Image | Buffer) // SDF compute→tile fragment

// ShadowResolvePass::Setup
DeclareResource(ShadowMask, Texture2D, RGBA16F, Persistent, owner=Shadow)
Read(ShadowDistanceField, Shadow, Fragment, ShaderRead)
Write(ShadowMask,         Shadow, Fragment, ColorAttachment)

// LightCullingPass::Setup
Read(LightBufferSSBO,        LightCulling, Compute, StorageRead)
Read(LightBoundsSSBO,        LightCulling, Compute, StorageRead)
Write(ClusterHeaderSSBO,     LightCulling, Compute, StorageWrite)
Write(ClusterLightIndexSSBO, LightCulling, Compute, StorageWrite)
Write(ClusterPackedLightSSBO,LightCulling, Compute, StorageWrite)
Write(ClusterCounterSSBO,    LightCulling, Compute, StorageWrite)

// ShadowPreparePass::Setup
Write(ShadowOccluderSSBO, Shadow, Host, StorageWrite)   // CPU 填充

// VFXEmissionSnapshotPass::Setup
// B6 修正（2026-08-04）：本 pass 真实写入 RadianceCascadesPass 的 m_particleEmissive（粒子 emissive
// 纹理，全屏 RGBA16F，EnsureResources 创建/resize）。不读 EmissiveBuffer；hdrSceneBuffer.fbo 仅作
// GL 状态恢复。粒子 SSBO 为 GPUParticleSystem 内部 backing，graph 无写者，不声明。
DeclareResource(ParticleEmissive, Texture2D, RGBA16F, Persistent, owner=RadianceCascades)
Write(ParticleEmissive, RadianceCascades, Fragment, ColorAttachment)
ImportResource(ParticleEmissive)                              // external backing 合同
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| B1 | 扩展 `RenderResourceTag`/`RenderOwnerTag` + 三个 switch 映射 case | - | [x] |
| B2 | ShadowBuildPass typed Setup + 移除手工 bind/barrier/flush | B1 + phase-aware barrier/import binding 合同 | [~]（phase barrier 已加；binding 仍 observer-only；真实 ShadowBuild 逐 pass GL gate 已过（合同级，见 B2/B3 gate 明细）；删手工 bind 仍阻塞于生产视觉/资源快照证据） |
| B3 | ShadowResolvePass typed Setup + 移除手工 barrier/FBO 恢复 | B1 + backing/import/binding 合同 | [~]（Setup 已加；原 compute→fragment barrier 保留；随 B2/B3 gate 一并验证（同 chain），删 barrier 仍阻塞于 B9 完成后的生产视觉证据） |
| B4 | LightCullingPass typed Setup + 移除手工 BindBufferBase/barrier/flush | B1 + B9 | [~]（Setup/graph binding 已接线；真实 LightCulling 逐 pass GL gate 已过（合同级，见 B4 gate 明细）；生产 Setup observer 接线已完成（6 条 observer-only `BindBufferBase`，gate 现在能直接解析真实 pass，2026-08-05）；删手工 bind 仍阻塞于真实生产视觉/资源快照证据，见 B4 明细 Blocker） |
| B5 | ShadowPreparePass Host 依赖声明 + 移除 flush | B1 | [x] |
| B6 | VFXEmissionSnapshotPass 处理（声明或并入 RadianceCascades） | B1 | [x]（用户决策：声明 `ParticleEmissive`，保留独立 pass；详见 B6 明细） |
| B7 | graph 校验断言：computed plan 含 shadow/cluster edge + transition | B2-B6 | [x]（typed access plan 测试已覆盖全部 4 条 cluster edge + transition 与 3 条 shadow edge/transition；负向诊断断言补齐，证据见 §7） |
| B8 | 视觉回归 + 边界脚本 + `legacy` 扫描 | B2-B6 | [~]（边界/legacy/unit/GPU 合同与 B12 专用真实 GL fixture 已通过；生产 Shadow/cluster/VFX 视觉回归证据仍缺失，见 B8 明细） |
| B9 | external backing import 合同（`ImportResource` + compiled plan 导出 + 校验 + 真实 pass 接线） | B1（实现完成） | [x]（observer-only 合同与资源表完成） |
| B10 | RenderContext 外部 backing handle snapshot 注入与 fail-closed 查询合同 | B9 | [x]（真实 owner snapshot 已注入；graph 不拥有 backing） |
| B11 | registry owner metadata 对齐（`ReclassifyResourceOwner` + Shadow/cluster/LightBuffer 真实 owner 接线 + unit tests） | B9/B10 | [x] |
| B12 | graph-driven binding 准入/执行合同（resolver + executor + unit tests） | B9/B10/B11 | [x]（准入与执行层实现完成；真实 GL 集成测试为剩余 gate，过前手工绑定保留） |
| B13 | 生产 GPU timing blocker 诊断与低风险优化 | B8/B12 | [~]（诊断字段已接线；尚无安全代码优化，预算不调整） |

### B9 明细：external backing import 合同

**目标**：解决/明确 shadow/cluster/LightBuffer 外部输入的 GL backing、import、binding-point/image-unit/attachment ownership 与 resize 生命周期阻塞，为将来安全删除手工绑定提供合同。observer-only，不动 ownership，不发 GL 调用。

**Resource table**（已盘点并接线到真实 pass，同 design §5.1）：

| 资源 | 真实 GL backing 创建/释放 owner | 绑定点/单元 | 格式 | resize 生命周期 |
| --- | --- | --- | --- | --- |
| `ShadowAtlas` | ShadowBuildPass `m_shadowAtlas`（FramebufferManager） | colorAttachment 0 | GL_RGBA16F | EnsureAtlasSize（capacity） |
| `ShadowDistanceField` | ShadowBuildPass `m_sdfField`（FramebufferManager） | imageUnit 0 / WRITE_ONLY / GL_RG16F | GL_RG16F | OnResize（screen） |
| `ShadowOccluderSSBO` | ShadowBuildPass `m_occluderBuffer`（ComputeBuffer） | bindingPoint 15 | SSBO | UploadOccluders（capacity） |
| `ShadowMask` | ShadowResolvePass `m_shadowMask`（FramebufferManager） | colorAttachment 0 | GL_RGBA16F | OnResize（screen） |
| `ClusterHeaderSSBO` | ClusteredLightingState `m_clusterHeaderBuffer` | bindingPoint 1 | SSBO | EnsureBufferCapacity |
| `ClusterLightIndexSSBO` | ClusteredLightingState `m_clusterLightIndexBuffer` | bindingPoint 2 | SSBO | EnsureBufferCapacity |
| `ClusterPackedLightSSBO` | ClusteredLightingState `m_clusterPackedLightBuffer` | bindingPoint 5 | SSBO | EnsureBufferCapacity |
| `ClusterCounterSSBO` | ClusteredLightingState `m_clusterCounterBuffer` | bindingPoint 4 | SSBO | EnsureBufferCapacity |
| `LightBoundsSSBO` | ClusteredLightingState `m_lightBoundsBuffer` | bindingPoint 3 | SSBO | EnsureBufferCapacity |
| `LightBufferSSBO` | LightManager `m_lightBuffer` | bindingPoint 0（LIGHT_LIST_IN） | SSBO | OrphanAndUpload |

**文件 reservation**：
- `src/engine/render/graph/RenderGraph.hpp`：新增 `ResourceImportInfo`、`CompiledResourceImport`、`RenderGraphBuilder::ImportResource`/`GetImports`、`CompiledRenderPlan::imports`、`Node::imports`。
- `src/engine/render/graph/RenderGraph.cpp`：`ImportResource` 实现、`Build()` 拷贝、`ValidateBuildContracts` import 校验段、`BuildCompiledPlan` 导出、`DumpPlan` 输出。
- `src/engine/render/passes/ShadowBuildPass.cpp` / `ShadowResolvePass.cpp` / `LightCullingPass.cpp`：Setup 内 `ImportResource` 接线。
- `tests/unit/RenderGraphValidationTest.cpp`：6 个新 TEST_CASE（contract/metadata，无 GPU）。
- `docs/designs/2026-08-03-render-engine-interface-migration-design.md`：§5.1 修正 format 表 + 新增 import 合同段。

**验收**：
- `ImportResource` 为纯声明：不发 GL、不改 ownership、graph 不得 alloc/resize/free imported backing（校验对 `Transient` descriptor 报 error）。
- compiled plan 导出 `plan.imports`（10 条），含 kind/format/backingOwner/resize 标志/绑定点/附件索引。
- fail-closed：Custom tag、同 pass 重复、跨 pass kind/format 冲突、Transient、descriptor 不一致 → error；Unknown owner、与 BindBufferBase/BindImageUnit 观察不一致 → warning。
- 真实 pass 无新 validation error；现有手工 `BindBufferBase`/`BindImageTexture` 与 ShadowResolve 原 compute→fragment barrier 全部保留。
- 6 个新测试通过；RenderGraph 相关全部 unit/integration 通过；`git diff --check` 无空白错误。

**剩余 blocker（删除手工绑定前置）**：B12 已实现 graph-driven binding 的显式准入与执行映射（`ResolvePassBindings`/`ApplyActivePassBindings` 在 `RenderGraph::Execute` 中 pass Execute 前挂钩，按 snapshot 经 GPUUtils 发出 BufferBase/ImageUnit 真实 bind）。`FramebufferManager`/`ComputeBuffer` 注册 registry 的 owner 硬编码/Unknown 与 graph owner 不一致的债务已由 B11 补齐（见下），故删除手工绑定唯一前置剩余为真实 GL 集成测试（验证 graph-driven bind 与手工 bind 行为等价、snapshot 覆盖全部运行路径）。故仍不得删除任何手工绑定。
**B10 当前结果**：RenderContext 现在接收 Shadow/cluster/LightBuffer 的每帧非拥有型 handle snapshot，并提供按 tag 查询；零句柄仍保持无效。该合同不发 GL 调用、不改变 ownership；B12 已在其上增加显式准入，手工绑定仍待生产 pass gate 验证后才能删除。
**B12 当前结果**：`ResolvePassBindings`/`ApplyActivePassBindings` 已实现显式准入+执行（BufferBase/ImageUnit），在 `RenderGraph::Execute` 内 pass Execute 前发出真实 GL bind（仅调用既有 GPUUtils API；graph 不持有 handle）。缺失 import/kind 不一致/零句柄/unsupported kind 全部 fail-closed 并记录 diagnostic。B8 专用真实 GL fixture 已验证最小 graph-driven/manual hash 等价及 fail-closed；生产 pass gate 尚未完成，故手工绑定仍保留。

### B11 明细：registry owner metadata 对齐（关闭 B10/B9 的 registry owner 债务）

**目标**：修复 B9/B10 遗留的 registry owner 债务——`FramebufferManager::Create` 将所有 FBO 注册为 `Scene`、`ComputeBuffer::Create` 将所有 SSBO 注册为 `Unknown`，与 graph import 合同的 `backingOwner`（Shadow/LightCulling/Lighting）不一致。B11 提供最小安全 API `GPUResourceRegistry::ReclassifyResourceOwner`，由真实 owner 在创建/resize 后把 observer 记录 owner 改为 graph 合同值，使 registry owner 与 RenderGraph owner contract 可验证一致。observer-only：不发 GL 调用、不改 allocation/resize/release/bind ownership、不动任何手工 `BindBufferBase`/`BindImageTexture`/ShadowResolve barrier。

**API 合同**（`GPUResourceRegistry`）：
- `ReclassifyResourceOwner(handle, kind, newOwnerTag) -> bool`：
  - 只更新**已存在**记录；missing handle / 零 handle fail-closed（LOG_WARN + 返回 false，零 handle 静默），不产生任何计数变更；
  - 按新旧 owner 重平衡 `bytesByOwner`（饱和减法，杜绝下溢）；`currentTotalBytes/peak/activeCount/created/destroyed` 不动；
  - 同 owner 幂等 no-op 返回 true（每帧/每次 capacity pass 重复调用安全）。

**接线点**（真实 owner 创建/resize 后立即调用；`FramebufferManager::Resize` 与 `ComputeBuffer::Create` 内部均 Destroy+Create，故每次 recreate 后必须重新 reclassify）：

| 资源 | 真实 GL backing 创建/释放 owner | registry 原 owner | 新 owner | 接线点 |
| --- | --- | --- | --- | --- |
| `ShadowAtlas` | ShadowBuildPass `m_shadowAtlas`（FramebufferManager） | Scene | Shadow | `EnsureAtlasSize` Create/Resize 后 |
| `ShadowDistanceField` | ShadowBuildPass `m_sdfField`（FramebufferManager） | Scene | Shadow | `OnResize` Create/Resize 后 |
| `ShadowOccluderSSBO` | ShadowBuildPass `m_occluderBuffer`（ComputeBuffer） | Unknown | Shadow | `UploadOccluders` Create 后 |
| `ShadowMask` | ShadowResolvePass `m_shadowMask`（FramebufferManager） | Scene | Shadow | `OnResize` Create/Resize 后 |
| `Cluster*` 5 个 SSBO | ClusteredLightingState（ComputeBuffer） | Unknown | LightCulling | `EnsureBufferCapacity` 每次 capacity pass 后（幂等） |
| `LightBufferSSBO` | LightManager `m_lightBuffer`（ComputeBuffer） | Unknown | Lighting | `Initialize` Create 后（OrphanAndUpload 不换 id） |

> **open decision（用户指示与代码合同分歧）**：任务指示 LightManager buffer"归 LightCulling"，但 LightCullingPass Setup 的 `ImportResource(LightBufferSSBO)` 声明 `backingOwner = RenderOwnerTag::Lighting`（`// LightManager`，LightCullingPass.cpp:130），design §5.1 资源表同样标注 `Lighting（LightManager）`。为满足首要验收"registry owner 与 RenderGraph owner contract 可验证一致"，按 **Lighting** 分类；如需改 LightCulling 须同步改 graph import 声明，未实施，列为待用户确认项。

**文件 reservation**：`src/engine/render/resources/GPUResourceRegistry.{hpp,cpp}`（新增 API）；`ShadowBuildPass.cpp`/`ShadowResolvePass.{hpp,cpp}`/`ClusteredLightingState.cpp`/`LightManager.cpp`（接线）；`tests/unit/GPUResourceRegistrySnapshotTest.cpp`（3 个新 TEST_CASE）。

**验收**：
- `ReclassifyResourceOwner` 只更新已存在记录；missing/零 handle fail-closed 无计数变更；`bytesByOwner` 精确重平衡；同 owner 幂等。
- Shadow 资源 registry owner = `Shadow`，cluster 5 buffer = `LightCulling`，LightBuffer = `Lighting`，均与 graph import 合同一致。
- 不改变任何 GL allocation/resize/release/bind；不删除任何手工 `BindBufferBase`/`BindImageTexture`/ShadowResolve barrier。
- 3 个新 unit test 通过（owner reclassification / unknown no-op / resize recreation pairing）；RenderGraph 相关全部 unit/integration 通过；`git diff --check` 无空白错误。

### B12 明细：graph-driven binding 准入/执行合同

**目标**：在 B9（import 合同）/B10（snapshot）/B11（registry owner）之上增加最小、显式、fail-closed 的 graph-driven binding 执行层：RenderGraph/RenderContext 依据当前 pass 的 compiled binding declarations（`BindBufferBase`/`BindImageUnit`）+ 匹配的 imported backing snapshot，在 pass Execute 前用既有 GPUUtils API 发出真实 GL bind。ColorAttachment/TextureUnit 明确 unsupported/diagnostic，不伪造。graph 不持有任何 GL handle，不 alloc/resize/release/改 owner。

**准入策略**（显式、fail-closed；任一不满足即拒绝该 binding 并记录 diagnostic，绝不 bind 零句柄）：
1. binding kind 仅支持 `BufferBase`/`ImageUnit`；`TextureUnit`/`ColorAttachment` → Warning diagnostic（unsupported），不产生 GL bind。
2. 同 tag 必须有匹配的 `ImportResource`，否则 Error denied（手工 bind 保持权威）。
3. import kind 与 binding kind 兼容（BufferBase↔StorageBuffer/UniformBuffer；ImageUnit↔Texture2D/Texture2DArray/Framebuffer），否则 Error denied。
4. snapshot 必须含该 tag，且 `ImportedBackingHandle::IsValidFor(import.kind)` 为真 + 实际绑定的 handle 非零，否则 Error denied（绝不 bind 零句柄）。
5. import 的 bindingPoint/imageUnit/imageAccess/imageFormat 与 binding 声明不一致 → Warning（非拒绝；手工 bind 保持权威）。
6. 通过准入的资源产出 `ResolvedBindingOperation`（BindBufferBase point/handle 或 BindImageTexture unit/handle/access/format），由 executor 用 `GPUUtils::BindBufferBase`/`BindImageTexture` 发出。

**API**：
- `RenderGraph::ResolvePassBindings(passIndex, context)` / `ResolveActivePassBindings(context)`：纯准入/解析，无 GL，返回 `BindingResolutionResult{operations, diagnostics, allAdmitted}`（可单测）。
- `RenderGraph::ApplyActivePassBindings(context)`：执行 admitted operations；在 `Execute` 中 `m_activeNodeIndex = node.passIndex;` 之后、`node.pass->Execute(context)` 之前调用；返回 allAdmitted；runtime diagnostics 记入 `m_runtimeBindingDiagnostics`（每帧开头清空）。
- `RenderContext::ApplyActivePassBindings()`：委托 activeGraph。
- `RenderGraphBuilder::BindTextureUnit`/`BindColorAttachment`：observer-only 声明（用于 unsupported 路径测试）。

**文件 reservation**：
- `src/engine/render/graph/RenderGraph.hpp`：`ResolvedBindingOperation`/`BindingResolutionResult`（nested）+ 三个 B12 方法 + `m_runtimeBindingDiagnostics` + `BindTextureUnit`/`BindColorAttachment`。
- `src/engine/render/graph/RenderGraph.cpp`：`ResolvePassBindings`/`ResolveActivePassBindings`/`ApplyActivePassBindings` 实现 + `Execute` 挂钩 + kind 兼容 helper。
- `src/engine/render/graph/RenderContext.hpp`：`ApplyActivePassBindings()` 委托方法。
- `tests/unit/RenderGraphValidationTest.cpp`：6 个新 TEST_CASE（resolver/diagnostic 层，无 GL）。
- `docs/designs/2026-08-03-render-engine-interface-migration-design.md`：§5.1 增补 B12 合同段。

**验收**：
- 只对「有对应 import、tag/kind 一致、snapshot 非零且 `IsValidFor` 为真」的资源执行；缺失/不一致/零句柄返回 false 并记录 diagnostic，不 bind 零句柄。
- 只调用既有 GPUUtils binding API；不 alloc/resize/release/改 owner；graph 不持有 GL handle（handle 全部来自 RenderContext 每帧 snapshot）。
- 现有手工 `BindBufferBase`/`BindImageTexture` 与 ShadowResolve 原 compute→fragment barrier 全部保留；重复绑定行为等价（同点同 handle 再绑定）。
- 6 个新 unit test 通过；RenderGraph 相关全部 unit/integration 通过；`git diff --check` 无空白错误。

**剩余 gate（删除手工绑定前置）**：B12 专用真实 GL fixture 已验证最小 SSBO/ImageUnit graph-driven-only 与 manual-only 输出 hash 等价，并验证 zero/missing/duplicate snapshot fail-closed；但该 fixture 不是生产 Shadow/LightCulling 路径。仍需逐 pass 证明生产 snapshot 覆盖和视觉/资源快照行为后，B2-B4 才能标完成，所有手工绑定与 ShadowResolve 原 barrier 继续保留。

### B2/B3 gate 明细：真实 ShadowBuildPass 生产路径逐 pass GL 等价性 gate（2026-08-04）

**目标**：把 B12 的最小 synthetic mirror fixture 推进为**真实生产路径**的逐 pass gate：用真实 `ShadowPreparePass -> ShadowBuildPass -> ShadowResolvePass` 链 + 真实 `QualityTierManager`（High→SDF）+ 真实 `RenderGraph::Execute`，在隐藏 1x1 GL context 上验证 B2/B3 的 binding 与 same-pass phase barrier 表面，明确 artifact/gate 分类。这是删除手工绑定的**第一个生产路径准入证据**，但不是生产视觉/资源快照的最终证据。

**真实路径覆盖**（全部真实代码，无 synthetic 镜像）：
- 3 个真实 pass 的 Setup/Execute：`graph.Build()` 零 validation error；`plan.phaseBarriers` 含 ShadowBuildPass Compute→Fragment 且 bits == `Barrier::Image|Barrier::Buffer`（0x220）；`plan.transitions` 含 ShadowOccluderSSBO Host→Compute（bits≠0）与 ShadowDistanceField Compute→Fragment（bits≠0）；`plan.bindings` 2 条（BufferBase@15 / ImageUnit@0）、`plan.imports` 3 条（ShadowBuildPass）。
- 等价性：帧 A（空 snapshot，graph-driven 全部 deny、真实手工 BindBufferBase/BindImageTexture 产出 SDF+mask）与帧 B（有效 snapshot，resolver 恰好准入 2 个生产操作、Execute 零 runtime diagnostic）的 SDF/mask 读回 hash **逐位相等**且均非 sentinel。
- fail-closed：零句柄 snapshot → resolver `allAdmitted=false`、operations 为空、runtime 记录 2 条 "zero/invalid handle"；帧 C 输出与帧 A 逐位相等（手工绑定安全网仍权威）。
- GL errors：三帧后 `glGetError` 清零（无新增）；registry：teardown 后无本测试可归因的 active-record 增长。
- snapshot 时序镜像 RenderSystem：帧 N 快照读帧 N-1 的真实 backing handle（SDF FBO/occluder SSBO 在 Execute 内创建）。

**文件 reservation**：
- `tests/integration/GraphBindingEquivalenceGLTest.cpp`：新增 1 个 `[GPU-Diagnostic]` TEST_CASE（`RenderGraph - real ShadowBuildPass graph-driven bind + phase barrier gate on real GL`，50 assertion）+ 辅助（RGBA 读回、glGetError、临时 settings、registry leak-check helper）。文件已在 CMake `SKIP_UNITY_BUILD` 列表，无需 CMake 改动。
- `docs/plans/2026-08-03-migration-phase-b-typed-passes-plan.md`：本明细 + §7 证据。

**阻塞原因（为什么 B2/B3 仍不标完成、手工绑定仍保留）**：本 gate 是**合同级**逐 pass 证据，不是生产视觉证据——无 screenshot/哈希基线，生产 gate artifact 仍 `NO_GO`（B8）。删除手工 `BindBufferBase`/`BindImageTexture` 仍需：(1) 生产场景（1280x720 3 fixture）下 graph-driven-only 与 manual-only 的渲染输出等价；(2) 生产资源快照无增长；(3) B4/LightCulling 同款逐 pass gate。

**验收**：
- 上述真实路径断言全部通过；`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 通过；`git diff --check` 无空白错误。
- 不改变生产代码、不删除任何手工绑定/ShadowResolve 原 barrier、不改 timing budget/SDF 精度/分辨率/gate verdict/B13 诊断。
- 测试 teardown 用 `ResourceManager::SetHeadless(true)` 规避 `ShadowBuildPass::Shutdown` 与 `unloadAll` 对同一 resources-owned shader 的二次 `UnloadShader`（raylib `RL_FREE(shader.locs)` 双重 free → 堆损坏；此为既有生产 teardown 隐患，不在本任务范围修复，已在测试注释记录）。**已修复（2026-08-04，见下 Owner blocker 明细）**：该 teardown 工作区已被真实 owner teardown 取代，生产路径双重释放根因已消除。

### B4 gate 明细：真实 LightCulling 生产路径逐 pass GL 等价性 gate（2026-08-05）

**目标**：把 B12 的最小 synthetic mirror fixture 推进为**真实 LightCulling 生产路径**的逐 pass gate：用真实 `LightCullingPass` + 真实 `ClusteredLightingState` + 真实 `LightManager` + 真实 `RenderGraph::Execute`，在隐藏 1x1 GL context 上验证 B4 的 compiled imports/access/transitions 表面与 graph-driven BufferBase 6 点准入面，明确 artifact/gate 分类。这是删除 LightCulling 手工 `BindBufferBase` 的**第一个生产路径准入证据**，但不是生产视觉/资源快照的最终证据。

**真实路径覆盖（TEST_CASE A，全部真实代码）**：
- 真实 `LightCullingPass`（`NoMoreDay::render::passes`）Setup/Execute 经真实 `RenderGraph::Build`/`Execute`；配置经真实 `QualityTierManager`（ForceTier High + 测试注入 RenderConfig 启用 v3/dynamic/clustered lighting，clusterTileSize=32、clusterZSliceCount=8、maxLights=4096）。
- 真实 backing：`LightManager::Initialize()` + `SetDisableViewCullingForTesting(true)` + `UpdateCandidates`（1 个 point light @(16,16) radius 40，view-culling 关闭后入选）；`ClusteredLightingState::BeginFrame/UploadLightBounds/ReadBackClusterHeaders` 全部由真实 pass 驱动。
- **compiled 表面断言**：`plan.imports` for LightCullingPass == 6（LightBufferSSBO bindingPoint 0 owner Lighting；ClusterHeaderSSBO=1、ClusterLightIndexSSBO=2、LightBoundsSSBO=3、ClusterCounterSSBO=4、ClusterPackedLightSSBO=5，owner LightCulling；全部 `resizeFollowsCapacity`）；`plan.bindings` for LightCullingPass == 6（生产 Setup 声明 6 条 observer-only `BindBufferBase`：LightBufferSSBO=0、ClusterHeaderSSBO=1、ClusterLightIndexSSBO=2、LightBoundsSSBO=3、ClusterCounterSSBO=4、ClusterPackedLightSSBO=5，与 import bindingPoint 及 BindingRegistry LightCulling domain 一致，resolver 可直接准入真实 6 点，见接线明细）；`plan.transitions` 含 LightBoundsSSBO Host→Compute（bits≠0）+ 4 条 cluster SSBO Compute→Fragment（bits==0x00002000）；无 Error 级 validation diagnostic。
- 等价性：帧 A（空 snapshot，6 条 observer 无 handle 可准入 → resolver fail-closed：ops 空、allAdmitted=false、6 条 "no imported backing snapshot" 诊断；真实手工 BindBufferBase 产出 cluster 数据）与帧 B（有效 snapshot，resolver **恰好准入 6 个 BindBufferBase operations**（points 0-5、handles 逐项==真实 id）、Execute 零 runtime diagnostic）的 cluster counts 读回 hash **逐位相等**且非平凡（8 个 cluster 各 pointCount=1、writeCursor=8、overflowSum=0；hash 只含确定性字段 pointCount/spotCount/areaCount，**排除 atomicAdd 顺序相关的 header.offset**）。
- fail-closed：帧 C 零句柄 snapshot → resolver allAdmitted=false、ops 空、runtime 记录 6 条 "zero/invalid handle" 诊断、输出与帧 A 逐位相等（手工绑定安全网权威）。
- GL errors：三帧后 `glGetError` 清零；registry：teardown 后无本测试可归因的 active-record 增长；真实 owner teardown：`resources.unloadAll()` 后 release ledger==1（恰好一个 `light_culling_compute` shader，exactly-once）。

**真实 6 点 BufferBase surface executor gate（TEST_CASE B，镜像 pass 但真实 GL surface）**：
- 测试本地 `LightCullingSurfacePass` 镜像生产 Execute（同一 `light_culling_compute` shader、同一 BeginFrame/bounds/uniform/dispatch/barrier/readback 序列），但 binding authority 可切换：Setup 声明 6 个 BindBufferBase observer（点 0-5，与 BindingRegistry LightCulling domain 一致）+ 6 个 ImportResource，Execute 在 ManualOnly 手 bind、GraphDrivenOnly 依赖 graph。
- 断言：有效 snapshot（6 个真实 handles）下 resolver **恰好准入 6 个 BindBufferBase operations**（points 0-5、handles 逐项==真实 id）；graph-driven-only 与 manual-only 帧的确定性 readback hash（counts+counter 32B+index[0..writeCursor)+packed[0..writeCursor)）**逐位相等**且非平凡；零句柄 snapshot → resolver `allAdmitted=false`、operations 空、runtime 记录 6 条 "zero/invalid handle"（fail-closed，帧不 dispatch）；ManualOnly 安全网仍产出逐位相等基线。
- 该 gate 用镜像 pass 证明 6 点 surface 在真实 GL 上 graph-driven-only 与 manual-only 输出**逐位相等**（删除手工绑定前的执行级等价证据）；真实 `LightCullingPass` 的 graph-driven 准入现由 TEST_CASE A 直接证明（生产 Setup 已接线 6 条 observer，见接线明细）。

**Blocker（B4 不标完成、真实手工绑定保留的原因）**：生产 Setup observer 接线已完成（2026-08-05）：真实 `LightCullingPass::Setup` 声明 6 条 observer-only `builder.BindBufferBase(tag, point)`（LightBufferSSBO=0、ClusterHeaderSSBO=1、ClusterLightIndexSSBO=2、LightBoundsSSBO=3、ClusterCounterSSBO=4、ClusterPackedLightSSBO=5，与 BindingRegistry LightCulling domain 符号及 import bindingPoint 一致），`plan.bindings`==6，gate 现在能**直接解析真实 pass**（帧 B 恰好准入 6 个 BindBufferBase operations，points 0-5、handles==真实 id）。删除生产 6 个手工 `BindBufferBase`（LightCullingPass.cpp L352-362）与 readback MemoryBarrier **仍阻塞于**：(1) 真实生产场景（1280x720，3 fixture）下 graph-driven-only 与 manual-only 渲染输出等价证据；(2) 生产资源快照无增长证据；(3) 生产 gate GO artifact。本 gate 与接线仍是合同级逐 pass 证据，非生产视觉证据。

**文件 reservation**：
- `tests/integration/GraphBindingEquivalenceGLTest.cpp`：2 个 `[GPU-Diagnostic]` TEST_CASE（`RenderGraph - B4 real LightCullingPass contract gate on real GL` 与 `RenderGraph - B4 real 6-point LightCulling BufferBase surface executor gate on real GL`）+ 辅助（字节 readback、cluster counts/counter/index/packed hash、ClusterConsumerPass、LightCullingSurfacePass）。文件已在 CMake `SKIP_UNITY_BUILD` 列表，无需 CMake 改动。
- `src/engine/render/passes/LightCullingPass.cpp`：`Setup` 新增 6 条 observer-only `builder.BindBufferBase(tag, point)` 声明（2026-08-05 接线，见接线明细）；Execute 内手工 `BindBufferBase`/`MemoryBarrier`/flush/dispatch 全部保留。
- `docs/plans/2026-08-03-migration-phase-b-typed-passes-plan.md`：本明细 + §7 证据。

**验收**：
- 上述真实路径断言全部通过（2 case，assertions 按实际执行记录）；`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 通过；`git diff --check` 无空白错误。
- 生产代码改动严格限于 `LightCullingPass::Setup` 新增 6 条 observer-only `BindBufferBase` 声明；不删除/移动任何手工 `BindBufferBase`/`MemoryBarrier`/flush/dispatch、不改 owner/import/access/算法、不改 timing budget/SDF/cluster 算法、不改 gate verdict、不触碰 B6/E2/F3/C/D/E。
- 测试 teardown 用真实 owner teardown（`clusterState.Shutdown()`/`LightManager::Shutdown()`/`resources.unloadAll()` + release ledger==1），不伪造 PASS。

### Owner blocker 明细：ResourceManager shader 唯一 owner 合同（双重释放修复，2026-08-04）

**问题（在 B2/B3 真实 ShadowBuild GL gate 中暴露）**：`ResourceManager::loadShader`/`loadComputeShader` 返回的 Shader 由 ResourceManager 缓存并登记 `GPUResourceRegistry`，其注释（RG-3）声明 `unloadAll()`/析构为唯一 GL 释放方；但 `ShadowBuildPass::Shutdown()` 对同一 shader 直接调用 raylib `UnloadShader`（`RL_FREE(shader.locs)`）。真实 teardown 路径 `Game::cleanup` → `RenderSystem::Shutdown`（pass Shutdown 先 UnloadShader）→ `m_resourceManager.unloadAll()`（m_shaders 仍持有同 id → 再次 UnloadShader 同一 program + 已释放 locs）→ `shader.locs` double-free → 堆损坏 `0xC0000374`。次要隐患：pass Shutdown 后 m_shaders 残留已卸载的悬空 Shader，重新 Initialize 会从缓存拿到 use-after-free。

**Owner 规则（本任务实施的最小合同）**：ResourceManager 是 `loadShader`/`loadComputeShader` 返回 shader 的**唯一 owner 与唯一 GL 释放方**；所有消费者（pass/系统）只借不还，**永不**对 manager 加载的 shader 调用 raylib `UnloadShader`；需要提前释放时只能调用新增的 `ResourceManager::ReleaseShader(id)`。`ShadowBuildPass::Shutdown` 只丢弃本地引用（与全部兄弟 pass 一致），ResourceManager 缓存保持有效，重新 Initialize 可复用同一存活 shader。

**API 合同（新增，fail-safe + 幂等）**：
- `bool ReleaseShader(entt::id_type id)`：未知/未加载 id → `false` 且无副作用；已加载 → 从 `m_shaders` 删除（先删后放，二次调用必然 no-op）→ 非 headless 时先 `UnregisterResource`（RG-3 observer 配对）再 `UnloadShader`，全程 `m_mutex` 同步 → 每次实际释放记入 release ledger（`m_shaderReleaseCount++`、`m_shaderReleaseIds.push_back(id)`，headless dummy 计入 ledger 但跳过 GL）。
- `size_t GetShaderReleaseCount() const` / `std::vector<entt::id_type> GetShaderReleaseIds() const`：只读 ledger 访问器，测试用其证明 exactly-once teardown（不伪造"无崩溃"证据）。
- `unloadAll()` shader 循环重构为统一走 `ReleaseShaderLocked`（唯一释放 choke point）。

**改动文件**（严格限于 owner 相关）：
- `src/engine/resource/ResourceManager.{hpp,cpp}`：新增 ReleaseShader/ReleaseShaderLocked + ledger + 访问器；unloadAll shader 路径委托。
- `src/engine/render/passes/ShadowBuildPass.cpp`：`Shutdown()` 删除两处 `UnloadShader`（保留置零与 owner 注释）；`.hpp` 未改。
- `tests/unit/ResourceManagerShaderOwnershipTest.cpp`（新增，GLOB_RECURSE 自动纳入）：Test 1 真实 GL + 真实 ShadowBuildPass，断言 pass Shutdown 不释放（ledger==0、缓存存活）且 `unloadAll` 恰好释放 2 个（ledger==2、ids 精确匹配、GL errors 清零）；Test 2 headless 隔离验证 ReleaseShader fail-safe/幂等。
- `tests/integration/GraphBindingEquivalenceGLTest.cpp`：移除 `SetHeadless(true)` 规避，改为真实 `unloadAll()` + `CHECK_EQ(GetShaderReleaseCount(), 2u)`；更新 LeakCheck 注释。
- 本计划文档（状态行 + 本明细 + §7 证据）。

**剩余范围（本任务边界内不扩大）**：grep 确认 ShadowBuildPass 是唯一对 manager 加载 shader 调用 UnloadShader 的消费者；其余 RM load 消费者（JFAPass/RadianceCascadesPass/OccluderExtractPass/LightCullingPass/GICompositePass/FluidSimulationPass/GPUFlowFieldSystem/GPUTextSystem layout/FogOfWarSystem/MDIRenderer/AssetLoadingSystem/GPUEntitySystem 5 个 compute）全部为 borrow-only、自身 UnloadShader 者均为自加载 shader（合法），已文档化核实，无需改动。headless 模式仅保留用于与本任务无关的既有隔离路径。

**验收**：
- 不改变 shader 行为、编译/热重载策略、GPU timing budget、gate verdict、B13 JFA 诊断；不删除任何 BindBufferBase/BindImageTexture、phase barrier 或 ShadowResolve barrier；无新增依赖。
- 新 unit test 2 用例通过；`GraphBindingEquivalenceGLTest` 真实 GL gate 在真实 owner teardown 下通过（ledger==2）；registry 无残留；`git diff --check`、`check_module_boundaries.py`、`check_legacy_reintroduction.py` 全部通过。

### B13 明细：生产 GPU timing blocker 诊断与低风险优化（2026-08-04）

**目标**：在不放宽 GPU gate budget、不改变 SDF 精度/分辨率、不删除手工绑定或 barrier 的前提下，定位并修复真实生产路径的 timing 超预算。

**基线**：最新 `nmd.tests.gpu.hardware` 使用重编 `NoMoreDay.exe` 运行 120 samples/100 toggles/60s stress；stderr 已为空，资源 snapshot 无净增长，GL severe error 为 0，但部分矩阵 cell 的 `JFAPass`、`ScenePass`、`LightingPass` P95 超过既定预算，artifact 仍为 `NO_GO`。

**已审计范围**：JFAPass 的 seed init、jump flood steps、distance resolve、upsample、fallback plus-2、incremental verification/readback，以及 `GPUHardwareValidationGate` 的 timing 判定。当前没有足够证据证明存在可安全删除的冗余 dispatch；减少 JFA step、改变 half-resolution 或禁用验证会改变质量或 gate 语义，因此本原子任务暂不改生产算法。

**诊断原子任务结果**：已从真实 `JFAPass::JFAFrameReport` 和 recovery 状态接线 `RenderSystem::JfaDiagnostics`；硬件 gate 每个 matrix cell 输出 `jfa` 对象，包含 mode、dispatch texel count、dirty/expanded area、recovery、verification 状态。最新 artifact 的 9 个真实 matrix cell 均包含字段，首 cell 为 `skip`、dispatch=0、面积=0、recovery=false。该接线 observer-only，不改变算法、预算或 gate verdict。

**下一原子任务**：用 artifact 按 JFA mode/dispatch count/rect area/recovery 与 timing 做固定硬件重复采样；只有确认冗余工作或非必要同步后，才实施局部优化。预算调整必须作为独立决策，不得作为优化替代。

**验收**：优化前后 SDF sign probe、GI paired delta、GL diagnostics、资源 snapshot 不回归；对应 pass P95 有可重复改善；未改变 gate budget；若无法满足则保留 `[~]` 和性能 blocker。

**当前结论**：诊断接线已验证（JFA 10 cases/344 assertions、Phase B 4 cases/112 assertions、legacy/module gates PASS）；最新硬件 gate 仍 `NO_GO`，但 stderr 为空、资源无净增长、GL severe error=0，失败集中在部分 `JFAPass`/`ScenePass`/`LightingPass` P95 超预算。尚未发现可安全删除的 dispatch、同步或 verification 步骤，因此不实施猜测性优化，B13 保持 `[~]`。

### B6 明细：VFXEmissionSnapshotPass 声明粒子 emissive backing（2026-08-04）

**目标**：按用户决策「声明资源、保留独立 VFXEmissionSnapshotPass、不并入 RadianceCascades、不改变 pass 顺序/视觉行为」，用最小 typed 声明覆盖该 pass 的真实资源访问，Setup 不再为空。声明为 observer-only：graph 不 alloc/resize/release/改 owner/GL-bind，Execute、shader、pass 顺序与视觉策略不变。

**真实资源盘点**（源码确认，替代旧计划中错误的 EmissiveBuffer 假设）：
- 本 pass 真正写入：RadianceCascadesPass `m_particleEmissive`（`FramebufferManager` FBO，全屏 RGBA16F，`EnsureResources` 创建/resize、`Shutdown` 释放，resizeFollowsScreen）。
- Execute 回调链：`RenderSystem.cpp:1523-1528` 注册，回调 `RadianceCascadesPass::PrepareVfxEmissionSnapshot`（:228-259，giEnabled 门控）→ `EnsureResources` → `GPUParticleSystem::RenderEmissionSnapshot(camera, m_particleEmissive.fbo, hdrSceneBuffer.fbo, w, h)`（:943-984，读粒子 SSBO `PARTICLES_IN`、m_indirectBuffer、m_quadVAO，`emission_snapshot` shader BLEND_ADDITIVE 渲染到 m_particleEmissive，结束仅恢复 hdrSceneBuffer.fbo GL 状态）。
- shader 验证：`emission_snapshot.vert` 只读粒子 SSBO；`.frag` 无纹理采样 → **不读 EmissiveBuffer、不读场景纹理**。
- consumer：RadianceCascadesPass `RunEmissiveMerge`（:500-533，`BindImageTexture(kParticleBinding=1, READ_ONLY, GL_RGBA16F)` 读 m_particleEmissive）。

**改动**：
- `RenderGraph.hpp`：`RenderResourceTag` 新增 `ParticleEmissive`（EmissiveBuffer 后）+ `ToResourceName`/`ToResourceTag` case。
- `RenderGraph.cpp`：`IsWriterAllowedForResource`/`IsFirstWriterValid`/`ExpectedFirstWriter` 各补 `ParticleEmissive → RadianceCascades`；`IsAdditionalWriterValid` 走默认 false（单写者）。
- `VFXEmissionSnapshotPass.cpp` Setup：`DeclareResource(ParticleEmissive, Texture2D, RGBA16F, Persistent, owner=RadianceCascades)` + `Write(ParticleEmissive, RadianceCascades, Fragment, ColorAttachment)` + `ImportResource(kind=Texture2D, format=RGBA16F, backingOwner=RadianceCascades, resizeFollowsScreen=true, colorAttachmentIndex=0)`（对齐 ShadowResolvePass 模式）。Execute/GetName/构造函数未动，不加 include 于 Execute。
- `tests/unit/RenderGraphValidationTest.cpp`：新增 TEST_CASE 用真实 VFXEmissionSnapshotPass 单 pass Build，断言 compiled plan 的 passOrder/imports/resources 元数据与无 validation error（零真实 GPU）。

**文件 reservation**：`RenderGraph.hpp`、`RenderGraph.cpp`、`VFXEmissionSnapshotPass.cpp`、`tests/unit/RenderGraphValidationTest.cpp`、design §5.1（tag 表 + 伪代码 + import 表 + Open decision #1 已解决）。

**验收**：
- Setup 非空，declares/imports 与真实 backing（m_particleEmissive）一致；不新增 alloc/resize/release/GL bind；Execute、shader、pass 顺序、视觉行为不变。
- 新 unit test 通过（passOrder/imports/resources 元数据 + !HasValidationErrors）；RenderGraph 相关 unit/integration 通过；`git diff --check` 无空白错误。
- 不删除任何手工绑定（B2-B4 范围外）。

**剩余风险（B6 边界内暂不处理）**：
- 本 pass 读的粒子 SSBO（GPUParticleSystem 内部 `m_particleBuffer`/`m_compactBuffer`）与 indirect buffer 不声明：graph 中无写者（避免 read-before-write），且 backing 属粒子系统内部管理。留待后续 phase。
- consumer 侧 RadianceCascadesPass 对 `ParticleEmissive` 的 `Read` 声明未加入：现有集成测试 `RenderGraphV5ContractsIntegrationTest` 的 RadianceCascades 链不含 VFXEmissionSnapshotPass，加入读声明会触发 read-before-write。推迟至 B7/后续（消费者边声明需同时把 VFXEmissionSnapshotPass 纳入该集成链）。

### B8 明细：视觉回归证据边界判定与最小 harness 合同（2026-08-04）

**结论（如实，不伪造）**：B8 的**边界脚本、`legacy` 扫描和 B12 最小真实 GL binding fixture**已验证；但 **Shadow/cluster/VFX 的生产视觉回归一致**仍无法在本地产生真实基线——没有 screenshot/哈希基线，也没有覆盖生产 pass 的 graph-driven vs 手工重绑输出。因此 B8 不得标记完成，本明细给出最小 harness 合同与明确 blocker。

**能力矩阵（本环境实测）**：

| 能力 | 现状 | 证据 |
| --- | --- | --- |
| 真实 GPU（RTX 4070S, GL 4.3, NVIDIA 591.86） | 可用（真实 GL 上下文创建成功） | `ctest -R "nmd.tests.gpu.contract\|nmd.tests.gpu.diagnostic"` 2/2 PASS（0.60s+22.36s）；`bin/NoMoreDayTests.exe` 22.5MB 已构建 |
| GPU 硬件 gate 生产 fixture（3 场景 1280x720 ROI/亮度/GPU timer/GL 错误/SDF/资源 snapshot） | 存在且为真实 fixture | `GPUHardwareValidationGate::GetStandardFixtures()`（cave/combat/outdoor）；M0-C W6.9 本机 ROI 非黑 0.62-0.89、SDF 通过、GL errors 256→0 |
| 生产 gate GO artifact | **未通过**（当前 artifact 为 NO_GO；主要阻塞为 timing budget） | `artifacts/gpu-gate/local-gpu-hardware/gpu_hardware_validation_artifact.json`：硬件/GL diagnostics/60s stress 通过，`leak_candidate_count=0`，stderr 为空，但部分 JFAPass/ScenePass/LightingPass P95 超预算，`gate_succeeded=false` |
| graph-driven bind vs 手工重绑等价性测试 | **最小合同 fixture 已存在并通过**；生产 pass 等价性仍未覆盖 | `GraphBindingEquivalenceGLTest` 两个 GPU-Diagnostic case：graph/manual SSBO+image hash 等价，zero/missing/duplicate snapshot fail-closed；`ctest -R nmd.tests.gpu.diagnostic` PASS |
| ShadowBuild phase barrier / ShadowResolve barrier 视觉/GL 错误等价 | **无直接 fixture**（graph transition barrierBits 已由 B7 unit 断言 0x00002000/非零，但那是 plan 级、非渲染输出） | B7 用例 5/5、115 assertion PASS |
| screenshot/哈希视觉回归基线 | **不存在**（`conductor/validation/screenshots/` 目录不存在；manifest 6 场景全 missing） | `python scripts/v3_screenshot_diff.py --allow-missing` → total=6 pass=0 warning=6 fail=0 |
| 边界脚本 | 通过 | `check_module_boundaries.py` PASS（ledger 0/0，schema 2.0，历史 71/71 为 S5 口径）；`check_legacy_reintroduction.py` PASS（222/71→220/70） |

**最小 B8 harness 合同**（专用 binding fixture 已实现；生产视觉部分仍不得执行为"通过"）：
- **输入场景**：复用 `GPUHardwareValidationGate::GetStandardFixtures()` 3 场景（cave_color_bleed/dynamic_combat_emissive/outdoor_light_pressure，seed 固定，1280x720），warmup=10、sampleFrames=120、NMD_GATE_SAMPLES/TOGGLE_LOOPS/STRESS 由 runner 参数驱动。
- **截图/图像阈值**：ROI 非黑 mean luma ≥ 0.02（非黑阈值）；SSIM ≥ 0.95、pixelDiff ≤ 2.0%（沿用 `v3_screenshot_diff.py` 阈值口径，需先有 baseline PNG 才有意义）；SDF 5 点符号探针通过。
- **GPU timer/GL error**：每场景 ≥ 120 有效 GPU timer 采样（P95 记录）；GL debug callback 高严重级错误计数为 0（现有 gate 已采 `gl_diagnostics.debug_message_count/severe_error_count`）。
- **资源 snapshot**：5 秒窗口 `GPUResourceRegistry` snapshot 无单调增长（现有 gate 已采 `stress_test.resource_snapshots`）。
- **等价性专门项**：`tests/integration/GraphBindingEquivalenceGLTest.cpp` 已用真实隐藏 GL context 驱动 `RenderGraph::Execute`，同一 compute shader 的 graph-driven-only/manual-only SSBO+RG16F image 输出 hash 相等，并覆盖 zero/missing/duplicate snapshot fail-closed。该 fixture 是合同级而非生产视觉证据；生产 Shadow/LightCulling/VFX 路径仍需单独 gate。
- **失败语义**：任一场景 ROI 黑/GL error>0/GPU timer 不可用/资源 snapshot 增长 → 该 gate 失败；生产 gate `NoMoreDay.exe --gpu-gate` 只有 rc==0 + schema valid + status==`GO` 才算通过（runner 合同）。headless/contract-only fixture（1x1 hidden context + harness）**永远不**算视觉通过。

**Blocker（B8 不可完成的原因，全部为缺失项而非失败项）**：
1. 无 screenshot/哈希基线（`conductor/validation/screenshots/` 不存在），无法进行真实图像级回归对比。
2. 生产 Shadow/LightCulling/VFX 路径的 graph-driven BindBufferBase/BindImageTexture 与手工重绑等价性尚无真实 GL 输出记录；最小 B12 fixture 已通过，但不能替代生产路径证据。
3. ShadowBuild phase barrier、ShadowResolve barrier 的视觉/GL 等价证据只在 plan 级（B7 unit）有断言，未在任何离屏渲染输出上验证。
4. 生产 GPU gate 的 GO artifact（`gate_succeeded==true`）尚未产生；当前运行的 120 帧/100 toggle/60s stress 仍为 NO_GO。修复 `src/app/Game.cpp` 的 `chcp 65001 >2&1` 重定向并重编 `NoMoreDay.exe` 后，artifact 的 `stderr_summary` 已为空；NO_GO 现在由真实 GPU timing 超预算触发，仍属 M0-C/B8 blocker。
5. 边界脚本与 `legacy` 扫描已通过（非 blocker，是 B8 中已真实完成的部分）。

**已真实验证（本会话）**：`*Phase B*` 4 用例/112 assertion PASS；B12 专用真实 GL fixture 2 用例/45 assertions PASS；`check_module_boundaries.py` PASS；`check_legacy_reintroduction.py` PASS；`ctest -R "nmd.tests.gpu.diagnostic"` 1/1 PASS；`nmd.tests.gpu.hardware` 已用最新 `NoMoreDay.exe` 重跑，stderr 噪音修复生效但 gate 仍 NO_GO。生产视觉回归和生产 pass 等价性仍未完成。

## 6. Test Method

- **unit**: 扩展 `RenderGraphValidationTest` 或新增 `ShadowClusterTypedAccessTest`：构建含 5 pass 的 graph，断言无空 Setup、compiled plan 含预期 shadow/cluster 资源 edge、跨 pass transition（compute→fragment）barrierBits 正确。
- **integration**: 影子/cluster 路径 smoke（如存在 fixture）；无则手动断言渲染帧数稳定且无 GL 错误。
- **manual**: 运行 build + 实机目测 shadow mask/SDF/cluster 视觉一致；`graph.GetCompiledPlan()` 打印 passOrder 含 5 pass。
- **B8 视觉回归边界**（2026-08-04）：当前仓库仍无 screenshot/哈希基线和生产 pass graph-driven-vs-manual 输出证据；但已有最小真实 GL binding 合同 fixture（详见 B8 明细），`v3_screenshot_diff.py` 仍全部 missing；GPU gate 生产 fixture 存在但 GO artifact 缺失。故 B8 视觉部分仅能执行**合同级**验证，不得宣称视觉通过。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  # B8 已真实验证的窄命令（本会话执行，全部通过）：
  ./bin/NoMoreDayTests.exe --test-case="*Phase B*"
  ./bin/NoMoreDayTests.exe --test-case="*B12*"
  ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.contract|nmd.tests.gpu.diagnostic" --output-on-failure
  ```

## 7. Verification Of Task Completion

- B1: 新枚举经 switch 映射无 fail-closed 缺口；编译期三处 switch 全 case 覆盖。
- B2-B4: Setup、import、snapshot 和 graph-driven BufferBase/ImageUnit 准入均可验证；真实 GL gate 通过后，才允许逐项移除对应手工 `BindBufferBase`/`BindImageTexture`，并保留 ShadowBuild phase barrier 与 ShadowResolve 必需屏障直到等价性证据成立。
- B5: ShadowPrepare Setup 声明 Host 写入，Execute 不再依赖 `ApplyRlglFlushTemplate`。
- B6: VFXEmissionSnapshot 有明确结论（声明或删除），无遗留回调旁路。**已完成（2026-08-04）**：Setup 声明 `ParticleEmissive`（写 + import，owner=RadianceCascades），用户决策保留独立 pass；验证证据见下。
- B7: `GetCompiledPlan().transitions` 含 SDF/cluster/shadow transition；无"missing producer"/"multiple writers"/cycle 诊断。**已完成（2026-08-04）**：见下方 B7 验证证据。
- B8: **不能标记完成**。已真实验证（2026-08-04）：`*Phase B*` 4 用例/112 assertion PASS、B12 专用真实 GL fixture 2 用例/45 assertions PASS、`check_module_boundaries.py` PASS（ledger 0/0）、`check_legacy_reintroduction.py` PASS（220/70）、`ctest -R "nmd.tests.gpu.diagnostic"` 1/1 PASS（真实 GPU）。最新 `nmd.tests.gpu.hardware` 使用重编 `NoMoreDay.exe` 后仍 NO_GO：stderr 已为空、资源无净增长、但部分 `JFAPass`/`ScenePass`/`LightingPass` p95 超预算。未验证 gate：无 screenshot 基线、无生产 Shadow/cluster/VFX 输出等价性证据、生产 gate GO artifact 缺失。详见 B8 明细。
- **当前 blocker:** B12 已实现 binding-point→真实 GL handle 的执行映射与 graph-driven binding 的显式准入（`ResolvePassBindings`/`ApplyActivePassBindings`），RenderGraph 在 pass Execute 前按每帧 snapshot 用 GPUUtils 发出 BufferBase/ImageUnit bind（TextureUnit/ColorAttachment 仅 diagnostic）。但真实 GL 集成测试未执行；该测试通过前仍不得删除 `BindBufferBase`/`BindImageTexture` 或 ShadowResolve 原执行点屏障。
- **当前验证证据:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；RenderGraph/registry 相关 26 个 unit test、215 个 assertion 通过。B2/B3 的正确性验证仅证明保留屏障后的基线，不证明迁移完成。
- **新增验证证据:** phase barrier/binding 合同测试 4 个、34 个 assertion 通过；RenderGraph/registry 合计 30 个 unit test、235 个 assertion 通过。
- **B9 验证证据:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；RenderGraph 相关 unit/integration 42+22 个测试全部通过（RenderGraph 42 个含 6 个新 import 合同测试、312 assertion；V5 合同/registry/ClusteredLighting 等 22 个、155 assertion）；完整套件 751 个中 747 通过，5 个失败均为既有环境性（GPU compute shader 加载 / 性能计时噪音，与本次改动路径无关，见 handoff）；`git diff --check` 仅 CRLF 提示、无空白错误。
- **B11 验证证据:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；registry/RenderGraph 过滤 54 个用例、439 assertion 全通过（含 3 个新 B11 TEST_CASE：owner reclassification / unknown no-op / resize recreation pairing，42 assertion）；`[Unit]*` 480 用例、6212 assertion 全通过；`[Integration]*` 141 用例中 2 个失败均为 `GPUEntityLifecycleRegistryTest`（既有未提交 G2 `ResourceManagerShader` 注册与 W5 余额断言冲突 + 环境 compute shader 加载，B9 handoff 同类；该测试不执行任何 B11 代码路径、registry 既有方法 diff 为纯增量，故非 B11 引入）；`git diff --check` 无空白错误。
- **B12 验证证据:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；RenderGraph 过滤 48 个用例、355 assertion 全通过（含 6 个新 B12 TEST_CASE：resolve success / missing snapshot / import kind incompatible / zero handle / unsupported kind / no-active-pass，全部 resolver 级、无 GL context）；`[Unit]*` 全部通过；完整套件 760 用例中 755 通过，5 个失败均为既有环境性（W5 `GPUEntityLifecycleRegistryTest` 未提交 G2 冲突 + GPU compute shader 加载 + 性能计时噪音，与 B12 路径无关，B9/B11 handoff 同类）；`check_module_boundaries.py` 与 `check_legacy_reintroduction.py` 均 PASS；`git diff --check` 无空白错误（仅 CRLF 提示）。
- **B12 边界补强证据:** 重复 `ImportedBackingHandle` tag 现在 fail-closed，不再以后者覆盖前者；新增 resolver unit case。`NoMoreDayTests` 构建成功；RenderGraph 过滤 34 个用例、248 assertions 全通过。
- **B12 GL gate 证据:** `GraphBindingEquivalenceGLTest` 在真实 GL context 下验证 graph-driven-only 与 manual-only 的 SSBO/image 输出 hash 等价，并验证 zero/missing/duplicate snapshot fail-closed；两个 case/45 assertions 全过，`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 1/1 通过（23.94s）。该 fixture 非生产 pass，故 B2-B4 仍保持未完成，不能据此删除手工绑定。
- **B2/B3 真实 ShadowBuild GL gate 证据（2026-08-04）:** `GraphBindingEquivalenceGLTest` 新增 `[GPU-Diagnostic] RenderGraph - real ShadowBuildPass graph-driven bind + phase barrier gate on real GL`：真实 ShadowPrepare/ShadowBuild/ShadowResolve 3-pass 链 + QualityTierManager High→SDF + 真实 RenderGraph::Execute，验证真实 graph-driven binding（2 操作准入）、same-pass phase barrier（plan 0x220 + Execute EmitPhaseBarrier 解析）、手工绑定等价（SDF/mask hash 逐位相等）、fail-closed（零句柄 deny 且输出不变）、GL errors=0、registry 无增长。1 case / 50 assertions 通过；`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 1/1 通过（25.00s，含既有 2 case）；`git diff --check` 无空白错误。该 gate 是合同级逐 pass 证据，非生产视觉证据；B2-B4 仍保持未完成，所有手工绑定与 ShadowResolve 原 barrier 继续保留。
- **B4 真实 LightCulling GL gate 证据（2026-08-05）:** `GraphBindingEquivalenceGLTest` 新增 2 个 `[GPU-Diagnostic]` TEST_CASE：(1) `RenderGraph - B4 real LightCullingPass contract gate on real GL`——真实 `LightCullingPass` + 测试本地 `ClusterConsumerPass`（Fragment Read 4 个 cluster SSBO）经真实 `RenderGraph::Execute`，验证 compiled imports==6（LightBufferSSBO point 0 owner Lighting + 5 cluster owner LightCulling，bindingPoint 0/1/2/3/4/5）、bindings==0（vacuous）、transitions（LightBoundsSSBO Host→Compute + 4 条 cluster Compute→Fragment bits==0x00002000）、帧 A/B/C（空/有效/零句柄 snapshot）cluster counts hash 逐位相等且非平凡（8 cluster 各 pointCount=1、writeCursor=8、overflowSum=0；排除 racy header.offset）、GL errors=0、registry 无增长、真实 owner teardown release ledger==1；(2) `RenderGraph - B4 real 6-point LightCulling BufferBase surface executor gate on real GL`——镜像 pass `LightCullingSurfacePass`（同一 shader/dispatch/barrier 序列，binding authority 可切换）证明真实 6 点 surface 被 resolver 恰好准入 6 个 BindBufferBase operations（points 0-5、handles==真实）、graph-driven-only 与 manual-only 确定性 readback hash 逐位相等、零句柄 fail-closed（6 条 "zero/invalid handle"、帧不 dispatch）、ManualOnly 安全网基线不变。2 cases / 114 assertions 通过；`& .\bin\NoMoreDayTests.exe --test-case=*B4*` → 2 cases / 114 assertions SUCCESS；`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 1/1 通过（23.77s，含 B2/B3、B12×2、B4×2）；`git diff --check` 无空白错误。**Blocker**：真实 `LightCullingPass::Setup` 未声明 BindBufferBase observer（`plan.bindings`==0、resolver vacuous），删手工绑定需先生产 Setup 接线（本任务范围外）；该 gate 是合同级逐 pass 证据，非生产视觉证据；B4 仍保持未完成，6 个手工 `BindBufferBase` 与 readback MemoryBarrier 继续保留。
- **B4 生产 Setup observer 接线证据（2026-08-05）:** `src/engine/render/passes/LightCullingPass.cpp` `Setup` 新增 6 条 observer-only `builder.BindBufferBase(tag, point)`（LightBufferSSBO=0、ClusterHeaderSSBO=1、ClusterLightIndexSSBO=2、LightBoundsSSBO=3、ClusterCounterSSBO=4、ClusterPackedLightSSBO=5，均用既有 `resolveLightCullingBinding` 解析 BindingRegistry LightCulling domain 符号，与 import bindingPoint 一致）；Execute 内 6 个手工 `BindBufferBase`、DispatchComputeNoBarrier、MemoryBarrier 0x2000、readback 全部保留。`GraphBindingEquivalenceGLTest` B4 contract gate 相应更新：compiled `plan.bindings` for LightCullingPass==6（points 0-5）；帧 A（空 snapshot）resolver fail-closed（ops 空、allAdmitted=false、6 条 "no imported backing snapshot" 诊断，手工绑定权威）；帧 B（有效 snapshot）resolver **恰好准入 6 个 BindBufferBase operations**（points 0-5、handles==真实 id、零 runtime diagnostic）；帧 C（零句柄 snapshot）fail-closed（6 条 "zero/invalid handle" 诊断、cluster counts hash 逐位不变）。验证：`cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；`& .\bin\NoMoreDayTests.exe --test-case=*B4*` → 2 cases / 143 assertions SUCCESS（114→143，新增 29 断言）；`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.gpu.diagnostic" --output-on-failure` 1/1 通过（22.31s）；`git diff --check`（限 3 个授权文件）无空白错误。**Blocker（更新，见 B4 gate 明细）**：生产 Setup observer 接线完成、gate 现在能直接解析真实 pass；但删除 6 个生产手工 `BindBufferBase`（LightCullingPass.cpp L352-362）与 readback MemoryBarrier 仍阻塞于真实生产场景（1280x720，3 fixture）graph-driven-only vs manual-only 渲染输出等价、生产资源快照无增长、生产 gate GO artifact 证据；B4 仍保持未完成，手工 binds/barrier 继续保留。
- **Owner blocker 修复验证证据（2026-08-04）:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；`ResourceManagerShaderOwnershipTest` 2 用例（真实 GL pass shutdown 不释放 + ReleaseShader fail-safe/幂等）通过；`GraphBindingEquivalenceGLTest` 真实 ShadowBuild GL gate 在移除 `SetHeadless` 规避、改为真实 `unloadAll()` 后通过（release ledger==2 证明 exactly-once GL teardown）；`git diff --check` 无空白错误；`check_module_boundaries.py` 与 `check_legacy_reintroduction.py` 均 PASS。生产 `Game::cleanup` → `RenderSystem::Shutdown` → `unloadAll` 路径不再双重释放。
- **B6 验证证据:** 见本次 B6 handoff（`git diff --check`、最窄 build/test、新 unit TEST_CASE 结果按实际执行记录）。
- **B7 验证证据（2026-08-04）:** 扩展 `tests/unit/RenderGraphValidationTest.cpp:608` `[Unit] RenderGraph - Phase B typed shadow/cluster access plan`（6 pass：Scene/ShadowPrepare/ShadowBuild/ShadowResolve/LightCulling/ClusterConsumer），断言点如下：
  - 资源在 compiled plan（:638-680）：ShadowAtlas/ShadowDistanceField/ShadowMask/ShadowOccluderSSBO + 全部 4 个 cluster SSBO（ClusterHeader/ClusterLightIndex/ClusterPackedLight/ClusterCounter）均存在。
  - producer→consumer edges（:682-721）：ShadowPrepare→ShadowBuild via ShadowOccluderSSBO；ShadowBuild→ShadowResolve via ShadowDistanceField；LightCulling→ClusterConsumer 全部 4 条 cluster edge。
  - transitions（:723-775）：ShadowOccluderSSBO Host→Compute bits≠0；ShadowDistanceField Compute→Fragment bits≠0；4 个 cluster SSBO Compute→Fragment bits==0x00002000（GL_SHADER_STORAGE_BARRIER_BIT）。
  - 显式负向诊断断言（:777-788）：遍历 `plan.diagnostics`，逐条断言 message 不含 `read-before-write` / `multiple write owners` / `cycle detected`（B7 contract 的 no missing producer / no multiple writers / no cycle）。
  - 结果：`cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；`bin/NoMoreDayTests.exe --test-case=*typed*shadow*plan*` → 1 用例 / 41 assertion 全通过；`--test-case=*Phase B*` → 5 用例 / 115 assertion 全通过（含本 B7 用例与 same-pass phase barrier 用例）。
  - `git diff --check` 无空白错误（仅 CRLF 提示，与既有改动一致）；`check_module_boundaries.py` → PASS；`check_legacy_reintroduction.py` → PASS（baseline 222/71 → current 220/70，无 marker 回归）。生产代码零改动（纯测试扩展 + 计划文档）。
- 提交经用户授权；handoff 如实报告。

## 8. Handoff Template

```text
package: phase-b-typed-passes
source baseline: <commit>
files changed: ...
contract changed: RenderResourceTag/RenderOwnerTag 新增枚举（见 design §5.1 表）；B12 新增 graph-driven binding 准入/执行合同（`RenderGraph::ResolvePassBindings`/`ApplyActivePassBindings`、`RenderContext::ApplyActivePassBindings`、`BindTextureUnit`/`BindColorAttachment` observer 声明）
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: M0-B spec §4 / debt RG-3/RG-5
remaining risk or blocker: ...
```
