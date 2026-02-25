#include "game/systems/skill/SummonCombatBridge.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <raymath.h>

namespace NoMoreDay::systems {

namespace {

void BlendMixedInheritance(CombatStats &base, const CombatStats &dynamic) {
  constexpr float kSnapshotWeight = 0.6f;
  constexpr float kDynamicWeight = 0.4f;
  for (size_t i = 0; i < base.flat_damage.size(); ++i) {
    base.flat_damage[i] =
        base.flat_damage[i] * kSnapshotWeight + dynamic.flat_damage[i] * kDynamicWeight;
    base.damage_multipliers[i] = base.damage_multipliers[i] * kSnapshotWeight +
                                 dynamic.damage_multipliers[i] * kDynamicWeight;
  }
  base.min_weapon_damage =
      base.min_weapon_damage * kSnapshotWeight + dynamic.min_weapon_damage * kDynamicWeight;
  base.max_weapon_damage =
      base.max_weapon_damage * kSnapshotWeight + dynamic.max_weapon_damage * kDynamicWeight;
  base.crit_chance =
      base.crit_chance * kSnapshotWeight + dynamic.crit_chance * kDynamicWeight;
  base.crit_damage =
      base.crit_damage * kSnapshotWeight + dynamic.crit_damage * kDynamicWeight;
  base.armor_pen =
      base.armor_pen * kSnapshotWeight + dynamic.armor_pen * kDynamicWeight;
  base.attack_speed =
      base.attack_speed * kSnapshotWeight + dynamic.attack_speed * kDynamicWeight;
}

void ApplyDamageScale(CombatStats &stats, float damageScale) {
  for (auto &mult : stats.damage_multipliers) {
    mult *= damageScale;
  }
  for (auto &flat : stats.flat_damage) {
    flat *= damageScale;
  }
  stats.min_weapon_damage *= damageScale;
  stats.max_weapon_damage *= damageScale;
}

} // namespace

CombatStats SummonCombatBridge::ResolveInheritedStats(entt::registry &registry,
                                                      entt::entity summon) {
  CombatStats resolved = {};
  const auto *summonComp = registry.try_get<SummonComponent>(summon);
  const auto *runtime = registry.try_get<SummonRuntimeState>(summon);
  const auto *profile = registry.try_get<SummonCombatProfile>(summon);

  if (summonComp && registry.valid(summonComp->owner)) {
    if (const auto *ownerStats = registry.try_get<CombatStats>(summonComp->owner)) {
      resolved = *ownerStats;
    }
  }

  const CombatStats *snapshotStats =
      (runtime && runtime->has_snapshot) ? &runtime->snapshot_stats : nullptr;

  if (profile) {
    switch (profile->inherit_mode) {
    case SummonInheritMode::Snapshot:
      if (snapshotStats) {
        resolved = *snapshotStats;
      }
      break;
    case SummonInheritMode::Dynamic:
      break;
    case SummonInheritMode::Mixed:
      if (snapshotStats) {
        CombatStats mixed = *snapshotStats;
        BlendMixedInheritance(mixed, resolved);
        resolved = mixed;
      }
      break;
    }
    ApplyDamageScale(resolved, profile->damage_scale);
  }

  return resolved;
}

bool SummonCombatBridge::ConsumeProcBudget(entt::registry &registry,
                                           entt::entity summon, float cost) {
  auto *runtime = registry.try_get<SummonRuntimeState>(summon);
  const auto *profile = registry.try_get<SummonCombatProfile>(summon);
  if (!runtime || !profile || profile->proc_budget_cap <= 0.0f) {
    return true;
  }
  if (runtime->proc_budget < cost) {
    return false;
  }
  runtime->proc_budget -= cost;
  return true;
}

void SummonCombatBridge::EnsureSummonAttribution(entt::registry &registry,
                                                 entt::entity summon) {
  if (!registry.valid(summon)) {
    return;
  }
  const auto *summonComp = registry.try_get<SummonComponent>(summon);
  if (!summonComp || !registry.valid(summonComp->owner)) {
    return;
  }

  auto &ctx = registry.get_or_emplace<SummonAttributionContext>(summon);
  ctx.owner = summonComp->owner;
  ctx.summon = summon;
  ctx.source_skill_id = summonComp->skill_id;
}

bool SummonCombatBridge::CastSpiritSwordShadow(entt::registry &registry,
                                               entt::entity summon,
                                               entt::entity target,
                                               const Vector2 &origin,
                                               bool is_giant) {
  if (!registry.valid(summon) || !registry.valid(target)) {
    return false;
  }
  if (!registry.all_of<Position>(target)) {
    return false;
  }
  if (!ConsumeProcBudget(registry, summon, 1.0f)) {
    return false;
  }

  registry.emplace_or_replace<CombatStats>(summon,
                                           ResolveInheritedStats(registry, summon));
  EnsureSummonAttribution(registry, summon);

  const auto &targetPos = registry.get<Position>(target);
  if (!SkillSystem::ShadowCast(registry, summon, 2, origin,
                               {targetPos.x, targetPos.y})) {
    return false;
  }

  auto &particleSys = GPUParticleSystem::Get();
  components::GPUParticle muzzle;
  muzzle.position = origin;
  const Vector2 dir = Vector2Normalize(Vector2Subtract(
      {targetPos.x, targetPos.y}, {origin.x, origin.y}));
  muzzle.velocity = Vector2Scale(dir, 100.0f);
  muzzle.color = is_giant ? GOLD : ColorAlpha(SKYBLUE, 0.8f);
  muzzle.lifetime = 0.15f;
  muzzle.maxLifetime = 0.15f;
  muzzle.scale = is_giant ? 4.0f : 2.0f;
  muzzle.flags = 2;
  particleSys.Emit(muzzle);

  if (is_giant) {
    auto splash = InkEffectHelper::CreateInkSplash(origin, 8, 15.0f, 120.0f);
    for (auto &p : splash) {
      p.color = GOLD;
      particleSys.Emit(p);
    }
  } else {
    particleSys.Emit(
        InkEffectHelper::CreateInkTrail(origin, Vector2Scale(dir, 150.0f), 0.8f, 0.3f));
  }

  return true;
}

void SummonCombatBridge::ApplyMeleeOrbitContact(entt::registry &registry,
                                                entt::entity summon,
                                                const SpatialHashGrid &grid,
                                                const Position &origin,
                                                float hit_radius,
                                                float base_damage) {
  if (!registry.valid(summon)) {
    return;
  }

  auto *summonComp = registry.try_get<SummonComponent>(summon);
  if (!summonComp || !registry.valid(summonComp->owner)) {
    return;
  }

  registry.emplace_or_replace<CombatStats>(summon,
                                           ResolveInheritedStats(registry, summon));
  EnsureSummonAttribution(registry, summon);

  grid.query(origin, hit_radius, [&](entt::entity target, const Position &targetPos) {
    if (!registry.all_of<EnemyTag, CombatStats>(target)) {
      return;
    }
    if (registry.any_of<KilledTag>(target)) {
      return;
    }

    const float distSq =
        Vector2DistanceSqr({origin.x, origin.y}, {targetPos.x, targetPos.y});
    if (distSq > hit_radius * hit_radius) {
      return;
    }
    if (!ConsumeProcBudget(registry, summon, 1.0f)) {
      return;
    }

    DamagePool pool;
    pool.Add(Tag::Physical, base_damage);
    DamageRequest request;
    request.attacker = summon;
    request.defender = target;
    request.skill_id = summonComp->skill_id;
    request.base_pool = pool;
    request.additional_tags = Tag::Melee | Tag::Hit;
    request.source_entity = summon;

    const auto result = DamagePipeline::Calculate(registry, request);
    CombatSystem::ApplyDamage(registry, target, result.total_damage, summon,
                              result.is_crit);

    if (result.total_damage > 0.0f) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateMinionHit(
                        summonComp->owner, summon, target, result.total_damage,
                        result.is_crit));
      auto splash =
          InkEffectHelper::CreateInkSplash({targetPos.x, targetPos.y}, 3, 5.0f, 80.0f);
      if (!splash.empty()) {
        GPUParticleSystem::Get().Emit(splash.front());
      }
    }
  });
}

} // namespace NoMoreDay::systems
