#include "SkillSystem.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp" // For Position
#include "../components/Projectile.hpp"
#include "../components/PlayerState.hpp" // For DashComponent
#include "../components/Buff.hpp"
#include "../components/AIComponent.hpp" // For EnemyTag
#include "../core/SkillRegistry.hpp"
#include "StatsSystem.hpp"
#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
#include "SpatialGrid.hpp"
#include "../tools/Logger.hpp"
#include "raymath.h"
#include <map>

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
        float speed = 1200.0f;

        // Apply burst velocity to owner
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * speed;
            vel->vy = dir.y * speed;
        }

        // Integrate with DashComponent to prevent movement override
        if (dash) {
            dash->isDashing = true;
            dash->dashTimer = 0.25f;
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = speed;
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
        proj.lifeTime = 0.25f; 
        proj.radius = 45.0f;   
        proj.pierce = true;
        proj.pierceCount = forcePierce ? 999 : 99; 
        
        if (stats) {
            proj.snapshot = *stats;
            for (auto& mult : proj.snapshot.damage_multipliers) mult *= moreDamageMult;

            if (exec.is_empowered) {
                for (auto& mult : proj.snapshot.damage_multipliers) mult *= 1.5f;
                LOG_INFO("Empowered Flowing Thrust spawned with 1.5x damage");
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
        }

        float spread = 0.4f + (totalCount * 0.05f); 
        float startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f; 
        float angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;

        for (int i = 0; i < totalCount; ++i) {
            float angle = startAngle + i * angleStep;
            Vector2 dir = Vector2Rotate(baseDir, angle);
            
            auto proj_ent = registry.create();
            registry.emplace<LocalLevelTag>(proj_ent);
            registry.emplace<Position>(proj_ent, pos->x, pos->y);
            registry.emplace<Velocity>(proj_ent, dir.x * 600.0f, dir.y * 600.0f);
            registry.emplace<ColorComponent>(proj_ent, GOLD); 
            
            auto& proj = registry.emplace<Projectile>(proj_ent);
            proj.owner = owner;
            proj.speed = 600.0f;
            proj.lifeTime = boomerang ? 2.0f : 1.2f;
            proj.radius = 35.0f; 
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
            }
        }

        LOG_INFO("Rending Wave fired {} projectiles from entity {}", totalCount, (uint32_t)owner);
    });

    // ID 3: Blade Formation (万剑诀)
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
                    break;
                }
            }
        }

        formation.max_swords = 1 + extraSwords;
        formation.current_swords = formation.max_swords; // For now, simple activation
        formation.attack_interval = 1.0f / (1.0f + freqInc);
        formation.search_radius = 200.0f * (1.0f + searchInc);
        
        LOG_INFO("Blade Formation activated: {} swords for entity {}", formation.max_swords, (uint32_t)owner);
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
        array.radius = 150.0f;
        
        // Talent scaling
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 6) {
                    // node 610: slow - we could add a flag here
                    break;
                }
            }
        }

        registry.emplace<SkillComponent>(array_ent, 6, owner);
        LOG_INFO("Sword Array summoned at ({}, {}) by entity {}", exec.target_pos.x, exec.target_pos.y, (uint32_t)owner);
    });

    // ID 5: Infinite Blades (万剑归宗)
    RegisterEffect(5, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 5;
        chan.channel_timer = 3.0f; // 3s channel
        chan.tick_interval = 0.1f; // 10 blades per second
        chan.target_pos = exec.target_pos;
        LOG_INFO("Infinite Blades channeling started for entity {}", (uint32_t)owner);
    });

    // ID 7: Mind Blade (心剑·无影)
    RegisterEffect(7, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 7;
        chan.channel_timer = 2.0f; 
        chan.tick_interval = 0.05f; // Fast ticks
        chan.target_pos = exec.target_pos;
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
                    // Node 400: Jin Zhong Zhao (Armor) - handled by StatsSystem automatically if we used stat_modifiers
                    // But we want to check for mechanics here if needed.
                    break;
                }
            }
        }

        // Apply Buff
        BuffEffect ward_buff;
        ward_buff.id = "blade_ward";
        ward_buff.name = "Blade Ward";
        ward_buff.description = "Interacts with projectiles and provides physical DR.";
        ward_buff.type = BuffType::Shield;
        ward_buff.duration = 10.0f;
        ward_buff.remaining = 10.0f;
        ward_buff.stacks = 1;
        ward_buff.max_stacks = 1;
        ward_buff.is_debuff = false;
        
        // +10% Physical Resistance (ResistPhysical is index 21 in StatType enum, index 0 in CombatStats.resistances)
        ward_buff.modifiers.push_back({StatType::ResistPhysical, ModifierMode::Flat, phys_dr});
        
        active_effects.AddOrRefresh(ward_buff);
        
        // Add Logic Component
        auto& ward = registry.emplace_or_replace<BladeWardComponent>(owner);
        ward.remaining = 10.0f;
        ward.sword_count = 3;
        
        registry.get_or_emplace<StatsDirty>(owner);
        LOG_INFO("Blade Ward activated for entity {}", (uint32_t)owner);
    });

    // ID 8: Blade Boomerang (御剑·回旋)
    RegisterEffect(8, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        if (!pos || !stats) return;

        Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));
        float speed = 800.0f;

        // --- BRANCH LOGIC ---
        bool hasPull = false;
        float pullStrength = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 8) {
                    // Talent: Ci Xi (磁吸) - ID 810
                    if (spec.allocated_points.contains(810) && spec.allocated_points.at(810) > 0) {
                        hasPull = true;
                        pullStrength = 300.0f;
                    }
                    // Talent: Gravity Field (重力场) - ID 811 (Actually 811 is Black Hole in json, 810 is Pull)
                    // Let's check json again. 810: 磁吸, 811: 剑气黑洞
                    if (spec.allocated_points.contains(811) && spec.allocated_points.at(811) > 0) {
                        pullStrength += 500.0f;
                    }
                    break;
                }
            }
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
        proj.radius = 40.0f;   
        proj.pierce = true;
        proj.pierceCount = 99; 
        proj.snapshot = *stats;
        proj.hasPull = hasPull;
        proj.pullStrength = pullStrength;

        registry.emplace<CombatStats>(proj_ent, proj.snapshot);
        registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

        auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.owner = owner;
        bc.returnTimer = 0.6f;
        bc.phase = BoomerangComponent::Outward;

        LOG_INFO("Blade Boomerang fired by entity {}", (uint32_t)owner);
    });

    // ID 9: Phantom Flash (绝影闪)
    RegisterEffect(9, [](entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        if (!pos) return;

        // 1. Dash backwards
        Vector2 dir = Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));
        float dashDist = 100.0f;
        
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * 1000.0f;
            vel->vy = dir.y * 1000.0f;
        }
        
        if (auto* dash = registry.try_get<DashComponent>(owner)) {
            dash->isDashing = true;
            dash->dashTimer = 0.1f;
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = 1000.0f;
        }

        // 2. Add Counter State
        auto& pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
        pf.counter_window = 0.5f;
        pf.triggered = false;

        LOG_INFO("Phantom Flash: Counter state active for entity {}", (uint32_t)owner);
    });
}

