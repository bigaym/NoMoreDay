#pragma once
#include <vector>
#include <array>
#include <entt/entt.hpp>
#include "ItemComponent.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// 可以持有物品的实体组件 (玩家, 箱子, 怪物)
struct InventoryComponent {
    // 物品以实体形式存储。
    // 当物品在背包中时，它们不应拥有 Position/Sprite 组件 (或应被禁用/隐藏)。
    std::vector<entt::entity> items;
    std::vector<entt::entity> bag_slots; // 背包扩展槽
    int capacity = 40;
    int gold = 0; // 当前持有的金币数量
    float potionCooldown = 0.0f; // 药水冷却时间 (秒)
    float sortCooldown = 0.0f;   // 整理冷却时间 (秒)
    
    bool isFull() const { // 背包是否已满
        return items.size() >= capacity;
    }
};

// 序列化 InventoryComponent
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InventoryComponent, items, capacity, gold) // 移除宏，改为手动在 SerializationSystem 中处理

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
