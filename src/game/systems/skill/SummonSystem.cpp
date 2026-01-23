#include "game/systems/skill/SummonSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <raymath.h>

namespace NoMoreDay::systems {

void SummonSystem::Update(entt::registry &registry, float dt,
                          const SpatialHashGrid &grid) {
  // 1. Lifetime Management
  auto summonView = registry.view<NoMoreDay::SummonComponent>();
  for (auto entity : summonView) {
    auto &summon = summonView.get<NoMoreDay::SummonComponent>(entity);
    summon.lifetime -= dt;
    if (summon.lifetime <= 0) {
      registry.destroy(entity);
      continue;
    }
  }

  // 2. Spirit Sword AI
  UpdateSpiritSwords(registry, dt, grid);
}

void SummonSystem::UpdateSpiritSwords(entt::registry &registry, float dt,
                                      const SpatialHashGrid &grid) {
  auto view =
      registry.view<NoMoreDay::SpiritSwordTag, NoMoreDay::SummonComponent,
                    NoMoreDay::SpiritSwordAI, Position>();

  for (auto entity : view) {
    auto &summon = view.get<NoMoreDay::SummonComponent>(entity);
    auto &ai = view.get<NoMoreDay::SpiritSwordAI>(entity);
    auto &pos = view.get<Position>(entity);

    if (!registry.valid(summon.owner))
      continue;
    auto *ownerPos = registry.try_get<Position>(summon.owner);
    if (!ownerPos)
      continue;

    auto *formation =
        registry.try_get<NoMoreDay::BladeFormationComponent>(summon.owner);

    // --- Phase 5: Melee Orbit (Talent 352) ---
    if (formation && formation->melee_orbit) {
      // 1. High Speed Orbit
      float speed = 6.0f; // Fast rotation
      ai.orbit_angle += dt * speed;

      // 2. Tighter Radius
      float radius = 50.0f;
      float targetX = ownerPos->x + cosf(ai.orbit_angle) * radius;
      float targetY = ownerPos->y + sinf(ai.orbit_angle) * radius;

      pos.x = targetX;
      pos.y = targetY;

      // 3. Contact Damage (Tick based)
      ai.attack_timer -= dt;
      if (ai.attack_timer <= 0.0f) {
        ai.attack_timer = 0.2f; // 5 hits/sec
        float hitRadius = 30.0f;

        grid.query({pos.x, pos.y}, hitRadius,
                   [&](entt::entity target, const Position &tPos) {
                     if (!registry.all_of<EnemyTag, CombatStats>(target))
                       return;
                     if (registry.any_of<KilledTag>(target))
                       return;

                     float distSq =
                         Vector2DistanceSqr({pos.x, pos.y}, {tPos.x, tPos.y});
                     if (distSq <= hitRadius * hitRadius) {
                       // Apply Damage
                       // Use owner as source
                       CombatSystem::ApplyDamage(registry, target, 25.0f,
                                                 summon.owner);

                       // VFX
                       auto &particles = GPUParticleSystem::Get();
                       particles.Emit(InkEffectHelper::CreateInkSplash(
                           {tPos.x, tPos.y}, 3, 5.0f, 80.0f)[0]);
                     }
                   });
      }
      continue; // Skip standard behavior
    }

    NoMoreDay::SpiritSwordMode mode =
        formation ? formation->mode : NoMoreDay::SpiritSwordMode::Guardian;
    float searchRadius = formation ? formation->search_radius : 300.0f;
    bool isGiant = formation && formation->has_giant_sword;

    // --- Targeting ---
    if (!registry.valid(ai.target) || registry.all_of<KilledTag>(ai.target)) {
      ai.target = entt::null;
      float bestPriority = -1e9f;

      grid.query(*ownerPos, searchRadius,
                 [&](entt::entity candidate, const Position &cPos) {
                   if (!registry.all_of<EnemyTag, Position>(candidate))
                     return;
                   float distSq = Vector2DistanceSqr({ownerPos->x, ownerPos->y},
                                                     {cPos.x, cPos.y});
                   if (distSq > searchRadius * searchRadius)
                     return;

                   float priority = 0.0f;
                   if (mode == NoMoreDay::SpiritSwordMode::Elite) {
                     if (auto *rarity = registry.try_get<EnemyRarityComponent>(
                             candidate)) {
                       if (rarity->rarity == EnemyRarityComponent::BOSS)
                         priority += 1000.0f;
                       else if (rarity->rarity == EnemyRarityComponent::ELITE)
                         priority += 500.0f;
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
    float orbitSpeed = isGiant ? 2.5f : 3.5f;
    ai.orbit_angle += dt * orbitSpeed;

    float radius = isGiant ? 55.0f : 35.0f;
    float targetX = ownerPos->x + cosf(ai.orbit_angle) * radius;
    float targetY = ownerPos->y + sinf(ai.orbit_angle) * radius;

    // Giant sword wobbles a bit for effect
    if (isGiant) {
      targetX += sinf(ai.orbit_angle * 2.0f) * 10.0f;
      targetY += cosf(ai.orbit_angle * 2.0f) * 10.0f;
    }

    // Smoothly move towards orbit position
    float moveSpeed = isGiant ? 8.0f : 12.0f;
    pos.x += (targetX - pos.x) * moveSpeed * dt;
    pos.y += (targetY - pos.y) * moveSpeed * dt;

    // --- Combat ---
    ai.attack_timer -= dt;
    if (ai.attack_timer <= 0 && registry.valid(ai.target)) {
      ai.attack_timer = ai.attack_interval;

      const auto &tPos = registry.get<Position>(ai.target);

      // Create proxy caster to modify stats and radius
      auto proxy = registry.create();
      registry.emplace<NoMoreDay::SpiritSwordTag>(proxy);
      registry.emplace<LocalLevelTag>(
          proxy); // Ensure it's cleaned up if level changes

      if (auto *pStats = registry.try_get<CombatStats>(summon.owner)) {
        CombatStats proxyStats = *pStats;
        if (isGiant) {
          for (auto &m : proxyStats.damage_multipliers)
            m *= 1.5f; // 150% Damage for Giant
        } else {
          for (auto &m : proxyStats.damage_multipliers)
            m *= 0.5f; // 50% Damage for small swords
        }
        registry.emplace<CombatStats>(proxy, proxyStats);
      }

      NoMoreDay::SkillSystem::ShadowCast(registry, proxy, 2, {pos.x, pos.y},
                                         {tPos.x, tPos.y});

      // Visual feedback
      auto &particleSys = GPUParticleSystem::Get();

      // Muzzle Flash
      components::GPUParticle muzzle;
      muzzle.position = {pos.x, pos.y};
      Vector2 dir =
          Vector2Normalize(Vector2Subtract({tPos.x, tPos.y}, {pos.x, pos.y}));
      muzzle.velocity = Vector2Scale(dir, 100.0f);
      muzzle.color = isGiant ? GOLD : ColorAlpha(SKYBLUE, 0.8f);
      muzzle.lifetime = 0.15f;
      muzzle.maxLifetime = 0.15f;
      muzzle.scale = isGiant ? 4.0f : 2.0f;
      muzzle.flags = 2; // Spark/Diamond
      particleSys.Emit(muzzle);

      if (isGiant) {
        // Heavier effect for Giant Sword
        auto splash =
            InkEffectHelper::CreateInkSplash({pos.x, pos.y}, 8, 15.0f, 120.0f);
        for (auto &p : splash) {
          p.color = GOLD;
          particleSys.Emit(p);
        }
      } else {
        // Small sword trail/flash
        particleSys.Emit(InkEffectHelper::CreateInkTrail(
            {pos.x, pos.y}, Vector2Scale(dir, 150.0f), 0.8f, 0.3f));
      }

      registry.destroy(proxy);
    }
  }
}

} // namespace NoMoreDay::systems