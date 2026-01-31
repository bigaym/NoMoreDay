#include "game/systems/item/InventorySystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/RenderSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EquipmentComponent.hpp" // ADDED THIS LINE
#include "game/components/MaterialBankComponent.hpp"
#include "game/components/MapFragmentComponent.hpp" // ADDED THIS LINE
#include "game/components/EffectComponent.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include <vector>
#include "game/systems/ui/UICrafting.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/ui/UISystem.hpp"

using namespace NoMoreDay;

entt::entity InventorySystem::createItem(entt::registry &registry, const ItemComponent &itemData, float x, float y)
{
    auto entity = registry.create();
    registry.emplace<ItemComponent>(entity, itemData);
    registry.emplace<Position>(entity, x, y);
    // TODO: 根据 itemData.id 或类型添加 SpriteComponent
    // 目前，没有精灵或使用默认占位符
    registry.emplace<ColorComponent>(entity, YELLOW); // 占位符颜色
    return entity;
}

bool InventorySystem::pickUpItem(entt::registry &registry, entt::entity character, entt::entity item)
{
    if (!registry.valid(character) || !registry.valid(item))
    {
        LOG_ERROR("背包: 拾取时角色或物品实体无效");
        return false;
    }

    auto *inventory = registry.try_get<NoMoreDay::InventoryComponent>(character);
    if (!inventory)
    {
        LOG_WARN("背包: 角色 {} 没有 InventoryComponent", (uint32_t)character);
        return false;
    }

    auto *itemComp = registry.try_get<NoMoreDay::ItemComponent>(item);
    if (!itemComp)
        return false;

    // --- Material Storage System ---
    // Exclude Map Fragments from auto-banking (they need to remain as entities for the Mosaic Editor)
    if (itemComp->type == ItemType::Material && !registry.any_of<MapFragmentComponent>(item)) {
        auto *materialBank = registry.try_get<NoMoreDay::MaterialBankComponent>(character);
        if (materialBank) {
            int newCount = materialBank->Add(itemComp->id, itemComp->quantity);
            
            // Visual Effect for Material Pickup
            if (registry.all_of<Position>(item)) {
                const auto& pos = registry.get<Position>(item);
                auto effect = registry.create();
                registry.emplace<Position>(effect, pos.x, pos.y);
                VisualEffect vEffect;
                vEffect.type = VisualEffectType::Pickup;
                using namespace NoMoreDay::Constants::Item;
                vEffect.lifeTime = PICKUP_VISUAL_EFFECT_DURATION;
                vEffect.color = GREEN; // Materials use Green for pickup effect
                registry.emplace<VisualEffect>(effect, vEffect);
            }

            LOG_INFO("Material System: Auto-banked '{}' x{} (Total: {})", itemComp->name, itemComp->quantity, newCount);
            registry.destroy(item);
            RenderSystem::s_itemGridDirty = true;
            return true;
        }
    }

    // --- 堆叠逻辑 ---
    if (itemComp->maxStack > 1)
    {
        for (auto existingEntity : inventory->items)
        {
            if (existingEntity == entt::null)
                continue;
            auto *existingItem = registry.try_get<NoMoreDay::ItemComponent>(existingEntity);
            // 检查 ID 相同且未满
            if (existingItem && existingItem->id == itemComp->id && existingItem->quantity < existingItem->maxStack)
            {
                int space = existingItem->maxStack - existingItem->quantity;
                int amountToAdd = std::min(space, itemComp->quantity);

                existingItem->quantity += amountToAdd;
                itemComp->quantity -= amountToAdd;

                LOG_INFO("背包: 堆叠合并 - 现有 {} + 新增 {} = {}", existingItem->quantity - amountToAdd, amountToAdd, existingItem->quantity);

                if (itemComp->quantity <= 0)
                {
                    // Visual Effect
                    if (registry.all_of<Position>(item)) {
                        const auto& pos = registry.get<Position>(item);
                        auto effect = registry.create();
                        registry.emplace<Position>(effect, pos.x, pos.y);
                        VisualEffect vEffect;
                        vEffect.type = VisualEffectType::Pickup;
                        using namespace NoMoreDay::Constants::Item;
                        vEffect.lifeTime = PICKUP_VISUAL_EFFECT_DURATION;
                        vEffect.color = WHITE;
                        registry.emplace<VisualEffect>(effect, vEffect);
                    }

                    registry.destroy(item);
                    RenderSystem::s_itemGridDirty = true;
                    return true;
                }
            }
        }
    }

    // 处理堆叠 (如果以后实现)
    // 寻找第一个空槽位
    bool placed = false;
    for (auto &slot : inventory->items)
    {
        if (slot == entt::null)
        {
            slot = item;
            placed = true;
            break;
        }
    }

    if (!placed)
    {
        // 如果没有空位但容量允许（例如尚未resize），则push_back
        if (inventory->items.size() < (size_t)inventory->capacity)
        {
            inventory->items.push_back(item);
        }
        else
        {
            // 真正满了
            return false;
        }
    }

    // 移除世界组件
    if (registry.all_of<Position>(item)) {
        const auto& pos = registry.get<Position>(item);
        
        // Visual Effect
        auto effect = registry.create();
        registry.emplace<Position>(effect, pos.x, pos.y);
        VisualEffect vEffect;
        vEffect.type = VisualEffectType::Pickup;
        using namespace NoMoreDay::Constants::Item;
        vEffect.lifeTime = PICKUP_VISUAL_EFFECT_DURATION;
        vEffect.color = WHITE;
        if (itemComp) {
            switch(itemComp->rarity) {
                 case Rarity::Magic: vEffect.color = SKYBLUE; break;
                 case Rarity::Rare: vEffect.color = YELLOW; break;
                 case Rarity::Legendary: vEffect.color = ORANGE; break;
                 default: vEffect.color = WHITE; break;
            }
        }
        registry.emplace<VisualEffect>(effect, vEffect);
    }

    registry.remove<Position>(item);
    if (registry.any_of<SpriteComponent>(item))
    {
        // 存储精灵信息？ItemComponent 应该定义外观。
        // 我们可以在掉落时直接移除并重新添加它。
        registry.remove<SpriteComponent>(item);
    }
    if (registry.any_of<ColorComponent>(item))
    {
        registry.remove<ColorComponent>(item);
    }

    // 物品进入背包后需要跟随玩家跨场景，替换 LocalLevelTag 为 PersistentTag
    if (registry.any_of<LocalLevelTag>(item))
    {
        registry.remove<LocalLevelTag>(item);
        registry.emplace_or_replace<PersistentTag>(item);
    }

    LOG_INFO("背包: 角色 {} 拾取了物品 '{}' ({})",
             (uint32_t)character, itemComp ? itemComp->name : "未知", (uint32_t)item);

    RenderSystem::s_itemGridDirty = true;
    return true;
}

