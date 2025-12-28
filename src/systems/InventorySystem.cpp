#include "InventorySystem.hpp"
#include "../tools/Logger.hpp"
#include "../components/Stats.hpp"

entt::entity InventorySystem::createItem(entt::registry& registry, const ItemComponent& itemData, float x, float y) {
 auto entity = registry.create();
 registry.emplace<ItemComponent>(entity, itemData);
 registry.emplace<Position>(entity, x, y);
 // TODO: 根据 itemData.id 或类型添加 SpriteComponent
 // 目前，没有精灵或使用默认占位符
 registry.emplace<ColorComponent>(entity, YELLOW); // 占位符颜色
 return entity;
}

bool InventorySystem::pickUpItem(entt::registry& registry, entt::entity character, entt::entity item) {
 if (!registry.valid(character) || !registry.valid(item)) {
 LOG_ERROR("背包: 拾取时角色或物品实体无效");
 return false;
 }
 
 auto* inventory = registry.try_get<InventoryComponent>(character);
 if (!inventory) {
 LOG_WARN("背包: 角色 {} 没有 InventoryComponent", (uint32_t)character);
 return false;
 }
 
 if (inventory->isFull()) {
 LOG_LIMITED_WARN(2.0f, "背包: 角色 {} 背包已满！", (uint32_t)character);
 return false;
 }
 
 // 处理堆叠 (如果以后实现)
 // 目前，只添加到列表中
 inventory->items.push_back(item);
 
 // 移除世界组件
 registry.remove<Position>(item);
 if (registry.any_of<SpriteComponent>(item)) {
 // 存储精灵信息？ItemComponent 应该定义外观。
 // 我们可以在掉落时直接移除并重新添加它。
 registry.remove<SpriteComponent>(item);
 }
 if (registry.any_of<ColorComponent>(item)) {
 registry.remove<ColorComponent>(item);
 }
 
 const auto* itemComp = registry.try_get<ItemComponent>(item);
 LOG_INFO("背包: 角色 {} 拾取了物品 '{}' ({})", 
 (uint32_t)character, itemComp ? itemComp->name : "未知", (uint32_t)item);
 
 return true;
}

bool InventorySystem::dropItem(entt::registry& registry, entt::entity character, entt::entity item) {
 if (!registry.valid(character) || !registry.valid(item)) return false;
 
 auto* inventory = registry.try_get<InventoryComponent>(character);
 auto* pos = registry.try_get<Position>(character);
 
 if (!inventory || !pos) return false;
 
 // 从背包向量中查找并移除
 auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
 if (it == inventory->items.end()) {
 LOG_ERROR("背包: 物品 {} 未在角色 {} 的背包中找到", (uint32_t)item, (uint32_t)character);
 return false;
 }
 
 inventory->items.erase(it);
 
 // 重新添加世界组件
 // 稍微偏移掉落
 registry.emplace_or_replace<Position>(item, pos->x + 20.0f, pos->y); 
 
 // 恢复视觉效果
 // TODO: 根据物品 ID 从 AssetRegistry 中查找纹理
 registry.emplace_or_replace<ColorComponent>(item, YELLOW); 
 
 const auto* itemComp = registry.try_get<ItemComponent>(item);
 LOG_INFO("背包: 角色 {} 丢弃了物品 '{}'", (uint32_t)character, itemComp ? itemComp->name : "未知");
 
 return true;
}

bool InventorySystem::equipItem(entt::registry& registry, entt::entity character, entt::entity item) {
 auto* equipment = registry.try_get<EquipmentComponent>(character);
 auto* itemComp = registry.try_get<ItemComponent>(item);
 
 if (!equipment || !itemComp) {
 LOG_ERROR("背包: 装备操作缺少装备或物品组件");
 return false;
 }
 
 EquipmentSlot slot = itemComp->slot;
 if (slot == EquipmentSlot::None) {
 LOG_WARN("背包: 无法装备物品 '{}' - 无效槽位", itemComp->name);
 return false;
 }
 
 // 检查槽位是否被占用
 entt::entity currentEquipped = equipment->get(slot);
 if (registry.valid(currentEquipped)) {
 LOG_DEBUG("背包: 槽位被占用，正在交换物品 '{}'", itemComp->name);
 // 首先卸下当前装备 (交换)
 if (!unequipItem(registry, character, slot)) {
 return false; // 卸下失败 (例如背包已满)
 }
 }
 
 // 从背包中移除
 auto* inventory = registry.try_get<InventoryComponent>(character);
 if (inventory) {
 auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
 if (it != inventory->items.end()) {
 inventory->items.erase(it);
 }
 }
 
 // 设置到槽位
 equipment->set(slot, item);
 
 // 应用属性 (可选: 可以在 CombatSystem 中动态计算)
 // 为了性能，我们可能希望在角色上缓存属性。
 
 // 核心修复：标记属性需要重新烘焙
 registry.get_or_emplace<NoMoreDay::StatsDirty>(character);

 LOG_INFO("背包: 角色 {} 将 '{}' 装备到槽位 {}", (uint32_t)character, itemComp->name, (int)slot);
 return true;
}

bool InventorySystem::unequipItem(entt::registry& registry, entt::entity character, EquipmentSlot slot) {
 auto* equipment = registry.try_get<EquipmentComponent>(character);
 auto* inventory = registry.try_get<InventoryComponent>(character);
 
 if (!equipment || !inventory) {
 LOG_ERROR("背包: 卸下操作缺少组件");
 return false;
 }
 
 if (inventory->isFull()) {
 LOG_LIMITED_WARN(2.0f, "背包: 无法卸下 - 角色 {} 的背包已满", (uint32_t)character);
 return false;
 }
 
 entt::entity item = equipment->get(slot);
 if (!registry.valid(item)) {
 LOG_DEBUG("背包: 在槽位 {} 中没有找到要卸下的物品", (int)slot);
 return false;
 }
 
 // 从槽位中移除
 equipment->set(slot, entt::null);
 
 // 添加到背包
 inventory->items.push_back(item);
 
 // 核心修复：标记属性需要重新烘焙
 registry.get_or_emplace<NoMoreDay::StatsDirty>(character);

 const auto* itemComp = registry.try_get<ItemComponent>(item);
 LOG_INFO("背包: 角色 {} 从槽位 {} 卸下了 '{}'", (uint32_t)character, itemComp ? itemComp->name : "未知", (int)slot);
 return true;
}

bool InventorySystem::hasItem(entt::registry& registry, entt::entity character, uint32_t itemId) {
 auto* inventory = registry.try_get<InventoryComponent>(character);
 if (!inventory) return false;
 
 for (auto entity : inventory->items) {
 auto* item = registry.try_get<ItemComponent>(entity);
 if (item && item->id == itemId) return true;
 }
 return false;
}

int InventorySystem::getItemCount(entt::registry& registry, entt::entity character, uint32_t itemId) {
 auto* inventory = registry.try_get<InventoryComponent>(character);
 if (!inventory) return 0;
 
 int count = 0;
 for (auto entity : inventory->items) {
 auto* item = registry.try_get<ItemComponent>(entity);
 if (item && item->id == itemId) {
 count += item->quantity;
 }
 }
 return count;
}
