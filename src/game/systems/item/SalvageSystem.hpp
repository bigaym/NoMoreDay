#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <cstdint>
#include <span>

namespace NoMoreDay {

struct ItemComponent;

namespace SalvageSystem {

    struct SalvageResult {
        uint32_t materialId;
        int count;
    };

    /**
     * @brief Checks if an item can be salvaged.
     * Criteria: High rarity (>= Magic), not quest/material type, and not a non-salvageable unique.
     */
    bool CanSalvage(const ItemComponent& item);

    /**
     * @brief Calculates the potential yield from salvaging an item without side effects.
     */
    std::vector<SalvageResult> CalculateYield(const ItemComponent& item);

    /**
     * @brief Executes the salvage process: adds materials to player, destroys the item.
     */
    void Execute(entt::registry& registry, entt::entity itemEntity, entt::entity playerEntity);

    /**
     * @brief Batch processes multiple items for salvage.
     */
    void BatchExecute(entt::registry& registry, const std::vector<entt::entity>& entities, entt::entity playerEntity);

} // namespace SalvageSystem

} // namespace NoMoreDay
