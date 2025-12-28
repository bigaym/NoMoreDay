#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/Common.hpp"

class InventorySystem {
public:
    // Creates a new item entity in the world (dropped on ground)
    static entt::entity createItem(entt::registry& registry, const ItemComponent& itemData, float x, float y);
    
    // Adds an item (entity) to an inventory. 
    // Removes Position/Sprite components from the item to "hide" it.
    // Returns true if successful.
    static bool pickUpItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // Removes an item from inventory and places it back in the world.
    static bool dropItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // Equips an item from inventory to the correct slot.
    // Swaps if slot is occupied.
    static bool equipItem(entt::registry& registry, entt::entity character, entt::entity item);
    
    // Unequips an item and puts it back in inventory.
    static bool unequipItem(entt::registry& registry, entt::entity character, EquipmentSlot slot);
    
    // Basic management
    static bool hasItem(entt::registry& registry, entt::entity character, uint32_t itemId);
    static int getItemCount(entt::registry& registry, entt::entity character, uint32_t itemId);
};
