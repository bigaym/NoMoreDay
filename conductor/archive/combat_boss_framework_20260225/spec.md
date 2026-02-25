# Combat Boss Framework — Specification

> Track ID: `combat_boss_framework_20260225`  
> Series: CS-M3-03 | Priority: P2 | Milestone: M3

---

## 1. Overview

建立 Boss 机制框架，支持多阶段 Boss 设计和标准化战斗合同交互。

## 2. Framework Components

### 2.1 Phase System
- Boss 支持多阶段（HP 阈值触发）。
- 每个阶段可变更行为模式/攻击模式/异常免疫。

### 2.2 Counter Windows
- 反制窗口计时精度 ≤ 1 帧。
- 窗口内特定行为（如打断/闪避）可获得奖励。

### 2.3 Failure Penalties
- 失败惩罚配置化（重试/弱化/传送等）。

### 2.4 Boss Ailment Interaction
- Boss 专属异常交互走 AilmentEngine 合同。
- Boss 可有特殊免疫/抗性配置。

## 3. Acceptance Criteria

- [ ] 至少 1 个多阶段 Boss 原型通过全流程验证。
- [ ] 反制窗口计时精度 ≤ 1 帧。
- [ ] Boss 专属异常走 AilmentEngine 合同。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 4. Risks

| Risk | Mitigation |
|---|---|
| Boss AI 复杂度导致帧时间飙升 | 状态机 + 预算约束 |
| 内容设计未成熟 | 框架先行，内容用原型验证 |
