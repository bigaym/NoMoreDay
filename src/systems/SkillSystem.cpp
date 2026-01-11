#include "SkillSystem.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp" // For Position
#include "../components/Projectile.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/PlayerState.hpp" // For DashComponent
#include "../components/Buff.hpp"
#include "../components/AIComponent.hpp" // For EnemyTag
#include "../core/SkillRegistry.hpp"
#include "RenderSystem.hpp"
#include "StatsSystem.hpp"
#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
#include "SpatialGrid.hpp"
#include "GPUParticleSystem.hpp" // Added
#include "../tools/Logger.hpp"
#include "raymath.h"
#include <map>
#include <algorithm>
#include <unordered_set>

namespace NoMoreDay {

static std::map<uint32_t, SkillSystem::CastCallback> s_skill_callbacks;
static std::vector<SkillSystem::SkillHook> s_pre_cast_hooks;
static std::vector<SkillSystem::SkillHook> s_post_cast_hooks;

void SkillSystem::InitHooks() {
    LOG_INFO("Initializing Skill Hooks...");
    ClearHooks();
    s_skill_callbacks.clear();
    
    // 1. Sword Intent & Empowered Logic
    AddPreCastHook([](entt::registry& registry, entt::entity execution_ent, SkillExecution& exec) {
        entt::entity caster = exec.owner;
        if (!registry.valid(caster)) return;

        if (registry.any_of<ShadowCastTag>(execution_ent)) return;

        if (auto* intent = registry.try_get<SwordIntentComponent>(caster)) {
            if (intent->stacks >= intent->max_stacks) {
                exec.is_empowered = true;
                intent->stacks = 0;
                LOG_INFO("Skill {} empowered by Sword Intent for entity {}", exec.skill_id, (uint32_t)caster);
            }
        }
    });

    // ID 1: Flowing Thrust (流云刺)
    RegisterEffect(1, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        auto* dash = registry.try_get<DashComponent>(owner);
        if (!pos) return;

        // 1. Dash towards target
        Vector2 startPos = {pos->x, pos->y};
        Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, startPos));
        float speed = 400.0f; // Reduced from 1200.0f (1/3 speed)