bool InventorySystem::dropItem(entt::registry &registry, entt::entity character, entt::entity item, int quantity)
{
    if (!registry.valid(character) || !registry.valid(item))
        return false;

    auto *inventory = registry.try_get<InventoryComponent>(character);
    auto *pos = registry.try_get<Position>(character);
    auto *itemComp = registry.try_get<ItemComponent>(item);

    if (!inventory || !pos || !itemComp)
        return false;

    // 确定实际丢弃数量
    int dropCount = (quantity < 0 || quantity >= itemComp->quantity) ? itemComp->quantity : quantity;

    if (dropCount < itemComp->quantity)
    {
        // --- 拆分堆叠 ---
        // 1. 减少原物品数量
        itemComp->quantity -= dropCount;

        // 2. 创建新实体作为掉落物
        auto droppedEntity = registry.create();
        ItemComponent newItemComp = *itemComp; // 复制数据
        newItemComp.quantity = dropCount;

        registry.emplace<ItemComponent>(droppedEntity, newItemComp);
        using namespace NoMoreDay::Constants::Item;
        registry.emplace<Position>(droppedEntity, pos->x + DROP_OFFSET_X, pos->y);
        registry.emplace<Radius>(droppedEntity, 15.0f);
        registry.emplace<LocalLevelTag>(droppedEntity); // Ensure it's cleaned up on scene change
        registry.emplace<LootTag>(droppedEntity); // Optimization for spatial grid
        registry.emplace<LabelCacheComponent>(droppedEntity); // Pre-calculate for render
        RenderSystem::s_itemGridDirty = true;

        // 恢复视觉效果 (简单处理：根据类型给颜色，或者复制原实体的 Sprite 如果有)
        if (registry.any_of<SpriteComponent>(item))
        {
            registry.emplace<SpriteComponent>(droppedEntity, registry.get<SpriteComponent>(item));
        }
        else
        {
            registry.emplace<ColorComponent>(droppedEntity, registry.any_of<ColorComponent>(item) ? registry.get<ColorComponent>(item).color : YELLOW);
        }

        LOG_INFO("背包: 角色 {} 拆分并丢弃了 {} 个 '{}'", (uint32_t)character, dropCount, itemComp->name);
    }
    else
    {
        // --- 丢弃整个实体 ---
        // 从背包向量中查找并移除
        auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
        if (it == inventory->items.end())
        {
            LOG_ERROR("背包: 物品 {} 未在角色 {} 的背包中找到", (uint32_t)item, (uint32_t)character);
            return false;
        }

        *it = entt::null; // 标记为空槽位，而不是移除

        // 重新添加世界组件
        using namespace NoMoreDay::Constants::Item;
        registry.emplace_or_replace<Position>(item, pos->x + DROP_OFFSET_X, pos->y);
        
        // 物品离开背包后，不再需要跨场景持久化，替换为 LocalLevelTag
        if (registry.any_of<PersistentTag>(item))
        {
            registry.remove<PersistentTag>(item);
        }
        registry.emplace_or_replace<LocalLevelTag>(item);
        registry.emplace_or_replace<Radius>(item, 15.0f);
        registry.emplace_or_replace<LootTag>(item); // Optimization for spatial grid
        registry.get_or_emplace<LabelCacheComponent>(item).Invalidate(); // Ensure re-render
        RenderSystem::s_itemGridDirty = true;

        // 恢复视觉效果 (如果之前被移除了)
        if (!registry.any_of<SpriteComponent>(item) && !registry.any_of<ColorComponent>(item))
        {
            // 尝试恢复默认颜色，实际应根据 ItemType
            registry.emplace<ColorComponent>(item, YELLOW);
        }

        LOG_INFO("背包: 角色 {} 丢弃了物品 '{}'", (uint32_t)character, itemComp->name);
    }

    return true;
}