void SkillSystem::Update(entt::registry& registry, systems::SpatialHashGrid& grid, float dt) {
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

        shadow.lifetime -= dt;
        if (shadow.lifetime <= 0.0f) {
            // Check if still casting
            bool still_casting = false;
            auto exec_view = registry.view<SkillExecution>();
            for(auto exec_ent : exec_view) {
                if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                    still_casting = true;
                    break;
                }
            }
            if (!still_casting) expired_shadows.push_back(entity);
        }
    }
    for (auto e : expired_shadows) registry.destroy(e);

    // Update Blade Formation (ID 3)
    auto formation_view = registry.view<BladeFormationComponent, Position>();
    for (auto entity : formation_view) {
        auto& formation = formation_view.get<BladeFormationComponent>(entity);
        const auto& pos = formation_view.get<Position>(entity);

        formation.attack_timer -= dt;
        if (formation.attack_timer <= 0.0f && formation.current_swords > 0) {
            // Find target
            entt::entity target = entt::null;
            float minDistSq = formation.search_radius * formation.search_radius;

            grid.query(pos, formation.search_radius, [&](entt::entity neighbor) {
                if (neighbor == entity) return;
                if (!registry.valid(neighbor) || !registry.all_of<EnemyTag, Position>(neighbor)) return;

                const auto& nPos = registry.get<Position>(neighbor);
                float dx = nPos.x - pos.x;
                float dy = nPos.y - pos.y;
                float distSq = dx*dx + dy*dy;

                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    target = neighbor;
                }
            });

            if (target != entt::null) {
                // Strike! (Shadow cast a simple thrust or custom ID)
                // For now, let's cast skill 1 (Flowing Thrust) as the "spirit sword strike"
                const auto& tPos = registry.get<Position>(target);
                ShadowCast(registry, entity, 1, {pos.x, pos.y}, {tPos.x, tPos.y});
                
                formation.attack_timer = formation.attack_interval;
                LOG_DEBUG("Blade Formation strike triggered for entity {}", (uint32_t)entity);
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

        array.damage_timer -= dt;
        if (array.damage_timer <= 0.0f) {
            // Pulsing damage
            grid.query(pos, array.radius, [&](entt::entity target) {
                if (target == array.owner || target == entity) return;
                if (!registry.valid(target) || !registry.all_of<HealthComponent, Position>(target)) return;

                const auto& tPos = registry.get<Position>(target);
                float dx = tPos.x - pos.x;
                float dy = tPos.y - pos.y;
                if (dx*dx + dy*dy <= array.radius * array.radius) {
                    // Hit!
                    DamagePool pool;
                    pool.Add(Tag::Physical, 10.0f); // Base array tick damage
                    auto result = DamagePipeline::Calculate(registry, array.owner, target, 6, pool, Tag::Area | Tag::Hit);
                    CombatSystem::ApplyDamage(registry, target, result.total_damage, array.owner, result.is_crit);
                }
            });
            array.damage_timer = array.damage_interval;
        }
    }
    for(auto e : expired_arrays) registry.destroy(e);

    // Update Channeling (ID 5 & 7)
    auto chan_view = registry.view<ChannelingComponent, Position>();
    for (auto entity : chan_view) {
        auto& chan = chan_view.get<ChannelingComponent>(entity);
        const auto& pos = chan_view.get<Position>(entity);

        chan.channel_timer -= dt;
        if (chan.channel_timer <= 0.0f) {
            registry.remove<ChannelingComponent>(entity);
            continue;
        }

        chan.tick_timer -= dt;
        if (chan.tick_timer <= 0.0f) {
            if (chan.skill_id == 5) {
                // Infinite Blades: Spray random blades
                float angle = (float)GetRandomValue(0, 360) * (PI / 180.0f);
                Vector2 dir = { cosf(angle), sinf(angle) };
                Vector2 strike_target = { pos.x + dir.x * 500.0f, pos.y + dir.y * 500.0f };
                ShadowCast(registry, entity, 2, {pos.x, pos.y}, strike_target); // Re-use Rending Wave logic
            } else if (chan.skill_id == 7) {
                // Mind Blade: Rapid narrow beam
                ShadowCast(registry, entity, 1, {pos.x, pos.y}, chan.target_pos); // Re-use Flowing Thrust logic
            }
            chan.tick_timer = chan.tick_interval;
        }
    }

    // Update Blade Ward
    auto ward_view = registry.view<BladeWardComponent>();
    for (auto entity : ward_view) {
        auto& ward = ward_view.get<BladeWardComponent>(entity);
        ward.remaining -= dt;
        if (ward.remaining <= 0.0f) {
            registry.remove<BladeWardComponent>(entity);
        }
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
    }

    auto exec_ent = registry.create();
    registry.emplace<LocalLevelTag>(exec_ent);
    auto& exec = registry.emplace<SkillExecution>(exec_ent);
    exec.skill_id = skill_id;
    exec.owner = shadow; 
    exec.state = SkillState::Casting; 
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
            intent.decay_timer += dt;
            if (intent.decay_timer >= intent.decay_interval) {
                intent.stacks--;
                intent.decay_timer = 0.0f;
                LOG_DEBUG("Entity {} Sword Intent decayed to {}", (uint32_t)entity, intent.stacks);
            }
        } else {
            intent.decay_timer = 0.0f;
        }
    }
}

void SkillSystem::OnSkillHit(entt::registry& registry, entt::entity attacker, entt::entity target, uint32_t skill_id, Tag hit_tags) {
    if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
        bool gainIntent = HasTag(hit_tags, Tag::Melee);
        
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
            intent->decay_timer = 0.0f; 
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
                        s_skill_callbacks[exec.skill_id](registry, entity, exec);
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
    if (stats) {
        float rcr = StatsSystem::GetStatWithTags(registry, entity, StatType::ResourceCostReduction, data->tags, slot.id) / 100.0f;
        float cost = data->mana_cost * (1.0f - std::min(0.9f, rcr));
        if (stats->mana < cost) return false;
        stats->mana -= cost;
    }

    if (slot.current_charges == data->max_charges) {
        float cdr = StatsSystem::GetStatWithTags(registry, entity, StatType::CooldownReduction, data->tags, slot.id) / 100.0f;
        float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
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

    LOG_INFO("Entity {} started casting skill: {}", (uint32_t)entity, data->name_key);
    return true;
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

} // namespace NoMoreDay