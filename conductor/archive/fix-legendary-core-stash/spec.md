# 技术规格书: 传奇核心存储规则放宽

## 1. 问题陈述
玩家反馈“Legendary Core (传奇核心)”无法存入仓库。
经查，该物品类型为 `ItemType::Material`。当前 `StashSystem::canStoreItem` 逻辑强制禁止所有 Material 类型的物品存入普通仓库，预设逻辑是材料应自动进入 MaterialBank。
然而，部分特殊材料（如“传奇核心”）可能设计为占用背包格子的珍贵道具，或者玩家通过特定途径（非拾取）获得后滞留在背包中，导致无法转移。

## 2. 解决方案
修改 `StashSystem::canStoreItem` 判定逻辑，建立白名单机制或仅根据配置放行特定 Material。
考虑到灵活性，我们将**允许所有 Material 存入仓库**，但保留警告或提示（代码层不需弹窗，仅解除逻辑封锁）。或者更好的是，仅禁止“自动进入 Bank”的那些基础材料，但区分“材料”与“通货/道具”较复杂。
最简单的热修方案：**移除对 `ItemType::Material` 的硬性禁止**。让玩家自由管理仓库空间。

**代码变更**:
```cpp
// StashSystem.cpp
bool StashSystem::canStoreItem(entt::registry& registry, entt::entity item) {
    // ... basic validation ...
    
    // REMOVED: if (itemComp->type == ItemType::Material) return false;
    // 允许任何有效物品存入
    
    return true;
}
```

## 3. 验收标准
1.  启动游戏，角色背包中持有 "Legendary Core"。
2.  打开仓库，将 Core 拖入仓库格子。
3.  操作成功，物品保留在仓库中。
4.  取出操作同样成功。
