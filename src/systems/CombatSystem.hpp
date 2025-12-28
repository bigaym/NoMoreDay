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

    /**
     * @brief Applies damage to an entity, handling Health reduction and potential death.
     * @param attacker Optional entity that caused the damage (for kill credit).
     * @return true if the entity died (health <= 0), false otherwise.
     */
    static bool ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker = entt::null);
};
