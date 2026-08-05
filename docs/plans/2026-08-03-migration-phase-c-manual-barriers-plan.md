# Phase C: 其余 pass 手工 barrier 收敛

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.2
> **关闭债务:** RG-5（executor 依赖 legacy sync）
> **依赖:** Phase B（graph 已含 shadow/cluster edge 后，C 组 barrier 才有声明式落点）
> **状态:** [x] 已完成（含 1 处方法偏差，见 §9）

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.2；M0-B spec §3 声明式 barrier。
- **范围**: 收敛 18 处 `GPUUtils::MemoryBarrier` 生产调用点（2026-08-05 重新 grep 核实；早期审计的 25 处含行号漂移与重复统计），全部转为 graph transition 或 phase barrier 或显式 host 同步。
- **边界**: 不为性能删除正确性 barrier；host readback 同步保留为显式代码（非 graph barrier）。不改变渲染输出。

## 2. Verified Baseline

2026-08-05 复核，6 个 pass 内实际 18 处调用点：
RadianceCascades:395/478/487/531/628、OccluderExtract:219/254、GIComposite:381、LightingPass:350、FluidSimulation:323/355/384/415/500、JFAPass:394/450/509/564。
（早期审计的 RadianceCascades:789、OccluderExtract:409、GIComposite:425、LightingPass:291、FluidSimulation:311/343/372/403/488/832、JFAPass:835 均为行号漂移/模板调用重复，非独立直调点；LightingPass:291 与 ShadowResolve:218 是 `ApplyComputeToFragmentBarrierTemplate` 模板调用，不在本包直调统计内。）

## 3. Implementation Rationale

按 design §4.2 规则逐点三分类：

1. **跨 pass 依赖**（compute 生产者 → 下游 pass 消费，且资源已在 graph 声明）→ 删除；`CompiledRenderPlan` 经 `MapGlBarrierBits` 生成 transition barrier。
2. **同 pass 内同步**（compute 写 image/SSBO 后同 pass 后续读）→ 用 `AddPhaseBarrier(source, target, bits)` 声明，在 Execute 精确执行点经 `RenderContext::EmitPhaseBarrier(source, target)` 发出（**方法偏差**：见 §9）。
3. **host readback 同步**（`GetBufferSubData`/`ReadPixels`）→ 保留显式 barrier，标注非 graph barrier。

每处改完后验证该 pass 的资源访问已声明；若某资源尚未在 graph 且安全加 typed 声明会触发 read-before-write 校验失败 → 保留手工 barrier 并记录为 waiver。

## 4. Pseudocode Guidance