        // Apply burst velocity to owner
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * speed;
            vel->vy = dir.y * speed;
        }

        // Integrate with DashComponent to prevent movement override
        if (dash) {
            dash->isDashing = true;
            dash->dashTimer = 0.375f; // Adjusted to maintain 150 range (400 * 0.375 = 150)
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = speed;
        }

        // --- VISUAL EFFECTS: Ink Trail ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        for (int i = 0; i < 8; ++i) {
            float offset = (float)i * 15.0f;
            Vector2 p = { startPos.x + dir.x * offset, startPos.y + dir.y * offset };
            particleSys.Emit(systems::InkEffectHelper::CreateInkTrail(p, Vector2Scale(dir, -20.0f), 1.5f + (float)i * 0.2f, 0.8f));
        }

        if (exec.is_empowered) {
            auto goldParticles = systems::InkEffectHelper::CreateInkSplash(startPos, 12, 10.0f, 150.0f);
            for (auto& p : goldParticles) {
                p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
                p.flags |= 2; // Add glow flag if supported, or just use gold color
                particleSys.Emit(p);
            }
            RenderSystem::AddScreenShake(0.15f);

            // Talent: Shadow Kill Array (影杀阵) - ID 124
            if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 1 && spec.allocated_points.contains(124) && spec.allocated_points.at(124) > 0) {
                        registry.emplace_or_replace<ShadowKillArrayReady>(owner);
                        LOG_INFO("Shadow Kill Array (影杀阵) Ready for entity {}", (uint32_t)owner);
                        break;
                    }
                }
            }
        }

        // --- BRANCH LOGIC ---
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
                            moreDamageMult *= 1.3f; // 30% More
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
                            float ms_bonus = (ms / 10.0f) * 0.01f * spec.allocated_points.at(113); // 1% per 10 MS per point
                            moreDamageMult *= (1.0f + ms_bonus);
                            LOG_INFO("Xun Jie Zhi Ren: +{:.1f}% More damage from MoveSpeed ({:.1f})", ms_bonus * 100.0f, ms);
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

        // 2. Spawn a "Thrust" projectile that moves with the dash
        auto proj_ent = registry.create();
        registry.emplace<LocalLevelTag>(proj_ent);
        registry.emplace<Position>(proj_ent, pos->x, pos->y);
        registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
        registry.emplace<ColorComponent>(proj_ent, isFrost ? BLUE : SKYBLUE); 
        
        auto& proj = registry.emplace<Projectile>(proj_ent);
        proj.owner = owner;
        proj.speed = speed;
        proj.lifeTime = 0.375f; // Adjusted to match dash (400 * 0.375 = 150)
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
    });

    // ID 2: Rending Wave (裂空斩)
    RegisterEffect(2, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        if (!pos || !stats) return;

        const auto* skillData = SkillRegistry::Get().GetSkill(exec.skill_id);
        Tag skillTags = skillData ? skillData->tags : Tag::None;
        float baseSpeed = skillData ? skillData->GetParam("speed", 300.0f) : 300.0f;
        float baseRadius = skillData ? skillData->GetParam("radius", 35.0f) : 35.0f;
        
        float baseLifetime = skillData ? skillData->GetParam("lifetime", 1.2f) : 1.2f;

        if (registry.any_of<SpiritSwordTag>(owner)) {
            baseRadius *= 0.5f;
            baseLifetime *= 0.75f;
            LOG_INFO("Spirit Sword Rending Wave: Radius halved, Lifetime reduced to 75%.");
        }

        Vector2 baseDir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));

        // --- BRANCH LOGIC ---
        int extraWaves = 0;
        bool boomerang = false;
        float moreDamageFromIntent = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 2) {
                    // Talent: Fen Hai (分海) - ID 210
                    if (spec.allocated_points.contains(210)) {
                        extraWaves = spec.allocated_points.at(210); // +1 projectile per point
                    }

                    // Talent: Fan Tian (翻天) - ID 220
                    if (spec.allocated_points.contains(220) && spec.allocated_points.at(220) > 0) {
                        boomerang = true;
                    }

                    // Talent: Sword Intent Scaling - ID 230
                    if (spec.allocated_points.contains(230)) {
                        if (auto* intent = registry.try_get<SwordIntentComponent>(owner)) {
                            moreDamageFromIntent = intent->stacks * 0.05f * spec.allocated_points.at(230); // 5% more per stack per point
                        }
                    }
                    break;
                }
            }
        }

        int totalCount = (int)StatsSystem::GetStatWithTags(registry, owner, StatType::ProjectileCount, skillTags, exec.skill_id);
        if (totalCount < 1) totalCount = 1; 
        totalCount += extraWaves;

        if (exec.is_empowered) {
            totalCount *= 2;
            LOG_INFO("Empowered Rending Wave: Double projectiles!");
            RenderSystem::AddScreenShake(0.2f); // Added shake for empowered skill
        }

        float spread = 0.4f + (totalCount * 0.05f); 
        float startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f; 
        float angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;

        for (int i = 0; i < totalCount; ++i) {
            float angle = startAngle + i * angleStep;
            Vector2 dir = Vector2Rotate(baseDir, angle);
            
            // --- VISUAL EFFECTS: Projectile Trail ---
            auto& particleSys = systems::GPUParticleSystem::Get();
            Color coreColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_CORE 
                                                : systems::InkEffectHelper::COLOR_SWORD_QI;
            Color glowColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_GLOW 
                                                : systems::InkEffectHelper::COLOR_FROST_LIGHT;
            auto trailParticles = systems::InkEffectHelper::CreateProjectileTrail(
                {pos->x, pos->y}, dir, coreColor, glowColor, 25.0f, 4);
            particleSys.EmitBatch(trailParticles);

            auto proj_ent = registry.create();
            registry.emplace<LocalLevelTag>(proj_ent);
            registry.emplace<Position>(proj_ent, pos->x, pos->y);
            registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed, dir.y * baseSpeed);
            registry.emplace<ColorComponent>(proj_ent, exec.is_empowered ? GOLD : WHITE); 
            
            auto& proj = registry.emplace<Projectile>(proj_ent);
            proj.owner = owner;
            proj.speed = baseSpeed;
            proj.lifeTime = boomerang ? 2.0f : baseLifetime;
            proj.radius = exec.is_empowered ? baseRadius * 1.7f : baseRadius; // Larger waves when empowered
            proj.pierce = true;
            proj.pierceCount = 99; 
            proj.snapshot = *stats;

            // Apply Intent Scaling
            if (moreDamageFromIntent > 0.0f) {
                for(auto& m : proj.snapshot.damage_multipliers) m *= (1.0f + moreDamageFromIntent);
            }

            if (exec.is_empowered) {
                for (auto& mult : proj.snapshot.damage_multipliers) mult *= 1.5f;
            }
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
            registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

            if (boomerang) {
                auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
                bc.owner = owner;
                bc.returnTimer = 0.5f; // Return after 0.5s
                bc.phase = BoomerangComponent::Outward;
                bc.returnSpeed = proj.speed * 1.2f;
            }
        }

        LOG_INFO("Rending Wave fired {} projectiles from entity {}", totalCount, (uint32_t)owner);
    });

    // ID 3: Blade Formation (灵剑决)
    RegisterEffect(3, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& formation = registry.get_or_emplace<BladeFormationComponent>(owner);
        
        int extraSwords = 0;
        float freqInc = 0.0f;
        float searchInc = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 3) {
                    // Node 300: 多重灵剑
                    if (spec.allocated_points.contains(300)) {
                        extraSwords = spec.allocated_points.at(300);
                    }
                    // Node 301: 疾风剑意
                    if (spec.allocated_points.contains(301)) {
                        freqInc = spec.allocated_points.at(301) * 0.1f;
                    }
                    // Node 302: 索敌范围
                    if (spec.allocated_points.contains(302)) {
                        searchInc = spec.allocated_points.at(302) * 0.2f;
                    }
                    // Node 310: 归一 (Convergence into giant sword)
                    if (spec.allocated_points.contains(310) && spec.allocated_points.at(310) > 0) {
                        formation.has_giant_sword = true;
                    }
                    // Node 311: 天崩地裂 (Ultimate shockwave)
                    if (spec.allocated_points.contains(311) && spec.allocated_points.at(311) > 0) {
                        formation.shockwave_on_crit = true;
                    }
                    // Node 321: 气劲回流 (Mana on hit)
                    if (spec.allocated_points.contains(321) && spec.allocated_points.at(321) > 0) {
                        formation.mana_on_hit = true;
                    }
                    // Node 322: 不灭剑魂 (Immortality)
                    if (spec.allocated_points.contains(322) && spec.allocated_points.at(322) > 0) {
                        formation.immortality_ready = true;
                    }
                    break;
                }
            }
        }

        formation.max_swords = 1 + extraSwords;
        formation.current_swords = formation.max_swords; 
        formation.attack_interval = 0.6f / (1.0f + freqInc);
        formation.search_radius = 270.0f * (1.0f + searchInc); 
        formation.is_empowered = exec.is_empowered;

        if (formation.has_giant_sword) {
            formation.max_swords = 1;
            formation.attack_interval *= 2.0f; 
        }

        if (exec.is_empowered) {
            formation.max_swords += 2;
            formation.attack_interval *= 0.5f; 
        }

        // --- MANAGE SPIRIT SWORDS (Refresh/Spawn) ---
        std::vector<entt::entity> existing_swords;
        auto view = registry.view<SpiritSwordTag, SummonComponent>();
        for(auto entity : view) {
            if(view.get<SummonComponent>(entity).owner == owner) {
                existing_swords.push_back(entity);
            }
        }

        // Refresh Existing
        for(auto entity : existing_swords) {
            auto& s = registry.get<SummonComponent>(entity);
            s.lifetime = s.max_lifetime;
        }

        // Adjust Count
        int current_count = (int)existing_swords.size();
        
        if (current_count > formation.max_swords) {
            for (int i = formation.max_swords; i < current_count; ++i) {
                registry.destroy(existing_swords[i]);
            }
            LOG_INFO("Blade Formation: Refreshed duration and removed {} excess swords.", current_count - formation.max_swords);
        } 
        else if (current_count < formation.max_swords) {
            auto* pos = registry.try_get<Position>(owner);
            uint32_t skillIcon = 3687043718; 

            int needed = formation.max_swords - current_count;
            for (int i = 0; i < needed; ++i) {
                auto sword = registry.create();
                registry.emplace<LocalLevelTag>(sword);
                registry.emplace<Position>(sword, pos ? *pos : Position{0,0});
                registry.emplace<Velocity>(sword, 0.0f, 0.0f);
                
                auto& summon = registry.emplace<SummonComponent>(sword);
                summon.owner = owner;
                summon.skill_id = 3;
                summon.lifetime = 10.0f;
                summon.max_lifetime = 10.0f;
                summon.icon_id = skillIcon;
                summon.name = "灵剑";

                registry.emplace<SpiritSwordTag>(sword);
                
                auto& ai = registry.emplace<SpiritSwordAI>(sword);
                ai.attack_interval = formation.attack_interval;
                
                int total_index = current_count + i;
                ai.attack_timer = (float)total_index * (ai.attack_interval / formation.max_swords); 
                ai.orbit_angle = (float)total_index / formation.max_swords * 2.0f * PI;
            }
            LOG_INFO("Blade Formation: Refreshed duration and spawned {} new swords.", needed);
        } else {
            LOG_INFO("Blade Formation: Refreshed duration of {} swords.", current_count);
        }
    });

    // ID 6: Sword Array (剑阵·诛仙)
    RegisterEffect(6, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto array_ent = registry.create();
        registry.emplace<LocalLevelTag>(array_ent);
        registry.emplace<Position>(array_ent, exec.target_pos.x, exec.target_pos.y);
        registry.emplace<ColorComponent>(array_ent, PURPLE);
        
        auto& array = registry.emplace<SwordArrayComponent>(array_ent);
        array.owner = owner;
        array.duration = 5.0f;
        array.radius = 75.0f; // Reduced from 150.0f
        array.is_empowered = exec.is_empowered;

        // --- VFX: Ground Array (Shader) ---
        auto& ve = registry.emplace<VisualEffect>(array_ent);
        ve.type = VisualEffectType::AoeArray;
        ve.lifeTime = array.duration;
        ve.color = exec.is_empowered ? GOLD : PURPLE;
        
        auto& ae = registry.emplace<ArrayEffect>(array_ent);
        ae.radius = array.radius;
        ae.thickness = 0.1f;
        ae.color = ve.color;

        // --- VFX: Area Effect Particles ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        
        // Use new area effect API - particles within the radius with color gradient
        Color coreColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_CORE 
                                             : systems::InkEffectHelper::COLOR_SHADOW_CORE;
        Color edgeColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_GLOW 
                                             : systems::InkEffectHelper::COLOR_SHADOW_GLOW;
        auto areaParticles = systems::InkEffectHelper::CreateAreaEffect(
            exec.target_pos, array.radius, coreColor, edgeColor, 30, 1.0f);
        particleSys.EmitBatch(areaParticles);
        
        // Boundary ring particles - thin line of sparks
        int ringCount = 25;
        for(int i = 0; i < ringCount; ++i) {
            float angle = (float)i / ringCount * 2.0f * PI;
            float r = array.radius + (float)GetRandomValue(-5, 5);
            Vector2 pPos = { exec.target_pos.x + cosf(angle) * r, 
                            exec.target_pos.y + sinf(angle) * r };
            
            // Tangent velocity for swirling effect
            Vector2 tangent = { -sinf(angle) * 15.0f, cosf(angle) * 15.0f };
            particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                pPos, tangent, systems::InkEffectHelper::COLOR_INK_LIGHT, 1.0f));
        }

        if (exec.is_empowered) {
            array.radius *= 1.5f;
            array.damage_interval *= 0.6f; // Faster pulse frequency
            LOG_INFO("Empowered Sword Array: 1.5x Radius and faster damage pulses!");
            
            // Gold Accents on boundary
            for(int i = 0; i < 15; ++i) {
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float r = array.radius * 0.8f + (float)GetRandomValue(0, (int)(array.radius * 0.4f));
                Vector2 pPos = { exec.target_pos.x + cosf(angle) * r, 
                                exec.target_pos.y + sinf(angle) * r };
                particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                    pPos, {0, -30.0f}, systems::InkEffectHelper::COLOR_GOLD_CORE, 2.0f));
            }
        }
        
        // Talent scaling
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 6) {
                    // node 610: slow 
                    if (spec.allocated_points.contains(610) && spec.allocated_points.at(610) > 0) {
                        array.has_slow = true;
                    }
                    // node 611: armor shred
                    if (spec.allocated_points.contains(611) && spec.allocated_points.at(611) > 0) {
                        array.has_armor_shred = true;
                    }
                    // node 612: execute
                    if (spec.allocated_points.contains(612) && spec.allocated_points.at(612) > 0) {
                        array.has_execute = true;
                    }
                    break;
                }
            }
        }

        registry.emplace<SkillComponent>(array_ent, 6u, owner);
        LOG_INFO("Sword Array summoned at ({}, {}) by entity {}", exec.target_pos.x, exec.target_pos.y, (uint32_t)owner);
    });

    // ID 5: Infinite Blades (万剑归宗)
    RegisterEffect(5, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        LOG_INFO("DEBUG: RegisterEffect(5) TRIGGERED for entity {}", (uint32_t)owner);
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 5;
        chan.channel_timer = 0.5f; // Increased buffer for stability
        chan.tick_interval = 0.1f; // 10 blades per second
        chan.tick_timer = -0.01f; // FORCE instant fire on first frame
        chan.target_pos = exec.target_pos;
        chan.is_empowered = exec.is_empowered;

        // Talent: Yi Qi Bao Fa (意气爆发) - ID 520
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 5 && spec.allocated_points.contains(520) && spec.allocated_points.at(520) > 0) {
                    if (auto* intent = registry.try_get<SwordIntentComponent>(owner)) {
                        if (intent->stacks >= 5) { // Assuming 5 stacks for "Double projectiles"
                            intent->stacks -= 5;
                            chan.extra_projectiles = true;
                            LOG_INFO("Yi Qi Bao Fa: Consumed 5 intent for double projectiles.");
                        }
                    }
                    break;
                }
            }
        }
        LOG_INFO("Infinite Blades channeling started for entity {}", (uint32_t)owner);
    });

    // ID 7: Mind Blade (心剑·无影)
    RegisterEffect(7, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 7;
        chan.channel_timer = 2.0f; 
        chan.tick_interval = 0.05f; // Fast ticks
        chan.tick_timer = 0.0f; // Ensure immediate first tick
        chan.target_pos = exec.target_pos;
        chan.is_empowered = exec.is_empowered;
        
        LOG_INFO("Mind Blade channeling started for entity {}", (uint32_t)owner);
    });

    // ID 4: Blade Ward (剑气护体)
    RegisterEffect(4, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& active_effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
        
        float phys_dr = 10.0f; // Base 10% Physical DR
        
        // Talent scaling
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 4) {
                    // Node 400: Jin Zhong Zhao (Armor/Phys DR)
                    if (spec.allocated_points.contains(400)) {
                         phys_dr += spec.allocated_points.at(400) * 5.0f; // 5% extra per point
                    }
                    // Node 401: Elemental Ward (Elemental Res)
                    float elemental_res = 0.0f;
                    if (spec.allocated_points.contains(401)) {
                        elemental_res = spec.allocated_points.at(401) * 3.0f; // 3% per point
                    }
                    // Node 410: Parrying Stance (Block Chance)
                    float block_inc = 0.0f;
                    if (spec.allocated_points.contains(410)) {
                        block_inc = spec.allocated_points.at(410) * 5.0f; // 5% per point
                    }

                    // Apply talents to buff
                    BuffEffect ward_buff;
                    ward_buff.id = "blade_ward";
                    ward_buff.name = "Blade Ward";
                    ward_buff.type = BuffType::Shield;
                    ward_buff.duration = 10.0f;
                    ward_buff.remaining = 10.0f;
                    
                    ward_buff.modifiers.push_back({StatType::ResistPhysical, ModifierMode::Flat, phys_dr});
                    if (elemental_res > 0.0f) {
                        ward_buff.modifiers.push_back({StatType::ResistFire, ModifierMode::Flat, elemental_res});
                        ward_buff.modifiers.push_back({StatType::ResistCold, ModifierMode::Flat, elemental_res});
                        ward_buff.modifiers.push_back({StatType::ResistLightning, ModifierMode::Flat, elemental_res});
                    }
                    if (block_inc > 0.0f) {
                        ward_buff.modifiers.push_back({StatType::BlockChance, ModifierMode::Flat, block_inc});
                    }
                    
                    active_effects.AddOrRefresh(ward_buff);
                    break;
                }
            }
        }

        
        // Add Logic Component
        auto& ward = registry.emplace_or_replace<BladeWardComponent>(owner);
        ward.remaining = 10.0f;
        ward.sword_count = 3;
        ward.is_solidified = false;

        // Apply talents to component
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 4) {
                    if (spec.allocated_points.contains(122) && spec.allocated_points.at(122) > 0) {
                        ward.is_solidified = true;
                    }
                    // Add more per-cast talent logic here if needed
                    break;
                }
            }
        }

        // --- VFX: Shield Activation ---
        auto* pos = registry.try_get<Position>(owner);
        if (pos) {
            auto& particleSys = systems::GPUParticleSystem::Get();
            int spirals = 3;
            for (int s = 0; s < spirals; ++s) {
                 for (int i = 0; i < 20; ++i) {
                    float t = (float)i / 20.0f;
                    float angle = t * 4.0f * PI + (s * 2.0f * PI / spirals);
                    float height = t * 60.0f;
                    float radius = 40.0f * (1.0f - t * 0.5f); // Cone shape
                    
                    Vector2 pPos = { pos->x + cosf(angle) * radius, pos->y + sinf(angle) * radius - height + 30.0f };
                    
                    components::GPUParticle p;
                    p.position = pPos;
                    p.velocity = { 0, -20.0f }; 
                    p.acceleration = { 0, 0 };
                    p.color = ColorAlpha(SKYBLUE, 0.5f);
                    p.lifetime = 1.0f;
                    p.maxLifetime = 1.0f;
                    p.scale = 1.5f;
                    p.flags = 13; // Ink
                    particleSys.Emit(p);
                 }
            }
        }

        if (exec.is_empowered) {
            ward.sword_count += 3;
            ward.interception_chance *= 2.0f; // Significantly higher interception
            LOG_INFO("Empowered Blade Ward: +3 swords and 2x interception chance!");
        }
        
        registry.get_or_emplace<StatsDirty>(owner);
        LOG_INFO("Blade Ward activated for entity {}", (uint32_t)owner);
    });

    // ID 8: Blade Boomerang (御剑·回旋)
    RegisterEffect(8, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        if (!pos || !stats) return;

        const auto* skillData = SkillRegistry::Get().GetSkill(8);
        float speed = skillData ? skillData->GetParam("speed", 400.0f) : 400.0f;
        float returnTimer = skillData ? skillData->GetParam("return_timer", 0.45f) : 0.45f;
        float radius = skillData ? skillData->GetParam("radius", 40.0f) : 40.0f;
        float basePull = skillData ? skillData->GetParam("pull_strength", 300.0f) : 300.0f;
        float gravityPull = skillData ? skillData->GetParam("gravity_strength", 500.0f) : 500.0f;

        Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));

        // --- BRANCH LOGIC ---
        bool hasPull = false;
        float pullStrength = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 8) {
                    // Talent: Ci Xi (磁吸) - ID 810
                    if (spec.allocated_points.contains(810) && spec.allocated_points.at(810) > 0) {
                        hasPull = true;
                        pullStrength = basePull;
                    }
                    // Talent: Gravity Field (重力场) - ID 811
                    if (spec.allocated_points.contains(811) && spec.allocated_points.at(811) > 0) {
                        pullStrength += gravityPull;
                    }
                    break;
                }
            }
        }

        // --- VFX: Launch Ink Splash ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        auto splash = systems::InkEffectHelper::CreateInkSplash({pos->x, pos->y}, 10, 10.0f, 100.0f);
        for(auto& p : splash) {
            p.velocity = Vector2Add(p.velocity, Vector2Scale(dir, 200.0f)); // Add forward momentum
            particleSys.Emit(p);
        }

        auto proj_ent = registry.create();
        registry.emplace<LocalLevelTag>(proj_ent);
        registry.emplace<Position>(proj_ent, pos->x, pos->y);
        registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
        registry.emplace<ColorComponent>(proj_ent, ORANGE); 
        
        auto& proj = registry.emplace<Projectile>(proj_ent);
        proj.owner = owner;
        proj.speed = speed;
        proj.lifeTime = 3.0f;
        proj.radius = radius;   
        proj.pierce = true;
        proj.pierceCount = 99; 
        proj.snapshot = *stats;
        proj.hasPull = hasPull;
        proj.pullStrength = pullStrength;

        if (exec.is_empowered) {
            proj.radius *= 1.5f;
            proj.pullStrength += 300.0f;
            proj.hasPull = true; // Empowered version always has pull
            LOG_INFO("Empowered Blade Boomerang: 1.5x Radius and stronger pull!");
        }

        registry.emplace<CombatStats>(proj_ent, proj.snapshot);
        registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

        auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.owner = owner;
        bc.returnTimer = returnTimer;
        bc.phase = BoomerangComponent::Outward;
        bc.returnSpeed = speed * 1.5f;

        LOG_INFO("Blade Boomerang fired by entity {}", (uint32_t)owner);
    });

    // ID 9: Phantom Flash (绝影闪)
    RegisterEffect(9, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        if (!pos) return;

        const auto* skillData = SkillRegistry::Get().GetSkill(9);
        float dashSpeed = skillData ? skillData->GetParam("dash_speed", 500.0f) : 500.0f;
        float dashDist = skillData ? skillData->GetParam("dash_dist", 50.0f) : 50.0f;

        // 1. Dash backwards
        Vector2 dir = Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));
        
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * dashSpeed; 
            vel->vy = dir.y * dashSpeed;
        }
        
        if (auto* dash = registry.try_get<DashComponent>(owner)) {
            dash->isDashing = true;
            dash->dashTimer = dashDist / dashSpeed; // Calculate time based on distance/speed
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = dashSpeed; 
        }

        // --- VISUAL EFFECTS: Dash Effect + Gold Mark ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        Vector2 startPos = { pos->x, pos->y };
        
        // Use new dash effect API - particles at start position spreading along dash direction
        auto dashParticles = systems::InkEffectHelper::CreateDashEffect(
            startPos, dir, systems::InkEffectHelper::COLOR_SHADOW_CORE, 
            dashDist, 20);
        particleSys.EmitBatch(dashParticles);
        
        // Gold Sword Mark (Burst of sparks at dash origin)
        for (int i = 0; i < 8; ++i) {
            Vector2 gVel = { (float)GetRandomValue(-80, 80), (float)GetRandomValue(-80, 80) };
            particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                startPos, gVel, systems::InkEffectHelper::COLOR_GOLD_CORE, 1.5f));
        }

        // 2. Add Counter State
        auto& pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
        pf.counter_window = 0.5f;
        pf.triggered = false;

        LOG_INFO("Phantom Flash: Counter state active for entity {}", (uint32_t)owner);
    });
}

