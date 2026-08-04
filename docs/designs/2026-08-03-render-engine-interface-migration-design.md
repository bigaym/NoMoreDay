# 渲染引擎全量接口迁移设计（Technical Specification）

> **Status:** proposed; research complete; Phase B technical contract implementation in progress.
>
> **Purpose:** 把渲染引擎中仍绕过 M0-B typed RenderGraph 契约的全部旧接口/手工路径，收敛到统一 typed descriptor/access 接口面，关闭 RG-1~RG-6 债务，达成 M0-B spec §4 全部 7 条验收标准，且不改变既有视觉顺序与质量策略。
>
> **Primary evidence:**
> [M0-B spec](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/spec.md)、
> [M0-B debt register](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/debt_register.md)、
> [M0-B legacy-access-inventory](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/legacy-access-inventory.md)、
> [V5 master specification](../../conductor/specs/rendering_engine_v5_master_spec.md)、
> [GPU rendering audit review](../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)。

## 1. Decision Summary

当前 `RenderGraph` typed API（`Read/Write(Tag,Owner[,Stage,Usage])`、`DeclareResource(TypedResourceDescriptor)`、`AddPassLocalBarrier`）已就绪，string-based 访问已 fail-closed 归零（S5）。但六类旧路径仍绕过统一契约：

| 组 | 迁移对象 | 涉及旧接口/手工路径 | 关闭债务 |
| --- | --- | --- | --- |
| G | 资源注册表补齐 | 8 处专用 VAO/VBO、`ResourceManager` compute shader、`GPUTimerQueryRing` GL query 未登记 registry | RG-3 |
| B | 5 个空 Setup pass | ShadowPrepare/Build/Resolve、LightCulling、VFXEmissionSnapshot 未声明 typed access，Execute 内手工 BindBufferBase/ImageTexture/MemoryBarrier | RG-3/RG-5 |
| C | 其余 pass 手工 barrier | RadianceCascades、OccluderExtract、GIComposite、Lighting、Fluid、JFA 共 25 处 `GPUUtils::MemoryBarrier` 直调 | RG-5 |
| E | 旧光照 fallback | LightingPass V2 直读 SSBO 降级、VolumetricLightPass 非 clustered 直绑路径 | 验收 §4.1 |
| D | RenderSystem 手工条件链 | 逐 pass OnResize fan-out、owner 追踪、HDR/GI 状态机、composite input 手工选择、Distortion 手工接线、lambda 内 BindFramebuffer | RG-1/RG-2 |
| F | reload/capability 单一路径 | 生产路径 `ShaderHotReloadManager`、`ShaderReloadGovernance` 仅覆盖 compute、capability matrix 未 gate 生产路径 | RG-4 |

执行序：`G → B → C → E → D`；F 独立并行（建议 B 完成后启动）。G 先行因它机械、低风险、可立即建立资源台账并暴露 MS-8 遗留泄漏候选。

## 2. Goals, Non-Goals, And Fixed Constraints

### 2.1 Goals

1. 每个生产 pass 的资源访问都用 typed descriptor/access 声明，`CompiledRenderPlan` 导出含 GI/HDR/SSBO/shadow/cluster/external target 的全部 edge 与 transition。
2. 手工 barrier 全部消除或显式声明为 pass-local，`GPUUtils::MemoryBarrier` 生产调用点归零。
3. registry 覆盖引擎创建的全部 texture/FBO/SSBO/VBO/VAO/query/persistent mapping，无双重所有权。
4. 生产渲染只经单一 reload/capability governance，缺失能力 fail-closed。
5. RenderSystem 的 resize/owner/状态机/接线收敛进 graph，消除手工条件链。

### 2.2 Non-Goals

- 不改变前序 Track 固化的视觉/GI 顺序与质量策略（spec §非目标）。
- 不实现跨队列、async compute、Vulkan 后端或通用 renderer abstraction。
- 不把 registry 变成 global raw-GL owner；不以观测名义绕过 RAII。
- 不实施 DRS、曝光或 JFA dirty-region。
- 不删除 string overload（V3/V5 旧 contract 源码兼容保留，运行期仍 fail-closed）。

### 2.3 Governing Constraints

