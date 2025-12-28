#include "InventorySystem.hpp"
#include "../tools/Logger.hpp"

entt::entity InventorySystem::createItem(entt::registry& registry, const ItemComponent& itemData, float x, float y) {
    auto entity = registry.create();
    registry.emplace<ItemComponent>(entity, itemData);
    registry.emplace<Position>(entity, x, y);
    // TODO: Add SpriteComponent based on itemData.id or type
    // For now, no sprite or a default placeholder
    registry.emplace<ColorComponent>(entity, YELLOW); // Placeholder color
    return entity;
}

bool InventorySystem::pickUpItem(entt::registry& registry, entt::entity character, entt::entity item) {
    if (!registry.valid(character) || !registry.valid(item)) {
        LOG_ERROR("Inventory: Invalid character or item entity during pickup");
        return false;
    }
    
    auto* inventory = registry.try_get<InventoryComponent>(character);
    if (!inventory) {
        LOG_WARN("Inventory: Character {} has no InventoryComponent", (uint32_t)character);
        return false;
    }
    
    if (inventory->isFull()) {
        LOG_LIMITED_WARN(2.0f, "Inventory: Character {} inventory is full!", (uint32_t)character);
        return false;
    }
    
    // Handle Stacking (if implemented later)
    // For now, just add to list
    inventory->items.push_back(item);
    
    // Remove World Components
    registry.remove<Position>(item);
    if (registry.any_of<SpriteComponent>(item)) {
        // Store sprite info? ItemComponent should define appearance.
        // We can just remove it and re-add it when dropped.
        registry.remove<SpriteComponent>(item);
    }
    if (registry.any_of<ColorComponent>(item)) {
        registry.remove<ColorComponent>(item);
    }
    
    const auto* itemComp = registry.try_get<ItemComponent>(item);
    LOG_INFO("Inventory: Character {} picked up item '{}' ({})", 
        (uint32_t)character, itemComp ? itemComp->name : "Unknown", (uint32_t)item);

    return true;
}

bool InventorySystem::dropItem(entt::registry& registry, entt::entity character, entt::entity item) {
    if (!registry.valid(character) || !registry.valid(item)) return false;
    
    auto* inventory = registry.try_get<InventoryComponent>(character);
    auto* pos = registry.try_get<Position>(character);
    
    if (!inventory || !pos) return false;
    
    // Find and remove from inventory vector
    auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
    if (it == inventory->items.end()) {
        LOG_ERROR("Inventory: Item {} not found in character {} inventory", (uint32_t)item, (uint32_t)character);
        return false;
    }
    
    inventory->items.erase(it);
    
    // Re-add World Components
    // Drop slightly offset
    registry.emplace_or_replace<Position>(item, pos->x + 20.0f, pos->y); 
    
    // Restore visual
    // TODO: Look up texture from AssetRegistry based on Item ID
    registry.emplace_or_replace<ColorComponent>(item, YELLOW); 
    
    const auto* itemComp = registry.try_get<ItemComponent>(item);
    LOG_INFO("Inventory: Character {} dropped item '{}'", (uint32_t)character, itemComp ? itemComp->name : "Unknown");

    return true;
}

bool InventorySystem::equipItem(entt::registry& registry, entt::entity character, entt::entity item) {
    auto* equipment = registry.try_get<EquipmentComponent>(character);
    auto* itemComp = registry.try_get<ItemComponent>(item);
    
    if (!equipment || !itemComp) {
        LOG_ERROR("Inventory: Missing equipment or item component for equip action");
        return false;
    }
    
    EquipmentSlot slot = itemComp->slot;
    if (slot == EquipmentSlot::None) {
        LOG_WARN("Inventory: Cannot equip item '{}' - invalid slot", itemComp->name);
        return false;
    }
    
    // Check if slot is occupied
    entt::entity currentEquipped = equipment->get(slot);
    if (registry.valid(currentEquipped)) {
        LOG_DEBUG("Inventory: Slot occupied, swapping item '{}'", itemComp->name);
        // Unequip current first (swap)
        if (!unequipItem(registry, character, slot)) {
            return false; // Failed to unequip (e.g. inventory full)
        }
    }
    
    // Remove from inventory
    auto* inventory = registry.try_get<InventoryComponent>(character);
    if (inventory) {
        auto it = std::find(inventory->items.begin(), inventory->items.end(), item);
        if (it != inventory->items.end()) {
            inventory->items.erase(it);
        }
    }
    
    // Set to slot
    equipment->set(slot, item);
    
    // Apply Stats (Optional: Can be calculated dynamically in CombatSystem)
    // For performance, we might want to cache stats on the character.
    
    LOG_INFO("Inventory: Character {} equipped '{}' to slot {}", (uint32_t)character, itemComp->name, (int)slot);
    return true;
}

bool InventorySystem::unequipItem(entt::registry& registry, entt::entity character, EquipmentSlot slot) {
    auto* equipment = registry.try_get<EquipmentComponent>(character);
    auto* inventory = registry.try_get<InventoryComponent>(character);
    
    if (!equipment || !inventory) {
        LOG_ERROR("Inventory: Missing components for unequip action");
        return false;
    }
    
    if (inventory->isFull()) {
        LOG_LIMITED_WARN(2.0f, "Inventory: Cannot unequip - inventory full for character {}", (uint32_t)character);
        return false;
    }
    
    entt::entity item = equipment->get(slot);
    if (!registry.valid(item)) {
        LOG_DEBUG("Inventory: No item found in slot {} to unequip", (int)slot);
        return false;
    }
    
    // Remove from slot
    equipment->set(slot, entt::null);
    
    // Add to inventory
    inventory->items.push_back(item);
    
    const auto* itemComp = registry.try_get<ItemComponent>(item);
    LOG_INFO("Inventory: Character {} unequipped '{}' from slot {}", (uint32_t)character, itemComp ? itemComp->name : "Unknown", (int)slot);
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
