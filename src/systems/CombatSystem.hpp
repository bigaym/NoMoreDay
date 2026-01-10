#pragma once
#include <entt/entt.hpp>
#include "../components/Common.hpp"
#include "SpatialGrid.hpp"
#include "raylib.h"

#include "../components/Stats.hpp"
#include "../components/Combat.hpp"



class CombatSystem {
public:
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
