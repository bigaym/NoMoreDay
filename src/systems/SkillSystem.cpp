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

void SkillSystem::InitHooks() {
    // ID 1: Flowing Thrust (流云刺)
    RegisterEffect(1, [](entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 target_pos) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        auto* dash = registry.try_get<DashComponent>(owner);
        if (!pos) return;

        // 1. Dash towards target
        Vector2 dir = Vector2Normalize(Vector2Subtract(target_pos, {pos->x, pos->y}));
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

        // 2. Spawn a "Thrust" projectile that moves with the dash
        auto proj_ent = registry.create();
        registry.emplace<LocalLevelTag>(proj_ent);
        registry.emplace<Position>(proj_ent, pos->x, pos->y);
        registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
        registry.emplace<ColorComponent>(proj_ent, SKYBLUE); // Visual
        
        auto& proj = registry.emplace<Projectile>(proj_ent);
        proj.owner = owner;
        proj.speed = speed;
        proj.lifeTime = 0.25f; 
        proj.radius = 45.0f;   // Increased from 30
        proj.pierce = true;
        proj.pierceCount = 99; 
        if (stats) proj.snapshot = *stats;

        registry.emplace<SkillComponent>(proj_ent, skill_id, owner);

        LOG_INFO("Flowing Thrust executed by entity {}", (uint32_t)owner);
    });

    // ID 2: Rending Wave (裂空斩)
    RegisterEffect(2, [](entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 target_pos) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        if (!pos || !stats) return;

        Vector2 baseDir = Vector2Normalize(Vector2Subtract(target_pos, {pos->x, pos->y}));

        float angles[] = {-0.3f, 0.0f, 0.3f}; // Widened fan
        for (float angle : angles) {
            Vector2 dir = Vector2Rotate(baseDir, angle);
            
            auto proj_ent = registry.create();
            registry.emplace<LocalLevelTag>(proj_ent);
            registry.emplace<Position>(proj_ent, pos->x, pos->y);
            registry.emplace<Velocity>(proj_ent, dir.x * 600.0f, dir.y * 600.0f);
            registry.emplace<ColorComponent>(proj_ent, GOLD); // Visual
            
            auto& proj = registry.emplace<Projectile>(proj_ent);
            proj.owner = owner;
            proj.speed = 600.0f;
            proj.lifeTime = 1.2f;
            proj.radius = 35.0f; // Increased from 20
            proj.pierce = true;
            proj.pierceCount = 5;
            proj.snapshot = *stats;

            registry.emplace<SkillComponent>(proj_ent, skill_id, owner);
        }

        LOG_INFO("Rending Wave fired 3 projectiles from entity {}", (uint32_t)owner);
    });
}

void SkillSystem::Update(entt::registry& registry, float dt) {
    UpdateCooldowns(registry, dt);
    UpdateStates(registry, dt);
    UpdateSwordIntent(registry, dt);
    UpdateShadows(registry, dt);
}

void SkillSystem::RegisterEffect(uint32_t skill_id, CastCallback callback) {
    s_skill_callbacks[skill_id] = callback;
}

bool SkillSystem::ShadowCast(entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 position, Vector2 target_pos) {
    const auto* data = SkillRegistry::Get().GetSkill(skill_id);
    if (!data) return false;

    // 1. Create Shadow Entity
    auto shadow = registry.create();
    registry.emplace<LocalLevelTag>(shadow);
    registry.emplace<ShadowEntityTag>(shadow);
    registry.emplace<Position>(shadow, position.x, position.y);
    registry.emplace<AnimationStateComponent>(shadow);
    registry.emplace<ShadowLifetime>(shadow, 1.0f); // Shadows last at least 1s

    // 2. Create Skill Execution tied to Shadow
    auto exec_ent = registry.create();
    registry.emplace<LocalLevelTag>(exec_ent);
    auto& exec = registry.emplace<SkillExecution>(exec_ent);
    exec.skill_id = skill_id;
    exec.owner = shadow; // The shadow is the visual actor
    exec.state = SkillState::Casting; // Shadows skip windup
    exec.timer = 0.05f;
    exec.target_pos = target_pos;
    
    // 3. Mark as Shadow Cast (for logic hooks)
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
                // Decay 1 stack per 0.5s after initial interval
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

void SkillSystem::UpdateShadows(entt::registry& registry, float dt) {
    auto view = registry.view<ShadowLifetime>();
    for (auto entity : view) {
        auto& lifetime = view.get<ShadowLifetime>(entity);
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0.0f) {
            // Check if shadow is still casting anything
            bool still_casting = false;
            auto exec_view = registry.view<SkillExecution>();
            for(auto exec_ent : exec_view) {
                if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                    still_casting = true;
                    break;
                }
            }

            if (!still_casting) {
                registry.destroy(entity);
            }
        }
    }
}

