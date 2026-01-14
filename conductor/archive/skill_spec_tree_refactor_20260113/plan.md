# 实施计划：剑修技能专精树深度重构 (Plan)

## 阶段 1：基础工具与数据架构 (Foundation & Tooling)
本阶段建立数据同步管道和 C++ 层的数据承载结构。
- [x] **任务 1.1**: 开发 `scripts/sync_skills_spec.py`。实现从 `职业设计草案_剑修.md` 提取节点逻辑并注入 `skills.json` 的功能。
- [x] **任务 1.2**: 扩展 `skills.json` 模式。增加 `specialization_tree` 字段，支持 4 分支结构及行为 ID 映射。
- [x] **任务 1.3**: 重构 C++ `SkillComponent`。增加专精激活状态位图 (`bitset`) 和动态属性缓存池。
- [x] **任务 1.4**: 实现 `BehaviorInjectionRegistry`。建立行为 ID（如 `shadow_caster`）与组件注入逻辑的映射关系。
- [x] **任务 1.5**: - [x] Task: Conductor - User Manual Verification '阶段 1' (Protocol in workflow.md)

## 阶段 2：核心行为引擎实现 (Core Mechanical Systems)
实现支撑专精效果的底层通用系统，这些是构建复杂技能形态的“积木”。
- [x] **任务 2.1**: **残影系统 (Shadow System)**。实现 `ShadowEntity` 的创建、属性继承（30% 基础）以及动作镜像逻辑。
- [x] **任务 2.2**: **折返逻辑 (Boomerang Logic)**。实现投射物的距离检测、力场反转及玩家追踪移动组件。
- [x] **任务 2.3**: **自动化索敌 (Seeking Logic)**。为灵剑和心剑实现基于网格扫描的高频自动锁定组件。
- [x] **任务 2.4**: **水墨特效着色器**。在 `RenderSystem` 中为持有 `ShadowTag` 的实体实现淡雅水墨/墨痕渐变效果。
- [x] **任务 2.5**: - [x] Task: Conductor - User Manual Verification '阶段 2' (Protocol in workflow.md)

## 阶段 3：高频输出技能集成 (Melee & Projectile Integration)
完成最核心的直接攻击型技能重构。
- [x] **任务 3.1**: **流云刺 (Flowing Thrust)**。集成影杀流（残影）、破阵流（暴击）及形态转化。
- [x] **任务 3.2**: **裂空斩 (Rending Wave)**。集成多重投射、折返（Boomerang）及剑意增幅。
- [x] **任务 3.3**: **御剑回旋 (Blade Boomerang)**。集成牵引聚怪、多段切割及速度缩放逻辑。
- [x] **任务 3.4**: **万剑归宗 (Infinite Blades)**。集成引导组件与剑意爆发。
- [x] **任务 3.5**: **剑阵 (Sword Array)**。集成AoE场与减速/破甲/斩杀天赋。
- [x] **任务 3.6**: - [x] Task: Conductor - User Manual Verification '阶段 3' (Protocol in workflow.md)

## 阶段 4：自动化与防御技能集成 (Automation & Defensive Integration)
实现具有持续存在感和辅助功能的技能。
- [x] **任务 4.1**: **灵剑决 (Blade Formation)**。集成多重灵剑、自动攻击频率缩放及护体剑罡逻辑。
- [x] **任务 4.2**: **剑气护体 (Blade Ward)**。实现投射物拦截、格挡/闪避后的自动反击。
- [x] **任务 4.3**: **剑阵·诛仙 (Sword Array)**。集成AoE场与减速/破甲/斩杀天赋。
- [x] **任务 4.4**: - [x] Task: Conductor - User Manual Verification '阶段 4' (Protocol in workflow.md)

## 阶段 5：高阶与终极技能集成 (High-Complexity & Ultimate Integration)
实现逻辑最复杂的全屏与引导类技能。
- [x] **任务 5.1**: **万剑归宗 (Infinite Blades)**。实现全屏锁定算法、引导层数叠加及满层剑意爆发。
- [x] **任务 5.2**: **心剑·无影 (Mind Blade)**。实现高频引导叠加、神识锁定及智力联动逻辑。
- [x] **任务 5.3**: **绝影闪 (Phantom Flash)**。实现“看破”状态判定、反击瞬移及隐身逻辑。
- [x] **任务 5.4**: - [x] Task: Conductor - User Manual Verification '阶段 5' (Protocol in workflow.md)

## 阶段 6：全局整合与验证 (Integration & Final Verification)
- [x] **任务 6.1**: **属性流水线整合**。确保装备/套装提供的“残影伤害”等特殊修饰符能正确作用于专精实体。
- [x] **任务 6.2**: **性能压力测试**。验证万级实体下，大规模残影生成与回收的 CPU/GPU 占用率。
- [x] **任务 6.3**: **回归测试**。确保所有 9 个技能在不同专精组合下的逻辑一致性。
- [x] **任务 6.4**: - [x] Task: Conductor - User Manual Verification '阶段 6' (Protocol in workflow.md)