void SkillSystem::Update(entt::registry& registry, systems::SpatialHashGrid& grid, float dt, tf::Executor* executor) {
    UpdateCooldowns(registry, dt);
    UpdateStates(registry, dt);
    UpdateSwordIntent(registry, dt);
    
    // Update Shadows
    auto shadow_view = registry.view<ShadowComponent>();
    std::vector<entt::entity> expired_shadows;
    for (auto entity : shadow_view) {
        auto& shadow = shadow_view.get<ShadowComponent>(entity);
        
        if (!shadow.triggered) {
            shadow.delay -= dt;
            if (shadow.delay <= 0.0f) {
                registry.emplace_or_replace<CombatStats>(entity, shadow.snapshot.stats);
                ShadowCast(registry, entity, shadow.snapshot.skill_id, shadow.snapshot.position, shadow.snapshot.target_pos);
                shadow.triggered = true;
            }
        }

        bool is_expired = false;
        if (shadow.triggered) {
            // If already triggered, disappear as soon as casting finishes
            bool still_casting = false;
            auto exec_view = registry.view<SkillExecution>();
            for(auto exec_ent : exec_view) {
                if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                    still_casting = true;
                    break;
                }
            }
            if (!still_casting) is_expired = true;
        }

        shadow.lifetime -= dt;
        if (shadow.lifetime <= 0.0f) is_expired = true;

        if (is_expired) {
            expired_shadows.push_back(entity);
        }
    }
    for (auto e : expired_shadows) registry.destroy(e);

    // Update Blade Formation (ID 3)
    auto formation_view = registry.view<BladeFormationComponent, Position>();
    for (auto entity : formation_view) {
        auto& formation = formation_view.get<BladeFormationComponent>(entity);
        const auto& pos = formation_view.get<Position>(entity);

        // Update current_swords count from actual entities
        int count = 0;
        auto swordView = registry.view<SpiritSwordTag, SummonComponent>();
        for (auto swordEnt : swordView) {
            if (swordView.get<SummonComponent>(swordEnt).owner == entity) {
                count++;
            }
        }
        formation.current_swords = count;

        // Talent: Ling Jian Hu Ti (灵剑护体) - ID 320
        if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 3 && spec.allocated_points.contains(320) && spec.allocated_points.at(320) > 0) {
                    auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(entity);
                    BuffEffect bladeDR;
                    bladeDR.id = "ling_jian_hu_ti";
                    bladeDR.name = "Ling Jian Hu Ti";
                    bladeDR.type = BuffType::Shield;
                    bladeDR.duration = 0.2f; // Short duration, refreshed every update
                    bladeDR.remaining = 0.2f;
                    
                    float dr_per_sword = 2.0f * spec.allocated_points.at(320); 
                    float total_dr = formation.current_swords * dr_per_sword;
                    
                    bladeDR.modifiers.push_back({StatType::ResistAll, ModifierMode::Flat, total_dr}); 
                    effects.AddOrRefresh(bladeDR);
                    break;
                }
            }
        }
    }

    // Update Sword Array (ID 6)
    auto array_view = registry.view<SwordArrayComponent, Position>();
    std::vector<entt::entity> expired_arrays;
    for (auto entity : array_view) {
        auto& array = array_view.get<SwordArrayComponent>(entity);
        const auto& pos = array_view.get<Position>(entity);

        array.duration -= dt;
        if (array.duration <= 0.0f) {
            expired_arrays.push_back(entity);
            continue;
        }

        // --- Continuous VFX: Sword Rain ---
        auto& particleSys = systems::GPUParticleSystem::Get();
        if (GetRandomValue(0, 100) < 15) { // Chance per frame to drop a sword
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float dist = sqrtf((float)GetRandomValue(0, 1000) / 1000.0f) * array.radius;
            Vector2 dropPos = { pos.x + cosf(angle) * dist, pos.y + sinf(angle) * dist };
            
            // Falling Sword Visual
            components::GPUParticle p;
            p.position = { dropPos.x, dropPos.y - 100.0f }; // Start from above
            p.velocity = { 0, 800.0f }; // Fast fall
            p.acceleration = { 0, 0 };
            p.color = array.is_empowered ? GOLD : ColorAlpha(SKYBLUE, 0.7f);
            p.lifetime = 0.125f; // Hits ground fast
            p.maxLifetime = 0.125f;
            p.scale = 2.0f;
            p.flags = 2; // Spark/Glow
            particleSys.Emit(p);

            // Ground Impact Splash
            auto splash = systems::InkEffectHelper::CreateInkSplash(dropPos, 4, 5.0f, 40.0f);
            for(auto& sp : splash) {
                sp.color = p.color;
                particleSys.Emit(sp);
            }
        }

        array.damage_timer -= dt;
        if (array.damage_timer <= 0.0f) {
            // Pulsing damage
            // Visual Effect: Ring of particles (Ink Shockwave)
            std::vector<components::GPUParticle> particles;
            int pCount = 60;
            auto& particleSys = systems::GPUParticleSystem::Get();
            
            // 1. Ink Ring expanding out slightly
            for(int i=0; i<pCount; ++i) {
                float angle = (float)i / pCount * 2.0f * PI;
                Vector2 offset = { cosf(angle) * (array.radius * 0.9f), sinf(angle) * (array.radius * 0.9f) };
                Vector2 pPos = { pos.x + offset.x, pos.y + offset.y };
                
                components::GPUParticle p;
                p.position = pPos;
                p.velocity = { offset.x * 2.0f, offset.y * 2.0f }; // Move outward
                p.acceleration = { 0, 0 };
                p.color = systems::InkEffectHelper::COLOR_INK_DARK;
                p.lifetime = 0.4f;
                p.maxLifetime = 0.4f;
                p.scale = 1.5f; 
                p.flags = 13; // Ink
                particles.push_back(p);
            }
            
            // 2. Inner implosion (Gold if empowered)
             if (array.radius > 50.0f) { // Only if reasonably large
                for(int i=0; i<10; ++i) {
                    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                    float dist = (float)GetRandomValue(0, (int)array.radius);
                    Vector2 pPos = { pos.x + cosf(angle) * dist, pos.y + sinf(angle) * dist };
                    
                    if (array.is_empowered) {
                        auto p = systems::InkEffectHelper::CreateGoldParticle(pPos, {0,0}, 0.8f);
                        p.lifetime = 0.5f;
                        particleSys.Emit(p);
                    } else {
                        particleSys.Emit(systems::InkEffectHelper::CreateInkTrail(pPos, {0,0}, 0.8f, 0.5f));
                    }
                }
             }

            systems::GPUParticleSystem::Get().EmitBatch(particles);

            // OPTIMIZATION: Collect unique targets and skip already killed ones
            std::vector<entt::entity> targets;
            grid.query(pos, array.radius, [&](entt::entity target) {
                if (target == array.owner || target == entity) return;
                // Check valid, enemy, and NOT already killed
                if (!registry.valid(target) || registry.all_of<KilledTag>(target) || !registry.all_of<EnemyTag, HealthComponent, Position>(target)) return;

                const auto& tPos = registry.get<Position>(target);
                float dx = tPos.x - pos.x;
                float dy = tPos.y - pos.y;
                if (dx*dx + dy*dy <= array.radius * array.radius) {
                    targets.push_back(target);
                }
            });

                if (!targets.empty()) {
                    // Deduplicate targets list
                    std::sort(targets.begin(), targets.end());
                    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

                    // Apply Talent Buffs/Status
                    for (auto target_ent : targets) {
                         auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(target_ent);
                         
                         if (array.has_slow) {
                             BuffEffect slow;
                             slow.id = "array_slow";
                             slow.name = "Sword Array Slow";
                             slow.type = BuffType::SpeedDown;
                             slow.duration = 1.0f;
                             slow.remaining = 1.0f;
                             slow.is_debuff = true;
                             
                             StatModifier m;
                             m.type = StatType::MoveSpeed;
                             m.mode = ModifierMode::PercentAdd;
                             m.value = -10.0f;
                             m.required_tags = Tag::None;
                             m.source = ModifierSource::Buff;
                             slow.modifiers.push_back(m);
                             
                             effects.AddOrRefresh(slow);
                         }

                         if (array.has_armor_shred) {
                             BuffEffect shred;
                             shred.id = "array_armor_shred";
                             shred.name = "Armor Shred";
                             shred.type = BuffType::DefenseDown;
                             shred.duration = 1.0f;
                             shred.remaining = 1.0f;
                             shred.is_debuff = true;
                             
                             StatModifier m;
                             m.type = StatType::Armor;
                             m.mode = ModifierMode::PercentAdd;
                             m.value = -5.0f;
                             m.required_tags = Tag::None;
                             m.source = ModifierSource::Buff;
                             shred.modifiers.push_back(m);
                             
                             effects.AddOrRefresh(shred);
                         }
                    }

                    DamagePool pool;
                    pool.Add(Tag::Physical, 10.0f); 
                    
                    // Simple Execute Logic for Node 612
                    DamagePipeline::CalculateBatch(registry, array.owner, targets, 6, pool, Tag::Area | Tag::Hit, entity, executor);
                    
                    if (array.has_execute) {
                        for (auto target_ent : targets) {
                            if (auto* hp = registry.try_get<HealthComponent>(target_ent)) {
                                if (hp->current / hp->max < 0.15f) {
                                    CombatSystem::ApplyDamage(registry, target_ent, hp->max * 0.1f, array.owner, false, true);
                                }
                            }
                        }
                    }
                }
            
            array.damage_timer = array.damage_interval;
        }
    }
    for(auto e : expired_arrays) registry.destroy(e);

    // Update Channeling (ID 5 & 7)
    auto chan_view = registry.view<ChannelingComponent, Position>();
    for (auto entity : chan_view) {
        auto& chan = chan_view.get<ChannelingComponent>(entity);
        const auto& pos = chan_view.get<Position>(entity);
        
        // For skill 7, target_pos is updated in InputSystem.cpp to follow mouse accurately.
        
        // 1. Duration Limit (5s hard cap)
        chan.total_duration += dt;
        if (chan.total_duration >= 5.0f) {
            registry.remove<ChannelingComponent>(entity);
            continue; 
        }

        chan.channel_timer -= dt;
        if (chan.channel_timer <= 0.0f) {
            registry.remove<ChannelingComponent>(entity);
            continue;
        }

        chan.tick_timer -= dt;

        // Continuous VFX for Mind Blade (ID 7) - Threads
        if (chan.skill_id == 7) {
             static int s_skill7FrameCount = 0;
             s_skill7FrameCount++;
             if (s_skill7FrameCount % 30 == 1) { // Log every 30 frames (~0.5s at 60fps)
                 LOG_INFO("[DEBUG-SKILL7] Continuous VFX active. Entity pos=({:.1f},{:.1f}), target=({:.1f},{:.1f}), tick_timer={:.3f}", 
                     pos.x, pos.y, chan.target_pos.x, chan.target_pos.y, chan.tick_timer);
             }
             
             auto& particleSys = systems::GPUParticleSystem::Get();
             Vector2 dir = Vector2Normalize(Vector2Subtract(chan.target_pos, {pos.x, pos.y}));
             
             // Main Ink Thread
             if (GetRandomValue(0, 100) < 50) { // 50% chance per frame
                 components::GPUParticle p = systems::InkEffectHelper::CreateInkTrail({pos.x, pos.y}, Vector2Scale(dir, -50.0f), 0.5f, 0.4f);
                 p.velocity = Vector2Scale(dir, 1500.0f); // Very fast
                 p.color = ColorAlpha(systems::InkEffectHelper::COLOR_INK_LIGHT, 0.3f); // Transparent
                 p.scale = 0.8f; // Thin
                 particleSys.Emit(p);
             }
             
             // Gold Core (Empowered)
             if (chan.is_empowered && GetRandomValue(0, 100) < 30) {
                 components::GPUParticle p = systems::InkEffectHelper::CreateGoldParticle({pos.x, pos.y}, Vector2Scale(dir, 1500.0f), 0.4f);
                 p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
                 particleSys.Emit(p);
             }
        }

        if (chan.tick_timer <= 0.0f) {
            if (chan.skill_id == 5) {
                // Update GPUParticleSystem logic here too if needed
                // ... 
                
                // Talent: Qi Ding Shen Xian (气定神闲) - ID 501
                if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
                    for (const auto& spec : active->specialized_slots) {
                        if (spec.skill_id == 5 && spec.allocated_points.contains(501) && spec.allocated_points.at(501) > 0) {
                            auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(entity);
                            BuffEffect chanDR;
                            chanDR.id = "infinite_blades_dr";
                            chanDR.duration = 0.5f;
                            chanDR.remaining = 0.5f;
                            chanDR.modifiers.push_back({StatType::ResistAll, ModifierMode::Flat, 5.0f * spec.allocated_points.at(501)}); 
                            effects.AddOrRefresh(chanDR);
                            break;
                        }
                    }
                }

                // Infinite Blades: Chaotic "Grass Script" Strokes
                // LOG_TRACE("DEBUG: Channeling Tick ID 5 for entity {}", (uint32_t)entity);
                auto& particleSys = systems::GPUParticleSystem::Get();
                std::vector<components::GPUParticle> particles;
                
                // Burst of strokes
                for(int i=0; i<8; ++i) { 
                     float pAngle = (float)GetRandomValue(0, 360) * DEG2RAD;
                     Vector2 pDir = { cosf(pAngle), sinf(pAngle) };
                     
                     // "Grass Script" = Fast, curving ink strokes
                     components::GPUParticle p = systems::InkEffectHelper::CreateInkTrail({pos.x, pos.y}, {0,0}, 1.0f, 0.6f);
                     float speed = (float)GetRandomValue(400, 800);
                     p.velocity = { pDir.x * speed, pDir.y * speed };
                     
                     // Tangential acceleration for curve
                     Vector2 tangent = { -pDir.y, pDir.x };
                     float curveStrength = (float)GetRandomValue(-1000, 1000);
                     p.acceleration = { tangent.x * curveStrength, tangent.y * curveStrength };
                     
                     p.scale = (float)GetRandomValue(15, 30) / 10.0f; // Varied thickness
                     
                     if (chan.is_empowered && GetRandomValue(0, 100) < 40) {
                         p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
                         p.flags |= 2; // Glow/Spark
                     } else {
                         p.color = systems::InkEffectHelper::COLOR_INK_DARK;
                     }
                     particles.push_back(p);
                     
                     // Extra Projectiles (Talent 520)
                     if (chan.extra_projectiles) {
                         components::GPUParticle extra = p;
                         extra.velocity = Vector2Rotate(p.velocity, 0.2f); // Slight offset
                         extra.color = ColorAlpha(p.color, 0.6f);
                         particles.push_back(extra);
                     }
                }
                
                // Screen effect: Large faint ink wash
                if (GetRandomValue(0, 100) < 30) {
                    components::GPUParticle p;
                    p.position = { pos.x + (float)GetRandomValue(-400, 400), pos.y + (float)GetRandomValue(-300, 300) };
                    p.velocity = { 0, 0 };
                    p.acceleration = { 0, 0 };
                    p.color = ColorAlpha(systems::InkEffectHelper::COLOR_INK_LIGHT, 0.05f);
                    p.lifetime = 1.0f;
                    p.maxLifetime = 1.0f;
                    p.scale = 20.0f; // Huge
                    p.flags = 13; // Ink
                    particles.push_back(p);
                }

                particleSys.EmitBatch(particles);

                // Logic: Cast Rending Wave (ID 2) towards target (Mouse)
                Vector2 dir = Vector2Normalize(Vector2Subtract(chan.target_pos, {pos.x, pos.y}));
                // Add some random spread
                float spread = (float)GetRandomValue(-30, 30) * DEG2RAD; 
                Vector2 fireDir = Vector2Rotate(dir, spread);

                Vector2 strike_target = { pos.x + fireDir.x * 250.0f, pos.y + fireDir.y * 250.0f };
                
                auto exec_ent = registry.create();
                registry.emplace<LocalLevelTag>(exec_ent);
                registry.emplace<ShadowCastTag>(exec_ent);
                auto& exec = registry.emplace<SkillExecution>(exec_ent);
                exec.skill_id = 2; // Rending Wave
                exec.owner = entity;
                exec.state = SkillState::Preparing; // Must be Preparing to trigger callback
                exec.timer = 0.05f;
                exec.target_pos = strike_target;
                exec.is_empowered = chan.is_empowered;
                
                if (auto* stats = registry.try_get<CombatStats>(entity)) {
                    exec.has_snapshot = true;
                    exec.snapshot.stats = *stats;
                    exec.snapshot.skill_id = 2;
                }

                // Talent: Yi Qi Bao Fa (意气爆发) - ID 520: Extra Projectile
                if (chan.extra_projectiles) {
                    auto extra_exec_ent = registry.create();
                    registry.emplace<LocalLevelTag>(extra_exec_ent);
                    registry.emplace<ShadowCastTag>(extra_exec_ent);
                    auto& extra_exec = registry.emplace<SkillExecution>(extra_exec_ent);
                    extra_exec.skill_id = 2;
                    extra_exec.owner = entity;
                    extra_exec.state = SkillState::Preparing;
                    extra_exec.timer = 0.05f;
                    
                    // Slightly varied target for extra projectle
                    extra_exec.target_pos = Vector2Add(strike_target, { (float)GetRandomValue(-20, 20), (float)GetRandomValue(-20, 20) });
                    extra_exec.is_empowered = chan.is_empowered;
                    
                    if (auto* stats = registry.try_get<CombatStats>(entity)) {
                        extra_exec.has_snapshot = true;
                        extra_exec.snapshot.stats = *stats;
                        extra_exec.snapshot.skill_id = 2;
                        for (auto& mult : extra_exec.snapshot.stats.damage_multipliers) mult *= 0.8f; // 80% damage for extra
                    }
                    LOG_DEBUG("Yi Qi Bao Fa: Spawned EXTRA Rending Wave execution.");
                }

            } else if (chan.skill_id == 7) {
                // Heart Sword: Shadowless (Spatial Cut)
                LOG_INFO("[DEBUG-SKILL7] TICK TRIGGERED! Emitting Spatial Cut VFX at tick_timer={:.3f}", chan.tick_timer);
                
                // 1. Calculate Cut Position (Clamped to Range)
                Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
                float dist = Vector2Length(diff);
                float max_range = 350.0f;
                Vector2 cutPos = chan.target_pos;
                Vector2 dir = {1.0f, 0.0f}; // Default if dist is 0

                if (dist > 0.001f) {
                    dir = Vector2Scale(diff, 1.0f / dist); // Normalize
                    if (dist > max_range) {
                        cutPos = { pos.x + dir.x * max_range, pos.y + dir.y * max_range };
                    }
                } else {
                    cutPos = { pos.x + 50.0f, pos.y }; 
                }
                
                LOG_INFO("[DEBUG-SKILL7] Cut position: ({:.1f},{:.1f}), dir: ({:.2f},{:.2f})", cutPos.x, cutPos.y, dir.x, dir.y);

                // 2. VFX: Spatial Cut (Perpendicular Line)
                auto& particleSys = systems::GPUParticleSystem::Get();
                Vector2 perp = { -dir.y, dir.x }; 
                float cutWidth = 100.0f;
                
                int emittedCount = 0;
                for(int i = 0; i < 15; ++i) {
                    float t = (float)i / 14.0f; 
                    float offset = (t - 0.5f) * cutWidth;
                    Vector2 pPos = { cutPos.x + perp.x * offset, cutPos.y + perp.y * offset };
                    
                    // Core Spark - Golden flash along the cut line
                    components::GPUParticle spark;
                    spark.position = pPos;
                    spark.velocity = Vector2Scale(dir, 60.0f);
                    spark.acceleration = { 0.0f, 0.0f };
                    spark.color = GOLD;
                    spark.scale = 4.0f;  // Smaller spark
                    spark.lifetime = 0.3f;
                    spark.maxLifetime = 0.3f;
                    spark.flags = 2;  // Spark/diamond shape
                    spark.growthRate = -6.0f; // Shrink to nothing
                    particleSys.Emit(spark);

                    // Outer Glow - Softer surrounding effect
                    components::GPUParticle glow;
                    glow.position = pPos;
                    glow.velocity = Vector2Scale(dir, 20.0f);
                    glow.acceleration = { 0.0f, 0.0f };
                    glow.color = ColorAlpha(ORANGE, 0.5f);
                    glow.scale = 6.0f;  // Smaller glow
                    glow.lifetime = 0.4f;
                    glow.maxLifetime = 0.4f;
                    glow.flags = 1;  // Soft glow
                    glow.growthRate = 3.0f; // Expand slightly
                    particleSys.Emit(glow);
                    
                    emittedCount += 2;
                }
                
                LOG_DEBUG("[MindBlade] Emitted {} VFX particles at ({:.1f},{:.1f})",
                    emittedCount, cutPos.x, cutPos.y);

                // 3. Logic: Spawn "Cut" Hitbox (Stationary Projectile)
                auto exec_ent = registry.create();
                registry.emplace<LocalLevelTag>(exec_ent);
                registry.emplace<Position>(exec_ent, cutPos.x, cutPos.y);
                registry.emplace<Velocity>(exec_ent, 0.0f, 0.0f);
                
                auto& proj = registry.emplace<Projectile>(exec_ent);
                proj.owner = entity;
                proj.radius = 60.0f; // AoE size
                proj.speed = 0.0f;
                proj.lifeTime = 0.1f; // Instant hit (one frame)
                proj.pierce = true;
                proj.pierceCount = 999;
                
                // Link stats
                if (auto* stats = registry.try_get<CombatStats>(entity)) {
                     // We can use SkillExecution to snapshot or just pass stats via new Projectile system features?
                     // ProjectileSystem reads owner's stats via DamagePipeline usually,
                     // OR it reads 'proj.snapshot' if we add it to Projectile component?
                     // Looking at ProjectileSystem.cpp:204, it reads registry.get<CombatStats>(entity) aka Projectile Entity?
                     // No, "damage_attacker = proj.owner".
                     // So as long as owner has stats, we are good.
                }

                auto& sc = registry.emplace<SkillComponent>(exec_ent);
                sc.skill_id = 7;

                chan.tick_timer = chan.tick_interval; // Reset tick
            }
            chan.tick_timer = chan.tick_interval; // Redundant but safe logic structure warning (fixed by above)
        }
    }

    // Update Blade Ward
    auto ward_view = registry.view<BladeWardComponent>();
    for (auto entity : ward_view) {
        auto& ward = ward_view.get<BladeWardComponent>(entity);
        ward.remaining -= dt;
        if (ward.remaining <= 0.0f) {
            registry.remove<BladeWardComponent>(entity);
            continue;
        }

        // Keep the buff refreshed if we want it to stay for the duration
        // Actually, the buff has its own duration in ActiveEffectsComponent.
        // We just need to sync them or let them be independent.
    }

    // Update Phantom Flash
    auto pf_view = registry.view<PhantomFlashComponent>();
    for (auto entity : pf_view) {
        auto& pf = pf_view.get<PhantomFlashComponent>(entity);
        pf.counter_window -= dt;
        if (pf.counter_window <= 0.0f || pf.triggered) {
            registry.remove<PhantomFlashComponent>(entity);
        }
    }
}