void SkillSystem::OnSkillHit(entt::registry& registry, entt::entity attacker, entt::entity target, uint32_t skill_id, Tag hit_tags) {
    // 1. Blade Ascendant: Sword Intent
    if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
        if (HasTag(hit_tags, Tag::Melee)) {
            if (intent->stacks < intent->max_stacks) {
                intent->stacks++;
                LOG_DEBUG("Entity {} Sword Intent increased to {}", (uint32_t)attacker, intent->stacks);
            }
            intent->decay_timer = 0.0f; // Reset decay
        }
    }

    // 2. Mana Restore on Hit
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
                        // Restart cooldown for next charge
                        auto* stats = registry.try_get<CombatStats>(entity);
                        float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
                        float cdr = stats ? stats->cooldown_reduction : 0.0f;
                        slot.cooldown = (data->cooldown / recovery) * (1.0f - cdr);
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
                    exec.state = SkillState::Casting;
                    exec.timer = 0.05f; // Short cast duration for prototype
                    
                    // Trigger effect
                    if (s_skill_callbacks.contains(exec.skill_id)) {
                        s_skill_callbacks[exec.skill_id](registry, exec.owner, exec.skill_id, exec.target_pos);
                    }
                    break;
                case SkillState::Casting:
                    exec.state = SkillState::Settle;
                    exec.timer = 0.1f; // Recovery duration
                    break;
                case SkillState::Settle:
                    // Reset owner to idle if not moving (this logic might be refined in AnimationSystem)
                    if (auto* anim = registry.try_get<AnimationStateComponent>(exec.owner)) {
                        anim->state = EntityAnimState::Idle;
                    }
                    registry.destroy(entity);
                    continue; // Skip the anim update below since entity is destroyed
                default:
                    registry.destroy(entity);
                    continue;
            }
        }

        // Update Owner Animation State if it exists
        if (auto* anim = registry.try_get<AnimationStateComponent>(exec.owner)) {
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

    // Check if already casting something (Basic lock)
    bool already_executing = false;
    auto exec_view = registry.view<SkillExecution>();
    for(auto exec_ent : exec_view) {
        if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
            already_executing = true;
            break;
        }
    }
    if (already_executing) return false;

    const auto* data = SkillRegistry::Get().GetSkill(slot.id);
    if (!data) return false;

    // 1. Check Charges and Cooldown
    if (slot.current_charges <= 0) return false;

    // 2. Check Resources (Mana)
    auto* stats = registry.try_get<CombatStats>(entity);
    if (stats) {
        float cost = data->mana_cost * (1.0f - stats->resource_cost_reduction);
        if (stats->mana < cost) return false;
        stats->mana -= cost;
    }

    // 3. Consume Charge and Start Cooldown if not already running
    if (slot.current_charges == data->max_charges) {
        float cdr = stats ? stats->cooldown_reduction : 0.0f;
        float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
        slot.cooldown = (data->cooldown / recovery) * (1.0f - cdr);
    }
    slot.current_charges--;

    // 4. Create Execution Entity
    auto exec_ent = registry.create();
    auto& exec = registry.emplace<SkillExecution>(exec_ent);
    exec.skill_id = slot.id;
    exec.owner = entity;
    exec.slot_index = slot_index;
    exec.state = SkillState::Preparing;
    exec.timer = 0.1f; // Preparation time (Wind-up)
    exec.target_pos = target_pos;

    LOG_INFO("Entity {} started casting skill: {}", (uint32_t)entity, data->name_key);

    return true;
}

}
