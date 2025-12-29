#pragma once
#include <entt/entt.hpp>
#include <array>
#include "ItemComponent.hpp"

namespace NoMoreDay {

struct EquipmentComponent {
    // Array to store entities in slots. 
    // Index corresponds to EquipmentSlot enum value.
    std::array<entt::entity, static_cast<size_t>(EquipmentSlot::Count)> slots;

    EquipmentComponent() {
        slots.fill(entt::null);
    }

    // Helper to get item in a slot
    entt::entity Get(EquipmentSlot slot) const {
        size_t index = static_cast<size_t>(slot);
        if (index < slots.size()) {
            return slots[index];
        }
        return entt::null;
    }

    // Set item in slot.
    void Set(EquipmentSlot slot, entt::entity item) {
        if (slot == EquipmentSlot::None || slot >= EquipmentSlot::Count) return;
        slots[static_cast<size_t>(slot)] = item;
    }

    // Alias for compatibility or clearer intent
    entt::entity get(EquipmentSlot slot) const { return Get(slot); }
    void set(EquipmentSlot slot, entt::entity item) { Set(slot, item); }

    // Unequip item from slot. Returns the entity that was removed.
    entt::entity Unequip(EquipmentSlot slot) {
        if (slot == EquipmentSlot::None || slot >= EquipmentSlot::Count) return entt::null;
        entt::entity item = slots[static_cast<size_t>(slot)];
        slots[static_cast<size_t>(slot)] = entt::null;
        return item;
    }
};

} // namespace NoMoreDay