void SkillSystem::RegisterEffect(uint32_t skill_id, CastCallback callback) {
    s_skill_callbacks[skill_id] = callback;
}

void SkillSystem::AddPreCastHook(SkillHook hook) {
    s_pre_cast_hooks.push_back(hook);
}

void SkillSystem::AddPostCastHook(SkillHook hook) {
    s_post_cast_hooks.push_back(hook);
}

void SkillSystem::ClearHooks() {
    s_pre_cast_hooks.clear();
    s_post_cast_hooks.clear();
}

bool SkillSystem::ShadowCast(entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 position, Vector2 target_pos) {
    const auto* data = SkillRegistry::Get().GetSkill(skill_id);
    if (!data) return false;

    entt::entity shadow = owner;
    
    if (!registry.any_of<ShadowComponent>(owner) && !registry.any_of<ShadowLifetime>(owner)) {
        shadow = registry.create();
        registry.emplace<LocalLevelTag>(shadow);
        registry.emplace<Position>(shadow, position.x, position.y);
        registry.emplace<Velocity>(shadow, 0.0f, 0.0f); // Ensure it has velocity for grid
        registry.emplace<AnimationStateComponent>(shadow);
        registry.emplace<ShadowLifetime>(shadow, 1.0f);
        
        if (registry.any_of<SpiritSwordTag>(owner)) {
            registry.emplace<SpiritSwordTag>(shadow);
        }
    }

    auto exec_ent = registry.create();
    registry.emplace<LocalLevelTag>(exec_ent);
    auto& exec = registry.emplace<SkillExecution>(exec_ent);
    exec.skill_id = skill_id;
    exec.owner = shadow; 
    exec.state = SkillState::Preparing; 
    exec.timer = 0.05f;
    exec.target_pos = target_pos;
    
    // Check if the caller provided a snapshot (either via ShadowComponent or manual call)
    if (auto* sc = registry.try_get<ShadowComponent>(owner)) {
        exec.has_snapshot = true;
        exec.snapshot = sc->snapshot;
        exec.is_empowered = sc->snapshot.is_empowered;
        registry.emplace_or_replace<CombatStats>(shadow, sc->snapshot.stats);
    } else if (auto* stats = registry.try_get<CombatStats>(owner)) {
        // Fallback: Use current owner stats
        exec.has_snapshot = true;
        exec.snapshot.stats = *stats;
        exec.snapshot.skill_id = skill_id;
        // No empowerment by default for non-snapshot casts unless we want it?
        
        registry.emplace_or_replace<CombatStats>(shadow, *stats);
    }

    registry.emplace<ShadowCastTag>(exec_ent);
    LOG_INFO("Shadow casting skill: {}", data->name_key);
    return true;
}

