#include "ProjectileSystem.hpp"
#include "../components/Projectile.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/EffectComponent.hpp" // For DamagePopup
#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
#include "SkillSystem.hpp"
#include "GPUParticleSystem.hpp" // Added
#include "../components/AIComponent.hpp"
#include "../tools/Logger.hpp"
#include "raylib.h"
#include "../utils/PhysicsUtils.hpp"
#include "raymath.h" // Added for Vector2 operations

namespace NoMoreDay {

void ProjectileSystem::Update(entt::registry& registry, systems::SpatialHashGrid& grid, float dt) {
    auto view = registry.view<Position, Velocity, Projectile>();
    
    // 用于延迟创建伤害飘字的数据结构，避免在 grid.query 回调中直接修改注册表
    struct PopupInfo {
        Position pos;
        DamagePopup popup;
    };
    std::vector<PopupInfo> popupsToCreate;

    std::vector<entt::entity> to_destroy;

    for (auto entity : view) {
        auto& pos = view.get<Position>(entity);
        auto& vel = view.get<Velocity>(entity);
        auto& proj = view.get<Projectile>(entity);

        // --- NEW: Boomerang Behavior ---
        if (auto* bc = registry.try_get<BoomerangComponent>(entity)) {
            if (bc->phase == BoomerangComponent::Outward) {
                bc->returnTimer -= dt;
                if (bc->returnTimer <= 0.0f) {
                    bc->phase = BoomerangComponent::Returning;
                }
            } else {
                // Returning phase: steer towards owner or specific target
                entt::entity targetEnt = registry.valid(bc->returnTarget) ? bc->returnTarget : bc->owner;

                if (registry.valid(targetEnt) && registry.all_of<Position>(targetEnt)) {
                    const auto& targetPos = registry.get<Position>(targetEnt);
                    Vector2 p = {pos.x, pos.y};
                    Vector2 tp = {targetPos.x, targetPos.y};
                    Vector2 toTarget = Vector2Subtract(tp, p);
                    float dist = Vector2Length(toTarget);
                    
                    if (dist < 20.0f) {
                        // Back to owner/target, destroy projectile
                        to_destroy.push_back(entity);
                        continue;
                    }
                    
                    float speed = (bc->returnSpeed > 0.1f) ? bc->returnSpeed : (proj.speed > 0.1f ? proj.speed : 800.0f);
                    Vector2 dir = Vector2Scale(Vector2Normalize(toTarget), speed); 
                    vel.vx = dir.x;
                    vel.vy = dir.y;
                } else {
                    // Target dead/invalid? Continue flying outward or just destroy?
                    // Original logic: Continue flying outward
                    bc->phase = BoomerangComponent::Outward; 
                }
            }
        }

        // --- NEW: Pull Logic ---
        if (proj.hasPull) {
            float pullRadius = proj.radius * 3.0f;
            grid.query({pos.x, pos.y}, pullRadius, [&](entt::entity target) {
                if (target == proj.owner || target == entity) return;
                if (!registry.valid(target) || !registry.all_of<Velocity, Position>(target)) return;
                if (!registry.any_of<EnemyTag>(target)) return; // Only pull enemies

                auto& tPos = registry.get<Position>(target);
                auto& tVel = registry.get<Velocity>(target);

                Vector2 dir = Vector2Normalize(Vector2Subtract({pos.x, pos.y}, {tPos.x, tPos.y}));
                tVel.vx += dir.x * proj.pullStrength * dt;
                tVel.vy += dir.y * proj.pullStrength * dt;
            });
        }

        // 1. Position Sync (For skills like Flowing Thrust that should follow the owner)
        // If it's skill 1 (Flowing Thrust), stick to the owner
        uint32_t skill_id = 0;
        if (auto* skillComp = registry.try_get<SkillComponent>(entity)) {
            skill_id = skillComp->skill_id;
            if (skill_id == 1 && registry.valid(proj.owner)) {
                auto* ownerPos = registry.try_get<Position>(proj.owner);
                auto* ownerVel = registry.try_get<Velocity>(proj.owner);
                if (ownerPos) {
                    pos.x = ownerPos->x;
                    pos.y = ownerPos->y;
                }
                if (ownerVel) {
                    vel.vx = ownerVel->vx;
                    vel.vy = ownerVel->vy;
                }
            }
        }

        // --- VISUAL EFFECTS: Continuous Ink Trail ---
        if (skill_id == 1 || skill_id == 2 || skill_id == 7 || skill_id == 8 || skill_id == 9) {
            auto& particleSys = systems::GPUParticleSystem::Get();
            Vector2 trailVel = Vector2Scale({vel.vx, vel.vy}, -0.1f);
            
            // ID 8 (Boomerang) gets a specialized rotating trail
            if (skill_id == 8) {
                // Two trails orbiting the center
                float time = (float)GetTime() * 10.0f;
                Vector2 offset1 = { cosf(time) * 10.0f, sinf(time) * 10.0f };
                Vector2 offset2 = Vector2Scale(offset1, -1.0f);
                
                particleSys.Emit(systems::InkEffectHelper::CreateInkTrail({pos.x + offset1.x, pos.y + offset1.y}, trailVel, 1.0f, 0.3f));
                particleSys.Emit(systems::InkEffectHelper::CreateInkTrail({pos.x + offset2.x, pos.y + offset2.y}, trailVel, 1.0f, 0.3f));
            } else if (skill_id == 7) {
                // Skill 7: Spatial Cut - Gold sparkling trail
                auto p = systems::InkEffectHelper::CreateInkTrail({pos.x, pos.y}, trailVel, 2.0f, 0.5f);
                p.color = GOLD;
                particleSys.Emit(p);
            } else {
                particleSys.Emit(systems::InkEffectHelper::CreateInkTrail({pos.x, pos.y}, trailVel, 1.2f, 0.4f));
            }
        }

        // 2. Lifetime
        proj.lifeTime -= dt;
        if (proj.lifeTime <= 0.0f) {
            to_destroy.push_back(entity);
            continue;
        }

        // 3. Collision Check
        bool hit = false;
        float check_radius = proj.radius + 10.0f; 
        
        // Track unique hits in this specific spatial query to handle grid cell overlaps
        std::vector<entt::entity> uniqueQueryHits;

        grid.query({pos.x, pos.y}, check_radius, [&](entt::entity target) {
            if (hit && !proj.pierce) return;
            // Early exit if out of piercing power (optimized for loop)
            if (proj.pierce && proj.pierceCount < 0) return;

            if (target == proj.owner) return;
            if (!registry.valid(target) || !registry.all_of<HealthComponent>(target)) return;

            // --- Multi-Hit Prevention ---
            // 1. Local query uniqueness (in case grid returns same entity twice)
            if (std::find(uniqueQueryHits.begin(), uniqueQueryHits.end(), target) != uniqueQueryHits.end()) return;
            uniqueQueryHits.push_back(target);

            // 2. Persistent projectile hit tracking (for piercing projectiles over multiple frames)
            if (std::find(proj.hitEntities.begin(), proj.hitEntities.end(), target) != proj.hitEntities.end()) return;

            const auto& tPos = registry.get<Position>(target);
            float dx = tPos.x - pos.x;
            float dy = tPos.y - pos.y;
            float distSq = dx*dx + dy*dy;

            if (distSq <= check_radius * check_radius) {
                // --- Interception Check ---
                if (auto* ward = registry.try_get<BladeWardComponent>(target)) {
                    // Base chance 15% per sword (3 swords default = 45%)
                    float chance = ward->sword_count * ward->interception_chance;
                    if ((float)GetRandomValue(0, 1000) / 1000.0f < chance) {
                        LOG_INFO("Projectile intercepted by Blade Ward on entity {}", (uint32_t)target);
                        
                        // VFX: Ink Block
                        auto& particleSys = systems::GPUParticleSystem::Get();
                        // 1. Ink Splash at interception point
                        auto splash = systems::InkEffectHelper::CreateInkSplash({pos.x, pos.y}, 8, 15.0f, 120.0f);
                        for(auto& p : splash) particleSys.Emit(p);
                        // 2. Metallic spark
                        particleSys.Emit(systems::InkEffectHelper::CreateGoldParticle({pos.x, pos.y}, {0, -50.0f}, 1.5f));

                        // Consume sword unless "Solidified"
                        if (!ward->is_solidified) {
                            ward->sword_count--;
                        }

                        hit = true; 
                        return; // Stop processing this target
                    }
                }

                // Hit confirmed
                proj.hitEntities.push_back(target); // Record the hit
                
                // --- VISUAL EFFECTS: Ink Splash on Hit ---
                if (skill_id == 2 || skill_id == 7) {
                    auto& particleSys = systems::GPUParticleSystem::Get();
                    auto splash = systems::InkEffectHelper::CreateInkSplash({pos.x, pos.y}, 8, 15.0f, 150.0f);
                    for (auto& p : splash) {
                        if (skill_id == 7) p.color = GOLD; // Gold for Mind Blade
                        particleSys.Emit(p);
                    }
                }

                Tag hit_tags = Tag::Projectile | Tag::Hit;
                
                uint32_t skill_id = 0;
                if (auto* skillComp = registry.try_get<SkillComponent>(entity)) {
                    skill_id = skillComp->skill_id;
                }

                // Calculate Damage via Pipeline
                DamagePool base;
                // If the projectile has its own CombatStats (snapshot), use it as the source of truth for damage calculation
                entt::entity damage_attacker = proj.owner;
                if (registry.all_of<CombatStats>(entity)) {
                    damage_attacker = entity;
                }
                
                auto result = DamagePipeline::Calculate(registry, damage_attacker, target, skill_id, base, hit_tags, entity);
                
                float finalDamage = result.total_damage;
                if (finalDamage <= 0.0f) finalDamage = 5.0f; // Minimum damage for prototype feedback

                // Apply Damage
                CombatSystem::ApplyDamage(registry, target, finalDamage, proj.owner, result.is_crit);
                
                // Apply Knockback
                // Use snapshot knockback if available, default to 0
                float knockbackForce = proj.snapshot.knockback;
                if (knockbackForce > 0.0f) {
                     Utils::ApplyKnockback(registry, target, {pos.x, pos.y}, knockbackForce);
                }

                // Trigger Skill Hit interactions
                SkillSystem::OnSkillHit(registry, proj.owner, target, skill_id, hit_tags, result.is_crit);

                LOG_DEBUG("Projectile {} hit {} for {:.1f} dmg, knockback={:.1f}", (uint32_t)entity, (uint32_t)target, finalDamage, knockbackForce);

                // --- Piercing Logic ---
                // Rule: 
                // If pierce=false: Always destroy on first hit.
                // If pierce=true: Decrement pierceCount. If count becomes < 0, destroy.
                if (!proj.pierce) {
                    hit = true;
                } else {
                    // pierceCount represents "Remaining Hits allowed after this one".
                    // Actually, "Pierce Count" usually means "Extra targets".
                    // If pierceCount = 1, it hits 1st target (count->0), then hits 2nd target (count->-1, destroy).
                    // So pierceCount=1 means "Hits 2 targets total".
                    proj.pierceCount--;
                    if (proj.pierceCount < 0) {
                        hit = true;
                    }
                }
            }
        });

        if (hit) {
            to_destroy.push_back(entity);
        }
    }

    for (auto entity : to_destroy) {
        if (registry.valid(entity)) registry.destroy(entity);
    }
}

} // namespace NoMoreDay
