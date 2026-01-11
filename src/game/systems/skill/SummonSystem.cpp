#include "game/systems/skill/SummonSystem.hpp"
#include "game/components/Common.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include <raymath.h>
#include <algorithm>

namespace NoMoreDay::systems {

void SummonSystem::Update(entt::registry& registry, float dt, const SpatialHashGrid& grid) {
    // 1. Lifetime Management
    auto summonView = registry.view<NoMoreDay::SummonComponent>();
    for (auto entity : summonView) {
        auto& summon = summonView.get<NoMoreDay::SummonComponent>(entity);
        summon.lifetime -= dt;
        if (summon.lifetime <= 0) {
            registry.destroy(entity);
            continue;
        }
    }

    // 2. Spirit Sword AI
    UpdateSpiritSwords(registry, dt, grid);
}

void SummonSystem::UpdateSpiritSwords(entt::registry& registry, float dt, const SpatialHashGrid& grid) {
    auto view = registry.view<NoMoreDay::SpiritSwordTag, NoMoreDay::SummonComponent, NoMoreDay::SpiritSwordAI, Position>();
    
    for (auto entity : view) {
        auto& summon = view.get<NoMoreDay::SummonComponent>(entity);
        auto& ai = view.get<NoMoreDay::SpiritSwordAI>(entity);
        auto& pos = view.get<Position>(entity);

        if (!registry.valid(summon.owner)) continue;
        auto* ownerPos = registry.try_get<Position>(summon.owner);
        if (!ownerPos) continue;

        auto* formation = registry.try_get<NoMoreDay::BladeFormationComponent>(summon.owner);
        NoMoreDay::SpiritSwordMode mode = formation ? formation->mode : NoMoreDay::SpiritSwordMode::Guardian;
        float searchRadius = formation ? formation->search_radius : 300.0f;

        // --- Targeting ---
        if (!registry.valid(ai.target) || registry.all_of<KilledTag>(ai.target)) {
            ai.target = entt::null;
            float bestPriority = -1e9f;

            grid.query(*ownerPos, searchRadius, [&](entt::entity candidate) {
                if (!registry.all_of<EnemyTag, Position>(candidate)) return;
                const auto& cPos = registry.get<Position>(candidate);
                float distSq = Vector2DistanceSqr({ownerPos->x, ownerPos->y}, {cPos.x, cPos.y});
                if (distSq > searchRadius * searchRadius) return;

                float priority = 0.0f;
                if (mode == NoMoreDay::SpiritSwordMode::Elite) {
                    if (auto* rarity = registry.try_get<EnemyRarityComponent>(candidate)) {
                        if (rarity->rarity == EnemyRarityComponent::BOSS) priority += 1000.0f;
                        else if (rarity->rarity == EnemyRarityComponent::ELITE) priority += 500.0f;
                    }
                }
                // Tie-breaker: distance to owner
                priority -= sqrtf(distSq) / 100.0f;

                if (priority > bestPriority) {
                    bestPriority = priority;
                    ai.target = candidate;
                }
            });
        }

        // --- Movement (Orbiting) ---
        ai.orbit_angle += dt * 3.0f;
        float radius = 45.0f;
        float targetX = ownerPos->x + cosf(ai.orbit_angle) * radius;
        float targetY = ownerPos->y + sinf(ai.orbit_angle) * radius;

        // Smoothly move towards orbit position
        pos.x += (targetX - pos.x) * 10.0f * dt;
        pos.y += (targetY - pos.y) * 10.0f * dt;

        // --- Combat ---
        ai.attack_timer -= dt;
        if (ai.attack_timer <= 0 && registry.valid(ai.target)) {
            ai.attack_timer = ai.attack_interval;
            
            const auto& tPos = registry.get<Position>(ai.target);
            
            // Create proxy caster to modify stats and radius
            auto proxy = registry.create();
            registry.emplace<NoMoreDay::SpiritSwordTag>(proxy);
            if (auto* pStats = registry.try_get<CombatStats>(summon.owner)) {
                CombatStats proxyStats = *pStats;
                for (auto& m : proxyStats.damage_multipliers) m *= 0.5f; // 50% Damage
                registry.emplace<CombatStats>(proxy, proxyStats);
            }
            
            NoMoreDay::SkillSystem::ShadowCast(registry, proxy, 2, {pos.x, pos.y}, {tPos.x, tPos.y});
            registry.destroy(proxy);
            
            // Visual feedback
            auto& particleSys = GPUParticleSystem::Get();
            particleSys.Emit(InkEffectHelper::CreateInkTrail({pos.x, pos.y}, {0, -50}, 1.0f, 0.5f));
        }
    }
}

} // namespace NoMoreDay::systems
