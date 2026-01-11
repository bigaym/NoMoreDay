#pragma once
#include <vector>
#include <array>
#include <entt/entt.hpp>
#include "game/components/ItemComponent.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// 可以持有物品的实体组件 (玩家, 箱子, 怪物)
struct InventoryComponent {
    static constexpr int BASE_CAPACITY = 40;
    static constexpr int MAX_BAG_SLOTS = 4;

    // 物品以实体形式存储。
    std::vector<entt::entity> items;
    std::array<entt::entity, MAX_BAG_SLOTS> bag_slots; // 背包扩展槽
    
    int capacity = BASE_CAPACITY;
    int gold = 0; // 当前持有的金币数量
    float potionCooldown = 0.0f; // 药水冷却时间 (秒)
    float sortCooldown = 0.0f;   // 整理冷却时间 (秒)
    
    // UI Scroll state
    float scrollOffset = 0.0f;
    
    InventoryComponent() {
        bag_slots.fill(entt::null);
        items.resize(BASE_CAPACITY, entt::null);
    }
    
    bool isFull() const { // 背包是否已满
        for (auto entity : items) {
            if (entity == entt::null) return false;
        }
        return true;
    }
};

// 序列化 InventoryComponent
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InventoryComponent, items, capacity, gold) // 移除宏，改为手动在 SerializationSystem 中处理

// 可以装备物品的实体组件 (玩家)
// EquipmentComponent is defined in EquipmentComponent.hpp


} // namespace NoMoreDay