- 保持既有插入顺序为拓扑 tie-breaker，不自动重排（spec §3）。
- 任何无法映射的 transition 或 capability 缺失必须 fail-closed，不能由手工 barrier 或名称回退旁路。
- 每种 barrier 映射用功能 + 有效 GPU timing 对照；禁止为性能删除正确性 barrier。
- registry 只接收 observer 事件，owner 负责 RAII 释放；`AdvanceFrame` 恰一次。
- 每 pass 迁移先诊断后迁移，禁止一次性替换渲染路径。
- `src/` 禁写 `legacy` 字样；`check_module_boundaries.py` 保持 71/71。
- 提交逐批经用户授权；生产状态仍 NO-GO，仅 M0-C 游戏二进制 artifact 可改变。

## 3. Verified Baseline And Problem Boundaries

### 3.1 Typed API 已就绪（非迁移对象）

- `RenderGraphBuilder`（RenderGraph.hpp:265-288）：typed Read/Write 4 组重载 + `DeclareResource` + `AddPassLocalBarrier`；string 重载 :267-268 由 `RejectLegacyStringAccess`（RenderGraph.cpp:572-588）无条件拒绝，生产 0 调用。
- `TypedResourceDescriptor`/`TypedPassAccess`/`ResourceKind`/`ResourceFormat`/`ExtentPolicy`/`ResourceUsage`/`ResourceLifetime`/`HistoryRelation`/`PipelineStage`/`PassAccessMode`（RenderResourceDescriptor.hpp:10-120）。
- `StableResourceId`/`ResolveStableResourceId`（:189-201）；`MapGlBarrierBits(prevStage,prevMode,nextStage,nextMode,kind)`（:203-252）。
- `CompiledRenderPlan.transitions` 含 previous/next stage/mode/barrierBits（hpp:348-358）；`GetCompiledPlan()` 供硬件门证据。
- `RenderOwnerTag` 16 个、`RenderResourceTag` 16 个（RenderGraph.hpp:16-54）；`ToResourceName`/`ToResourceTag`/`ToOwnerName` 为 switch 映射，新增枚举必须同步补 case。

### 3.2 G 组缺口（registry 未覆盖）

- 8 处专用 VAO/VBO，全部为 `rlLoadVertexArray()`+`rlLoadVertexBuffer()` 模式（`ResourceKind::VertexArray`/`VertexBuffer`）：
  PopupRenderer.cpp:224、MDIRenderer.cpp:82、GPUParticleSystem.cpp:446、trail/GPUTrailRenderer.cpp:52（仅 dummy VAO）、GPULootSystem.cpp:384、GPUSkillEffectSystem.cpp:405、GPUTextSystem.cpp:74、passes/FluidSimulationPass.cpp:89。
- `ResourceManager::loadShader`（resource/ResourceManager.cpp:266-310）VS/FS 未记录 `ShaderReloadGovernance`；`loadComputeShader`（:319-368）已记录（:365-368）。
- `GPUTimerQueryRing`（render/debug/GPUTimerQueryRing.cpp）：`glGenQueries`/`glDeleteQueries` 动态解析（:39-40,150-156），未登记 registry（`ResourceKind::QueryRing`）。
- `GPUResourceRegistry`（resources/GPUResourceRegistry.hpp）：`RegisterResource`(:57)/`UnregisterResource`(:60)/`UpdateResourceSize`(:61)/`AdvanceFrame`(:62)/`TakeSnapshot`(:70)；key=`(kind<<32)|handle`(:84)。

### 3.3 B 组缺口（5 个空 Setup pass）

- `LightCullingPass`（Setup L41-43 空；Execute 手工 BindBufferBase 6 SSBO L260-270、DispatchComputeNoBarrier L272、MemoryBarrier(0x2000) L277、ApplyRlglFlushTemplate L290）。
- `ShadowPreparePass`（Setup L23-25 空；纯 CPU L164-208 + flush L219）。
- `ShadowBuildPass`（Setup L38-40 空；L359-363 BindBufferBase/BindImageTexture、L364 Dispatch、L372 MemoryBarrier(Image|Buffer)、L210-212 直绑 FBO+Viewport+ClearBackground、L221-269 BeginShaderMode+FullscreenQuad、L272-276 手工恢复 hdr FBO、L380 flush）。
- `ShadowResolvePass`（Setup L32-34 空；L164 ApplyComputeToFragmentBarrierTemplate、L166-167 BindFramebuffer(m_shadowMask.fbo)、L186-197 手工 bind+FBO 恢复、L200 flush）。
- `VFXEmissionSnapshotPass`（Setup L12 空；Execute L14-18 仅回调 m_callback）。
- 资源宿主：ShadowBuildPass `m_sdfField`/`m_shadowAtlas`/`m_occluderBuffer`（ShadowBuildPass.hpp:64-66）；ShadowResolvePass `m_shadowMask`（ShadowResolvePass.hpp:46）；`ClusteredLightingState` 5 个 ComputeBuffer（ClusteredLightingState.hpp:118-122）。这些资源不在 `RenderResourceTag` 枚举内，须先扩展枚举（见 §5.1）。

