#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "core/logging/Logger.hpp"
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

    // 将物品从背包装备到特定的槽位。
    // 如果 targetSlot 为 None，则由物品默认槽位决定。
    // 如果槽位被占用，则进行交换。
    static bool equipItem(entt::registry& registry, entt::entity character, entt::entity item, NoMoreDay::EquipmentSlot targetSlot = NoMoreDay::EquipmentSlot::None);
    
    // 卸下物品并将其放回背包。
    static bool unequipItem(entt::registry& registry, entt::entity character, NoMoreDay::EquipmentSlot slot);
    
    // 使用物品 (消耗品)
    static bool useItem(entt::registry& registry, entt::entity character, entt::entity item);

    // --- 背包扩展管理 ---
    static bool equipBag(entt::registry& registry, entt::entity character, entt::entity bagItem, int slotIndex);
    static bool unequipBag(entt::registry& registry, entt::entity character, int slotIndex, bool putBackInInventory = true);
    static void recalculateCapacity(entt::registry& registry, entt::entity character);

    // --- 拖拽交换增强 API ---
    static bool swapInventoryItemIntoEquipment(entt::registry& registry, entt::entity character, int sourceInventoryIndex, NoMoreDay::EquipmentSlot targetSlot);
    static bool moveEquippedItemToInventorySlot(entt::registry& registry, entt::entity character, NoMoreDay::EquipmentSlot sourceSlot, int targetInventoryIndex);
    static bool moveBagItemToInventorySlot(entt::registry& registry, entt::entity character, int bagSlotIndex, int targetInventoryIndex);

    // 基本管理
    static bool hasItem(entt::registry& registry, entt::entity character, uint32_t itemId);
    static int getItemCount(entt::registry& registry, entt::entity character, uint32_t itemId);

    // 整理背包：合并可堆叠物品并排序
    static void organize(entt::registry& registry, entt::entity character);

    // --- UI 命令权威操作 (R1: system-owned, 供 GameUiCommandHandler 调用) ---
    // 将背包 fromIndex 槽位的物品移动到空槽 toIndex (仅空目标)。
    // 校验源槽位有物品、目标槽位为空且索引在容量范围内。
    static bool moveItem(entt::registry& registry, entt::entity character, int fromIndex, int toIndex);

    // 交换背包两个槽位的物品 (两个槽位都必须已占用且索引有效)。
    static bool swapItems(entt::registry& registry, entt::entity character, int indexA, int indexB);

    // 锁定/解锁背包中的物品 (防误分解/出售)。物品必须在角色背包/装备/包槽内。
    static bool setItemLocked(entt::registry& registry, entt::entity character, entt::entity item, bool locked);

    // 每帧更新：处理物品吸附（磁力）和自动拾取逻辑
    static void update(entt::registry& registry, float dt);
};
