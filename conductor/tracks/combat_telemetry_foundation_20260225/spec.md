# Combat Telemetry Foundation — Specification

> Track ID: `combat_telemetry_foundation_20260225`  
> Series: CS-M2-05 | Priority: P1 | Milestone: M2

---

## 1. Overview

建立战斗遥测基础设施，为后续 Anti-Meta 和 Release Gate 提供数据支撑。

## 2. Metrics Coverage (Improvement Plan §6.2)

| 类别 | 指标 | 采集方式 |
|---|---|---|
| DamagePipeline | avg / p95 / p99 计算耗时 | 嵌入式计时器 |
| StatsSystem | `GetStatWithTags` 调用次数、缓存命中率、锁等待 | 计数器 + 滑动窗口 |
| CombatEventDispatcher | 每帧总量与类型分布 | 按帧计数 |
| Trigger | 拦截率、深度分布、每秒调用量 | Guard 钩子统计 |
| Summon | 单位数、目标切换频率、事件量、预算命中率 | 按帧采样 |

## 3. Requirements

- 指标采集对战斗帧时间影响 < 0.1ms。
- 指标可输出到日志/终端（开发阶段）。
- 后续可扩展为持久化存储或实时面板。
- 采集可通过编译开关或运行时 flag 关闭。

## 4. Acceptance Criteria

- [ ] 核心指标面板可输出到日志/终端。
- [ ] 指标采集开销 < 0.1ms。
- [ ] 覆盖上表 5 类指标。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 5. Risks

| Risk | Mitigation |
|---|---|
| 采集开销影响性能 | 使用 thread_local 计数器，避免锁竞争 |
| 指标过多导致日志噪音 | 分级输出，默认只输出摘要 |