bool InventorySystem::destroyItem(entt::registry &registry, entt::entity character, entt::entity item, int quantity)
{
    if (!registry.valid(character) || !registry.valid(item))
        return false;

    auto *inventory = registry.try_get<InventoryComponent>(character);
    auto *itemComp = registry.try_get<ItemComponent>(item);

    if (!inventory || !itemComp)
        return false;

    int destroyCount = (quantity < 0 || quantity >= itemComp->quantity) ? itemComp->quantity : quantity;

    if (destroyCount < itemComp->quantity)
    {
        // 仅减少数量
        itemComp->quantity -= destroyCount;
        LOG_INFO("背包: 销毁了 {} 个 '{}'", destroyCount, itemComp->name);
    }
    else
    {
        // 销毁整个实体
        auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
        if (it != inventory->items.end())
        {
            *it = entt::null;
        }
        registry.destroy(item);
        LOG_INFO("背包: 彻底销毁了物品 '{}'", itemComp->name);
    }
    return true;
}

bool InventorySystem::equipItem(entt::registry &registry, entt::entity character, entt::entity item, EquipmentSlot targetSlot)
{
    auto *equipment = registry.try_get<EquipmentComponent>(character);
    auto *itemComp = registry.try_get<ItemComponent>(item);

    if (!equipment || !itemComp)
    {
        LOG_ERROR("背包: 装备操作缺少装备或物品组件");
        return false;
    }

    // [NEW] Level Requirement Check
    if (registry.all_of<PlayerStats>(character)) {
        const auto& stats = registry.get<PlayerStats>(character);
        if (stats.level < itemComp->itemLevel) {
            LOG_WARN("背包: 无法装备 - 等级不足 (需 Lv.{}, 当前 Lv.{})", itemComp->itemLevel, stats.level);
            
            UISystem::State.showMessageBox = true;
            snprintf(UISystem::State.messageBoxText, 64, "等级不足 (%d)", itemComp->itemLevel);
            UISystem::State.messageBoxTimer = 1.5f;
            return false;
        }
    }

    EquipmentSlot slot = (targetSlot != EquipmentSlot::None) ? targetSlot : itemComp->slot;

    // 验证槽位匹配
    bool canEquip = (slot == itemComp->slot);
    if (!canEquip) {
        // Special case for Rings
        bool isItemRing = (itemComp->slot == EquipmentSlot::Ring || itemComp->slot == EquipmentSlot::Ring1 || itemComp->slot == EquipmentSlot::Ring2);
        bool isSlotRing = (slot == EquipmentSlot::Ring1 || slot == EquipmentSlot::Ring2);
        if (isItemRing && isSlotRing) canEquip = true;

        // NEW: Titan's Grip allows 2H weapons in OffHand or switching MainHand items to OffHand
        if (registry.all_of<TitanGripTrait>(character) && itemComp->type == ItemType::Weapon) {
            if (slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand) {
                canEquip = true;
            }
        }
    }

    if (!canEquip) {
        LOG_WARN("背包: 物品 '{}' 无法装备到槽位 {}", itemComp->name, (int)slot);
        return false;
    }

    // 只有在没有指定 targetSlot 时才执行自动分配戒指逻辑
    if (targetSlot == EquipmentSlot::None && slot == EquipmentSlot::Ring) {
        if (equipment->get(EquipmentSlot::Ring1) == entt::null) {
            slot = EquipmentSlot::Ring1;
        } else if (equipment->get(EquipmentSlot::Ring2) == entt::null) {
            slot = EquipmentSlot::Ring2;
        } else {
            slot = EquipmentSlot::Ring1; // 默认替换戒指1
        }
    }

    if (slot == EquipmentSlot::None)
    {
        LOG_WARN("背包: 无法装备物品 '{}' - 无效槽位", itemComp->name);
        return false;
    }

    // --- 双手武器逻辑 ---
    bool hasTitanGrip = registry.all_of<TitanGripTrait>(character);

    // 1. 如果装备的是双手武器 (主手)，必须先卸下副手物品 (除非有 Titan's Grip)
    if (slot == EquipmentSlot::MainHand && itemComp->isTwoHanded && !hasTitanGrip) {
        if (registry.valid(equipment->get(EquipmentSlot::OffHand))) {
            if (!unequipItem(registry, character, EquipmentSlot::OffHand)) {
                LOG_WARN("背包: 无法装备双手武器 - 副手卸下失败 (背包已满?)");
                return false;
            }
        }
    }
    // 2. 如果装备的是副手物品，必须检查主手是否为双手武器 (除非有 Titan's Grip)
    if (slot == EquipmentSlot::OffHand) {
        entt::entity mhItem = equipment->get(EquipmentSlot::MainHand);
        if (registry.valid(mhItem)) {
            auto* mhComp = registry.try_get<ItemComponent>(mhItem);
            if (mhComp && mhComp->isTwoHanded && !hasTitanGrip) {
                if (!unequipItem(registry, character, EquipmentSlot::MainHand)) {
                    LOG_WARN("背包: 无法装备副手 - 双手武器卸下失败 (背包已满?)");
                    return false;
                }
            }
        }
    }

    // 检查槽位是否被占用
    entt::entity currentEquipped = equipment->get(slot);
    if (registry.valid(currentEquipped))
    {
        LOG_DEBUG("背包: 槽位被占用，正在交换物品 '{}'", itemComp->name);
        // 首先卸下当前装备 (交换)
        if (!unequipItem(registry, character, slot))
        {
            return false; // 卸下失败 (例如背包已满)
        }
    }

    // 从背包中移除
    auto *inventory = registry.try_get<InventoryComponent>(character);
    if (inventory)
    {
        auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
        if (it != inventory->items.end())
        {
            *it = entt::null;
        }
    }

    // 设置到槽位
    equipment->set(slot, item);

    // 核心修复：标记属性需要重新烘焙
    registry.get_or_emplace<StatsDirty>(character);

    LOG_INFO("背包: 角色 {} 将 '{}' 装备到槽位 {}", (uint32_t)character, itemComp->name, (int)slot);
    return true;
}

