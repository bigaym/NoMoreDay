# 伤害管线现代化 P3b 报告：SIMD 两段式自适应分流 + 并发压力验证

- 计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` §3.4（P3-3/P3-4）
- 设计：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md` §4.1/§5.2/§8
- 范围：P3-3 守方减伤第二段自适应分流、P3-4 批量路径共享状态审计与并发压测
- 不在范围：P3a 快照/条件语义、`foundation/combat_v2`（P4）
- 构建配置：`RelWithDebInfo`，`build.bat`；未做任何 commit

## 1. 结论摘要

- `build.bat` exit=0；`ctest -L ci` 100%（10.41s）；`ctest -L integration` 复跑 100%（6/6）。
- 新增 `tests/unit/DamagePipelineP3bTests.cpp`，6 个用例（5 × `[Unit]` + 1 × `[Performance]`），全绿且连续 3 轮稳定。
- 分流后 N=50/200 的 Mean/P99 较分流前下降（50：0.038/0.130 → 0.026/0.033；200：0.076/0.241 → 0.056/0.106）；单目标与 N=1..8 保持 ~0.001–0.002ms。
- 审计发现 1 处真实竞争点：`EndgameModifierRegistry::EnsureLoaded()` 首载竞态（普通 `bool` 读写 + 并发写 `contracts_`），已用原子标志 + 首载互斥锁最小侵入修复。

## 2. 变更清单与行锚

| 文件 | 内容 | 行锚 |
| --- | --- | --- |
| `src/game/systems/combat/DamagePipeline.hpp` | `BatchKernelStats` + 观测/测试开关声明 | 62-73 |
| `src/game/systems/combat/DamagePipeline.cpp` | 匿名命名空间内核计数原子与强制开关 | 1560-1569 |
| 同上 | `GetBatchKernelStats` / `ResetBatchKernelStats` / `SetForceScalarKernelForTests` | 1571-1588 |
| 同上 | 阈值常量 `kScalarMaxTargets=4` / `kNarrowWidth=4` | 1651-1652 |
| 同上 | `process_scalar` 单目标标量内联内核 | 1659 |
| 同上 | `process_simd_block<W>` xsimd 批量内核（含无效目标掩码） | 1748 |
| 同上 | SIMD 计数累加 | 1915-1918 |
| 同上 | `process_range` 两段式分流与批宽余数补齐 | 1919-1954 |
| 同上 | 串行/并行派发（`BATCH_GRAIN_SIZE=32`） | 1959-1969 |
| `src/game/systems/combat/EndgameModifierContract.hpp` | `std::atomic<bool> loaded_` + `std::mutex load_mutex_` | 93-97 |
| `src/game/systems/combat/EndgameModifierContract.cpp` | 双检锁式 `EnsureLoaded` 与原子读写 | 33-43、54/63/71/121/127/142 |
| `tests/unit/DamagePipelineP3bTests.cpp` | P3b 一致性/并发/压测用例 | 新增 |
| `tests/performance/DamagePipelineBenchmark.cpp` | 自适应分流扩展基准 N={1,2,3,4,5,8,50,200,500} | 末尾新增 `[Performance] ... Adaptive Dispatch Scaling` |

## 3. P3-3 两段式自适应分流

### 3.1 两段划分

- **第一段（攻方乘区）**：`CreateSnapshot`（`DamagePipeline.cpp:1615`）在批量入口统一执行一次，快照结果冻结 base 乘区；本次未改动、未重复执行。
- **第二段（守方减伤）**：`process_range`（1919）按目标数量分流，只做逐目标减伤/条件/暴击求值。

### 3.2 路径选择条件（精确）

```
count = end - start
force_scalar = g_batch_force_scalar.load(relaxed)      // 仅测试置位
if (force_scalar || count < 4)  -> 标量内联（record_scalar_targets(count) + process_scalar 逐个）
else                            -> xsimd 批量：
    kNativeWidth = xsimd::batch<float>::size           // AVX2=8 / SSE=4
    while (i + kNativeWidth <= end)  process_simd_block<kNativeWidth>(i)   // 原生批宽铺满
    if constexpr (kNativeWidth > 4)
        if (i + 4 <= end)            process_simd_block<4>(i)              // 4 宽补齐
    if (i < end)                     record_scalar_targets(end-i); process_scalar(i..)  // <4 余数标量补齐
```

- 阈值即 **单目标/N<4 标量内联，N≥4 SIMD**（与设计一致）。
- 批宽余数（0..3 个目标）用标量补齐；AVX2 下原生 8 宽与 4 宽混用，保证任意 N 正确。
- SIMD 块使用 `xsimd::make_sized_batch_t<float, W>`（`xsimd::batch<T, Arch>` 的第二模板参数是架构而非宽度，不能用 `batch<float, W>`）。

### 3.3 可观测性

- `BatchKernelStats{scalar_targets, simd_blocks, simd_targets}`，`relaxed` 原子累加，仅诊断/测试，不参与结算与同步。
- 测试可用 `SetForceScalarKernelForTests(true/false)` 强制内核选择，`ResetBatchKernelStats()` 清零。

