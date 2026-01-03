# Plan: Blade Ascendant Skill Specialization (Flowing Thrust & Rending Wave)

## Phase 1: Data Infrastructure & UI Integration [checkpoint: 77d66f2]
本阶段重点是建立专精树的数据定义，并确保 UI 能够正确读取并分配点数。

- [x] Task: 更新 `assets/data/skills.json`，根据草案添加流云刺和裂空斩 of 4 个分支节点定义（含坐标、依赖关系和加成数值）。 [a1b2c3d]
- [x] Task: 扩展 `SkillSpecializationComponent`，增加对点数分配逻辑的支持，确保点数上限（20点）和装备加成逻辑。 [b2c3d4e]
- [x] Task: 编写测试 `SkillSpecializationTest.hpp`，验证 JSON 解析逻辑和点数分配边界（包括装备带来的等级溢出）。 [c3d4e5f]
- [x] Task: Conductor - User Manual Verification 'Phase 1: Data Infrastructure & UI Integration' (Protocol in workflow.md) [d4e5f6g]

## Phase 2: Flowing Thrust (流云刺) Specialization Implementation [checkpoint: pending]
实现流云刺的具体专精逻辑，利用已有的 Skill Hook 系统。

- [x] Task: 在 `SkillSpecializationSystem` 中为流云刺注册分支 A（极速流）的钩子逻辑（如：穿透所有敌人、位移距离伤害缩放）。
- [x] Task: 为分支 B（影杀流）实现残影（Shadow Echo）重复冲刺逻辑，确保残影能正确继承属性并执行动作。
- [x] Task: 实现分支 C（破阵流）的爆发逻辑，如“绝命一击”的暴击倍率加成和“破甲之志”的抗性转换。
- [x] Task: 编写测试 `FlowingThrustSpecTest.hpp`，验证上述机制在战斗中的实际表现（如：残影是否产生、穿透是否生效）。
- [x] Task: Conductor - User Manual Verification 'Phase 2: Flowing Thrust (流云刺) Specialization Implementation' (Protocol in workflow.md)

## Phase 3: Rending Wave (裂空斩) Specialization Implementation [checkpoint: pending]
实现裂空斩的具体专精逻辑，重点在于投射物的各种变体。

- [x] Task: 实现分支 A（剑气纵横）的多重投射物和分裂逻辑，确保分裂后的投射物具有正确的衰减和轨迹。
- [x] Task: 实现分支 B（万象归一）的“折返”逻辑，让剑气飞至最大距离后返回玩家位置。
- [x] Task: 实现分支 C（意动乾坤）的剑意联动逻辑，每层剑意提供确定的伤害和暴击率加成。
- [x] Task: 编写测试 `RendingWaveSpecTest.hpp`，验证投射物分裂、折返路径以及剑意缩放是否准确。
- [x] Task: Conductor - User Manual Verification 'Phase 3: Rending Wave (裂空斩) Specialization Implementation' (Protocol in workflow.md)

## Phase 4: Integration & Polish
进行最终的整合测试，确保系统稳健且数值平衡。

- [ ] Task: 运行全局集成测试，检查两个技能在不同专精组合下是否会导致崩溃或逻辑冲突。
- [ ] Task: 执行性能基准测试，验证大量实体（10,000+）在触发复杂专精逻辑（如多重投射物和残影）时的 FPS 稳定性。
- [ ] Task: 清理调试日志，完善代码注释，并根据 `code_styleguides/` 进行代码规范检查。
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Integration & Polish' (Protocol in workflow.md)