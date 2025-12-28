#pragma once
#include <entt/entt.hpp>
#include "../components/Common.hpp"
#include "SpatialGrid.hpp"
#include "raylib.h"

#include "../components/Stats.hpp"
#include "../components/Combat.hpp"

class CombatSystem {
public:
    // Processes attack inputs, manages cooldowns, and resolves hits
    static void update(entt::registry& registry, systems::SpatialHashGrid& grid, const Camera2D& camera, float dt);

    /**
     * @brief Calculates final damage after mitigation.
     */
    static float CalculateDamage(const NoMoreDay::CombatStats& attacker, const NoMoreDay::CombatStats& defender, float baseDamage, NoMoreDay::DamageType type);
};
