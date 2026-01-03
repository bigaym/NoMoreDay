#include "ProjectileSystem.hpp"
#include "../components/Projectile.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/EffectComponent.hpp" // For DamagePopup
#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
#include "SkillSystem.hpp"
#include "../tools/Logger.hpp"
#include "raylib.h"

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
                // Hit confirmed
                proj.hitEntities.push_back(target); // Record the hit
                
                Tag hit_tags = Tag::Projectile | Tag::Hit;
                
                uint32_t skill_id = 0;
                if (auto* skillComp = registry.try_get<SkillComponent>(entity)) {
                    skill_id = skillComp->skill_id;
                }

                // Calculate Damage via Pipeline
                DamagePool base;
                auto result = DamagePipeline::Calculate(registry, proj.owner, target, skill_id, base, hit_tags, entity);
                
                float finalDamage = result.total_damage;
                if (finalDamage <= 0.0f) finalDamage = 5.0f; // Minimum damage for prototype feedback

                CombatSystem::ApplyDamage(registry, target, finalDamage, proj.owner);
                
                // Trigger Skill Hit interactions
                SkillSystem::OnSkillHit(registry, proj.owner, target, skill_id, hit_tags);

                LOG_DEBUG("Projectile {} hit {} for {:.1f} dmg", (uint32_t)entity, (uint32_t)target, finalDamage);

                if (!proj.pierce) {
                    hit = true;
                } else if (proj.pierceCount > 0) {
                    proj.pierceCount--;
                    if (proj.pierceCount <= 0) hit = true;
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