void SkillSystem::UpdateSwordIntent(entt::registry& registry, float dt) {
    auto view = registry.view<SwordIntentComponent>();
    for (auto entity : view) {
        auto& intent = view.get<SwordIntentComponent>(entity);
        if (intent.stacks > 0) {
            intent.time_since_last_gain += dt;

            if (intent.time_since_last_gain >= intent.grace_period) {
                intent.decay_tick_timer += dt;
                while (intent.decay_tick_timer >= intent.decay_interval) {
                    if (intent.stacks > 0) {
                        intent.stacks--;
                        LOG_DEBUG("Entity {} Sword Intent decayed to {} (Grace period expired)", (uint32_t)entity, intent.stacks);
                    }
                    intent.decay_tick_timer -= intent.decay_interval;
                    if (intent.stacks <= 0) {
                        intent.decay_tick_timer = 0.0f;
                        break;
                    }
                }
            } else {
                intent.decay_tick_timer = 0.0f;
            }

            // Visuals (Only if window is ready to avoid RNG pollution in headless tests)
            if (IsWindowReady() && registry.all_of<Position>(entity)) {
                const auto& pos = registry.get<Position>(entity);
                if (GetRandomValue(0, 100) < intent.stacks * 3) {
                    components::GPUParticle p;
                    p.position = { pos.x + GetRandomValue(-15, 15), pos.y + GetRandomValue(-30, 0) };
                    p.velocity = { 0, -30.0f };
                    p.acceleration = { 0, 0 };
                    p.color = WHITE; // Or slight blue tint
                    p.lifetime = 0.5f;
                    p.maxLifetime = 0.5f;
                    p.scale = 1.0f + (intent.stacks * 0.1f);
                    p.flags = 2; // Spark
                    systems::GPUParticleSystem::Get().Emit(p);
                }
            }

        } else {
            intent.time_since_last_gain = 0.0f;
            intent.decay_tick_timer = 0.0f;
        }
    }
}

