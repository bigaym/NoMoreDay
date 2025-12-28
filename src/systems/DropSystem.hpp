#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class DropSystem {
public:
    /**
     * @brief Processes killed entities and generates loot based on their DropTableComponent.
     */
    static void update(entt::registry& registry);

    /**
     * @brief Core logic to calculate drops for a specific entity.
     * Can be called independently for testing or special events.
     */
    static void GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim);
};

} // namespace NoMoreDay
