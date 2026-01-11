/**
 * @file BladeBoomerang.cpp
 * @brief 御剑回旋 (ID 8) - 回旋镖技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/data/SkillRegistry.hpp"
#include "engine/render/GPUParticleSystem.hpp"

#include "core/logging/Logger.hpp"
#include "raymath.h"
#include "game/components/SkillDefs.hpp" // For SwordIntent

namespace NoMoreDay::skills {

struct BladeBoomerang : SkillBehaviorBase<BladeBoomerang> {
    static constexpr uint32_t kSkillId = 8;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
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

        bool hasPull = false;
        float pullStrength = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 8) {
                    if (spec.allocated_points.contains(810) && spec.allocated_points.at(810) > 0) {
                        hasPull = true;
                        pullStrength = basePull;
                    }
                    if (spec.allocated_points.contains(811) && spec.allocated_points.at(811) > 0) {
                        pullStrength += gravityPull;
                    }
                    break;
                }
            }
        }

        auto& particleSys = systems::GPUParticleSystem::Get();
        auto splash = systems::InkEffectHelper::CreateInkSplash({pos->x, pos->y}, 10, 10.0f, 100.0f);
        for (auto& p : splash) {
            p.velocity = Vector2Add(p.velocity, Vector2Scale(dir, 200.0f));
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
            proj.hasPull = true;
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
    }

    static void DoHit(entt::registry& registry, entt::entity attacker, entt::entity target, Tag hit_tags, bool is_crit) {
        if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == kSkillId) {
                    // ID 813: Ju Ling (Chance to gain Intent)
                    if (spec.allocated_points.contains(813)) {
                        int pts = spec.allocated_points.at(813);
                        if (pts > 0 && GetRandomValue(0, 100) < 15 * pts) {
                             if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
                                 if (intent->stacks < intent->max_stacks) {
                                     intent->stacks++;
                                     intent->time_since_last_gain = 0.0f;
                                     intent->decay_tick_timer = 0.0f;
                                     LOG_DEBUG("Blade Boomerang (813): Gained Intent via Hit");
                                 }
                             }
                        }
                    }
                    break;
                }
            }
        }
    }
};

REGISTER_SKILL_BEHAVIOR(BladeBoomerang)

void RegisterBladeBoomerang() {}

} // namespace NoMoreDay::skills