### 3.4 C 组缺口（手工 barrier）

25 处 `GPUUtils::MemoryBarrier`（GPUUtils.cpp:151-158 动态解析）：
RadianceCascades:395/478/487/531/628/789、OccluderExtract:219/254/409、GIComposite:381/425、LightingPass:291/350、FluidSimulation:311/343/372/403/488/832、JFAPass:394/450/509/564/835。这些 barrier 语义分三类：跨 pass 依赖（graph transition 覆盖）、pass-local 同步（AddPassLocalBarrier）、host readback 同步（保留，非 graph barrier）。

### 3.5 E 组缺口（旧光照 fallback）

- `LightingPass.cpp:310-406`：clustered 失败时降级 V2 直读 SSBO（uLightCount 循环遍历，fallback=V2Lighting）。
- `VolumetricLightPass.cpp:172-181`：非 clustered 直绑 SSBO + 循环遍历。
- 主路径 `ClusteredLightingState`（shared 5 个 cluster buffer）已确认新契约；LightCullingPass 需 `v3Enabled && dynamicLightingEnabled && clusteredLightingEnabled` 才执行（LightCullingPass.cpp:129-133）。

### 3.6 D 组缺口（RenderSystem 手工条件链）

`RenderSystem::render` 内：
- 逐 pass OnResize fan-out（:1319-1344 创建分支、:1357-1381 resize 分支，两处重复）；`EnsureGiPassesSized`（:297-308）；`RenderGraph::OnResize`（RenderGraph.cpp:381-387）从未被调用。
- HDR 切换状态机（:1246-1273）；GI re-enable 状态机（:1388-1399）；offscreen seed blit（:1404-1418）。
- 手工 owner 追踪 sceneHdrOwner/ldrOwner（:1463-1572）与 composite input tag 手工选择（:1585-1593）+ 变更日志（:1595-1613）。
- Distortion 手工 `SetInputBuffer(g_postProcessPass->GetOutputBuffer())`（:1580）。
- lambda 内手工 BindFramebuffer/Viewport/Clear（:1470-1480 ScenePass、:1615-1633 Composite）。
- 每帧重建 graph（:1466）+ 条件 AddPass（:1485-1583）。
- 已有正确锚点：`AdvanceFrame` 恰一次在 graph.Execute 成功后（:1700-1704）；FlushRing 先于 DRS/HUD（:1705-1713）。

### 3.7 F 组缺口（reload/capability 双轨）

- `ShaderHotReloadManager` 在生产路径：RenderSystem.cpp:927-987 注册 12 watch、:1115-1117 Clear、:1223-1231 每帧 SetEnabled+PollAndReload（仅 Debug + config）。
- `ShaderReloadGovernance` 仅 compute 分支记录（ResourceManager.cpp:365-368）；VS/FS `loadShader` 未记录。
- `DeviceCapabilityMatrix` 仅 `GPUHardwareValidationGate.cpp:880` 使用，未 gating 生产路径。

## 4. Cross-Cutting Contracts

### 4.1 Tag 扩展规则

新增 `RenderResourceTag` 与 `RenderOwnerTag` 枚举值必须同步更新 `ToResourceName`/`ToResourceTag`/`ToOwnerName`（switch fail-closed）。命名与现有资源区分：shadow 资源前缀 `Shadow*`，cluster 资源后缀 `SSBO`。

### 4.2 Barrier 判定规则

```text
For each manual MemoryBarrier call site:
  cross-pass dependency on graph-declared resource  -> delete; graph transition covers
  same-pass sync (compute-write then fragment-read) -> AddPassLocalBarrier(bits)
  host readback sync (GetBufferSubData/ReadPixels)  -> keep explicit (not a graph barrier)
```

