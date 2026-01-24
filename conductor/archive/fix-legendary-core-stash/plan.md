# 实现计划: 解除材料存储限制

## Phase 1: 逻辑修改 (The Unlock)
**目标**: 允许 ItemType::Material 进入仓库。

- [ ] **Task 1.1**: 修改 `StashSystem::canStoreItem`
    - 删除对 `ItemType::Material` 的检测。
    - **位置**: `src/game/systems/item/StashSystem.cpp`
    - **风险**: 可能导致玩家误将大量基础材料手动存入仓库占位，但这属于玩家自由。

## Phase 2: 验证 (The Verify)
- [ ] **Task 2.1**: 游戏内测试
    - 启动游戏，确认 Legendary Core 可存入。

## 总结
工时: 0.1h
类型: Hotfix