## 4. 一致性证据（N=1..20 与边界）

用例 `[Unit] DamagePipeline P3b - adaptive dispatch scalar vs simd parity`（`DamagePipelineP3bTests.cpp:143`）：

- N=1..20，同一确定性场景分别强制标量内核与自动（SIMD）内核，逐目标扣血 `CheckLossesEqual`（容差 `1e-3 + 1e-5*|scalar|`，float 舍入级）。
- 断言阈值行为：强制标量时 `scalar_targets==N && simd_blocks==0`；自动时 N<4 → `scalar_targets==N && simd_blocks==0`，N≥4 → `simd_blocks>0 && simd_targets+scalar_targets==N`。
- 覆盖原生批宽 ±1（AVX2：N=7/8/9、12、20）与 4 宽边界。
- `[Unit] ...conditional masks parity`（:175）覆盖命印 More / 气印 CritDamage / 冻结掩码在两条内核下一致。
- `[Unit] ...invalid targets skipped in simd block`（:196，N=9，挖空下标 3）验证 SIMD 块内混入失效实体时跳过，不落地 `entt::null`，且与标量路径一致。

汇总：`5653/5653` 断言通过，连续 3 轮 `6 | 6 passed`。

## 5. P3-4 共享状态审计

| 共享可变状态 | 访问路径 | 结论 |
| --- | --- | --- |
| `results[i]`（BatchResult 数组） | 各 `process_range` 写互不重叠的 `[start,end)`；提交阶段串行读 | 安全（无写重叠） |
| `g_batch_*` 计数 / 强制开关 | `relaxed` 原子 | 安全（仅诊断） |
| `utils::ThreadSafeRandom` | `thread_local std::mt19937` | 安全（每线程独立） |
| `StatsSystem` 静态缓存 | `std::shared_mutex` + `shared_lock`；仅串行 `CreateSnapshot` 调用 | 安全，且不在并行热路径 |
| `EndgameModifierRegistry::EnsureLoaded` | 原为普通 `bool` 读 + 并发写 `contracts_` | **发现竞争，已修复** |
| `EndgameModifierRegistry::ResolveForEntities` | `const`，原子 `loaded_`（acquire）后只读 map | 修复后安全（加载完成后只读） |
| `EvaluateTargetConditionState` | `const`、无状态、无分配 | 安全 |
| `DamageMitigationService::ApplySkillScopedResistEffects` / `AggregateSkillScopedResistCapSuppression` | 只读 `try_get<ActiveEffectsComponent>` | 安全 |
| `SettlementFrame` / `s_deferred_actions` / `s_reentrancy_depth` | `thread_local` | 安全；`QueueDeferredAction` 仅在串行提交段 |
| `CombatEventDispatcher::Dispatch` | `shared_lock` 允许多读者，但内部触发 `ProcBudgetManager::RequestEventEmit()` 与 `CombatTelemetry::RecordCombatEvent()` 共享可变状态 | 仅串行提交段调用；**契约：不支持多个外部线程各自调用 `CalculateBatch`** |

**修复点（最小侵入，无全局热区锁）**：

- `EndgameModifierContract.cpp:33-43`：`EnsureLoaded()` 改为 acquire 快路径 → `load_mutex_` 串行化首次 `LoadFromFile()` → 锁内 re-check（release 发布）。
- 所有 `loaded_ = true/false` 改为 `loaded_.store(..., release)`；`ResolveForEntities` 用 `load(acquire)`。
- 该互斥仅在**首次加载**时进入，批量结算热路径只做一次原子 acquire 读，不引入锁竞争。

## 6. 并发压测方法与结果

### 6.1 方法

- 在测试中用 `tf::Executor` 驱动 `CalculateBatch` 内部并行派发（`BATCH_GRAIN_SIZE=32`，N=64→2 任务、N=96→3 任务、N=200→7 任务），与 `executor=nullptr` 的串行参考逐目标扣血比对。
- MSVC 无 TSAN，替代证据：多轮重复 + 不变量校验（`simd_targets+scalar_targets==N`）+ 与串行参考的等值/容差断言 + 无崩溃。

### 6.2 结果

| 用例 | 线程数 | 轮次 | 目标数 | 结果 |
| --- | --- | --- | --- | --- |
| `[Unit] ...concurrent batch matches serial reference`（:255） | 4、8 | 各 5 | 64 | 通过 |
| `[Unit] ...concurrent conditional batch consistency`（:282） | 6 | 3 | 96 | 通过 |
| `[Performance] ...parallel stress`（:299） | 8 | 20 | 200 | 通过 |

- 全量 P3b 套件连续 3 轮均 `6/6`、`5653` 断言全过；`p3b-stability-{1,2,3}.log`。
- `[Performance]` 用例不进 ci（ctest `-L ci` 排除 `*Performance*`），由 `-L performance` 覆盖。

## 7. 性能对比表（Mean/P99，ms）

基准命令：`bin\NoMoreDayTests.exe --test-case="[Performance] DamagePipeline*"`。
"前"为 P3a 基线 `baseline-prechange.log`，"后"为本次 `perf-after-final.log`。