### 4.3 Registry 记账规则（沿用 2026-08-02 W5 契约）

observer-only；重复注册拒绝+诊断；尺寸更新防下溢；注销先于 GL 释放；handle 复用须先注销；PersistentMapping 先于 backing buffer 移除；`AdvanceFrame` 恰一次。

### 4.4 渲染行为 vs 同步行为分离

shadow tile 的 FBO bind/viewport/clear/FullscreenQuad 属渲染行为，保留在 Execute；仅 barrier 与资源访问声明化。

## 5. Phase Designs

### 5.1 Phase B：5 个空 Setup pass typed 迁移（核心）

**新增 tag（9 资源 + 2 owner）**：

| 新 RenderResourceTag | kind | format | lifetime | 宿主 |
| --- | --- | --- | --- | --- |
| `ShadowAtlas` | Texture2D | RGBA16F | Persistent | ShadowBuildPass `m_shadowAtlas` |
| `ShadowDistanceField` | Texture2D | RG16F（FramebufferManager 用 GL_RG16F） | Persistent | ShadowBuildPass `m_sdfField` |
| `ShadowMask` | Texture2D | RGBA16F（实际 FBO 格式） | Persistent | ShadowResolvePass `m_shadowMask` |
| `ShadowOccluderSSBO` | StorageBuffer | - | Persistent | ShadowBuildPass `m_occluderBuffer` |
| `ClusterHeaderSSBO` | StorageBuffer | - | Persistent | ClusteredLightingState `m_clusterHeaderBuffer` |
| `ClusterLightIndexSSBO` | StorageBuffer | - | Persistent | `m_clusterLightIndexBuffer` |
| `ClusterPackedLightSSBO` | StorageBuffer | - | Persistent | `m_clusterPackedLightBuffer` |
| `ClusterCounterSSBO` | StorageBuffer | - | Persistent | `m_clusterCounterBuffer` |
| `LightBoundsSSBO` | StorageBuffer | - | Persistent | `m_lightBoundsBuffer` |
| `ParticleEmissive` | Texture2D | RGBA16F | Persistent | RadianceCascadesPass `m_particleEmissive`（B6 新增，2026-08-04） |

新 owner：`Shadow`（3 pass）、`LightCulling`（LightCullingPass）。`ParticleEmissive` 复用既有 owner `RadianceCascades`，无需新 owner。

**pass 声明草图**（伪代码，非完整实现）：

```text
// ShadowBuildPass::Setup
DeclareResource(ShadowAtlas, Texture2D, RGBA16F, Persistent, owner=Shadow)
DeclareResource(ShadowDistanceField, Texture2D, RG16F, Persistent, owner=Shadow)
DeclareResource(ShadowOccluderSSBO, StorageBuffer, Persistent, owner=Shadow)
Write(ShadowAtlas, Shadow, Fragment, ColorAttachment)
Write(ShadowDistanceField, Shadow, Compute, StorageWrite)
Read(ShadowOccluderSSBO, Shadow, Compute, StorageRead)
AddPhaseBarrier(Compute, Fragment, Image|Buffer)
BindBufferBase(ShadowOccluderSSBO, kOccluderBinding)      // observer-only
BindImageUnit(ShadowDistanceField, kSdfImageBinding, WRITE_ONLY, GL_RG16F)  // observer-only
ImportResource(ShadowAtlas / ShadowDistanceField / ShadowOccluderSSBO)      // external backing 合同

// ShadowResolvePass::Setup
DeclareResource(ShadowMask, Texture2D, RGBA16F, Persistent, owner=Shadow)
Read(ShadowDistanceField, Shadow, Fragment, ShaderRead)   // graph 生成 compute->fragment transition
Write(ShadowMask, Shadow, FramebufferAttachment, ColorAttachment)
ImportResource(ShadowMask)                                // external backing 合同

// LightCullingPass::Setup
// 注意：不声明 Read(LightBufferSSBO) —— Lighting(写者) 在其后，声明会触发 read-before-write。
Write(ClusterHeaderSSBO, LightCulling, Compute, StorageWrite)
Write(ClusterLightIndexSSBO, LightCulling, Compute, StorageWrite)
Write(ClusterPackedLightSSBO, LightCulling, Compute, StorageWrite)
Write(ClusterCounterSSBO, LightCulling, Compute, StorageWrite)
Write(LightBoundsSSBO, LightCulling, Host, StorageWrite)
Read(LightBoundsSSBO, LightCulling, Compute, StorageRead)
ImportResource(Cluster* 5 + LightBufferSSBO)              // external backing 合同

// ShadowPreparePass::Setup
Write(ShadowOccluderSSBO, Shadow, Host, StorageWrite)     // CPU 填充，不触碰 occluder GL buffer

// VFXEmissionSnapshotPass::Setup
// B6 修正（2026-08-04）：本 pass 真实写入的是 RadianceCascadesPass 的粒子 emissive backing
// （m_particleEmissive，全屏 RGBA16F，EnsureResources 创建/resize）。pass 不读 EmissiveBuffer、
// 不采样场景纹理；hdrSceneBuffer.fbo 仅作 GL 状态恢复。粒子 SSBO 为 GPUParticleSystem 内部
// backing（graph 无写者），不声明，避免 read-before-write / 臆造。
DeclareResource(ParticleEmissive, Texture2D, RGBA16F, Persistent, owner=RadianceCascades)
Write(ParticleEmissive, RadianceCascades, Fragment, ColorAttachment)
ImportResource(ParticleEmissive)                              // external backing 合同
```