void SkillSystem::OnSkillHit(entt::registry& registry, entt::entity attacker, entt::entity target, uint32_t skill_id, Tag hit_tags, bool is_crit) {
    if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
        bool gainIntent = HasTag(hit_tags, Tag::Melee);
        
        // Critical hits always generate Intent (for Sword skills)
        if (is_crit && (skill_id != 0)) { // Assuming basic attacks (skill 0) might count? Or just mapped skills?
             // Let's assume any critical hit from a skill generates intent for a Blade Ascendant
             gainIntent = true;
        }
        
        // Talent: Rending Wave Intent Scaling (ID 230)
        if (skill_id == 2) {
            if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 2 && spec.allocated_points.contains(230) && spec.allocated_points.at(230) > 0) {
                        if (GetRandomValue(0, 100) < 50) gainIntent = true; 
                        break;
                    }
                }
            }
        }

        // --- Talent: Jian Yi Ying Ying (剑意盈盈) - ID 121 ---
        if (skill_id == 1) {
            if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 1 && spec.allocated_points.contains(121) && spec.allocated_points.at(121) > 0) {
                        // 25% chance per point on hit
                        if (GetRandomValue(0, 100) < 25 * spec.allocated_points.at(121)) {
                            gainIntent = true;
                        }
                        break;
                    }
                }
            }
        }

        // --- Talent: Shadow Kill Array (ID 124) ---
        if (skill_id == 1 && is_crit) {
             if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 1 && spec.allocated_points.contains(124) && spec.allocated_points.at(124) > 0) {
                        // 20% chance per point on crit to trigger Shadow Kill Array
                        if (GetRandomValue(0, 100) < 20 * spec.allocated_points.at(124)) {
                            registry.get_or_emplace<ShadowKillArrayReady>(attacker);
                            LOG_INFO("Shadow Kill Array READY for entity {}", (uint32_t)attacker);
                        }
                        break;
                    }
                }
            }
        }

        // Talent: Blade Boomerang Ju Ling (ID 813) - 15% chance to gain intent per pull
        if (skill_id == 8) {
            if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
                for (const auto& spec : active->specialized_slots) {
                    if (spec.skill_id == 8 && spec.allocated_points.contains(813) && spec.allocated_points.at(813) > 0) {
                        if (GetRandomValue(0, 100) < 15 * spec.allocated_points.at(813)) gainIntent = true;
                        break;
                    }
                }
            }
        }

        if (gainIntent) {
            if (intent->stacks < intent->max_stacks) {
                intent->stacks++;
                LOG_DEBUG("Entity {} Sword Intent increased to {}", (uint32_t)attacker, intent->stacks);
            }
            intent->time_since_last_gain = 0.0f; 
            intent->decay_tick_timer = 0.0f;
        }
    }

    // --- Talent: Blade Formation Hits ---
    if (skill_id == 2 || skill_id == 3) { // Spirit Swords use ID 2
        if (auto* formation = registry.try_get<BladeFormationComponent>(attacker)) {
            // Node 311: Shockwave 
            if (is_crit && formation->shockwave_on_crit) {
                 const auto& tPos = registry.get<Position>(target);
                 auto& particleSys = systems::GPUParticleSystem::Get();
                 auto splash = systems::InkEffectHelper::CreateInkSplash({tPos.x, tPos.y}, 12, 10.0f, 150.0f);
                 for(auto& p : splash) {
                     p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
                     particleSys.Emit(p);
                 }
                 CombatSystem::ApplyDamage(registry, target, 15.0f, attacker, false, true);
                 LOG_INFO("Blade Formation: Shockwave triggered on entity {}", (uint32_t)target);
            }

            // Node 321: Qi Jin Hui Liu (Mana on hit)
            if (formation->mana_on_hit) {
                if (auto* attackerStats = registry.try_get<CombatStats>(attacker)) {
                    attackerStats->mana = std::min(attackerStats->max_mana, attackerStats->mana + 2.0f);
                    LOG_DEBUG("Blade Formation (321): Restore 2 mana on hit");
                }
            }
        }
    }

    if (auto* stats = registry.try_get<CombatStats>(attacker)) {
        float mana_gain = StatsSystem::GetStatWithTags(registry, attacker, StatType::ManaOnHit, hit_tags);
        if (mana_gain > 0.0f) {
            stats->mana += mana_gain;
            if (stats->mana > stats->max_mana) stats->mana = stats->max_mana;
            LOG_DEBUG("Entity {} restored {:.1f} mana on hit", (uint32_t)attacker, mana_gain);
        }
    }
}

