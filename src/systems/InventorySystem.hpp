#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/Common.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>

class InventorySystem {
public:
    // 在世界中创建一个新的物品实体（掉落在地上）
    static entt::entity createItem(entt::registry& registry, const NoMoreDay::ItemComponent& itemData, float x, float y);
    
    // 将物品（实体）添加到背包中。
    // 从物品中移除 Position/Sprite 组件以“隐藏”它。
    // 成功则返回 true。
    static bool pickUpItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // 从背包中移除物品并将其放回世界。
    // quantity: 要丢弃的数量。如果为 -1 或 >= 当前堆叠数量，则丢弃整个物品实体。
    // 否则，将拆分堆叠并创建一个新的掉落实体。
    static bool dropItem(entt::registry& registry, entt::entity character, entt::entity item, int quantity = -1);
    
    // 销毁物品（从背包中移除并销毁实体，或减少堆叠数量）
    static bool destroyItem(entt::registry& registry, entt::entity character, entt::entity item, int quantity = -1);

    // 将物品从背包装备到正确的槽位。
    // 如果槽位被占用，则进行交换。
    static bool equipItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // 卸下物品并将其放回背包。
    static bool unequipItem(entt::registry& registry, entt::entity character, NoMoreDay::EquipmentSlot slot);
    
    // 使用物品 (消耗品)
    static bool useItem(entt::registry& registry, entt::entity character, entt::entity item);

    // --- 背包扩展管理 ---
    static bool equipBag(entt::registry& registry, entt::entity character, entt::entity bagItem, int slotIndex);
    static bool unequipBag(entt::registry& registry, entt::entity character, int slotIndex, bool putBackInInventory = true);
    static void recalculateCapacity(entt::registry& registry, entt::entity character);

    // 基本管理
    static bool hasItem(entt::registry& registry, entt::entity character, uint32_t itemId);
    static int getItemCount(entt::registry& registry, entt::entity character, uint32_t itemId);

    // 整理背包：合并可堆叠物品并排序
    static void organize(entt::registry& registry, entt::entity character);

    // 每帧更新：处理物品吸附（磁力）和自动拾取逻辑
    static void update(entt::registry& registry, float dt);
};
