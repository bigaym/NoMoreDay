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

    /**
     * @brief Batch salvages the player's inventory items matching the filter.
     * The filter evaluation is owned by SalvageSystem (R1: system-owned
     * operation for GameUiCommandHandler; the UI sends only the filter values).
     *
     * @param rarityMask      Bitmask over NoMoreDay::Rarity (1 << rarity); 0 = no restriction.
     * @param keepIfTier6Plus Keep items that carry any T6+ affix.
     * @param excludeLocked   Skip locked items.
     * @param outSalvaged     Optional out-param receiving the destroyed entities.
     * @return Number of items salvaged.
     */
    int BatchExecuteFiltered(entt::registry& registry, entt::entity playerEntity,
                             uint32_t rarityMask, bool keepIfTier6Plus,
                             bool excludeLocked,
                             std::vector<entt::entity>* outSalvaged = nullptr);

} // namespace SalvageSystem

} // namespace NoMoreDay
