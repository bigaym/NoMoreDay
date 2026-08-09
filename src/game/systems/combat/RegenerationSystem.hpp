#pragma once

#include <entt/entt.hpp>
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/Common.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

/**
 * @brief 负责处理生命值和法力值的随时间自然恢复。
 * 此系统应在每帧运行。
 */
class RegenerationSystem {
public:
    static void update(entt::registry& registry, float dt) {
        // Get current game time for barrier delay calculation
        float currentTime = static_cast<float>(GetTime());
        
        auto view = registry.view<CombatStats>();
        
        for (auto entity : view) {
            if (registry.all_of<KilledTag>(entity)) continue; // Don't regenerate dead things
            
            auto& stats = view.get<CombatStats>(entity);
            bool isPlayer = registry.all_of<PlayerTag>(entity);
            
            float effectiveHealthRegen = stats.health_regen;
            float effectiveManaRegen = stats.mana_regen;

            // 1. 处理生命回复
            if (registry.all_of<HealthComponent>(entity)) {
                auto& hp = registry.get<HealthComponent>(entity);
                
                // 同步最大生命值
                hp.max = stats.max_health;

                if (hp.current < hp.max && effectiveHealthRegen > 0.0f) {
                    hp.current += effectiveHealthRegen * dt;
                    if (hp.current > hp.max) hp.current = hp.max;
                }
                
                // 同步回 stats 供 UI 读取
                stats.health = hp.current;

                if (isPlayer) {
                    // 仅在值发生变化或处于调试目的时记录 (限制日志频率)
                    static float logTimer = 0.0f;
                    logTimer += dt;
                    if (logTimer > 1.0f) {
                        LOG_TRACE("Player Regen: HP {:.1f}/{:.1f} (+{:.1f}/s), Mana {:.1f}/{:.1f} (+{:.1f}/s)", 
                            hp.current, hp.max, effectiveHealthRegen,
                            stats.mana, stats.max_mana, effectiveManaRegen);
                        logTimer = 0.0f;
                    }
                }
            } else {
                if (stats.health < stats.max_health && effectiveHealthRegen > 0.0f) {
                    stats.health += effectiveHealthRegen * dt;
                    if (stats.health > stats.max_health) stats.health = stats.max_health;
                }
            }

            // 2. 处理法力回复
            if (stats.mana < stats.max_mana && effectiveManaRegen > 0.0f) {
                stats.mana += effectiveManaRegen * dt;
                if (stats.mana > stats.max_mana) stats.mana = stats.max_mana;
            }

            // 3. 处理护盾回复/衰减 (Hybrid Barrier: ES + Ward Mode)
            if (auto* barrier = registry.try_get<BarrierComponent>(entity)) {
                float timeSinceDamage = currentTime - barrier->last_damage_time;

                // --- ES Mode: Regeneration ---
                // Only regenerate if:
                // 1. Time since last damage > barrier_delay
                // 2. Current barrier < max_barrier
                // 3. barrier_regen > 0
                if (timeSinceDamage > stats.barrier_delay && 
                    stats.barrier < stats.max_barrier && 
                    stats.barrier_regen > 0.0f) {
                    stats.barrier += stats.barrier_regen * dt;
                    if (stats.barrier > stats.max_barrier) {
                        stats.barrier = stats.max_barrier;
                    }
                }

                // --- Ward Mode: Decay ---
                // Decay barrier if it exceeds max_barrier (temporary shield from skills/kills)
                // Formula: EffectiveDecay = barrier_decay / (1 + barrier_retention)
                if (stats.barrier > stats.max_barrier && stats.barrier_decay > 0.0f) {
                    float effectiveDecay = stats.barrier_decay / (1.0f + stats.barrier_retention);
                    float excessBarrier = stats.barrier - stats.max_barrier;
                    float decayAmount = excessBarrier * effectiveDecay * dt;
                    stats.barrier -= decayAmount;
                    // Don't decay below max_barrier
                    if (stats.barrier < stats.max_barrier) {
                        stats.barrier = stats.max_barrier;
                    }
                }

                // Ensure barrier doesn't go negative
                if (stats.barrier < 0.0f) {
                    stats.barrier = 0.0f;
                }
            }
        }
    }
};

} // namespace NoMoreDay