bool InventorySystem::unequipItem(entt::registry &registry, entt::entity character, EquipmentSlot slot)
{
    auto *equipment = registry.try_get<EquipmentComponent>(character);
    auto *inventory = registry.try_get<InventoryComponent>(character);

    if (!equipment || !inventory)
    {
        LOG_ERROR("背包: 卸下操作缺少组件");
        return false;
    }

    // 检查是否有空槽位 (不能仅依赖 isFull，因为 items 可能被 resize 填充了 null)
    bool hasSpace = false;
    for (const auto &slotItem : inventory->items)
    {
        if (slotItem == entt::null)
        {
            hasSpace = true;
            break;
        }
    }

    if (!hasSpace)
    {
        using namespace NoMoreDay::Constants::Item;
        LOG_LIMITED_WARN(POTION_DEFAULT_COOLDOWN, "背包: 无法卸下 - 角色 {} 的背包已满", (uint32_t)character);
        return false;
    }

    entt::entity item = equipment->get(slot);
    if (!registry.valid(item))
    {
        LOG_DEBUG("背包: 在槽位 {} 中没有找到要卸下的物品", (int)slot);
        return false;
    }

    // 从槽位中移除
    equipment->set(slot, entt::null);

    // 添加到背包
    // 寻找第一个空槽位
    bool placed = false;
    for (auto &slot : inventory->items)
    {
        if (slot == entt::null)
        {
            slot = item;
            placed = true;
            break;
        }
    }
    // 如果没有空槽位但未满（理论上不应发生，因为已检查 isFull），则追加
    if (!placed)
        inventory->items.push_back(item);

    // 核心修复：标记属性需要重新烘焙
    registry.get_or_emplace<StatsDirty>(character);

    const auto *itemComp = registry.try_get<ItemComponent>(item);
    LOG_INFO("背包: 角色 {} 从槽位 {} 卸下了 '{}'", (uint32_t)character, itemComp ? itemComp->name : "未知", (int)slot);
    return true;
}

