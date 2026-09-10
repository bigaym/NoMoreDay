#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/Common.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "raylib.h"

#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/Combat.hpp"

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
     * @param skill_id 造成该次伤害的来源技能ID，用于 OnKill 事件携带击杀来源 (0=非技能/未知)。
     * @return 如果实体死亡（生命值 <= 0）则返回 true，否则返回 false。
     */
static bool ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker = entt::null, bool isCrit = false, bool showVFX = true, DamageApplyResult* applyResult = nullptr, uint32_t skill_id = 0);

    /**
     * @brief 统一的敌人死亡处理链：打 KilledTag、派发 OnKill / OnOverkill、
     *        触发词缀死亡效果 (MonsterAffixSystem::OnEnemyDeath) 并累加玩家击杀统计。
     *        正常伤害致死与处决致死 (AreaFieldDeliverySystem 绝命法场) 共用。
     * @param target 生命值已置 0 的敌人实体 (非 PlayerTag)。
     * @param attacker 击杀者，仅当其携带 PlayerStats 时累加 killCount。
     * @param overkill 溢出伤害量，仅用于 OnOverkill 事件 (处决路径传 0)。
     * @param rawDamage 本次致死伤害量，仅用于 OnOverkill 事件。
     * @param skill_id 造成致死的来源技能ID，随 OnKill 事件派发 (0=非技能/未知)。
     */
    static void KillEnemy(entt::registry& registry, entt::entity target,
                          entt::entity attacker, float overkill = 0.0f,
                          float rawDamage = 0.0f, uint32_t skill_id = 0);
};
