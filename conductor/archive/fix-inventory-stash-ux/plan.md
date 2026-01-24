# 实现计划: 背包与仓库 UX 修复

## Phase 1: 基础设施修复 (The Foundation)
**目标**: 确保数据结构存在且输入系统具备阻断能力。

- [ ] **Task 1.1**: 玩家实体初始化修复 (GameplayState)
    - 在 `InitializeEntities` 中添加 `PersonalStashComponent`。
    - 预设 1 个解锁页签。
    - **预估**: 0.5h
- [ ] **Task 1.2**: 全局输入阻断机制 (Input Blocking)
    - 更新 `UIState` 结构体。
    - 修改 `UIStash` 逻辑以设置 `isTyping`。
    - 修改 `InputSystem` 以响应 `isTyping`。
    - **预估**: 0.5h
- [ ] **验证**: 进入游戏，验证个人仓库可打开，搜索时角色不动。

## Phase 2: 交互逻辑完善 (The Interaction)
**目标**: 实现双向拖拽。

- [ ] **Task 2.1**: 后端逻辑扩展 (StashSystem)
    - 实现 `withdrawToSpecificSlot`。
    - 处理“移动”与“交换”两种情况。
    - **预估**: 1.0h
- [ ] **Task 2.2**: 前端拖拽适配 (UIInventory)
    - 在 `Draw` 函数的物品格渲染循环中，增加对 `isDraggingFromStash` 的检测。
    - 调用 Task 2.1 的接口。
    - **预估**: 1.0h
- [ ] **验证**: 全面测试仓库<->背包的拖拽及其交换逻辑。

## 总结
总预估工时: 3.0h
风险: 
- 交换逻辑可能涉及物品所有权转移（Entity Destroy/Create 或 Component 转移），需确保 UUID 和持久化数据不丢失（NoMoreDay 使用 Entity ID 引用，直接交换 Entity ID 即可，风险较低）。
