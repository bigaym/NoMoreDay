# Combat Proc Budget V1 — Specification

> Track ID: `combat_proc_budget_v1_20260225`  
> Series: CS-M2-04 | Priority: P1 | Milestone: M2

---

## 1. Overview

建立 `ProcBudgetManager`，对高频触发行为施加预算约束，防止无限放大路径。

## 2. Budget Dimensions

| 维度 | 单位 | 说明 |
|---|---|---|
| `life_on_hit_per_sec` | HP/s | 击中回血上限 |
| `mana_on_hit_per_sec` | MP/s | 击中回蓝上限 |
| `ailment_proc_per_sec` | count/s | 异常状态触发频率上限 |
| `trigger_proc_per_sec` | count/s | 触发技能频率上限 |
| `event_emit_per_frame` | count/frame | 每帧事件派发上限 |

## 3. Budget Strategy

1. 先扣预算再触发收益。
2. 超预算按策略降采样（probabilistic）或延迟（queue），不可无限放行。
3. 预算参数可配置化（来自 config 或合同定义）。

## 4. Acceptance Criteria

- [x] 高攻速 + 多召唤场景无指数收益爆炸。
- [x] 预算参数可配置（不硬编码）。
- [x] 超预算时行为可预测（日志可追踪降采样率）。
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 5. Risks

| Risk | Mitigation |
|---|---|
| 预算导致体感"削弱过猛" | 参数配置化 + 分层阈值（先软限后硬限） |
| 降采样策略选择影响公平性 | 使用确定性降采样（非随机），保证可复现 |