**Execute 内变更**：资源绑定在 backing/import/binding ownership 合同落地前保持手工 `BindBufferBase`/`BindImageTexture`，只新增 observer-only binding declaration；跨 pass 的 `MemoryBarrier`/flush 逐步迁移到 graph transition。ShadowBuild 的同一 Execute 内 compute→Hybrid tile fragment 不能使用 pass-entry `AddPassLocalBarrier`，使用 `AddPhaseBarrier(Compute, Fragment, bits)` 声明，并由 `RenderContext::EmitPhaseBarrier` 在 dispatch 后、tile draw 前发出。保留渲染行为（tile FBO bind/viewport/clear/FullscreenQuad）。ShadowPrepare 的 CPU 数据流经 host write 到 SSBO，需声明 Host/Transfer 同步段。

**B2 technical contract now implemented:** `RenderGraphBuilder::AddPhaseBarrier` compiles phase declarations; `RenderGraph::Execute` exposes the active pass through `RenderContext`; `EmitPhaseBarrier` resolves and issues the declared bits at the call site. `RenderGraphBuilder::BindBufferBase`/`BindImageUnit` compile observer-only binding metadata and validate tag/descriptor consistency; they do not allocate, import, bind, resize, or release GL backing. Therefore B2/B3 must not remove manual binds until a separate backing/import/binding ownership task is designed and tested.

**External backing import contract (2026-08-04, blocker resolution):** the shadow/cluster/LightBuffer GL backings listed above are created, resized, and released OUTSIDE the graph (`FramebufferManager` framebuffers, `ComputeBuffer` SSBOs, `LightManager`/`ClusteredLightingState` buffers). `RenderGraphBuilder::ImportResource(ResourceImportInfo)` lets each pass declare at Setup time that it reaches such a resource through externally owned backing. The declaration is observer-only and follows the B2 rule set:

- the graph **never** allocates, resizes, frees, or GL-binds imported backing;
- ownership remains with `backingOwner` (the pass/state that calls `FramebufferManager::Create/Resize/Destroy`, `ComputeBuffer::Create/Release`, or `LightManager` buffer management);
- `bindingPoint`/`imageUnit`/`imageAccess`/`imageFormat`/`colorAttachmentIndex` document the manual GL surface the pass uses today, so a future phase can swap manual binds for graph-driven ones without changing ownership;
- resize lifecycle is expressed by `resizeFollowsScreen` (screen-size FBOs) / `resizeFollowsCapacity` (capacity-grown SSBOs/atlas); both false = fixed-size backing.

Compiled plan exports these as `CompiledResourceImport` rows (`plan.imports`). Validation fails closed on: Custom tag, duplicate import per pass, conflicting kind/format across passes, `Transient` descriptor (graph would allocate it), kind/format mismatch with the pass descriptor; warns on unknown `backingOwner` and on mismatch with a `BindBufferBase`/`BindImageUnit` observation (manual binds stay authoritative).

