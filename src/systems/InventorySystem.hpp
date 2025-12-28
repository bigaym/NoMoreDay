#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/Common.hpp"

class InventorySystem {
public:
    // 在世界中创建一个新的物品实体（掉落在地上）
    static entt::entity createItem(entt::registry& registry, const ItemComponent& itemData, float x, float y);
    
    // 将物品（实体）添加到背包中。
    // 从物品中移除 Position/Sprite 组件以“隐藏”它。
    // 成功则返回 true。
    static bool pickUpItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // 从背包中移除物品并将其放回世界。
    static bool dropItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // 将物品从背包装备到正确的槽位。
    // 如果槽位被占用，则进行交换。
    static bool equipItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // 卸下物品并将其放回背包。
    static bool unequipItem(entt::registry& registry, entt::entity character, EquipmentSlot slot);
    
    // 基本管理
    static bool hasItem(entt::registry& registry, entt::entity character, uint32_t itemId);
    static int getItemCount(entt::registry& registry, entt::entity character, uint32_t itemId);
};
