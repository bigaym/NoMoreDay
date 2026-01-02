# Specification: 技能 UI 与 Buff 显示系统 (Skill UI & Buff Display System)

## 1. 概述 (Overview)
本 Track 旨在完善游戏的战斗 HUD (Heads-Up Display)，提供直观的技能状态反馈、资源监控和 Buff 系统。通过在屏幕底部建立固定的技能栏以及在资源条上方显示状态效果，提升玩家在战斗中的信息获取效率。

## 2. 功能需求 (Functional Requirements)

### 2.1 技能快捷栏 (Skill Hotbar)
- **布局**: 在屏幕底部中央显示 5 个固定槽位（对应 Q, W, E, R, RMB）。
- **技能绑定**: 自动读取 `ActiveSkillsComponent` 中的槽位数据并显示对应图标。
- **状态反馈 (Radial Wipe)**:
    - **冷却 (Cooldown)**: 使用顺时针旋转的半透明黑色遮罩表示剩余冷却时间。
    - **资源不足 (Insufficient Mana)**: 当法力值不足以释放技能时，图标整体呈现蓝色或暗淡色调。
- **资源与充能显示**:
    - **蓝耗数字**: 在技能图标左下角显示蓝色小字，实时反映该技能的法力消耗。
    - **可用次数/充能 (Charges)**: 对于可充能技能，在图标右下角显示当前可用的次数。

### 2.2 Buff/Debuff 显示系统 (Buff/Debuff Display)
- **位置**: 居中显示于生命/法力资源条上方。
- **图标设计**:
    - 采用“文字 + 箭头”方案（例如：攻击力增益显示为“攻 ↑”，暴击率减益显示为“暴 ↓”）。
    - 建立静态映射表存储不同属性与其对应文字、箭头的关系。
- **视觉增强**:
    - **计时器**: 显示剩余持续时间的数字倒计时或圆形遮罩。
    - **层数 (Stacks)**: 在图标角落显示当前堆叠层数。
    - **边框颜色**: 增益效果 (Buff) 使用绿色/金色边框，减益效果 (Debuff) 使用红色/紫色边框。

### 2.3 交互与悬停提示 (Interaction & Tooltips)
- **技能悬停提示 (Skill Tooltips)**: 鼠标悬停时显示包含名称、详细描述、标签、伤害倍率、蓝耗、冷却的信息框。
- **动态星盘联动**: Tooltip 需动态显示受当前星盘天赋加成后的属性变化（例如：显示受星盘加成后的总伤害加成）。
- **Buff 悬停提示**: 鼠标悬停在 Buff 图标上显示该效果的名称、来源及详细描述。

## 3. 技术细节 (Technical Details)
- **UI 渲染**: 继承 `UIRenderer` 的零分配绘制原则。
- **数据管理**:
    - 实现 `BuffRegistry` 进行图标/文本的静态映射。
    - 扩展 `UISystem` 逻辑以支持动态读取 `AstrolabeComponent` 的加成数据并注入 Tooltip。
- **ECS 集成**: 实时轮询 `ModifierList`、`ActiveSkillsComponent` 以及新增的 `ActiveEffectsComponent`。

## 4. 验收标准 (Acceptance Criteria)
- [ ] 底部 5 个技能槽位能够正确显示绑定的技能图标。
- [ ] 技能冷却遮罩、左下角蓝耗数字、右下角充能次数显示正确。
- [ ] Buff 栏显示正确的“文字+箭头”图标，且颜色边框与增益/减益类型匹配。
- [ ] 技能提示框 (Tooltip) 能正确显示技能基础数据及星盘提供的额外加成。
- [ ] 性能测试：HUD 更新在 10,000+ 实体环境下保持极低开销。

## 5. 超出范围 (Out of Scope)
- 技能按键的自定义改键功能。
- 复杂的 Buff 粒子特效。
