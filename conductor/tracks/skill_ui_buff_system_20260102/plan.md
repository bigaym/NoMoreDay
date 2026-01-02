# Plan: 技能 UI 与 Buff 显示系统 (Skill UI & Buff Display System)

## Phase 1: 基础设施与静态数据 (Infrastructure & Data) [checkpoint: b495878]
- [x] Task: 定义 `ActiveEffectsComponent` 用于追踪当前激活的 Buff/Debuff 生命周期 [0a102c5]
- [x] Task: 实现 `BuffRegistry` 静态类，建立 `StatType` 到 “文字+箭头” 图标的映射表 [12200c4]
- [x] Task: 扩展 `SkillData` 结构，增加 `charge_count`（最大充能次数）字段 [253982d]
- [x] Task: 编写单元测试：验证 `BuffRegistry` 的映射检索逻辑 [12200c4]
- [x] Task: Conductor - User Manual Verification 'Phase 1: Infrastructure & Data' (Protocol in workflow.md)

## Phase 2: 底部技能快捷栏实现 (Skill Hotbar Implementation)
- [x] Task: 在 `UIRenderer` 中建立 5 个底部固定槽位的绘制逻辑
- [x] Task: 实现技能图标的“径向遮罩” (Radial Wipe) 冷却动画
- [x] Task: 在技能图标左下角渲染蓝色的法力消耗数字小字
- [x] Task: 在图标右下角渲染当前可用充能次数数字
- [x] Task: 实现法力不足时的“图标暗淡”视觉状态
- [x] Task: 编写测试：验证技能栏在不同 CD 和蓝量状态下的渲染状态切换
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Skill Hotbar Implementation' (Protocol in workflow.md)

## Phase 3: Buff/Debuff 显示系统实现 (Buff System Implementation)
- [x] Task: 实现资源条上方的 Buff 容器布局，支持多图标横向排列
- [x] Task: 实现基于“文字+箭头”的图标绘制逻辑，并根据类型添加绿色/红色边框
- [x] Task: 实现 Buff 图标上的圆形持续时间进度显示
- [x] Task: 实现层数 (Stacks) 数字的角落渲染
- [ ] Task: 集成 `XPAwardingSystem` 或 `CombatSystem` 中的触发逻辑，确保 Buff 能正确进入 UI
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Buff System Implementation' (Protocol in workflow.md)

## Phase 4: 悬停提示与星盘联动 (Tooltips & Integration)
- [x] Task: 实现 HUD 上的鼠标碰撞检测，识别技能或 Buff 图标悬停
- [x] Task: 实现基础 Tooltip 浮窗绘制，包含名称、详细描述和标签
- [ ] Task: 逻辑开发：从 `AstrolabeComponent` 读取加成数据并计算注入到技能 Tooltip 中
- [ ] Task: 编写集成测试：验证激活星盘节点后，技能 Tooltip 中的数值是否实时更新
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Tooltips & Integration' (Protocol in workflow.md)

## Phase 5: 优化与性能验证 (Polish & Performance)
- [ ] Task: 统一 UI 主题风格（边框粗细、字体缩放、半透明度）
- [ ] Task: 性能压测：在 10,000+ 实体且存在大量状态变化的情况下验证 HUD 帧率稳定性
- [ ] Task: 修复 UI 层级遮挡问题（确保 HUD 始终位于最顶层）
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Polish & Performance' (Protocol in workflow.md)
