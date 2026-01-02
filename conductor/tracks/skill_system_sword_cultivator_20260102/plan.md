# Plan: 剑修核心技能系统与星盘联动

## Phase 1: 基础设施与数据驱动 (Foundation & Data)
- [x] Task: 定义技能基础组件 (`SkillComponent`, `SkillSlotComponent`)
- [x] Task: 创建 `assets/data/skills.json` 规范并定义剑修原型技能数据
- [x] Task: 实现 `SkillRegistry` 类，负责从 JSON 加载并管理技能静态数据
- [x] Task: 编写单元测试：验证技能数据加载与标签解析的正确性
- [x] Task: Conductor - User Manual Verification 'Phase 1: Foundation & Data' (Protocol in workflow.md)

## Phase 2: 技能释放逻辑与输入集成 (Skill Execution & Input)
- [x] Task: 实现 `SkillSystem` 基础逻辑（输入映射、CD 计时、法力消耗）
- [x] Task: 建立技能释放状态机（准备 -> 施放钩子 -> 结算 -> 结束）
- [x] Task: 实现基础的施法动作同步（动画触发占位符）
- [x] Task: 编写测试：验证技能 CD 状态机和资源扣除逻辑
- [x] Task: Conductor - User Manual Verification 'Phase 2: Skill Execution & Input' (Protocol in workflow.md)

## Phase 3: 标签感知属性系统与伤害流水线增强 (Tag-Aware Stats & Pipeline)
- [x] Task: 扩展 `StatsSystem`，支持按标签查询属性加成（如 `GetStatWithTags("Damage", {"Physical", "Melee"})`）
- [x] Task: 更新 `DamagePipeline`，在计算环节动态注入技能实体的标签快照
- [x] Task: 实现“标签转换” (Tag Conversion) 逻辑（如 50% Physical -> Fire）
- [x] Task: 编写测试：验证拥有不同标签的技能是否能正确享受星盘中的对应加成
- [x] Task: Conductor - User Manual Verification 'Phase 3: Tag-Aware Stats & Pipeline' (Protocol in workflow.md)

## Phase 4: 剑修专属机制：剑意与影子施法 (Special Mechanics)
- [x] Task: 实现 `SwordIntentComponent` 及其积累/衰减系统
- [x] Task: 实现 `ShadowCasting` 框架：支持创建临时的残影实体执行技能逻辑
- [x] Task: 实现“击中回蓝” (Mana on Hit) 组件与结算逻辑
- [x] Task: 实现关键天赋 `SwordHeart` (剑心通明) 的属性倍率修正逻辑
- [x] Task: 编写测试：验证剑意堆叠数值和影子施法的实体生命周期管理
- [~] Task: Conductor - User Manual Verification 'Phase 4: Special Mechanics' (Protocol in workflow.md)

## Phase 5: 原型技能开发与综合测试 (Prototype Skills & Verification)
- [x] Task: C++ 实现“流云刺”钩子逻辑（位移量、击中判定）
- [x] Task: C++ 实现“裂空斩”钩子逻辑（剑气投射物生成）
- [x] Task: 集成测试：在游戏主循环中验证完整战斗链路（技能 -> 标签加成 -> 伤害输出 -> resource 恢复）
- [x] Task: 性能压测：在大规模实体环境下验证影子施法的内存/计算开销
- [x] Task: Conductor - User Manual Verification 'Phase 5: Prototype Skills & Verification' (Protocol in workflow.md)
