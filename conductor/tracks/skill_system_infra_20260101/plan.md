# 实施计划 (plan.md) - 职业与技能系统基础设施

## 阶段 1：标签注册表与自动化工具 (Tag Registry & Automation) [checkpoint: 0d72ec2]
*目标：建立基于位掩码的标签系统，并通过 Python 脚本实现自动化维护。*

- [x] **任务 1.1**：创建 `assets/data/tags.json` 定义基础标签（物理、火焰、近战、投射物等）。 [7b90b7d]
- [x] **任务 1.2**：编写 `scripts/gen_tags.py` 脚本，将 JSON 转换为 C++ 头文件（生成 `enum class Tag : uint64_t` 和反射名称）。 [2a7c5e1]
- [x] **任务 1.3**：在 CMake 中配置该脚本的自动调用，并集成生成的头文件到项目中。 [4b7ff4c]
- [x] **任务 1.4**：编写 `TagSystemTest.hpp`，验证位掩码匹配和字符串转换逻辑。 [e07a3ec]
- [ ] **任务 1.3**：在 CMake 中配置该脚本的自动调用，并集成生成的头文件到项目中。
- [ ] **任务 1.4**：编写 `TagSystemTest.hpp`，验证位掩码匹配和字符串转换逻辑。
- [ ] Task: Conductor - User Manual Verification '阶段 1：标签注册表' (Protocol in workflow.md)

## 阶段 2：数据结构与修饰符定义 (Data Structures & Modifiers) [checkpoint: c387ce8]
*目标：定义伤害池和多层修饰符的数据模型。*

- [x] **任务 2.1**：实现 `DamagePool` 结构体，使用固定数组存储多类型点伤，并提供标签过滤方法。 [c745968]
- [x] **任务 2.2**：定义 `DamageModifier` 结构体，支持 `Convert`, `GainExtra`, `Increased`, `More` 等操作类型。 [c745968]
- [x] **任务 2.3**：实现 `SkillModifierComponent` 和 `GlobalModifierComponent` 用于 ECS 实体存储。 [c745968]
- [x] **任务 2.4**：编写单元测试验证 `DamagePool` 的合并与清除逻辑。 [c745968]
- [ ] Task: Conductor - User Manual Verification '阶段 2：数据结构' (Protocol in workflow.md)

## 阶段 3：伤害计算流水线实现 (Damage Pipeline Implementation)
*目标：实现五步伤害计算算法。*

- [ ] **任务 3.1**：实现 `DamagePipeline` 类的骨架，并集成到 `CombatSystem` 中（或作为独立工具类）。
- [ ] **任务 3.2**：编写 `DamagePipelineTest.hpp`，预设复杂的转换场景（例如：50% 物理转火焰，应用两层加成）。
- [ ] **任务 3.3**：实现第一步（基础池）和第二步（转换/附加）逻辑。
- [ ] **任务 3.4**：实现第三步（Increased）和第四步（More）逻辑，确保修饰符按优先级应用。
- [ ] **任务 3.5**：实现第五步（最终结算），集成暴击和防御占位。
- [ ] Task: Conductor - User Manual Verification '阶段 3：流水线逻辑' (Protocol in workflow.md)

## 阶段 4：性能优化与回归测试 (Optimization & Final Verification)
*目标：确保零分配性能并完成全系统集成。*

- [ ] **任务 4.1**：对 `DamagePipeline` 进行性能剖析，确保核心循环中无动态内存分配。
- [ ] **任务 4.2**：运行所有现有的战斗测试（CombatSystemTest），确保新系统引入后无功能倒退。
- [ ] **任务 4.3**：更新项目文档，记录标签定义规则和流水线计算逻辑。
- [ ] Task: Conductor - User Manual Verification '阶段 4：最终验收' (Protocol in workflow.md)
