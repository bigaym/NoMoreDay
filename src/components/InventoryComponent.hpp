#pragma once
#include <vector>
#include <array>
#include <entt/entt.hpp>
#include "ItemComponent.hpp"

// Component for Entities that can hold items (Player, Chests, Mobs)
struct InventoryComponent {
    // Items are stored as entities. 
    // When in inventory, they should NOT have Position/Sprite components (or be disabled/hidden).
    std::vector<entt::entity> items;
    int capacity = 40;
    
    bool isFull() const {
        return items.size() >= capacity;
    }
};

// Component for Entities that can equip items (Player)
struct EquipmentComponent {
    // Index corresponds to EquipmentSlot enum
    std::array<entt::entity, (size_t)EquipmentSlot::Count> slots;

    EquipmentComponent() {
        slots.fill(entt::null);
    }
    
    entt::entity get(EquipmentSlot slot) const {
        if (slot == EquipmentSlot::None || slot >= EquipmentSlot::Count) return entt::null;
        return slots[(size_t)slot];
    }
    
    void set(EquipmentSlot slot, entt::entity item) {
        if (slot == EquipmentSlot::None || slot >= EquipmentSlot::Count) return;
        slots[(size_t)slot] = item;
    }
};
