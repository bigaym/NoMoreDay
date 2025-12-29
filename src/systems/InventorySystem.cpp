#include "InventorySystem.hpp"
#include "../tools/Logger.hpp"
#include "../components/Stats.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include <vector>

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
                    // 全部合并完成，销毁地面实体
                    registry.destroy(item);
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

    LOG_INFO("背包: 角色 {} 拾取了物品 '{}' ({})",
             (uint32_t)character, itemComp ? itemComp->name : "未知", (uint32_t)item);

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
        registry.emplace<Position>(droppedEntity, pos->x + 20.0f, pos->y);

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
        registry.emplace_or_replace<Position>(item, pos->x + 20.0f, pos->y);

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

bool InventorySystem::equipItem(entt::registry &registry, entt::entity character, entt::entity item)
{
    auto *equipment = registry.try_get<EquipmentComponent>(character);
    auto *itemComp = registry.try_get<ItemComponent>(item);

    if (!equipment || !itemComp)
    {
        LOG_ERROR("背包: 装备操作缺少装备或物品组件");
        return false;
    }

    EquipmentSlot slot = itemComp->slot;
    if (slot == EquipmentSlot::None)
    {
        LOG_WARN("背包: 无法装备物品 '{}' - 无效槽位", itemComp->name);
        return false;
    }

    // --- 双手武器逻辑 ---
    // 1. 如果装备的是双手武器 (主手)，必须先卸下副手物品
    if (slot == EquipmentSlot::MainHand && itemComp->isTwoHanded) {
        if (registry.valid(equipment->get(EquipmentSlot::OffHand))) {
            if (!unequipItem(registry, character, EquipmentSlot::OffHand)) {
                LOG_WARN("背包: 无法装备双手武器 - 副手卸下失败 (背包已满?)");
                return false;
            }
        }
    }
    // 2. 如果装备的是副手物品，必须检查主手是否为双手武器
    if (slot == EquipmentSlot::OffHand) {
        entt::entity mhItem = equipment->get(EquipmentSlot::MainHand);
        if (registry.valid(mhItem)) {
            auto* mhComp = registry.try_get<ItemComponent>(mhItem);
            if (mhComp && mhComp->isTwoHanded) {
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
        LOG_LIMITED_WARN(2.0f, "背包: 无法卸下 - 角色 {} 的背包已满", (uint32_t)character);
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

bool InventorySystem::equipBag(entt::registry &registry, entt::entity character, entt::entity bagItem, int slotIndex)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    auto *itemComp = registry.try_get<ItemComponent>(bagItem);

    if (!inv || !itemComp || itemComp->type != ItemType::Bag)
        return false;
    // 假设最大支持 4 个背包槽
    if (slotIndex < 0 || slotIndex >= 4)
        return false;

    // 确保容器足够大
    if (inv->bag_slots.size() <= (size_t)slotIndex)
    {
        inv->bag_slots.resize(slotIndex + 1, entt::null);
    }

    // 如果槽位已有背包，先卸下
    if (registry.valid(inv->bag_slots[slotIndex]))
    {
        if (!unequipBag(registry, character, slotIndex))
            return false;
    }

    // 从物品列表中移除该实体
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

bool InventorySystem::unequipBag(entt::registry &registry, entt::entity character, int slotIndex)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    if (!inv || slotIndex < 0 || slotIndex >= (int)inv->bag_slots.size())
        return false;

    entt::entity bagItem = inv->bag_slots[slotIndex];
    if (!registry.valid(bagItem))
        return false;

    inv->bag_slots[slotIndex] = entt::null;

    // 放回背包
    bool placed = false;
    for (auto &slot : inv->items)
    {
        if (slot == entt::null)
        {
            slot = bagItem;
            placed = true;
            break;
        }
    }
    if (!placed)
        inv->items.push_back(bagItem); // 溢出处理

    recalculateCapacity(registry, character);
    LOG_INFO("背包: 从槽位 {} 卸下了背包", slotIndex);
    return true;
}

void InventorySystem::recalculateCapacity(entt::registry &registry, entt::entity character)
{
    auto *inv = registry.try_get<InventoryComponent>(character);
    if (!inv)
        return;

    int total = 56; // 基础容量 (1页 = 7x8 = 56格)
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

    // 调整背包大小以匹配容量 (填充 null)
    // 注意：如果容量减小（卸下背包），resize 会截断多余的物品，这符合预期（或者需要额外的掉落逻辑）
    // 这里我们强制调整大小以匹配当前页数配置
    inv->items.resize(total, entt::null);
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
    inv->sortCooldown = 1.0f; // 设置1秒冷却

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
        float pickupRange = (stats.pickup_range > 50.0f) ? stats.pickup_range : 75.0f;
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

                    LOG_DEBUG("InventorySystem: Picked up {} gold. Total: {}", goldComp.amount, inventory.gold);
                    registry.destroy(goldEntity);
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