The render context now receives an observer-only per-frame snapshot of the real
external handles for shadow, clustered-lighting, and `LightBufferSSBO` resources.
Owners provide the snapshot; the context exposes lookup by resource tag and keeps
zero handles invalid. Duplicate snapshots for one tag are rejected fail-closed.
This resolves handle discovery without transferring allocation, resize, release,
or GL-bind ownership to the graph. Graph-driven GL binding remains gated on a
real GL integration test; manual binds remain authoritative until then.

Per-resource import metadata (as wired in the passes):

| 资源 | kind | format | backingOwner | resize 合同 | 绑定点/单元 |
| --- | --- | --- | --- | --- | --- |
| `ShadowAtlas` | Texture2D | RGBA16F | Shadow | resizeFollowsCapacity（EnsureAtlasSize） | colorAttachment 0 |
| `ShadowDistanceField` | Texture2D | RG16F | Shadow | resizeFollowsScreen（OnResize） | imageUnit 0, WRITE_ONLY, GL_RG16F |
| `ShadowOccluderSSBO` | StorageBuffer | - | Shadow | resizeFollowsCapacity（UploadOccluders） | bindingPoint 15 |
| `ShadowMask` | Texture2D | RGBA16F | Shadow | resizeFollowsScreen（OnResize） | colorAttachment 0 |
| `ClusterHeaderSSBO` | StorageBuffer | - | LightCulling | resizeFollowsCapacity | bindingPoint 1 |
| `ClusterLightIndexSSBO` | StorageBuffer | - | LightCulling | resizeFollowsCapacity | bindingPoint 2 |
| `ClusterPackedLightSSBO` | StorageBuffer | - | LightCulling | resizeFollowsCapacity | bindingPoint 5 |
| `ClusterCounterSSBO` | StorageBuffer | - | LightCulling | resizeFollowsCapacity | bindingPoint 4 |
| `LightBoundsSSBO` | StorageBuffer | - | LightCulling | resizeFollowsCapacity | bindingPoint 3 |
| `LightBufferSSBO` | StorageBuffer | - | Lighting（LightManager） | resizeFollowsCapacity（OrphanAndUpload） | bindingPoint 0 |
| `ParticleEmissive` | Texture2D | RGBA16F | RadianceCascades | resizeFollowsScreen（EnsureResources） | colorAttachment 0（B6 新增，2026-08-04） |

已知 registry 债务已由 B11 补齐（2026-08-04）：`GPUResourceRegistry::ReclassifyResourceOwner` 允许真实 owner 在创建/resize 后把 observer 记录 owner 改为 graph 合同值——Shadow 三资源归 `Shadow`、cluster 5 buffer 归 `LightCulling`、LightBuffer 归 `Lighting`（与上方 LightBuffer 行一致），registry owner 与 import 合同 `backingOwner` 可验证一致。observer-only：不改 GL allocation/resize/release/bind ownership、不删除任何手工绑定/barrier。剩余唯一前置：graph-driven GL binding 的显式执行映射与真实 GL 集成测试。

**Graph-driven binding admission/execution contract (B12, 2026-08-04):** `RenderGraph` 现可在当前 pass 执行前，把 compiled binding declarations（`BindBufferBase`/`BindImageUnit`）对匹配的 imported backing snapshot 做显式准入并发出真实 GL bind（只调用既有 `GPUUtils::BindBufferBase`/`BindImageTexture`；graph 不持有任何 GL handle，handle 全部来自 `RenderContext` 每帧 snapshot；不 alloc/resize/release/改 owner）。准入是 fail-closed 且显式的：只对「同 tag 有 `ImportResource`、import kind 与 binding kind 兼容、snapshot 非零且 `ImportedBackingHandle::IsValidFor` 为真」的资源执行；重复 snapshot tag、缺失 import/snapshot、kind 不一致、零句柄 → 拒绝并记录 validation/runtime diagnostic，绝不 bind 零句柄。`TextureUnit`/`ColorAttachment` binding 明确 unsupported（Warning diagnostic，不伪造执行）。API 显式区分准入与执行：`RenderGraph::ResolvePassBindings`（纯解析，无 GL，返回 `BindingResolutionResult{operations, diagnostics, allAdmitted}`）、`RenderGraph::ApplyActivePassBindings`（执行器，在 `Execute` 中 pass Execute 前调用）、`RenderContext::ApplyActivePassBindings`（委托 activeGraph）。手工 `BindBufferBase`/`BindImageTexture` 与 ShadowResolve 原 compute→fragment barrier 全部保留：graph-driven bind 是行为等价的重复绑定（同点同 handle 再绑定）。最小真实 GL fixture `GraphBindingEquivalenceGLTest` 已验证 SSBO/ImageUnit graph-driven-only 与 manual-only 输出 hash 等价及 zero/missing/duplicate snapshot fail-closed；生产 Shadow/LightCulling/VFX 路径仍需逐 pass gate，故 B2-B4 不得标完成、不得删除任何手工绑定。

