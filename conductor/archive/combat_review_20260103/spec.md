# Specification: Combat System Logic Integrity Review

## 1. Overview
本任务旨在对战斗系统的核心逻辑进行深度审阅和验证，确保伤害计算、Buff 处理、技能专精机制以及属性快照（Snapshotting）在代码实现层面上与设计文档严格一致。通过完善单元测试和静态代码审查，消除潜在的逻辑缺陷。

## 2. Functional Requirements
- **伤害流水线 (Damage Pipeline) 校验**:
    - 验证基础伤害、加成系数（Increased）、乘法修正（More）的计算顺序。
    - 验证护甲与抗性对最终伤害的削减逻辑。
- **Buff 与状态系统校验**:
    - 验证 Buff 堆叠层数的增减与刷新逻辑。
    - 验证 Buff 对 `CombatStats` 的动态属性修正是否即时生效。
- **剑修 (Blade Ascendant) 专精逻辑校验**:
    - 审查技能树中复杂机制（如影子施法触发、剑意动态消耗）的实现逻辑。
- **数值快照 (Snapshotting) 机制校验**:
    - 确保投射物（Projectile）和持续伤害（DoT）在产生瞬间能够正确捕获当前的属性状态。

## 3. Verification & Quality Assurance
- **自动化单元测试**:
    - 在 `tests/` 目录下新增或更新针对上述逻辑的测试用例。
    - 使用断言（Assertions）验证特定输入下的数值计算输出是否符合预期。
- **静态代码审查**:
    - 逐行审阅 `src/systems/CombatSystem.cpp`、`DamagePipeline.cpp` 和 `BuffSystem.cpp`。
    - 对照 `设计文档/战斗系统与属性设计.md` 检查代码实现的一致性。

## Acceptance Criteria
- [ ] 所有新增的战斗逻辑单元测试通过且无回归错误。
- [ ] 完成一份静态代码审查报告，记录发现的逻辑偏差并进行修复。
- [ ] 确保快照机制在多线程环境（Taskflow 架构）下的原子性或正确性。

## 5. Out of Scope
- 本任务不涉及战斗性能（如 10k 实体下的 FPS）的优化。
- 不涉及新的视觉特效（VFX）或音效实现。