bool InventorySystem::useItem(entt::registry& registry, entt::entity character, entt::entity item) {
    auto* itemComp = registry.try_get<ItemComponent>(item);
    auto* stats = registry.try_get<CombatStats>(character);
    auto* inv = registry.try_get<InventoryComponent>(character);

    if (!itemComp || !stats || !inv) return false;

    // 只有消耗品可以“使用”
    if (itemComp->type != ItemType::Consumable) return false;

    // 检查药水全局冷却
    if (inv->potionCooldown > 0.0f) {
        LOG_WARN("药水冷却中... ({:.1f}s)", inv->potionCooldown);
        return false;
    }

    bool effectApplied = false;
    float recoverAmount = 0.0f;

    // 根据物品 ID 处理效果 (匹配 ItemFactory 中的 ID)
    if (itemComp->id == 101) { // 生命药水
        if (stats->health >= stats->max_health) {
             LOG_INFO("生命值已满，无需使用生命药水。");
             return false;
        }
        using namespace NoMoreDay::Constants::Item;
        recoverAmount = POTION_HEAL_AMOUNT;
        stats->health = std::min(stats->max_health, stats->health + recoverAmount);
        effectApplied = true;
        LOG_INFO("使用了生命药水，恢复 50 点生命值。当前: {:.0f}/{:.0f}", stats->health, stats->max_health);
    } else if (itemComp->id == 102) { // 法力药水
        if (stats->mana >= stats->max_mana) {
             LOG_INFO("法力值已满，无需使用法力药水。");
             return false;
        }
        using namespace NoMoreDay::Constants::Item;
        recoverAmount = POTION_MANA_AMOUNT;
        stats->mana = std::min(stats->max_mana, stats->mana + recoverAmount);
        effectApplied = true;
        LOG_INFO("使用了法力药水，恢复 50 点法力值。当前: {:.0f}/{:.0f}", stats->mana, stats->max_mana);
    } else if (itemComp->id == 10001) { // Legendary Core
        // Open Legendary Crafting UI (Merging Tab)
        UICrafting::OpenMergePanel();
        LOG_INFO("Opening Legendary Crafting (Merging) Panel via Legendary Core");
        // Do NOT consume the item here. It is used as a reagent inside the UI.
        return false; 
    }

    if (effectApplied) {
        // Dispatch Event for Legendary Affixes
        CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateOnUsePotion(character, itemComp->id, recoverAmount));

        // 设置药水冷却 (例如 1 秒)
        using namespace NoMoreDay::Constants::Item;
        inv->potionCooldown = POTION_DEFAULT_COOLDOWN;

        // 减少数量或销毁
        if (itemComp->quantity > 1) {
            itemComp->quantity--;
        } else {
            // 从背包向量中移除
            auto it = std::find(inv->items.begin(), inv->items.end(), item);
            if (it != inv->items.end()) {
                *it = entt::null;
            }
            registry.destroy(item);
        }
        return true;
    }

    return false;
}

