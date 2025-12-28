#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class StatsSystem {
public:
    /**
     * @brief Recalculates all combat stats for a specific entity based on primary stats and modifiers.
     */
    static void Recalculate(entt::registry& registry, entt::entity entity);

    /**
     * @brief System update: recalculates stats for all entities with StatsDirty tag.
     */
    static void update(entt::registry& registry);
};

} // namespace NoMoreDay