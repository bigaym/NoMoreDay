#pragma once
#include <vector>
#include <array>
#include <entt/entt.hpp>
#include "ItemComponent.hpp"

namespace NoMoreDay {

// 可以持有物品的实体组件 (玩家, 箱子, 怪物)
struct InventoryComponent {
    // 物品以实体形式存储。
    // 当物品在背包中时，它们不应拥有 Position/Sprite 组件 (或应被禁用/隐藏)。
    std::vector<entt::entity> items;
    int capacity = 40;
    
    bool isFull() const { // 背包是否已满
        return items.size() >= capacity;
    }
};

// 可以装备物品的实体组件 (玩家)
struct EquipmentComponent {
    // 索引对应 EquipmentSlot 枚举
    std::array<entt::entity, (size_t)EquipmentSlot::Count> slots;

    EquipmentComponent() {
        slots.fill(entt::null);
    }
    
    entt::entity get(EquipmentSlot slot) const {
        if (slot == EquipmentSlot::None || (size_t)slot >= (size_t)EquipmentSlot::Count) return entt::null;
        return slots[(size_t)slot];
    }
    
    void set(EquipmentSlot slot, entt::entity item) {
        if (slot == EquipmentSlot::None || (size_t)slot >= (size_t)EquipmentSlot::Count) return;
        slots[(size_t)slot] = item;
    }
};

} // namespace NoMoreDay