bool InventorySystem::equipBag(entt::registry &registry, entt::entity character, entt::entity bagItem, int slotIndex)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    auto *itemComp = registry.try_get<ItemComponent>(bagItem);

    if (!inv || !itemComp || itemComp->type != ItemType::Bag)
        return false;
    
    if (slotIndex < 0 || slotIndex >= InventoryComponent::MAX_BAG_SLOTS)
        return false;

    // 如果槽位已有背包，先卸下
    if (registry.valid(inv->bag_slots[slotIndex]))
    {
        if (!unequipBag(registry, character, slotIndex))
            return false;
    }

    // 从物品列表中移除该实体 (如果它在背包里)
    auto it = std::find(inv->items.begin(), inv->items.end(), bagItem);
    if (it != inv->items.end())
    {
        *it = entt::null;
    }

    inv->bag_slots[slotIndex] = bagItem;
    recalculateCapacity(registry, character);

    LOG_INFO("背包: 已装备背包 '{}' 到槽位 {}", itemComp->name, slotIndex);
    return true;
}

bool InventorySystem::unequipBag(entt::registry &registry, entt::entity character, int slotIndex, bool putBackInInventory)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    if (!inv || slotIndex < 0 || slotIndex >= InventoryComponent::MAX_BAG_SLOTS)
        return false;

    entt::entity bagItem = inv->bag_slots[slotIndex];
    if (!registry.valid(bagItem))
        return false;

    auto* bagComp = registry.try_get<ItemComponent>(bagItem);
    if (!bagComp) return false;

    // --- 严谨的卸下检查 ---
    // 1. 计算新容量
    int newCapacity = inv->capacity - bagComp->bagCapacity;
    
    // 2. 统计当前实际持有的物品总数 (不含 null)
    int occupiedCount = 0;
    for (auto entity : inv->items) {
        if (registry.valid(entity)) occupiedCount++;
    }
    
    // 3. 如果物品总数超过了减小后的容量，则禁止卸下
    // 如果我们是要放回背包，我们需要多留一个空位给背包本身
    int requiredCapacity = occupiedCount + (putBackInInventory ? 1 : 0);
    if (requiredCapacity > newCapacity) {
        LOG_WARN("背包: 无法卸下背包！当前物品总数 ({}) 超过了卸下后的容量 ({})。", requiredCapacity, newCapacity);
        return false;
    }

    // 4. 执行卸下
    inv->bag_slots[slotIndex] = entt::null;

    // 将物品紧凑化到数组前端，以确保 resize(newCapacity) 不会截断有效物品
    std::vector<entt::entity> activeItems;
    activeItems.reserve(requiredCapacity);
    for (auto entity : inv->items) {
        if (registry.valid(entity)) activeItems.push_back(entity);
    }
    
    if (putBackInInventory) {
        activeItems.push_back(bagItem);
    }

    // 重填 items
    inv->items.assign(newCapacity, entt::null);
    for (size_t i = 0; i < activeItems.size() && i < (size_t)newCapacity; ++i) {
        inv->items[i] = activeItems[i];
    }

    recalculateCapacity(registry, character);
    LOG_INFO("背包: 从槽位 {} 卸下了背包", slotIndex);
    return true;
}

