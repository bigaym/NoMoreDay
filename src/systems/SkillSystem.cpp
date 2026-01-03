#include "SkillSystem.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp" // For Position
#include "../components/Projectile.hpp"
#include "../components/PlayerState.hpp" // For DashComponent
#include "../core/SkillRegistry.hpp"
#include "StatsSystem.hpp"
#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
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

                    // Talent: Momentum (势如破竹)
                    float dist = Vector2Distance(startPos, exec.target_pos);
                    if (dist > 150.0f) {
                        moreDamageMult *= 1.3f; // 30% More
                        LOG_INFO("Momentum: +30% More damage due to distance ({:.1f})", dist);
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
}

void SkillSystem::Update(entt::registry& registry, float dt) {
    UpdateCooldowns(registry, dt);
    UpdateStates(registry, dt);
    UpdateSwordIntent(registry, dt);
}

void SkillSystem::RegisterEffect(uint32_t skill_id, CastCallback callback) {
    LOG_INFO("Registering effect for skill {}", skill_id);
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
    
    if (auto* sc = registry.try_get<ShadowComponent>(owner)) {
        exec.has_snapshot = true;
        exec.snapshot = sc->snapshot;
        exec.is_empowered = sc->snapshot.is_empowered;
        registry.emplace_or_replace<CombatStats>(shadow, sc->snapshot.stats);
    }

    registry.emplace<ShadowCastTag>(exec_ent);
    LOG_INFO("Shadow at ({:.1f}, {:.1f}) casting skill: {}", position.x, position.y, data->name_key);
    return true;
}

void SkillSystem::UpdateSwordIntent(entt::registry& registry, float dt) {
    auto view = registry.view<SwordIntentComponent>();
    for (auto entity : view) {
        auto& intent = view.get<SwordIntentComponent>(entity);
        if (intent.stacks > 0) {
            intent.decay_timer += dt;
            if (intent.decay_timer >= intent.decay_interval) {
                float over = intent.decay_timer - intent.decay_interval;
                if (over >= 0.5f) {
                    intent.stacks--;
                    intent.decay_timer = intent.decay_interval;
                    LOG_DEBUG("Entity {} Sword Intent decayed to {}", (uint32_t)entity, intent.stacks);
                }
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
    if (view.begin() == view.end()) return;

    for (auto entity : view) {
        auto& exec = view.get<SkillExecution>(entity);
        exec.timer -= dt;
        // LOG_TRACE("Skill {} in state {} timer {:.3f}", exec.skill_id, (int)exec.state, exec.timer);

        if (exec.timer <= 0.0f) {
            switch (exec.state) {
                case SkillState::Preparing:
                    LOG_INFO("Skill {} on entity {} transitioning from Preparing to Casting (timer reached 0)", exec.skill_id, (uint32_t)entity);
                    for (auto& hook : s_pre_cast_hooks) {
                        hook(registry, entity, exec);
                    }
                    exec.state = SkillState::Casting;
                    exec.timer = 0.05f; 
                    if (s_skill_callbacks.contains(exec.skill_id)) {
                        LOG_INFO("Triggering callback for skill {}", exec.skill_id);
                        s_skill_callbacks[exec.skill_id](registry, entity, exec);
                    } else {
                        LOG_WARN("No callback found for skill {}", exec.skill_id);
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