```text
for each site in [18 call sites]:
    classify(site):
        cross-pass -> remove; ensure producer/consumer typed access exists (else defer/waiver)
        same-pass  -> builder.AddPhaseBarrier(prevStage, nextStage, site.bits);
                      replace manual call with context.EmitPhaseBarrier(prevStage, nextStage)
                      at the exact execution point
        host-readback -> keep; wrap with comment "host readback sync (not a graph barrier)"
assert: GPUUtils::MemoryBarrier production call sites == 0
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| C1 | 审计 18 处（复核后）：按三分类建立 per-site 迁移表（文件:行 → 分类 → 处置） | - | [x] |
| C2 | RadianceCascades 5 处收敛（395/478/487/531/628，同 pass → AddPhaseBarrier+Emit） | B、C1 | [x] |
| C3 | OccluderExtract 2 处收敛（219 同 pass → Emit；254 跨 pass → 删除 + Write 改 Compute/StorageWrite） | B、C1 | [x] |
| C4 | GIComposite 1 处收敛（381 同 pass Compute→Fragment → Emit） | B、C1 | [x] |
| C5 | LightingPass 1 处收敛（350 跨 pass cluster SSBO → 删除 + 条件 Read 声明） | B、C1 | [x] |
| C6 | FluidSimulation 5 处收敛（323/355/384/415 同 pass Compute；500 Compute→Vertex → Emit） | B、C1 | [x] |
| C7 | JFAPass 4 处收敛（394/450/509 同 pass → Emit；564 跨 pass → 删除 + Write 改 Compute/StorageWrite） | B、C1 | [x] |
| C8 | 断言生产 `GPUUtils::MemoryBarrier` 调用点归零 + 回归（build/ctest/边界/legacy/diff 全绿） | C2-C7 | [x] |

## 6. Test Method

- **unit**: `RenderGraphValidationTest` 新增 2 个 Phase C 用例（`[Unit] RenderGraph - Phase C GI chain same-pass phase barriers + cross-pass transitions`、`[Unit] RenderGraph - Phase C LightingPass consumes cluster SSBOs via graph transitions`）：断言各 pass 的 phase barrier 声明进入 compiled plan、OccluderMask/DistanceField/cluster SSBO 的跨 pass transition（bits 0x28 / 0x2000）由 graph 发出、消费 pass 为生产 pass（LightingPass）。
- **integration**: 各 pass 对应集成测试 smoke（GI、JFA、fluid、shadow、lighting）。`RenderGraphV5ContractsIntegrationTest` 4 个含 LightingPass 的 partial-graph 用例增加 `ResetQualityTierConfigBaseline()`（消除 ClusteredLightingIntegrationTest 对 QualityTierManager 单例 config 的测试顺序泄漏）。
- **static**: grep 断言 6 个 pass 内 `GPUUtils::MemoryBarrier` 直调点归零（保留工具函数定义、RenderGraph 执行器 3 处、ShadowBuild:515 B2 fallback、LightCulling:391 B4 host readback）。
- **命令**:
  ```powershell
  cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2
  ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
  ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  git diff --check
  ```

## 7. Verification Of Task Completion

- C1: 18 处逐点分类表落地，无未分类项（见 §9 迁移表）。
- C2-C7: 各 pass 不再直调 `GPUUtils::MemoryBarrier`；同 pass 场景经 `AddPhaseBarrier`+`EmitPhaseBarrier`；跨 pass 场景经 graph transition；host readback（LightCulling:391）显式保留。
- C8: grep 6 pass 生产调用点 0；build 成功；ctest unit 9/9、integration 仅剩已知无关 GPUEntityLifecycleRegistryTest W5 失败；check_module_boundaries PASS；check_legacy_reintroduction PASS；`git diff --check` 通过（仅 CRLF 行尾警告）。
- 不提交任何 commit；handoff 如实报告。

## 8. Handoff Template

```text
package: phase-c-manual-barriers
source baseline: <commit>
files changed: ...
contract changed: 无新增枚举；AddPassLocalBarrier 生产开始被调用
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: M0-B spec §3 / debt RG-5
remaining risk or blocker: ...
```

## 9. Deviation And Evidence

### 9.1 方法偏差：同 pass 同步用 AddPhaseBarrier + EmitPhaseBarrier（而非 plan 字面的 AddPassLocalBarrier）

B2 合同（`RenderGraph.hpp` phase barrier 注释）与 Phase B 测试（`RenderGraphValidationTest.cpp` 断言「same-pass SDF barrier must NOT be represented by a pass-entry local barrier (which fires before Execute)」）明确：**pass-entry barriers（AddPassLocalBarrier）与跨 pass transition 都在 `pass->Execute` 之前发出，无法覆盖 Execute 内部同 pass 的 phase transition**。GL 语义要求 barrier 必须位于写 dispatch 与读 dispatch 之间。ShadowBuildPass 已确立先例（`AddPhaseBarrier(Compute, Fragment, bits)` + `if (!context.EmitPhaseBarrier(...)) fallback`）。因此 15 个同 pass site 改用 `AddPhaseBarrier` 声明 + `EmitPhaseBarrier` 在精确执行点发出；为支持该用法，`RenderContext::EmitPhaseBarrier` 改为 `const` 方法（只读 activeGraph 指针）。plan §3/§4/§6 的 AddPassLocalBarrier 字面按此偏差修正。

### 9.2 per-site 迁移表（2026-08-05 复核）

| site | 分类 | 处置 |
|---|---|---|
| RadianceCascades:395 | 同 pass Compute→Compute，Image\|TexFetch 0x28 | AddPhaseBarrier(Compute,Compute,0x28) + Emit 原位 |
| RadianceCascades:478（material 循环内） | 同 pass，原 bits Image 0x20 | Emit（得 0x28 超集，保守） |
| RadianceCascades:487 | 同 pass，0x28 | Emit 原位 |
| RadianceCascades:531 | 同 pass，0x28 | Emit 原位 |
| RadianceCascades:628（cascade 循环内） | 同 pass，0x28 | Emit 原位 |
| OccluderExtract:219 | 同 pass，0x28 | AddPhaseBarrier + Emit（RunExtractPass 两次调用均发） |
| OccluderExtract:254 | **跨 pass**（OccluderMask→JFA） | **删除**；Write(OccluderMask) 改 4 参 Compute/StorageWrite → transition=0x28 |
| GIComposite:381 | 同 pass Compute→Fragment，Image\|Buffer 0x220 | AddPhaseBarrier(Compute,Fragment,0x220) + Emit 原位 |
| LightingPass:350 | **跨 pass**（cluster SSBO→fragment） | **删除**；Setup 条件加 3 个 Read(Cluster*, LightCulling, Fragment, ShaderRead)（gated `v3Enabled && clusteredLightingEnabled`，与 Execute 运行时守卫一致）→ transition=0x2000；删除死成员 m_hasClusterSyncFrame/m_lastClusterSyncFrame 与 kGLShaderStorageBarrierBit |
| FluidSimulation:323/355/384/415 | 同 pass，Buffer 0x200 | AddPhaseBarrier(Compute,Compute,0x200) + Emit |
| FluidSimulation:500 | 同 pass Compute→Vertex，Buffer\|TexFetch 0x208 | AddPhaseBarrier(Compute,Vertex,0x208) + Emit |
| JFAPass:394/450/509 | 同 pass，Image\|Buffer\|TexFetch 0x228（超集，保守） | AddPhaseBarrier(Compute,Compute,0x228) + Emit |
| JFAPass:564 | **跨 pass**（DistanceField→RadianceCascades） | **删除**；Write(DistanceField) 改 4 参 Compute/StorageWrite → transition=0x28 |

### 9.3 waiver（无）

无 waiver。LightingPass cluster 读声明的 read-before-write 风险通过 gated 声明（v3&&clustered）消除：生产图在 v3&&clustered 时 LightCullingPass 必在 LightingPass 前（RenderSystem 顺序），且默认 config 下 gate 关闭，partial-graph 测试不受影响。

### 9.4 测试顺序泄漏修复

`ClusteredLightingIntegrationTest` 通过 `const_cast` 直接改写 QualityTierManager 单例 config（v3Enabled/clusteredLightingEnabled=true）且不还原，导致 `RenderGraphV5ContractsIntegrationTest` 的 partial-graph（含 LightingPass、无 LightCulling）在后续用例中触发 cluster read-before-write 校验错误。修复：在 `RenderGraphV5ContractsIntegrationTest` 4 个含 LightingPass 的图用例开头调用 `ResetQualityTierConfigBaseline()`（`ForceTier(Low)` 重建单例 config）。

### 9.5 验证证据（2026-08-05）

- build: `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功（NoMoreDayTests.exe 产出）。
- unit: `ctest -L unit` → 9/9 PASS；新增 Phase C 用例 `--test-case="*Phase C*"` → 2 cases / 59 assertions 全过。
- integration: `ctest -L integration` → 4/6 PASS；`nmd.tests.integration` 与 `nmd.tests.ai.integration` 仅剩 **known unrelated failure**：GPUEntityLifecycleRegistryTest W5（:197/:200/:334 `activeResourceCount` 回滚断言，与 Phase C 无关，未修复）。
- static: `check_module_boundaries.py` PASS（0 逆向边）；`check_legacy_reintroduction.py` PASS（220/70，无回归）；`git diff --check` 通过（仅工作树既有 CRLF 警告）。
- C8 grep：6 个 pass 内 `GPUUtils::MemoryBarrier` 直调点 = 0；剩余调用点均为禁改区（RenderGraph.cpp:585/593/650 graph 执行器、ShadowBuild:515 B2 fallback、LightCulling:391 B4 host readback、GPUUtils.cpp:151/155 定义、RenderSyncContracts.hpp:20 模板、GPUFlowField/GPULoot/GPUParticle/MDIRenderer/PersistentBuffer 非本包）。