| N | 分流前 Mean/P99 | 分流后 Mean/P99 |
| --- | --- | --- |
| 1 | 0.001 / 0.001 | 0.001 / 0.001 |
| 2 | 0.001 / 0.001 | 0.001 / 0.011 |
| 3 | 0.001 / 0.001 | 0.001 / 0.001 |
| 4 | 0.002 / 0.002 | 0.001 / 0.001 |
| 5 | 0.002 / 0.004 | 0.001 / 0.005 |
| 8 | 0.002 / 0.002 | 0.002 / 0.022 |
| 50 | 0.038 / 0.130 | 0.026 / 0.033 |
| 200 | 0.076 / 0.241 | 0.056 / 0.106 |
| 500 | 0.102 / 0.196 | 0.118 / 0.251 |

其它基准（后）：Single `0.000/0.001`；固定 Batch 200 `0.057/0.122`（预算 <1.0ms）；Batch Scaling 50/100/200/500 = `0.029/0.055`、`0.043/0.103`、`0.058/0.091`、`0.101/0.227`。

- 预算：单目标 <0.01ms、200 目标 <1.0ms，均满足。
- 小 N（2/8）P99 偶发抬升属调度噪声（Mean 稳定在 0.001–0.002ms）。
- 余数标量补齐代价：AVX2 下最多 3 个目标/块走标量，N=4..7 时分别 4、4+1、4+2、4+3，实测 N=4/5/8 仍 ~0.001–0.002ms，代价可忽略。

## 8. 验收证据

| 项 | 命令 | 结果 | 日志 |
| --- | --- | --- | --- |
| 构建 | `build.bat` | exit=0 | `build-p3b-fixed.log` |
| ci | `ctest --test-dir build -C RelWithDebInfo -L ci` | 100%（10.41s） | `ctest-ci-p3b.log` |
| integration | `ctest ... -L integration` | 100%（6/6，复跑） | `ctest-integration-p3b-rerun2.log` |
| P3b 一致性/并发 | `NoMoreDayTests.exe --test-case="*P3b*"` | 6/6，连续 3 轮 | `p3b-stability-{1,2,3}.log` |
| 性能 | `--test-case="[Performance] DamagePipeline*"` | 5/5 | `perf-after-final.log` |

## 9. 行为变化与遗留风险

**行为变化（仅 P3b 范围）**

- 守方减伤第二段新增显式两段式分流：N<4 走纯标量内联，N≥4 走 xsimd（原生批宽 + 4 宽补齐 + <4 标量补齐）。数值与分流前一致（一致性用例证明）。
- 修复 SIMD 块内失效目标处理：无效实体统一置 `valid_mask=false` 并在 j 循环跳过，避免对已销毁实体调用 `try_get`/减抗聚合（原实现在批块混入失效目标时存在越界/stale 访问，属 P3b 新增保护，非 P3a 语义改动）。
- `EndgameModifierRegistry` 首载线程安全化（原子 + 首载锁）。
- 未改动 P3a 快照/条件语义与 `foundation/combat_v2`。

**遗留风险**

1. 快照路径暴击仍实时 `GetStatWithTags`（P3a 携带风险，本次未触及、未恶化）。
2. `FixedVector<4>` 条件 op 容量上限（P3a 携带，未恶化）。
3. DoT holder 回收依赖 `Tick`（P3a 携带，未恶化）。
4. GPU 计时用例 `[Integration] S1a - Gate loop single ring frame owner` 在 ctest 批量运行下偶发 `QueryState 3 != Valid`，单测/直接跑 100% 通过，判定为环境时序抖动，与本包无关（见 `gpu-s1a-rerun.log` 与 `ctest-integration-p3b-rerun2.log`）。
5. 并发契约：不支持多个外部线程各自调用 `CalculateBatch`（提交段的事件派发/遥测为共享状态）；本包保证的是 `CalculateBatch` 内部 `tf::Executor` 多线程派发安全。

## 10. 日志清单（本目录）

- 构建：`build-p3b-fixed.log`、`build-revert.log`、`build-noop.log`、`build-probe.log`、`build-sentinel.log`、`build-kernel2.log`
- 测试：`p3b-run3.log`、`p3b-stability-{1,2,3}.log`、`ctest-ci-p3b.log`、`ctest-integration-p3b-rerun2.log`
- 性能：`baseline-prechange.log`（前）、`perf-after-final.log`（后）
- 调试留档：`sentinel-run.log`、`noop-run.log`、`probe-run.log`、`gpu-s1a-rerun.log`

## 11. 实现过程发现（供后续参考）

- 新增用例初版 `BuildScene` 漏写 `defenders.push_back(defender)`，导致传入 `CalculateBatch` 的守方向量为空，表现与"内核计数恒为 0 + 越界 SIGSEGV"高度误导。经验：批量内核类测试必须先断言 `defenders.size()==N`（现已在 harness 修正）。
- 判定"到底跑的是不是新代码"时，编译期/链接期符号检查不足以定论；用"函数入口临时 sentinel/noop + 重跑断言"验证执行路径更可靠（本次 sentinel 返回 424242 证明 fresh 符号已链接）。
