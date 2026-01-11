#pragma once
#include <entt/entt.hpp>
#include "game/components/Common.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "raylib.h"

#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"



class CombatSystem {
public:
    struct Constants {
        static constexpr float DEFAULT_ATTACK_COOLDOWN = 1.0f;
        static constexpr float DEFAULT_ATTACK_RANGE = 60.0f;
        static constexpr float DEFAULT_ATTACK_ARC = 120.0f;
        static constexpr float ATTACK_EFFECT_LIFETIME = 0.2f;
        static constexpr float BLOCK_MITIGATION_FACTOR = 100.0f; // Mitigation = Block / (Block + Factor)
        static constexpr float CRIT_DAMAGE_FALLBACK = 1.5f;
        static constexpr float SCREEN_SHAKE_THRESHOLD = 100.0f;
    };

    // 处理攻击输入、管理冷却时间并解决命中
    static void update(entt::registry& registry, NoMoreDay::systems::SpatialHashGrid& grid, const Camera2D& camera, float dt);

    /**
     * @brief 计算减伤后的最终伤害。
     */
    static float CalculateDamage(const NoMoreDay::CombatStats& attacker, const NoMoreDay::CombatStats& defender, float baseDamage, NoMoreDay::DamageType type);

    /**
     * @brief 对实体施加伤害，处理生命值减少和潜在的死亡。
     * @param attacker 造成伤害的可选实体（用于击杀奖励）。
     * @param isCrit 是否为暴击（用于显示数字和特效）。
     * @param showVFX 是否显示受击特效粒子。
     * @return 如果实体死亡（生命值 <= 0）则返回 true，否则返回 false。
     */
    static bool ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker = entt::null, bool isCrit = false, bool showVFX = true);
};
