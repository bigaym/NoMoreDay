# Combat Defense Contract V1 — Specification

> Track ID: `combat_defense_contract_v1_20260225`  
> Series: CS-M2-02 | Priority: P1 | Milestone: M2

---

## 1. Overview

建立统一的防御结算顺序合同，消除当前防御公式散落在多处的状态。

## 2. Defense Order Contract

固定顺序（不可跳步）：

1. **命中与闪避判定** — 闪避成功则整数伤害 = 0
2. **格挡判定** — 格挡成功按格挡率减免
3. **护甲/抗性减免** — 基于伤害类型应用对应抗性
4. **全局减伤与特殊减伤** — 如 Fortify、技能被动等
5. **屏障/护盾吸收** — 先消耗屏障再消耗 HP
6. **生命值结算与死亡判定**

## 3. Requirements

- 顺序在 `DamagePipeline` 的 Mitigation 阶段实现为单一函数链。
- 各步结果可通过调试开关输出到日志（`COMBAT_DEFENSE_DEBUG`）。
- 集成测试覆盖：护甲减免、抗性减免、屏障吸收各至少 1 个。

## 4. Acceptance Criteria

- [x] 防御顺序有单一实现且不可跳步。
- [x] 调试模式下各步减免值可追踪。
- [x] 集成测试覆盖 3 种防御机制。
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 5. Risks

| Risk | Mitigation |
|---|---|
| 旧代码中有顺序跳步的特判 | 审计后迁移，保留兼容开关 |