void InventorySystem::recalculateCapacity(entt::registry &registry, entt::entity character)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    if (!inv)
        return;

    int total = InventoryComponent::BASE_CAPACITY;
    for (auto entity : inv->bag_slots)
    {
        if (registry.valid(entity))
        {
            if (auto *item = registry.try_get<ItemComponent>(entity))
            {
                total += item->bagCapacity;
            }
        }
    }
    inv->capacity = total;

    // 调整背包大小以匹配容量
    if ((int)inv->items.size() != total) {
        inv->items.resize(total, entt::null);
    }
}

bool InventorySystem::hasItem(entt::registry &registry, entt::entity character, uint32_t itemId)
{
    auto *inventory = registry.try_get<InventoryComponent>(character);
    if (!inventory)
        return false;

    for (auto entity : inventory->items)
    {
        auto *item = registry.try_get<ItemComponent>(entity);
        if (item && item->id == itemId)
            return true;
    }
    return false;
}

int InventorySystem::getItemCount(entt::registry &registry, entt::entity character, uint32_t itemId)
{
    auto *inventory = registry.try_get<InventoryComponent>(character);
    if (!inventory)
        return 0;

    int count = 0;
    for (auto entity : inventory->items)
    {
        auto *item = registry.try_get<ItemComponent>(entity);
        if (item && item->id == itemId)
        {
            count += item->quantity;
        }
    }
    return count;
}

void InventorySystem::organize(entt::registry &registry, entt::entity character)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    if (!inv || inv->items.empty())
        return;

    // 冷却检查
    if (inv->sortCooldown > 0.0f)
    {
        LOG_WARN("背包: 整理冷却中 ({:.1f}s)", inv->sortCooldown);
        return;
    }
    using namespace NoMoreDay::Constants::Item;
    inv->sortCooldown = SORT_COOLDOWN; // 设置1秒冷却

    LOG_INFO("背包: 开始整理角色 {} 的背包 (全局排序)", (uint32_t)character);

    // --- 1. 合并堆叠 (Stacking) ---
    std::map<uint32_t, std::vector<entt::entity>> itemGroups;
    for (entt::entity itemEntity : inv->items)
    {
        if (registry.valid(itemEntity))
        {
            if (auto *itemComp = registry.try_get<ItemComponent>(itemEntity))
            {
                if (itemComp->maxStack > 1)
                {
                    itemGroups[itemComp->id].push_back(itemEntity);
                }
            }
        }
    }

    for (auto &[id, entities] : itemGroups)
    {
        if (entities.size() <= 1)
            continue;

        for (size_t i = 0; i < entities.size(); ++i)
        {
            auto *targetItem = registry.try_get<ItemComponent>(entities[i]);
            if (!targetItem || targetItem->quantity >= targetItem->maxStack)
                continue;

            for (size_t j = i + 1; j < entities.size(); ++j)
            {
                auto *sourceItem = registry.try_get<ItemComponent>(entities[j]);
                if (!sourceItem || sourceItem->quantity <= 0)
                    continue;

                int space = targetItem->maxStack - targetItem->quantity;
                int amountToMove = std::min(space, sourceItem->quantity);

                if (amountToMove > 0)
                {
                    targetItem->quantity += amountToMove;
                    sourceItem->quantity -= amountToMove;
                }
                if (targetItem->quantity >= targetItem->maxStack)
                    break;
            }
        }
    }

    // --- 2. 收集有效物品并预取排序数据 (优化性能) ---
    struct ItemSortData
    {
        entt::entity entity;
        int type;
        int rarity;
        int slot;
        uint32_t id;
        int quantity;
    };

    std::vector<ItemSortData> sortList;
    sortList.reserve(inv->items.size());

    for (entt::entity itemEntity : inv->items)
    {
        if (!registry.valid(itemEntity))
            continue;

        auto *itemComp = registry.try_get<ItemComponent>(itemEntity);
        if (itemComp && itemComp->quantity > 0)
        {
            sortList.push_back({itemEntity,
                                (int)itemComp->type,
                                (int)itemComp->rarity,
                                (int)itemComp->slot,
                                itemComp->id,
                                itemComp->quantity});
        }
        else
        {
            // 销毁空/无效物品
            if (registry.valid(itemEntity))
                registry.destroy(itemEntity);
        }
    }

    // --- 3. 排序 ---
    std::sort(sortList.begin(), sortList.end(), [](const ItemSortData &a, const ItemSortData &b)
              {
        if (a.type != b.type) return a.type < b.type;
        if (a.rarity != b.rarity) return a.rarity > b.rarity; // 稀有度降序
        if (a.slot != b.slot) return a.slot < b.slot;
        if (a.id != b.id) return a.id < b.id;
        return a.quantity > b.quantity; });

    // --- 4. 重建 items 向量 ---
    std::vector<entt::entity> newItems;
    newItems.reserve(inv->capacity);

    for (const auto &data : sortList)
    {
        newItems.push_back(data.entity);
    }

    // 填充剩余的 null
    if (newItems.size() < (size_t)inv->capacity)
    {
        newItems.resize(inv->capacity, entt::null);
    }

    inv->items = std::move(newItems);

    LOG_INFO("背包: 整理完成 ({} 个物品)。", sortList.size());
}