void SkillSystem::UpdateCooldowns(entt::registry& registry, float dt) {
    auto view = registry.view<ActiveSkillsComponent>();
    for (auto entity : view) {
        auto& active = view.get<ActiveSkillsComponent>(entity);
        for (auto& slot : active.slots) {
            if (slot.id == 0) continue;
            
            const auto* data = SkillRegistry::Get().GetSkill(slot.id);
            if (!data) continue;

            if (slot.current_charges < data->max_charges) {
                slot.cooldown -= dt;
                if (slot.cooldown <= 0.0f) {
                    slot.current_charges++;
                    if (slot.current_charges < data->max_charges) {
                        auto* stats = registry.try_get<CombatStats>(entity);
                        float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
                        float cdr = StatsSystem::GetStatWithTags(registry, entity, StatType::CooldownReduction, data->tags, slot.id) / 100.0f;
                        slot.cooldown = (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
                    } else {
                        slot.cooldown = 0.0f;
                    }
                }
            }
        }
    }
}

void SkillSystem::UpdateStates(entt::registry& registry, float dt) {
    auto view = registry.view<SkillExecution>();
    for (auto entity : view) {
        auto& exec = view.get<SkillExecution>(entity);
        exec.timer -= dt;

        if (exec.timer <= 0.0f) {
            switch (exec.state) {
                case SkillState::Preparing:
                    for (auto& hook : s_pre_cast_hooks) {
                        hook(registry, entity, exec);
                    }
                    exec.state = SkillState::Casting;
                    exec.timer = 0.05f; 
                    if (s_skill_callbacks.contains(exec.skill_id)) {
                        s_skill_callbacks[exec.skill_id](registry, exec.owner, exec);
                    } else {
                        LOG_WARN("UpdateStates: No callback found for skill ID {} on entity {}", exec.skill_id, (uint32_t)entity);
                    }
                    break;
                case SkillState::Casting:
                    exec.state = SkillState::Settle;
                    exec.timer = 0.1f; 
                    for (auto& hook : s_post_cast_hooks) {
                        hook(registry, entity, exec);
                    }
                    break;
                case SkillState::Settle:
                    if (auto* anim = registry.try_get<AnimationStateComponent>(entity)) {
                        anim->state = EntityAnimState::Idle;
                    }
                    registry.remove<SkillExecution>(entity);
                    continue; 
                default:
                    registry.remove<SkillExecution>(entity);
                    continue;
            }
        }

        if (auto* anim = registry.try_get<AnimationStateComponent>(entity)) {
            switch (exec.state) {
                case SkillState::Preparing: anim->state = EntityAnimState::SkillWindup; break;
                case SkillState::Casting:   anim->state = EntityAnimState::SkillCasting; break;
                case SkillState::Settle:    anim->state = EntityAnimState::SkillRecovery; break;
                default: break;
            }
            anim->state_timer = exec.timer;
        }
    }
}

bool SkillSystem::TryCast(entt::registry& registry, entt::entity entity, int slot_index, Vector2 target_pos) {
    auto* active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active || slot_index < 0 || slot_index >= (int)active->slots.size()) return false;

    auto& slot = active->slots[slot_index];
    if (slot.id == 0) return false;

    if (registry.any_of<SkillExecution>(entity)) return false; 

    const auto* data = SkillRegistry::Get().GetSkill(slot.id);
    if (!data) return false;

    if (slot.current_charges <= 0) return false;

    auto* stats = registry.try_get<CombatStats>(entity);
    float rcr = stats ? StatsSystem::GetStatWithTags(registry, entity, StatType::ResourceCostReduction, data->tags, slot.id) / 100.0f : 0.0f;
    float base_cost = data->mana_cost * (1.0f - std::min(0.9f, rcr));

    // --- Shadow Kill Array (ID 124) Duplication Logic ---
    bool shadow_duplicate = false;
    if (registry.any_of<ShadowKillArrayReady>(entity)) {
        bool excluded = HasTag(data->tags, Tag::Movement) || 
                        HasTag(data->tags, Tag::Buff) || 
                        HasTag(data->tags, Tag::Aura) || 
                        HasTag(data->tags, Tag::Channeled);
        
        if (!excluded) {
            auto* pStats = registry.try_get<PlayerStats>(entity);
            float currentTime = (float)GetTime();
            if (pStats && (currentTime - pStats->last_shadow_trigger_time >= 3.0f)) {
                float extra_cost = base_cost * 0.5f;
                if (stats && stats->mana >= (base_cost + extra_cost)) {
                    shadow_duplicate = true;
                }
            }
        }
    }

    if (stats) {
        float total_cost = base_cost;
        if (shadow_duplicate) total_cost += base_cost * 0.5f;

        if (stats->mana < total_cost) return false;
        stats->mana -= total_cost;
    }

    if (shadow_duplicate) {
        auto* pStats = registry.try_get<PlayerStats>(entity);
        if (pStats) pStats->last_shadow_trigger_time = (float)GetTime();
        registry.remove<ShadowKillArrayReady>(entity);

        auto* pos = registry.try_get<Position>(entity);
        Vector2 spawnPos = pos ? Vector2{pos->x, pos->y} : Vector2{0,0};

        auto shadow_ent = registry.create();
        registry.emplace<LocalLevelTag>(shadow_ent);
        registry.emplace<Position>(shadow_ent, spawnPos.x, spawnPos.y);
        registry.emplace<AnimationStateComponent>(shadow_ent);
        registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(PURPLE, 0.4f));
        registry.emplace<ShadowCloneComponent>(shadow_ent);
        
        auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
        sc.delay = 0.1f;
        sc.lifetime = 1.0f;
        sc.snapshot.skill_id = slot.id;
        sc.snapshot.position = spawnPos;
        sc.snapshot.target_pos = target_pos;
        if (stats) {
            sc.snapshot.stats = *stats;
            // Reduction to 50% damage will be applied in DamagePipeline
        }
        LOG_INFO("Shadow Kill Array: Duplicating skill {} for entity {}", slot.id, (uint32_t)entity);
    }

    if (slot.current_charges == data->max_charges) {
        float cdr = StatsSystem::GetStatWithTags(registry, entity, StatType::CooldownReduction, data->tags, slot.id) / 100.0f;
        float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
        // Optimization: For Channeled skills with very long cooldowns (like 60s),
        // we might NOT want to start cooldown here but when channeling ends?
        // But preventing abuse is safer.
        slot.cooldown = (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
    }
    slot.current_charges--;

    auto& exec = registry.emplace<SkillExecution>(entity);
    exec.skill_id = slot.id;
    exec.owner = entity;
    exec.slot_index = slot_index;
    exec.state = SkillState::Preparing;
    exec.timer = 0.1f; 
    exec.target_pos = target_pos;

    LOG_INFO("TryCast SUCCESS: Entity {} casting skill ID {} ({})", (uint32_t)entity, slot.id, data->name_key);
    return true;
}