### 5.2 Phase C：手工 barrier 收敛

逐点按 §4.2 规则判定 25 处调用点。已可归类的：
- LightingPass:350 → cluster 输出消费，改 graph transition（配合 B 组 LightingPass 增加 Read(Cluster*) 声明）。
- 其余 pass 内 compute→image/fragment 自同步 → `AddPassLocalBarrier`。
- 跨 pass 的 barrier（如 JFA→RadianceCascades 的 SDF 读、OccluderExtract→JFA）→ 删除，靠 typed access transition。

产出：`GPUUtils::MemoryBarrier` 生产调用点归零（保留工具函数供 host readback 等非 barrier 用途）。

### 5.3 Phase E：旧光照 fallback 收敛

clustered 为主路径后，删除 LightingPass V2 直读 fallback 与非 clustered volumetric 路径；fallback 语义用 feature flag 显式化（`clusteredLightingEnabled=false` 时 fail-closed 或明确降级策略），不再运行时静默降级。

### 5.4 Phase D：RenderSystem 手工条件链收敛

- 接通 `RenderGraph::OnResize(width,height)`，用 descriptor extentPolicy 驱动统一 resize，移除 :1319-1381 两处重复 fan-out 与 :297-308 手动链。
- 移除 :1463-1593 owner 追踪与 composite input 手工选择，改 graph 推断 `最后一个 Write(SceneHdrColor) 的 owner`。
- 收敛 HDR 切换（:1246-1273）与 GI re-enable（:1388-1399）状态机为 descriptor/registry snapshot 驱动。
- Distortion 输入改 typed access（移除 :1580 手工 SetInputBuffer）。
- lambda 内 BindFramebuffer/Viewport/Clear 收敛为 graph attachment 声明（保留渲染行为）。

### 5.5 Phase G：资源注册表补齐

在 8 处 VAO/VBO 创建成功处调 `RegisterResource(handle, VertexArray/VertexBuffer, ...)`，GL 释放前 `UnregisterResource`；`ResourceManager::loadShader` VS/FS 分支补 `ShaderReloadGovernance` 记录 + shader program 登记 `ShaderProgram`；`GPUTimerQueryRing` gen/delete 处登记 `QueryRing`。

### 5.6 Phase F：reload/capability 单一路径

- 移除生产路径 `ShaderHotReloadManager`（RenderSystem.cpp:927-987 注册、:1223-1231 Poll），统一到 `ShaderReloadGovernance`。
- `ShaderReloadGovernance` 覆盖 VS/FS + include 递归 hash + 失败保留上次成功 fingerprint 重试（spec §3）。
- `DeviceCapabilityMatrix` 接入生产路径：GL 4.3/SSBO/compute/image/barrier/format/timer/debug callback 缺失时依赖 feature fail-closed。
- GL debug callback 安装（P0 S3 未做项）。

## 6. Implementation Order And Subagent Work Packages

### 6.1 Dependency Graph

```text
G registry completeness (independent)
B typed passes (foundation)  ->  C manual barriers  ->  E lighting fallback
                                                            |
D RenderSystem chains (depends B/C)  <------------------------+
F reload/capability (parallel, start after B)
```

### 6.2 Package Lifecycle

每包顺序：capture baseline → reserve files → 先加回归测试 → 最小变更 → focused test → 包级 build/broader check → 独立 review → 如实报告（含 waiver/剩余风险）。不得从推断结论声称完成。

### 6.3 Shared Commands

```powershell
./build.bat check
./build.bat
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
python scripts/check_module_boundaries.py
python scripts/check_legacy_reintroduction.py
```

### 6.4 Handoff Template

```text
package: source baseline / files changed / contract changed /
focused tests + exact result / broader build-test + known unrelated failures /
artifact path / Track docs updated / remaining risk or blocker
```

## 7. Acceptance Matrix

