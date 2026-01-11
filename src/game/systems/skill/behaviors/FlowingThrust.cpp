/**
 * @file FlowingThrust.cpp
 * @brief 流云刺 (ID 1) - 突刺技能行为实现
 * 
 * 冲刺向目标方向，沿途造成物理伤害。
 * 
 * 天赋分支:
 * - 110 贯日: 无限穿透
 * - 112 风行: 释放后获得移速加成
 * - 113 迅捷之刃: 移速转化为伤害
 * - 114 势如破竹: 远距离增伤
 * - 120 留影: 生成残影重复施法
 * - 121 剑意盈盈: 命中时概率获得剑意
 * - 124 影杀阵: 强化时触发
 * - 130 洞悉弱点: 对满血敌人暴击率提升
 * - 140 寒霜刺: 物理转冰霜
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Buff.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/SkillDefs.hpp" // For ActiveSkillsComponent

#include "engine/render/RenderSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
    static constexpr uint32_t kSkillId = 1;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        auto* dash = registry.try_get<DashComponent>(owner);
        if (!pos) return;

        // 1. Dash towards target
        Vector2 startPos = {pos->x, pos->y};
        Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, startPos));
        float speed = 400.0f;

        // Apply burst velocity to owner
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * speed;
            vel->vy = dir.y * speed;
        }

        // Integrate with DashComponent
        if (dash) {
            dash->isDashing = true;
            dash->dashTimer = 0.375f;
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = speed;
        }

        // --- VISUAL EFFECTS: Ink Trail ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        for (int i = 0; i < 8; ++i) {
            float offset = (float)i * 15.0f;
            Vector2 p = { startPos.x + dir.x * offset, startPos.y + dir.y * offset };
            particleSys.Emit(systems::InkEffectHelper::CreateInkTrail(
                p, Vector2Scale(dir, -20.0f), 1.5f + (float)i * 0.2f, 0.8f));
        }

        if (exec.is_empowered) {
            auto goldParticles = systems::InkEffectHelper::CreateInkSplash(startPos, 12, 10.0f, 150.0f);
            for (auto& p : goldParticles) {
                p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
                p.flags |= 2;
                particleSys.Emit(p);
            }
            RenderSystem::AddScreenShake(0.15f);

            // Talent: Shadow Kill Array (影杀阵) - ID 124
            if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 1 && spec.allocated_points.contains(124) && 
                        spec.allocated_points.at(124) > 0) {
                        registry.emplace_or_replace<ShadowKillArrayReady>(owner);
                        LOG_INFO("Shadow Kill Array (影杀阵) Ready for entity {}", (uint32_t)owner);
                        break;
                    }
                }
            }
        }

        // --- TALENT BRANCH LOGIC ---
        float moreDamageMult = 1.0f;
        bool forcePierce = false;
        bool spawnShadow = false;
        bool isFrost = false;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 1) {
                    // Talent: Guan Ri (贯日) - ID 110
                    if (spec.allocated_points.contains(110) && spec.allocated_points.at(110) > 0) {
                        forcePierce = true;
                    }

                    // Talent: Liu Ying (留影) - ID 120
                    if (spec.allocated_points.contains(120) && spec.allocated_points.at(120) > 0) {
                        spawnShadow = true;
                    }

                    // Talent: Frost Thrust (寒霜刺) - ID 140
                    if (spec.allocated_points.contains(140) && spec.allocated_points.at(140) > 0) {
                        isFrost = true;
                    }

                    // Talent: Momentum (势如破竹) - ID 114
                    if (spec.allocated_points.contains(114) && spec.allocated_points.at(114) > 0) {
                        float dist = Vector2Distance(startPos, exec.target_pos);
                        if (dist > 150.0f) {
                            moreDamageMult *= 1.3f;
                            LOG_INFO("Momentum: +30% More damage due to distance ({:.1f})", dist);
                        }
                    }

                    // Talent: Feng Xing (风行) - ID 112
                    if (spec.allocated_points.contains(112) && spec.allocated_points.at(112) > 0) {
                        auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
                        BuffEffect swift;
                        swift.id = "flowing_thrust_swift";
                        swift.name = "Feng Xing";
                        swift.type = BuffType::SpeedUp;
                        swift.duration = 2.0f;
                        swift.remaining = 2.0f;
                        swift.modifiers.push_back({StatType::MoveSpeed, ModifierMode::PercentAdd, 30.0f});
                        effects.AddOrRefresh(swift);
                        registry.get_or_emplace<StatsDirty>(owner);
                        LOG_INFO("Feng Xing swiftness applied to entity {}", (uint32_t)owner);
                    }

                    // Talent: Xun Jie Zhi Ren (迅捷之刃) - ID 113
                    if (spec.allocated_points.contains(113) && spec.allocated_points.at(113) > 0) {
                        if (auto* combat = registry.try_get<CombatStats>(owner)) {
                            float ms = combat->move_speed;
                            float ms_bonus = (ms / 10.0f) * 0.01f * spec.allocated_points.at(113);
                            moreDamageMult *= (1.0f + ms_bonus);
                            LOG_INFO("Xun Jie Zhi Ren: +{:.1f}% More damage from MoveSpeed ({:.1f})", 
                                     ms_bonus * 100.0f, ms);
                        }
                    }
                    break;
                }
            }
        }

        // --- Branch B: Liu Ying (Shadow Echo) ---
        if (spawnShadow && !registry.any_of<ShadowCastTag>(owner)) {
            auto shadow_ent = registry.create();
            registry.emplace<LocalLevelTag>(shadow_ent);
            registry.emplace<Position>(shadow_ent, startPos.x, startPos.y);
            registry.emplace<AnimationStateComponent>(shadow_ent);
            registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(SKYBLUE, 0.5f));
            
            auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
            sc.delay = 0.5f;
            sc.lifetime = 1.5f;
            sc.snapshot.skill_id = 1;
            sc.snapshot.position = startPos;
            sc.snapshot.target_pos = exec.target_pos;
            if (stats) {
                sc.snapshot.stats = *stats;
                for (auto& mult : sc.snapshot.stats.damage_multipliers) mult *= 0.3f;
            }
            LOG_INFO("Liu Ying: Shadow Echo created for Flowing Thrust");
        }

        // 2. Spawn a "Thrust" projectile
        auto proj_ent = registry.create();
        registry.emplace<LocalLevelTag>(proj_ent);
        registry.emplace<Position>(proj_ent, pos->x, pos->y);
        registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
        registry.emplace<ColorComponent>(proj_ent, isFrost ? BLUE : SKYBLUE);
        
        auto& proj = registry.emplace<Projectile>(proj_ent);
        proj.owner = owner;
        proj.speed = speed;
        proj.lifeTime = 0.375f;
        proj.radius = exec.is_empowered ? 70.0f : 45.0f;
        proj.pierce = true;
        proj.pierceCount = forcePierce ? 999 : 99;
        
        if (stats) {
            proj.snapshot = *stats;
            for (auto& mult : proj.snapshot.damage_multipliers) mult *= moreDamageMult;

            if (exec.is_empowered) {
                for (auto& mult : proj.snapshot.damage_multipliers) mult *= 1.5f;
                LOG_INFO("Empowered Flowing Thrust spawned with 1.5x damage and larger radius");
            }
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
        }

        if (isFrost) {
            auto& skillMods = registry.emplace<SkillModifierComponent>(proj_ent);
            skillMods.damage_modifiers.push_back({Tag::Physical, Tag::Cold, 1.0f, ModifierType::Convert});
            LOG_INFO("Flowing Thrust converted to Cold");
        }

        registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);
        LOG_INFO("Flowing Thrust executed by entity {}", (uint32_t)owner);
    }

    static void DoHit(entt::registry& registry, entt::entity attacker, entt::entity target, Tag hit_tags, bool is_crit) {
        if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == kSkillId) {
                    // ID 121: Jian Yi Ying Ying (Chance to gain Intent)
                    if (spec.allocated_points.contains(121)) {
                        int pts = spec.allocated_points.at(121);
                        if (pts > 0 && GetRandomValue(0, 100) < 25 * pts) {
                             if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
                                 if (intent->stacks < intent->max_stacks) {
                                     intent->stacks++;
                                     intent->time_since_last_gain = 0.0f;
                                     intent->decay_tick_timer = 0.0f;
                                     LOG_DEBUG("Flowing Thrust (121): Gained Intent via Hit");
                                 }
                             }
                        }
                    }
                    
                    // ID 124: Shadow Kill Array (On Crit)
                    if (is_crit && spec.allocated_points.contains(124)) {
                         int pts = spec.allocated_points.at(124);
                         if (pts > 0 && GetRandomValue(0, 100) < 20 * pts) {
                             registry.get_or_emplace<ShadowKillArrayReady>(attacker);
                             LOG_INFO("Flowing Thrust (124): Shadow Kill Array READY");
                         }
                    }
                    break;
                }
            }
        }
    }
};

// Auto-register on static initialization
REGISTER_SKILL_BEHAVIOR(FlowingThrust)

} // namespace NoMoreDay::skills
