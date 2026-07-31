# M0-B R1/R2 Legacy Access 收敛审计 Inventory

> **Track ID**: `gpu_rendergraph_resource_foundation_20260726`
> **来源**: P0 渲染整改实施方案 S5（`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S5）
> **审计基线**: HEAD `beace9f`（2026-07-31），本次 S5 收敛改动为工作区未提交变更
> **状态**: 收敛完成 — 0 个未处置 string-based 使用点；0 个已批准例外；0 个待批准例外
> **默认拒绝策略**: 生产路径一律禁止 string-based `Read`/`Write`/`DeclareResource`；不得以名称回退或手工 barrier 旁路（见 spec.md §2/§3 与 plan.md 退出标准）。

---

## 1. 审计范围与方法

- **范围文件**：
  - `src/engine/render/graph/RenderGraph.hpp` / `RenderGraph.cpp`（builder/validation/compiled-plan 全部访问入口与合同校验）
  - `src/engine/render/passes/*.cpp`（全部 21 个 pass 的 `Setup(RenderGraphBuilder&)` 声明点）
  - `tests/unit/RenderGraphValidationTest.cpp`、`tests/integration/RenderGraphV3ContractsIntegrationTest.cpp`、`tests/integration/RenderGraphV5ContractsIntegrationTest.cpp`、`tests/performance/RenderGraphContractBenchmark.cpp`（使用点审计）
- **枚举方法**：
  - `builder\.(Read|Write|DeclareResource)` 全量 grep，逐一核对参数形式（string 字面量 / `std::string` 变量 / `RenderResourceTag` 枚举 / `TypedPassAccess` / `TypedResourceDescriptor`）。
  - 对每个使用点判定：调用形式、携带的 Tag/Owner/Stage/Usage/stableResourceId 信息、barrier/transition 语义是否与 typed 等价。
- **关键结论**：生产 `src/` 内 **0 个** string-based 调用点（38 个 `Read`/`Write` 与 6 个 `DeclareResource` 全部为 typed 形式）；测试内 string 调用仅存于 `tests/unit/RenderGraphValidationTest.cpp`，其中 1 处需收敛使用点（S0 stable-id 测试）已改 typed，另 2 处为 deny 负例夹具（见 §4.1/§5）。

## 2. 默认拒绝策略与 fail-closed 行为

| 规则 | 实现 | 证据 |
| --- | --- | --- |
| string-based `Read`/`Write` 默认拒绝 | `RenderGraphBuilder::Read/Write(const std::string&)` 设置 `ResourceAccess::isStringBasedAccess=true`；`RenderGraph::RejectLegacyStringAccess` 在 `Build` **无条件**执行（不受 NDEBUG、`s_validationEnabled` 影响），对任何 string-based access 追加 `Severity::Error` 诊断并返回拒绝（`RenderGraph.cpp:572`） | 诊断文本：`string-based access is denied by default; declare typed access via Read/Write(Tag, Owner, Stage, Usage)` |
| 不得以名称回退 | 拒绝不再依赖 `ToResourceTag(name)` 推断身份；`Custom` 名资源同样被拒，杜绝 name-drift 空洞 | `RenderGraph.cpp:572-588`（`RejectLegacyStringAccess`，按 `isStringBasedAccess` 全量扫描） |
| 不得以手工 barrier 旁路 | 本任务不新增/移除任何 `MemoryBarrier`/`AddPassLocalBarrier` 调用；typed 收敛点 barrier 位与收敛前逐位一致（见 §4） | `git diff` 仅改调用形式与校验，未触碰 `Execute` barrier 逻辑 |
| 已知资源 owner 合同 | typed access 仍受既有 `IsWriterAllowedForResource`/`IsFirstWriterValid`/`read-before-write` 等合同约束（未改动） | `RenderGraph.cpp` ValidateBuildContracts |
| 失败传播 | 任一 string-based access → `m_hasValidationErrors=true` 且 `Build` **无条件抛** `std::logic_error`（`RenderGraph.cpp:440-447`，不受 NDEBUG、`s_validationEnabled` 影响）；发布构建同样拒绝执行，`CompiledRenderPlan` 仍保留诊断 | `RenderGraph.cpp:407-458` |
| 回归测试 | `tests/unit/RenderGraphValidationTest.cpp:114` `[Unit] RenderGraph - string-based access is denied by default`（Write+Read 各一，断言 `Build` 无条件抛 `std::logic_error`） | focused 测试 31/31 cases, 217/217 assertions 通过 |

## 3. string-based API 面（`RenderGraph.hpp/.cpp`）处置

| 条目 | 位置 | 当前状态 | 处置结论 |
| --- | --- | --- | --- |
| `RenderGraphBuilder::Read(const std::string&)` | `RenderGraph.hpp:267` / `RenderGraph.cpp:270` | 存在；生产 0 调用方 | **fail-closed 保留**：仅作 V3/V5 旧 contract 源码兼容入口（plan.md Task 1.3「保留旧 contract 作迁移诊断」）；任何运行期调用即被 §2 校验拒绝。不新增调用方。 |
| `RenderGraphBuilder::Write(const std::string&)` | `RenderGraph.hpp:268` / `RenderGraph.cpp:280` | 存在；生产 0 调用方 | **fail-closed 保留**：同上。 |
| `ResourceAccess::isStringBasedAccess` | `RenderGraph.hpp:202` | 新增标记字段 | 由两个 string overload 置位，validation 依据其拒绝。 |
| `ToResourceTag`/`ToResourceName` | `RenderGraph.hpp:96/56` | 保留 | 仅供 typed `RenderResourceTag` 枚举<->规范名映射与诊断使用；不再作为身份推断入口。 |

> 说明：string overload 不删除，避免破坏 V3/V5 旧 contract 源码兼容（spec.md §4「旧 V3/V5 contract 入口持续兼容」）；「默认拒绝」由 Build 期 fail-closed 校验强制执行，而非仅靠约定。

## 4. 全部使用点枚举与处置（覆盖 0 遗漏）

### 4.1 string-based 使用点（总 1 处需收敛，收敛 1 处）

> deny 测试（`RenderGraphValidationTest.cpp:120/124`）另有 2 处 string 调用，为**负例夹具**（验证无条件 fail-closed），不计入需收敛使用点。

| # | 位置 | 收敛前 | 收敛后 | 行为等价性 | 处置 |
| --- | --- | --- | --- | --- | --- |
| S-1 | `tests/unit/RenderGraphValidationTest.cpp:415`（S0 stable pass id determinism，lambda 内 `builder.Write(name + "Color")`） | string-based `Write`，resource=`<PassName>Color`，`ToResourceTag` 推断为 `Custom`，`ownerTag=Unknown`，stage/usage 默认 `FramebufferAttachment`/`ColorAttachment`，`stableResourceId=StableResourceId(name+"Color")` | typed `TypedPassAccess`：`resourceName=name+"Color"`、`mode=Write`、`stage=FramebufferAttachment`、`usageFlags=ColorAttachment`、`stableResourceId=StableResourceId(name+"Color")`，经 `builder.Write(access)` 提交 | 完全等价：同一 resourceName、同一 stableResourceId、同一 stage/usage/mode；`ValidateBuildContracts` 对 `Custom` 资源判定路径相同；compiled plan 资源/edge/transition 不变 | **已收敛**（`tests/unit/RenderGraphValidationTest.cpp:429`） |

### 4.2 生产 pass 访问使用点（总 38 处：Read 19 + Write 19，全部 typed，无需收敛）

每个使用点均携带 Build 期明确的 Tag/Owner/Stage/Usage，`Read(Tag,Owner)`/`Write(Tag,Owner)` 两参形式使用默认 stage/usage（Read→`Fragment`/`ShaderRead`，Write→`FramebufferAttachment`/`ColorAttachment`），与显式四参形式语义一致。

| pass | 位置（Read） | 位置（Write） | Tag/Owner/Stage/Usage | 处置 |
| --- | --- | --- | --- | --- |
| ScenePass | — | `:21` SceneHdrColor/Scene/FBA/ColorAttachment; `:25` SceneDepth/Scene/FBA/DepthAttachment | typed | 已收敛（原本 typed） |
| LightingPass | `:52` SceneHdrColor/Lighting/Fragment/ShaderRead | `:56` SceneHdrColor/Lighting/FBA/ColorAttachment | typed | 已收敛 |
| HeightShadowPass | `:44` SceneHdrColor/HeightShadow | `:46` SceneHdrColor/HeightShadow | typed | 已收敛 |
| OccluderExtractPass | — | `:77` OccluderMask/OccluderExtract | typed | 已收敛 |
| JFAPass | `:118` OccluderMask/OccluderExtract | `:120` DistanceField/JFA | typed | 已收敛 |
| RadianceCascadesPass | `:54` SceneHdrColor/RadianceCascades; `:56` DistanceField/JFA | `:58` EmissiveBuffer/RadianceCascades; `:60` RadianceMap/RadianceCascades | typed | 已收敛 |
| GICompositePass | `:52` SceneHdrColor/GIComposite; `:54` RadianceMap/RadianceCascades | `:56` SceneHdrColor/GIComposite | typed | 已收敛 |
| FluidSimulationPass | `:63` SceneHdrColor/FluidSimulation; `:65` DistanceField/FluidSimulation; `:67` RadianceMap/FluidSimulation | `:69` SceneHdrColor/FluidSimulation | typed | 已收敛 |
| VolumetricLightPass | `:44` SceneHdrColor/Volumetric | `:46` SceneHdrColor/Volumetric | typed | 已收敛 |
| VFXPass | `:22` SceneHdrColor/VFX/Fragment/ShaderRead; `:26` SceneDepth/VFX/Fragment/ShaderRead | `:30` SceneHdrColor/VFX/FBA/ColorAttachment | typed | 已收敛 |
| GPUTextPass | `:14` SceneHdrColor/VFX | `:16` SceneHdrColor/VFX | typed | 已收敛 |
| GPULootPass | `:14` SceneHdrColor/VFX | `:15` SceneHdrColor/VFX | typed | 已收敛 |
| UIWorldPass | `:12` SceneHdrColor/UIWorld | `:14` SceneHdrColor/UIWorld | typed | 已收敛 |
| PostProcessPass | `:66` SceneHdrColor/PostProcess/Fragment/ShaderRead | `:70` PostProcessLdrColor/PostProcess/FBA/ColorAttachment | typed | 已收敛 |
| DistortionPass | `:53` PostProcessLdrColor/Distortion | `:55` DistortionLdrColor/Distortion | typed | 已收敛 |
| CompositePass | `:25` `Read(m_inputResourceTag, m_inputOwnerTag, Fragment, ShaderRead)`（构造期传入的 typed Tag/Owner） | `:28` FinalOutputColor/Composite/FBA/ColorAttachment | typed | 已收敛 |

### 4.3 `DeclareResource` 使用点（总 6 处，全部 `TypedResourceDescriptor`，typed）

| pass | 位置 | name/tag/kind/format/lifetime | 处置 |
| --- | --- | --- | --- |
| ScenePass | `:19` | SceneColor/SceneHdrColor/Texture2D/RGBA16F/Transient | typed，已收敛 |
| JFAPass | `:129` | JFASeedField/Custom/Texture2D/RG16F/Transient | typed，已收敛 |
| JFAPass | `:138` | DistanceFieldSubresource/DistanceField/Texture2D/R16F/Persistent | typed，已收敛 |
| PostProcessPass | `:64` | PostProcessLdrColor/PostProcessLdrColor/Texture2D/R8/Transient | typed，已收敛 |
| VFXPass | `:20` | VFXParticleSSBO/VFXParticleSSBO/StorageBuffer/R32F/Persistent | typed，已收敛 |
| CompositePass | `:23` | FinalOutputColor/FinalOutputColor/Framebuffer/R8/External | typed，已收敛 |

### 4.4 未声明 graph 访问的 pass（不属于 string-based 使用点，记录备查）

以下 pass `Setup` 未声明任何 graph access（`(void)builder;`），其同步仍为 Execute 内原始 GL/手工 barrier，属 debt_register RG-5「executor 仍依赖 legacy sync 行为」范围，非 S5 string-access 收敛范围：

- `LightCullingPass`（`LightCullingPass.cpp:42`）
- `ShadowPreparePass`（`ShadowPreparePass.cpp:23`）
- `ShadowBuildPass`（`ShadowBuildPass.cpp:42`，Execute 内手工 `MemoryBarrier`）
- `ShadowResolvePass`（`ShadowResolvePass.cpp:32`，Execute 内 `ApplyComputeToFragmentBarrierTemplate`）
- `VFXEmissionSnapshotPass`（`VFXEmissionSnapshotPass.cpp:12`）

## 5. 覆盖统计

| 指标 | 值 |
| --- | --- |
| string-based 使用点总数 | **1** |
| 已收敛（改 typed access） | **1**（S-1） |
| 已批准例外 | **0** |
| 待批准例外 | **0** |
| 未处置使用点 | **0** |
| 生产 pass typed 访问使用点（审阅覆盖） | 38（Read 19 + Write 19） |
| 生产 pass typed `DeclareResource` 使用点（审阅覆盖） | 6 |
| pass 总数 | 21（16 有 typed 访问 / 6 有 typed descriptor / 5 无声明访问） |
| 收敛后 string-based 调用点（生产） | **0**；测试另有 2 处为 deny 负例夹具（`RenderGraphValidationTest.cpp:120/124`） |

## 6. 例外登记

本任务**未批准任何例外**。string overload 作为「fail-closed 保留的旧 contract 入口」存在，不属于例外（任何运行期调用即失败）。若未来确有生产用例必须使用 string 形式，须由 M0-B spec 批准并补记 owner/原因/到期版本/测试/fail-closed 行为，遵循「默认拒绝」约定。

## 7. 验证证据

| 验证 | 结果 |
| --- | --- |
| `python scripts/check_module_boundaries.py` | 71/71 PASS（20 files） |
| `cmd.exe /c build.bat check` | 全部前置检查通过（legacy/version marker、模块边界、ABI、skill contract、assets 等） |
| `cmd.exe /c build.bat`（完整构建） | `[Build] Build completed successfully.` + `[Build] All steps completed successfully` 双标记存在（日志 `%TEMP%\opencode\s5fix-build.log`） |
| `bin\NoMoreDayTests.exe --test-case="*RenderGraph*"` | 31/31 cases, 217/217 assertions passed（`Status: SUCCESS!`；deny 用例断言 `Build` 无条件抛 `std::logic_error`） |
| `ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure` | 15 tests, 12 passed, 3 failed：`nmd.tests.integration`（既有 `[Integration] GI - Long-run Stability Proxy` 硬件读回失败）+ `nmd.tests.unit`/`nmd.tests.skill.unit`（既有 `HeavenlySwordClosureTests.cpp:97` skill 数据失败，与渲染无关）；均接受 |
| `git diff --check` | 无 whitespace 错误（exit 0；LF/CRLF 警告为仓库既有行为） |
| `python scripts/check_legacy_reintroduction.py` | PASS：无 marker/classification 回归（未增加 `legacy` 等四支柱 marker） |

## 8. 剩余风险

- **低**：string overload 仍存在于 API 面；其调用由 `Build` 期**无条件 fail-closed**（`RejectLegacyStringAccess`，不受 NDEBUG、`s_validationEnabled` 影响，抛 `std::logic_error` 拒绝执行），故运行期误用必然失败。「生产 0 调用方」目前靠 grep 审计保障，若需更强约束可后续编译期删除（会破坏 V3/V5 旧 contract 源码兼容，须按 spec §4 决策）。
- **已知（非本任务范围）**：RG-3/RG-5（未声明访问的 shadow/lightculling pass 与 executor 手工 barrier）仍是 M0-B 未闭合债务，与 string access 无关，见 `debt_register.md`。
- 本任务未触碰 `GPUResourceRegistry`/`GPUHardwareValidationGate`（S4 并行）、`GPUTimerQueryRing`/`RenderProfiler`/`RenderSystem`（S1b）、CMake/build.bat/PCH。