void SkillSystem::HandleSkillInput(entt::registry& registry, entt::entity entity, int slot_index, Vector2 target_pos) {
    auto* active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active || slot_index < 0 || slot_index >= (int)active->slots.size()) return;

    auto& slot = active->slots[slot_index];
    if (slot.id == 0) return;

    // 1. Maintain Channeling
    if (auto* chan = registry.try_get<ChannelingComponent>(entity)) {
        if (chan->skill_id == slot.id) {
            chan->channel_timer = 0.25f; // Keep alive
            chan->target_pos = target_pos;
            // Maybe handle ticking here if we want instant feedback?
            // No, update loop handles it.
            return;
        }
        // If channeling something else, we ignore input (or we could interrupt)
        return; 
    }

    // 2. Start New Cast
    TryCast(registry, entity, slot_index, target_pos);
}

bool SkillSystem::AddTalentPoint(entt::registry& registry, entt::entity entity, uint32_t skill_id, uint32_t node_id) {
    auto* active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active) return false;

    SpecializedSkill* specialized = nullptr;
    for (auto& slot : active->specialized_slots) {
        if (slot.skill_id == skill_id) {
            specialized = &slot;
            break;
        }
    }
    if (!specialized) {
        LOG_WARN("Cannot add talent point: Skill {} is not specialized for entity {}", skill_id, (uint32_t)entity);
        return false;
    }

    if (active->available_talent_points <= 0) {
        LOG_WARN("Cannot add talent point: No points available for entity {}", (uint32_t)entity);
        return false;
    }

    if (specialized->GetPointsSpent() >= specialized->GetMaxPoints()) {
        LOG_WARN("Cannot add talent point: Skill {} has reached max points ({}/{})", 
            skill_id, specialized->GetPointsSpent(), specialized->GetMaxPoints());
        return false;
    }

    const auto* tree = SkillRegistry::Get().GetSkillTree(skill_id);
    if (!tree) return false;

    auto node_it = tree->nodes.find(node_id);
    if (node_it == tree->nodes.end()) return false;
    const auto& node = node_it->second;

    int current_pts = specialized->allocated_points.contains(node_id) ? specialized->allocated_points.at(node_id) : 0;
    if (current_pts >= node.max_points) {
        LOG_WARN("Cannot add talent point: Node {} already at max ({}/{})", node_id, current_pts, node.max_points);
        return false;
    }

    for (uint32_t pre_id : node.prerequisites) {
        int pre_pts = specialized->allocated_points.contains(pre_id) ? specialized->allocated_points.at(pre_id) : 0;
        if (pre_pts <= 0) {
            LOG_WARN("Cannot add talent point: Prerequisite {} not met for node {}", pre_id, node_id);
            return false;
        }
    }

    active->available_talent_points--;
    specialized->allocated_points[node_id] = current_pts + 1;
    registry.get_or_emplace<StatsDirty>(entity);

    LOG_INFO("Entity {} spent talent point on Skill {} -> Node {} ({}/{})", 
        (uint32_t)entity, skill_id, node_id, specialized->allocated_points[node_id], node.max_points);

    return true;
}

bool SkillSystem::ResetTalents(entt::registry& registry, entt::entity entity, uint32_t skill_id) {
    auto* active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active) return false;

    SpecializedSkill* specialized = nullptr;
    for (auto& slot : active->specialized_slots) {
        if (slot.skill_id == skill_id) {
            specialized = &slot;
            break;
        }
    }
    if (!specialized) return false;

    int points_to_refund = 0;
    for (auto [node_id, pts] : specialized->allocated_points) {
        points_to_refund += pts;
    }

    active->available_talent_points += points_to_refund;
    specialized->allocated_points.clear();
    
    registry.get_or_emplace<StatsDirty>(entity);
    LOG_INFO("Entity {} reset talents for Skill {}. Refunded {} points.", (uint32_t)entity, skill_id, points_to_refund);

    return true;
}

bool SkillSystem::ClearAllTalents(entt::registry& registry, entt::entity entity) {
    auto* active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active) return false;

    int total_refunded = 0;
    for (auto& slot : active->specialized_slots) {
        if (slot.skill_id == 0) continue;
        
        for (auto [node_id, pts] : slot.allocated_points) {
            total_refunded += pts;
        }
        slot.allocated_points.clear();
    }

    active->available_talent_points += total_refunded;
    registry.get_or_emplace<StatsDirty>(entity);
    LOG_INFO("Entity {} cleared all talents. Refunded {} points.", (uint32_t)entity, total_refunded);

    return true;
}

} // namespace NoMoreDay