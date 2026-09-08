#include "game/systems/skill/SummonAISystem.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/systems/skill/SummonCombatBridge.hpp"
#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace NoMoreDay::systems {

void SummonAISystem::Update(entt::registry &registry, float dt,
                            const SpatialHashGrid &grid) {
  auto view = registry.view<SpiritSwordTag, SummonComponent, SummonAIProfile,
                            SummonRuntimeState, SpiritSwordAI, Position>();

  for (auto entity : view) {
    auto &summon = view.get<SummonComponent>(entity);
    auto &aiProfile = view.get<SummonAIProfile>(entity);
    auto &runtime = view.get<SummonRuntimeState>(entity);
    auto &ai = view.get<SpiritSwordAI>(entity);
    auto &pos = view.get<Position>(entity);

    if (!registry.valid(summon.owner)) {
      continue;
    }
    const auto *ownerPos = registry.try_get<Position>(summon.owner);
    if (!ownerPos) {
      continue;
    }

    const auto *formation =
        registry.try_get<BladeFormationComponent>(summon.owner);
    const bool isGiant = formation && formation->has_giant_sword;

    if (formation) {
      aiProfile.leash_radius = formation->search_radius;
      aiProfile.command_mode = formation->mode == SpiritSwordMode::Elite
                                   ? SummonCommandMode::Aggressive
                                   : SummonCommandMode::Assist;
      aiProfile.role =
          formation->melee_orbit ? SummonRole::Melee : SummonRole::Orbit;
    }

    if (formation && formation->melee_orbit &&
        aiProfile.role == SummonRole::Melee) {
      const float speed = 6.0f;
      ai.orbit_angle += dt * speed;

      const float radius = 50.0f;
      pos.x = ownerPos->x + std::cos(ai.orbit_angle) * radius;
      pos.y = ownerPos->y + std::sin(ai.orbit_angle) * radius;

      ai.attack_timer -= dt;
      if (ai.attack_timer <= 0.0f) {
        ai.attack_timer = 0.2f;
        SummonCombatBridge::ApplyMeleeOrbitContact(registry, entity, grid, pos);
      }
      continue;
    }

    const entt::entity previousTarget = runtime.current_target;
    runtime.retarget_timer -= dt;
    const bool targetInvalid =
        !registry.valid(runtime.current_target) ||
        registry.any_of<KilledTag>(runtime.current_target) ||
        !registry.all_of<Position>(runtime.current_target);

    if (targetInvalid || runtime.retarget_timer <= 0.0f) {
      runtime.current_target = entt::null;
      ai.target = entt::null;
      runtime.retarget_timer = (std::max)(0.05f, aiProfile.retarget_interval);

      if (aiProfile.command_mode != SummonCommandMode::Passive) {
        // 集中号令 (Node 314)：优先攻击最近一次命中目标
        if (formation && formation->has_concentrate &&
            registry.valid(formation->last_skill_hit_target) &&
            registry.all_of<EnemyTag, Position>(formation->last_skill_hit_target) &&
            !registry.any_of<KilledTag>(formation->last_skill_hit_target)) {
          const auto &tPos = registry.get<Position>(formation->last_skill_hit_target);
          const float distSq = Vector2DistanceSqr({ownerPos->x, ownerPos->y}, {tPos.x, tPos.y});
          if (distSq <= aiProfile.leash_radius * aiProfile.leash_radius) {
            runtime.current_target = formation->last_skill_hit_target;
            ai.target = runtime.current_target;
          }
        }

        if (!registry.valid(runtime.current_target)) {
          float bestPriority = -1e9f;
          const float searchRadius = aiProfile.leash_radius;
          grid.query(*ownerPos, searchRadius,
                     [&](entt::entity candidate, const Position &candidatePos) {
                       if (!registry.all_of<EnemyTag, Position>(candidate)) {
                         return;
                       }
                       if (registry.any_of<KilledTag>(candidate)) {
                         return;
                       }

                       const float distSq = Vector2DistanceSqr(
                           {ownerPos->x, ownerPos->y}, {candidatePos.x, candidatePos.y});
                       if (distSq > searchRadius * searchRadius) {
                         return;
                       }

                       float priority = -std::sqrt(distSq) / 100.0f;
                       if (aiProfile.command_mode == SummonCommandMode::Aggressive) {
                         if (const auto *rarity =
                                 registry.try_get<EnemyRarityComponent>(candidate)) {
                           if (rarity->rarity == EnemyRarityComponent::BOSS) {
                             priority += 1000.0f;
                           } else if (rarity->rarity ==
                                      EnemyRarityComponent::ELITE) {
                             priority += 500.0f;
                           }
                         }
                       }

                       if (priority > bestPriority) {
                         bestPriority = priority;
                         runtime.current_target = candidate;
                       }
                     });
          ai.target = runtime.current_target;
        }
      }
    }

    if (previousTarget != runtime.current_target) {
      CombatTelemetry::Get().RecordSummonTargetSwitch();
    }

    const float orbitSpeed = isGiant ? 2.5f : 3.5f;
    ai.orbit_angle += dt * orbitSpeed;
    const auto *sentinel = registry.try_get<OrbitingSentinelComponent>(summon.owner);
    const float orbitRadius = sentinel ? sentinel->orbit_radius : (isGiant ? 55.0f : 35.0f);
    float targetX = ownerPos->x + std::cos(ai.orbit_angle) * orbitRadius;
    float targetY = ownerPos->y + std::sin(ai.orbit_angle) * orbitRadius;

    if (isGiant) {
      targetX += std::sin(ai.orbit_angle * 2.0f) * 10.0f;
      targetY += std::cos(ai.orbit_angle * 2.0f) * 10.0f;
    }

    const float moveSpeed = isGiant ? 8.0f : 12.0f;
    pos.x += (targetX - pos.x) * moveSpeed * dt;
    pos.y += (targetY - pos.y) * moveSpeed * dt;

    float effectiveDt = dt;
    if (formation && formation->sword_step_haste > 0.0f) {
      if (const auto *effects = registry.try_get<ActiveEffectsComponent>(summon.owner)) {
        if (effects->Get(BuffId::SwordStep) != nullptr) {
          effectiveDt *= (1.0f + formation->sword_step_haste);
        }
      }
    }

    // 剑阵共鸣 (Node 355): 当处于 [剑阵·诛仙] 范围内时，灵剑获得 50% 攻击速度加成
    if (formation && formation->has_array_resonance) {
      auto arrayView = registry.view<SwordArrayComponent, Position>();
      for (auto arrEnt : arrayView) {
        const auto &arr = arrayView.get<SwordArrayComponent>(arrEnt);
        if (arr.owner == summon.owner) {
          const auto &arrPos = arrayView.get<Position>(arrEnt);
          const float d2 = Vector2DistanceSqr({ownerPos->x, ownerPos->y}, {arrPos.x, arrPos.y});
          if (d2 <= arr.radius * arr.radius) {
            effectiveDt *= 1.50f;
            break;
          }
        }
      }
    }

    ai.attack_timer -= effectiveDt;
    if (ai.attack_timer <= 0.0f && registry.valid(runtime.current_target)) {
      ai.attack_timer = ai.attack_interval;
      SummonCombatBridge::CastSpiritSwordShadow(
          registry, entity, runtime.current_target, {pos.x, pos.y}, isGiant);
    }
  }
}

} // namespace NoMoreDay::systems