| Phase | Required automated evidence | Completion condition |
| --- | --- | --- |
| G | registry snapshot 覆盖 8 VAO/VBO + compute shader + query；无 duplicate/missing | 台账与 S4 五秒快照基线无膨胀 |
| B | `*RenderGraph*` 合同测试 + 新 shadow/cluster typed 声明测试 | 5 pass 无空 Setup；graph 导出全部 edge；手工 Barrier/BindBufferBase 归零；视觉一致（2026-08-04 B8 注：最小 graph-driven-vs-manual 真实 GL fixture 已通过，但生产 Shadow/cluster/VFX 视觉一致证据仍缺失，无 screenshot 基线且生产 gate GO artifact 缺失；B8 未完成，详见 Phase B plan B8 明细） |
| C | barrier 收敛单测 + graph transition 覆盖断言 | `GPUUtils::MemoryBarrier` 生产调用点归零；无隐藏跨 pass barrier |
| E | clustered 主路径集成测试 | V2 直读/非 clustered 路径收敛；fallback 语义显式化 |
| D | `RenderGraph::OnResize` 调用点断言 + resize/toggle 回归 | 手工 fan-out/owner 追踪/状态机/接线移除；resize/HDR/GI 切换无回归 |
| F | reload failure/capability fallback 自动化测试 | 生产路径仅经单一 governance；GL debug callback 安装 |
| 整体 | spec.md §4 全部 7 条 + RG-1~RG-6 关闭 | 构建/unit/integration/CI/perf/smoke 通过，V3/V5 contract 兼容 |

## 8. Risks, Rollback, And Open Decisions

| Risk | Mitigation / rollback |
| --- | --- |
| 迁移揭露未声明依赖 | 每 pass 先跑 graph 诊断，缺失生产者/顺序违反先记录再迁移 |
| 过度保守 barrier 回归性能 | 每种映射用功能 + 有效 GPU timing 对照，不为性能删正确性 barrier |
| 新增枚举破坏 switch fail-closed | validation 构建对 switch 缺口 fail-closed；同步补 To*Name 映射 |
| shadow tile 渲染行为误并入 barrier | 仅声明访问与 barrier，渲染行为留在 Execute |
| MS-8 18 个泄漏候选 | G 组台账互斥推进，泄漏修复先于生产 GO |
| `legacy` 字样回归 | `check_legacy_reintroduction.py` 全程 PASS |

**Open decisions**（需用户或后续设计确认）：
1. ~~`VFXEmissionSnapshotPass`：迁移为声明 EmissiveBuffer 依赖，还是删除并并入 RadianceCascades。~~ **已解决（2026-08-04，B6）**：保留独立 pass，在 Setup 声明真实资源 `ParticleEmissive`（m_particleEmissive 纹理，写 + import，owner=RadianceCascades），不并入 RadianceCascades、不改变 pass 顺序或视觉行为。旧计划中 `Read(EmissiveBuffer, ...)` 为错误假设——本 pass 不读 EmissiveBuffer（见 §5.1 修正伪代码）。
2. E 组 `clusteredLightingEnabled=false` 的语义：fail-closed 还是显式降级路径。
3. F 组 `ShaderHotReloadManager`：整体删除还是仅从生产路径移出（保留 dev 工具）。

## 9. Definition Of Overall Completion

下列全部为真时整体完成：

1. 所有生产 pass 的资源用 typed descriptor/access 声明，compiled plan 导出含 GI/HDR/SSBO/shadow/cluster/external target。
2. compiled plan 对生产者、读写、cycle、顺序、history 与 transition 给确定性诊断，正常计划保持批准顺序。
3. compute image/SSBO、attachment-to-sample、history ping-pong、external composite transition 有声明式同步；手工 barrier 生产调用点归零。
4. registry 覆盖引擎创建的全部 texture/FBO/SSBO/VBO/VAO/query/persistent mapping，无双重所有权。
5. profiler 不将未就绪 query 记为零，自动降级不混用 CPU fallback 与 GPU 数据。
6. ABI/binding check、include 修改、失败重试、capability fallback 与 GL diagnostics 有自动化测试。
7. 构建、unit、integration、CI、performance 和目标 GPU smoke 通过，旧 V3/V5 contract 入口持续兼容。

本设计不改变生产 NO-GO 状态；仅 M0-C 游戏二进制 artifact 可改变生产就绪判定。
