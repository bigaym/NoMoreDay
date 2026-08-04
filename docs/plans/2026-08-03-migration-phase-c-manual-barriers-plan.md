# Phase C: 其余 pass 手工 barrier 收敛

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.2
> **关闭债务:** RG-5（executor 依赖 legacy sync）
> **依赖:** Phase B（graph 已含 shadow/cluster edge 后，C 组 barrier 才有声明式落点）
> **状态:** [ ] 未开始

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.2；M0-B spec §3 声明式 barrier。
- **范围**: 收敛 25 处 `GPUUtils::MemoryBarrier` 生产调用点，全部转为 graph transition 或 pass-local barrier 或显式 host 同步。
- **边界**: 不为性能删除正确性 barrier；host readback 同步保留为显式代码（非 graph barrier）。不改变渲染输出。

## 2. Verified Baseline

25 处调用点：RadianceCascades:395/478/487/531/628/789、OccluderExtract:219/254/409、GIComposite:381/425、LightingPass:291/350、FluidSimulation:311/343/372/403/488/832、JFAPass:394/450/509/564/835。

## 3. Implementation Rationale

按 design §4.2 规则逐点三分类：

1. **跨 pass 依赖**（compute 生产者 → 下游 pass 消费，且资源已在 graph 声明）→ 删除；`CompiledRenderPlan` 经 `MapGlBarrierBits` 生成 transition barrier。
2. **同 pass 内同步**（compute 写 image/SSBO 后同 pass fragment 读）→ 用 `AddPassLocalBarrier(bits)` 声明。
3. **host readback 同步**（`GetBufferSubData`/`ReadPixels`/cluster 读回）→ 保留显式 barrier，标注非 graph barrier。

每处改完后验证该 pass 的资源访问已声明；若某资源尚未在 graph（例如 cluster buffer 在 B 组前）则推迟到 B 完成后再收敛。

## 4. Pseudocode Guidance

```text
for each site in [25 call sites]:
    classify(site):
        cross-pass -> remove; ensure producer/consumer typed access exists (else defer to B)
        same-pass  -> builder.AddPassLocalBarrier(MapBitsToLocal(site.bits))
        host-readback -> keep; wrap with comment "host readback sync (not a graph barrier)"
assert: GPUUtils::MemoryBarrier production call sites == 0
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| C1 | 审计 25 处：按三分类建立 per-site 迁移表（文件:行 → 分类 → 处置） | - | [ ] |
| C2 | RadianceCascades 6 处收敛 | B、C1 | [ ] |
| C3 | OccluderExtract 3 处收敛 | B、C1 | [ ] |
| C4 | GIComposite 2 处收敛 | B、C1 | [ ] |
| C5 | LightingPass 2 处收敛（配合 cluster 读声明） | B、C1 | [ ] |
| C6 | FluidSimulation 6 处收敛 | B、C1 | [ ] |
| C7 | JFAPass 5 处收敛 | B、C1 | [ ] |
| C8 | 断言生产 `GPUUtils::MemoryBarrier` 调用点归零 + 回归 | C2-C7 | [ ] |

## 6. Test Method

- **unit**: `RenderGraphValidationTest` 扩展：断言 pass 内 compute→fragment 同步经 `AddPassLocalBarrier` 生效（barrier 出现在 pass-local barrier 集合而非手工调用）。
- **integration**: 各 pass 对应集成测试 smoke（GI、JFA、fluid、shadow、lighting）。
- **static**: grep 断言 `GPUUtils::MemoryBarrier` 生产调用点归零（保留工具函数定义与 host 同步调用）。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  ```

## 7. Verification Of Task Completion

- C1: 25 处逐点分类表落地，无未分类项。
- C2-C7: 各 pass 不再直调 `GPUUtils::MemoryBarrier`（跨 pass 场景）；同 pass 场景经 `AddPassLocalBarrier`；host readback 显式保留。
- C8: grep 生产调用点 0；build 双成功；ctest 无新增失败；边界 71/71；`legacy` 扫描通过。
- 提交经用户授权；handoff 如实报告。

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
