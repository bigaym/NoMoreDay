#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay {

class StatsSystem {
public:
    // Core update logic:
    // PrimaryStats + WeaponComponent -> CombatStats -> HealthComponent
    static void update(entt::registry& registry);
};

} // namespace NoMoreDay
