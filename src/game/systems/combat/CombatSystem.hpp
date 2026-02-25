#pragma once
#include <entt/entt.hpp>
#include "game/components/Common.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "raylib.h"

#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"

#ifndef COMBAT_LEGACY_CALC_ENABLED
#define COMBAT_LEGACY_CALC_ENABLED 0
#endif

class CombatSystem {
public:
    struct DamageApplyResult {
        float requested_damage = 0.0f;
        float health_applied = 0.0f;
        float barrier_absorbed = 0.0f;
        bool was_prevented = false;
    };

    // Constants moved to NoMoreDay::Constants::Combat::System

    // 处理攻击输入、管理冷却时间并解决命中
    static void update(entt::registry& registry, NoMoreDay::systems::SpatialHashGrid& grid, const Camera2D& camera, float dt);

    /**
     * @brief 计算减伤后的最终伤害。
     */
    [[deprecated("Use DamagePipeline::Calculate")]]
    static float CalculateDamage(const NoMoreDay::CombatStats& attacker, const NoMoreDay::CombatStats& defender, float baseDamage, NoMoreDay::DamageType type);

    /**
     * @brief 对实体施加伤害，处理生命值减少和潜在的死亡。
     * @param attacker 造成伤害的可选实体（用于击杀奖励）。
     * @param isCrit 是否为暴击（用于显示数字和特效）。
     * @param showVFX 是否显示受击特效粒子。
     * @return 如果实体死亡（生命值 <= 0）则返回 true，否则返回 false。
     */
    static bool ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker = entt::null, bool isCrit = false, bool showVFX = true, DamageApplyResult* applyResult = nullptr);
};