void InventorySystem::update(entt::registry &registry, float dt)
{
    auto playerView = registry.view<PlayerTag, Position, InventoryComponent, CombatStats>();
    auto goldView = registry.view<GoldComponent, Position>();

    for (auto playerEntity : playerView)
    {
        const auto &playerPos = playerView.get<Position>(playerEntity);
        auto &inventory = playerView.get<InventoryComponent>(playerEntity);
        const auto &stats = playerView.get<CombatStats>(playerEntity);

        // 更新药水冷却
        if (inventory.potionCooldown > 0.0f)
        {
            inventory.potionCooldown -= dt;
        }
        // 更新整理冷却
        if (inventory.sortCooldown > 0.0f)
        {
            inventory.sortCooldown -= dt;
        }

        // 初始吸附范围：如果属性中的范围较小，则使用默认的 150.0f，方便测试和调整
        using namespace NoMoreDay::Constants::Item;
        float pickupRange = (stats.pickup_range > MIN_EFFECTIVE_PICKUP_RANGE) ? stats.pickup_range : DEFAULT_PICKUP_RANGE;
        float pickupRangeSq = pickupRange * pickupRange;

        for (auto goldEntity : goldView)
        {
            auto &goldPos = goldView.get<Position>(goldEntity);
            const auto &goldComp = goldView.get<GoldComponent>(goldEntity);

            float dx = playerPos.x - goldPos.x;
            float dy = playerPos.y - goldPos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < pickupRangeSq)
            {
                float dist = std::sqrt(distSq);

                // 吸附阈值 (例如 15 像素内自动拾取)
                if (dist < 15.0f)
                {
                    // 增加金币，防止溢出 (int 最大值)
                    long long newGold = (long long)inventory.gold + goldComp.amount;
                    if (newGold > std::numeric_limits<int>::max())
                        inventory.gold = std::numeric_limits<int>::max();
                    else
                        inventory.gold = (int)newGold;

                    // Visual Effect
                    auto effect = registry.create();
                    registry.emplace<Position>(effect, goldPos.x, goldPos.y);
                    VisualEffect vEffect;
                    vEffect.type = VisualEffectType::GoldSparkle;
                    vEffect.lifeTime = 0.3f;
                    vEffect.color = GOLD;
                    registry.emplace<VisualEffect>(effect, vEffect);

                    LOG_DEBUG("InventorySystem: Picked up {} gold. Total: {}", goldComp.amount, inventory.gold);
                    registry.destroy(goldEntity);
                    RenderSystem::s_itemGridDirty = true;
                }
                else
                {
                    // 磁力移动效果：距离越近速度越快
                    // 基础速度 400 + 距离加成
                    float speed = 400.0f + (pickupRange - dist) * 8.0f;
                    float moveX = (dx / dist) * speed * dt;
                    float moveY = (dy / dist) * speed * dt;

                    goldPos.x += moveX;
                    goldPos.y += moveY;
                }
            }
        }
    }
}
