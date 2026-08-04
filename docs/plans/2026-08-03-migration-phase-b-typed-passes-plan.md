# Phase B: 5 个空 Setup pass typed 迁移

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.1
> **关闭债务:** RG-3/RG-5（最大残留组）
> **依赖:** 无（B 是 C/D 的前置地基）
> **状态:** [~] 进行中（B1/B5/B6/B7/B9-B12 完成；B8 合同级真实 GL binding fixture 已通过，但生产视觉回归证据仍缺失；B2-B4 的生产路径准入仍待逐 pass gate）

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
| B2 | ShadowBuildPass typed Setup + 移除手工 bind/barrier/flush | B1 + phase-aware barrier/import binding 合同 | [~]（phase barrier 已加；binding 仍 observer-only；删手工 bind 阻塞于 B9） |
| B3 | ShadowResolvePass typed Setup + 移除手工 barrier/FBO 恢复 | B1 + backing/import/binding 合同 | [~]（Setup 已加；原 compute→fragment barrier保留；删 barrier 阻塞于 B9） |
| B4 | LightCullingPass typed Setup + 移除手工 BindBufferBase/barrier/flush | B1 + B9 | [~]（Setup/graph binding 已接线；删手工 bind 待真实 GL gate） |
| B5 | ShadowPreparePass Host 依赖声明 + 移除 flush | B1 | [x] |
| B6 | VFXEmissionSnapshotPass 处理（声明或并入 RadianceCascades） | B1 | [x]（用户决策：声明 `ParticleEmissive`，保留独立 pass；详见 B6 明细） |
| B7 | graph 校验断言：computed plan 含 shadow/cluster edge + transition | B2-B6 | [x]（typed access plan 测试已覆盖全部 4 条 cluster edge + transition 与 3 条 shadow edge/transition；负向诊断断言补齐，证据见 §7） |
| B8 | 视觉回归 + 边界脚本 + `legacy` 扫描 | B2-B6 | [~]（边界/legacy/unit/GPU 合同与 B12 专用真实 GL fixture 已通过；生产 Shadow/cluster/VFX 视觉回归证据仍缺失，见 B8 明细） |
| B9 | external backing import 合同（`ImportResource` + compiled plan 导出 + 校验 + 真实 pass 接线） | B1（实现完成） | [x]（observer-only 合同与资源表完成） |
| B10 | RenderContext 外部 backing handle snapshot 注入与 fail-closed 查询合同 | B9 | [x]（真实 owner snapshot 已注入；graph 不拥有 backing） |
| B11 | registry owner metadata 对齐（`ReclassifyResourceOwner` + Shadow/cluster/LightBuffer 真实 owner 接线 + unit tests） | B9/B10 | [x] |
| B12 | graph-driven binding 准入/执行合同（resolver + executor + unit tests） | B9/B10/B11 | [x]（准入与执行层实现完成；真实 GL 集成测试为剩余 gate，过前手工绑定保留） |

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
| 生产 gate GO artifact | **未通过**（当前 artifact 为 NO_GO；不是渲染矩阵失败） | `artifacts/gpu-gate/local-gpu-hardware/gpu_hardware_validation_artifact.json`：硬件/矩阵/GL diagnostics/60s stress 全部通过，`leak_candidate_count=0`，但 runner stderr 有 `'1' 不是内部或外部命令`，`gate_succeeded=false` |
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
