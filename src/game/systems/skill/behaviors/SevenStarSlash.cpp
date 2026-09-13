#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"

#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace NoMoreDay::skills {

namespace {

struct SevenStarScarComponent {
  entt::entity owner = entt::null;
  float radius = 0.0f;
  float damage = 0.0f;
};

struct CandidateTarget {
  entt::entity entity = entt::null;
  Vector2 position{};
  EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
  float distance_sq = 0.0f;
};

struct SlashHitSummary {
  int totalHits = 0;
  int firstSixHits = 0;
  int sameTargetHitsBeforeFinal = 0;
  int uniqueTargetsHit = 0;
  bool finalSlashCrit = false;
  entt::entity focusedTarget = entt::null;
};

float DistanceSquared(const Vector2 &a, const Vector2 &b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return (dx * dx) + (dy * dy);
}

Vector2 ToVector2(const Position &position) {
  return Vector2{position.x, position.y};
}

Vector2 Add(const Vector2 &a, const Vector2 &b) {
  return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 Sub(const Vector2 &a, const Vector2 &b) {
  return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 Scale(const Vector2 &v, float scale) {
  return Vector2{v.x * scale, v.y * scale};
}

Vector2 NormalizeSafe(const Vector2 &value, const Vector2 &defaultDirection) {
  const float len_sq = (value.x * value.x) + (value.y * value.y);
  if (len_sq <= 0.0001f) {
    return defaultDirection;
  }
  const float inv_len = 1.0f / std::sqrt(len_sq);
  return Vector2{value.x * inv_len, value.y * inv_len};
}

int GetCurrentBladeResource(const entt::registry &registry, entt::entity entity) {
  if (const auto *resource = registry.try_get<BladeResourceComponent>(entity)) {
    return resource->current;
  }
  if (const auto *intent = registry.try_get<SwordIntentComponent>(entity)) {
    return intent->stacks;
  }
  return 0;
}

EnemyRarityComponent::Rarity GetTargetRarity(const entt::registry &registry,
                                             entt::entity entity) {
  if (const auto *rarity = registry.try_get<EnemyRarityComponent>(entity)) {
    return rarity->rarity;
  }
  return EnemyRarityComponent::NORMAL;
}

bool IsEliteOrBoss(EnemyRarityComponent::Rarity rarity) {
  return rarity >= EnemyRarityComponent::ELITE;
}

std::vector<CandidateTarget> GatherTargets(entt::registry &registry,
                                           entt::entity owner,
                                           const Vector2 &center,
                                           float radius,
                                           bool prioritizeRare) {
  std::vector<CandidateTarget> targets;
  const float radiusSq = radius * radius;
  auto view = registry.view<EnemyTag, Position, CombatStats>();
  for (const entt::entity entity : view) {
    if (entity == owner) {
      continue;
    }
    const auto &position = view.get<Position>(entity);
    const Vector2 pos = ToVector2(position);
    const float distSq = DistanceSquared(pos, center);
    if (distSq > radiusSq) {
      continue;
    }

    targets.push_back({.entity = entity,
                       .position = pos,
                       .rarity = GetTargetRarity(registry, entity),
                       .distance_sq = distSq});
  }

  std::sort(targets.begin(), targets.end(), [prioritizeRare](const auto &lhs,
                                                             const auto &rhs) {
    if (prioritizeRare && lhs.rarity != rhs.rarity) {
      return lhs.rarity > rhs.rarity;
    }
    if (lhs.distance_sq != rhs.distance_sq) {
      return lhs.distance_sq < rhs.distance_sq;
    }
    return entt::to_integral(lhs.entity) < entt::to_integral(rhs.entity);
  });
  return targets;
}

bool IsLiveTargetCandidate(entt::registry &registry, entt::entity entity) {
  if (registry.all_of<KilledTag>(entity)) {
    return false;
  }
  if (const auto *stats = registry.try_get<CombatStats>(entity)) {
    if (stats->health <= 0.0f) {
      return false;
    }
  }
  if (const auto *health = registry.try_get<HealthComponent>(entity)) {
    if (health->current <= 0.0f) {
      return false;
    }
  }
  return true;
}

std::vector<CandidateTarget> GatherLiveTargets(entt::registry &registry,
                                               entt::entity owner,
                                               const Vector2 &center,
                                               float radius,
                                               bool prioritizeRare) {
  auto targets = GatherTargets(registry, owner, center, radius, prioritizeRare);
  targets.erase(std::remove_if(targets.begin(), targets.end(),
                               [&](const CandidateTarget &candidate) {
                                 return !IsLiveTargetCandidate(registry,
                                                               candidate.entity);
                               }),
                targets.end());
  return targets;
}

DamageExecutionResult ApplySlashDamage(entt::registry &registry,
                                       entt::entity owner,
                                       entt::entity target, float damage,
                                       uint32_t skillId,
                                       float critChanceBonus = 0.0f,
                                       float critDamageBonus = 0.0f,
                                       bool forceCrit = false) {
  DamagePool pool;
  pool.Add(Tag::Physical, damage);

  auto *ownerStats = registry.try_get<CombatStats>(owner);
  float oldCritChance = 0.0f;
  float oldCritDamage = 0.0f;
  if (ownerStats != nullptr) {
    oldCritChance = ownerStats->crit_chance;
    oldCritDamage = ownerStats->crit_damage;
    ownerStats->crit_chance += critChanceBonus;
    ownerStats->crit_damage += critDamageBonus;
  }

  DamageRequest request;
  request.attacker = owner;
  request.defender = target;
  request.skill_id = skillId;
  request.base_pool = pool;
  request.additional_tags = Tag::Melee | Tag::SwordSkill | Tag::Hit;
  if (forceCrit) {
    request.additional_tags = request.additional_tags | Tag::Critical;
  }
  request.source_entity = owner;
  auto result = ResolveDamage(registry, request, owner);

  if (ownerStats != nullptr) {
    ownerStats->crit_chance = oldCritChance;
    ownerStats->crit_damage = oldCritDamage;
  }
  return result;
}

void SpawnScar(entt::registry &registry, entt::entity owner, const Vector2 &position,
               float duration, float radius, float damage) {
  auto scar = registry.create();
  registry.emplace<Position>(scar, position.x, position.y);
  registry.emplace<SevenStarScarComponent>(scar, owner, radius, damage);
  registry.emplace<DelayedDestroyComponent>(scar, duration);
}

void ExplodeScars(entt::registry &registry, entt::entity owner,
                  entt::entity focusedTarget, float centerRadius,
                  float focusedSplashMultiplier) {
  std::vector<entt::entity> scarEntities;
  auto scarView = registry.view<Position, SevenStarScarComponent>();
  for (const entt::entity scar : scarView) {
    const auto &scarData = scarView.get<SevenStarScarComponent>(scar);
    if (scarData.owner != owner) {
      continue;
    }

    const Vector2 scarPos = ToVector2(scarView.get<Position>(scar));
    auto targets = GatherLiveTargets(registry, owner, scarPos, scarData.radius, false);
    for (const auto &candidate : targets) {
      (void)ApplySlashDamage(registry, owner, candidate.entity, scarData.damage,
                             seven_star_shared::kSevenStarSlashSkillId);
    }

    if (focusedTarget != entt::null) {
      if (const auto *focusPos = registry.try_get<Position>(focusedTarget)) {
        if (DistanceSquared(scarPos, ToVector2(*focusPos)) <= centerRadius * centerRadius) {
          (void)ApplySlashDamage(registry, owner, focusedTarget,
                                 scarData.damage * focusedSplashMultiplier,
                                 seven_star_shared::kSevenStarSlashSkillId);
        }
      }
    }

    scarEntities.push_back(scar);
  }

  for (const entt::entity scar : scarEntities) {
    registry.destroy(scar);
  }
}

void GrantStarVeil(entt::registry &registry, entt::entity owner, int points,
                   float healthCapRatio) {
  auto *stats = registry.try_get<CombatStats>(owner);
  if (stats == nullptr || points <= 0) {
    return;
  }
  const float dexterity = std::max(0.0f, stats->effective_dexterity);
  const float addedBarrier = dexterity * static_cast<float>(points);
  const float cap = stats->max_health * healthCapRatio;
  stats->barrier = std::min(cap, stats->barrier + addedBarrier);
  (void)registry.get_or_emplace<BarrierComponent>(owner);
}

void GrantSwordStepMirage(entt::registry &registry, entt::entity owner) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect buff;
  buff.id = std::string(BuffIdToString(BuffId::SevenStarSwordStepMirage));
  buff.name = "Sword Step Mirage";
  buff.type = BuffType::SpeedUp;
  buff.duration = 1.5f;
  buff.remaining = 1.5f;
  buff.modifiers.push_back({.value = 15.0f,
                            .type = StatType::CritChance,
                            .mode = ModifierMode::Flat});
  buff.modifiers.push_back({.value = 100.0f,
                            .type = StatType::DodgeRating,
                            .mode = ModifierMode::Flat});
  buff.modifiers.push_back({.value = 20.0f,
                            .type = StatType::MoveSpeed,
                            .mode = ModifierMode::PercentAdd});
  effects.AddOrRefresh(buff);
}

void GrantRevolvingEdgeWindow(entt::registry &registry, entt::entity owner,
                              int points) {
  if (points <= 0) {
    return;
  }
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect buff;
  buff.id = seven_star_shared::kRevolvingEdgeBuffId;
  buff.name = "Revolving Edge";
  buff.type = BuffType::PowerBoost;
  buff.duration = 2.0f;
  buff.remaining = 2.0f;
  buff.stacks = points;
  buff.max_stacks = 3;
  effects.AddOrRefresh(buff);
}

void GrantQiyaoWindow(entt::registry &registry, entt::entity owner, int hits) {
  const int stacks = std::clamp(hits, 0, 3);
  if (stacks <= 0) {
    return;
  }
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect buff;
  buff.id = seven_star_shared::kQiyaoBuffId;
  buff.name = "Sevenfold Momentum";
  buff.type = BuffType::PowerBoost;
  buff.duration = 3.0f;
  buff.remaining = 3.0f;
  buff.stacks = stacks;
  buff.max_stacks = 3;
  effects.AddOrRefresh(buff);
}

void GrantReturningStepWindow(entt::registry &registry, entt::entity owner) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect buff;
  buff.id = seven_star_shared::kReturningStepBuffId;
  buff.name = "Returning Step";
  buff.type = BuffType::SpeedUp;
  buff.duration = 3.0f;
  buff.remaining = 3.0f;
  effects.AddOrRefresh(buff);
}

Vector2 ResolveLandingPosition(entt::registry &registry, entt::entity owner,
                               const Vector2 &defaultLanding, entt::entity focusedTarget,
                               bool behindTarget) {
  if (!behindTarget || focusedTarget == entt::null) {
    return defaultLanding;
  }

  const auto *ownerPos = registry.try_get<Position>(owner);
  const auto *targetPos = registry.try_get<Position>(focusedTarget);
  if (ownerPos == nullptr || targetPos == nullptr) {
    return defaultLanding;
  }

  const Vector2 targetVec = ToVector2(*targetPos);
  const Vector2 fromOwner = NormalizeSafe(Sub(targetVec, ToVector2(*ownerPos)),
                                          Vector2{1.0f, 0.0f});
  return Add(targetVec, Scale(fromOwner, 18.0f));
}

} // namespace

struct SevenStarSlash : SkillBehaviorBase<SevenStarSlash> {
  static constexpr uint32_t kSkillId = seven_star_shared::kSevenStarSlashSkillId;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    const auto *skillData = SkillRegistry::Get().GetSkill(kSkillId);
    auto *ownerPos = registry.try_get<Position>(owner);
    auto *ownerStats = registry.try_get<CombatStats>(owner);
    if (skillData == nullptr || ownerPos == nullptr || ownerStats == nullptr) {
      return;
    }

    const SevenStarSlashSpecState specState = ResolveSpecState(registry, owner);

    // 机制数值一次性读取：避免在斩击循环内重复查表（设计 §4.4）。
    const float targetLockRescuePerPoint = GetMech(
        seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::TargetLock,
        "rescue_radius_per_point", 0.15f);
    const float critChancePerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::CritChance,
                "crit_chance_per_point", 0.02f);
    const float quickStarRefundPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::QuickStar,
                "cooldown_refund_per_point", 0.04f);
    const float chaseStepRefundPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ChaseStep,
                "movement_refund_per_point", 0.08f);
    const float endlessSevenFinalDamageMult =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::EndlessSeven,
                "final_damage_mult", 0.8f);
    const float voidTreadDurationPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::VoidTread,
                "invulnerable_duration_per_point", 0.03f);
    const float starVeilBarrierCapRatio =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::StarVeil,
                "barrier_health_cap_ratio", 0.15f);
    const float gateOfLifeHealPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::GateOfLife,
                "heal_percent_per_point", 0.02f);
    const float lingeringScarDurationPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::LingeringScar,
                "scar_duration_per_point", 0.4f);
    const float poleStarOrbitDamageMult =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::PoleStarOrbit,
                "damage_mult", 0.85f);
    const float starfallDamageMult =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::Starfall,
                "damage_mult", 1.35f);
    const float starfallRadiusPerPoint = GetMech(
        seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ShatteredConstellation,
        "starfall_radius_per_point", 0.08f);
    const float starfallFinalDamagePerPoint = GetMech(
        seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ShatteredConstellation,
        "starfall_final_damage_per_point", 0.10f);
    const float orbitStreakDamagePerPoint = GetMech(
        seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ShatteredConstellation,
        "orbit_streak_damage_per_point", 0.04f);
    const float scarRuinDurationPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ScarRuin,
                "scar_duration_per_point", 0.3f);
    const float scarRuinRadiusPerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ScarRuin,
                "scar_radius_per_point", 6.0f);
    const float scarRuinExplodePerPoint =
        GetMech(seven_star_shared::kSevenStarSlashSkillId, SevenStarSlashNodes::ScarRuin,
                "explode_damage_per_point", 0.30f);

    const float baseRadius = skillData->GetParam("radius", 96.0f);
    const float flowBonusPerStack =
        skillData->GetParam("flow_bonus_per_stack", 0.06f);
    const float singleTargetExecuteBonus =
        skillData->GetParam("single_target_execute_bonus", 0.5f);
    float invulnerableDuration =
        skillData->GetParam("invulnerable_duration", 0.5f);
    invulnerableDuration +=
        voidTreadDurationPerPoint * static_cast<float>(specState.voidTreadPoints);

    const int resourceToSpend = GetCurrentBladeResource(registry, owner);
    if (resourceToSpend > 0) {
      (void)SkillSystem::ConsumeSwordIntent(registry, owner, resourceToSpend, kSkillId);
      exec.is_empowered = true;
      if (resourceToSpend >= SkillConstants::DEFAULT_MAX_SWORD_INTENT) {
        seven_star_shared::ResetSkillCooldown(
            registry, owner, seven_star_shared::kFlowingThrustSkillId);
      }
    }

    // 节点 1000 非 trigger 节点：SkillRegistry 对其使用默认契约 range_mult=1.0，
    // 迁移前 getModifier 实际返回 1.0；默认值与机制表补键值保持一致以避免行为变化。
    float acquisitionRadius =
        baseRadius * (1.0f + GetMech(kSkillId, SevenStarSlashNodes::TargetLock,
                                     "acquisition_radius_per_point", 1.0f) *
                                static_cast<float>(specState.targetLockPoints));
    auto targets = GatherLiveTargets(registry, owner, exec.target_pos,
                                     acquisitionRadius, specState.sevenFocus);
    entt::entity focusedTarget = !targets.empty() ? targets.front().entity : entt::null;
    const bool singleTarget = targets.size() == 1;

    // 普通形态斩击数来自机制表技能级节点 0（原 skills.json params.slash_count 死键）；
    // 星落形态固定 4 段（3 段环绕 + 1 段收束），不受该键影响。
    const int slashCount =
        specState.starfall
            ? 4
            : static_cast<int>(GetMech(kSkillId, 0, "slash_count", 7.0f));
    float hitRadius = specState.starfall ? baseRadius * 0.50f : baseRadius * 0.28f;
    if (specState.starScarFollow) {
      hitRadius *= GetMech(kSkillId, SevenStarSlashNodes::StarScarFollow,
                           "hit_radius_mult", 1.2f);
    }
    float baseDamageMultiplier = 1.0f;
    if (specState.poleStarOrbit) {
      hitRadius = baseRadius * 0.34f;
      baseDamageMultiplier *= poleStarOrbitDamageMult;
    }
    if (specState.starfall) {
      baseDamageMultiplier *= starfallDamageMult;
      hitRadius *= 1.0f + starfallRadiusPerPoint * static_cast<float>(specState.shatteredConstellationPoints);
    }

    registry.emplace_or_replace<InvulnerableComponent>(
        owner, InvulnerableComponent{invulnerableDuration, 0.0f, owner, SKYBLUE,
                                     hitRadius});

    const float averageWeaponDamage =
        0.5f * (ownerStats->min_weapon_damage + ownerStats->max_weapon_damage);
    const float baseSlashDamage = std::max(
        1.0f,
        averageWeaponDamage * baseDamageMultiplier *
            (skillData->weapon_damage_mult +
             static_cast<float>(resourceToSpend) * flowBonusPerStack));

    SlashHitSummary summary;
    summary.focusedTarget = focusedTarget;
    std::unordered_set<entt::entity> uniqueHits;
    int resourceRefunds = 0;
    int orbitStreak = 0;
    entt::entity orbitLastTarget = entt::null;

    const Vector2 castCenter = exec.target_pos;
    const Vector2 ownerStart = ToVector2(*ownerPos);
    const bool hadSwordStep =
        seven_star_shared::FindBuff(registry, owner,
                                    BuffIdToString(BuffId::SwordStep)) !=
        nullptr;

    for (int slashIndex = 0; slashIndex < slashCount; ++slashIndex) {
      const bool isFinalSlash = (slashIndex == slashCount - 1);
      Vector2 slashCenter = castCenter;
      std::vector<CandidateTarget> slashTargets;

      if (specState.sevenFocus && focusedTarget != entt::null) {
        if (const auto *targetPos = registry.try_get<Position>(focusedTarget)) {
          slashCenter = ToVector2(*targetPos);
        }
      } else if (specState.poleStarOrbit) {
        const Vector2 anchor =
            focusedTarget != entt::null && registry.try_get<Position>(focusedTarget) != nullptr
                ? ToVector2(*registry.try_get<Position>(focusedTarget))
                : castCenter;
        const float angle =
            static_cast<float>(slashIndex) * (2.0f * PI / static_cast<float>(slashCount));
        const float orbitRadius = baseRadius * 0.45f;
        slashCenter = Add(anchor, Vector2{std::cos(angle) * orbitRadius,
                                          std::sin(angle) * orbitRadius});
      } else if (specState.starfall) {
        const Vector2 anchor =
            focusedTarget != entt::null && registry.try_get<Position>(focusedTarget) != nullptr
                ? ToVector2(*registry.try_get<Position>(focusedTarget))
                : castCenter;
        if (isFinalSlash) {
          slashCenter = anchor;
        } else {
          const float angle = static_cast<float>(slashIndex) * (2.0f * PI / 3.0f);
          const float offset = baseRadius * 0.35f;
          slashCenter = Add(anchor, Vector2{std::cos(angle) * offset,
                                            std::sin(angle) * offset});
        }
      } else if (!targets.empty()) {
        slashCenter = targets[static_cast<size_t>(slashIndex) % targets.size()].position;
      }

      slashTargets = GatherLiveTargets(registry, owner, slashCenter, hitRadius,
                                       specState.sevenFocus);
      if (slashTargets.empty() && specState.targetLockPoints > 0 && focusedTarget != entt::null) {
        if (const auto *targetPos = registry.try_get<Position>(focusedTarget)) {
          const float rescueRadius = hitRadius * (1.0f + targetLockRescuePerPoint * specState.targetLockPoints);
          if (DistanceSquared(slashCenter, ToVector2(*targetPos)) <= rescueRadius * rescueRadius) {
            slashTargets.push_back({.entity = focusedTarget,
                                    .position = ToVector2(*targetPos),
                                    .rarity = GetTargetRarity(registry, focusedTarget),
                                    .distance_sq = DistanceSquared(slashCenter,
                                                                   ToVector2(*targetPos))});
          }
        }
      }

      if (specState.sevenFocus && !slashTargets.empty()) {
        slashTargets.resize(1);
      }

      for (const auto &candidate : slashTargets) {
        float slashDamage = baseSlashDamage;
        // 暴击率统一为分数制：每点 +2% = 0.02
        float critChanceBonus = critChancePerPoint * static_cast<float>(specState.critChancePoints);
        float critDamageBonus = 0.0f;

        if (isFinalSlash) {
          slashDamage *= 1.0f + GetMech(kSkillId, SevenStarSlashNodes::FinalSlash,
                                        "final_slash_damage_per_point", 0.12f) *
                                    static_cast<float>(specState.finalSlashPoints);
          if (singleTarget) {
            slashDamage *= 1.0f + singleTargetExecuteBonus;
          }
          if (specState.endlessSeven) {
            slashDamage *= endlessSevenFinalDamageMult;
          }
          if (candidate.entity == focusedTarget) {
            slashDamage *=
                1.0f + static_cast<float>(summary.sameTargetHitsBeforeFinal) *
                           (GetMech(kSkillId, SevenStarSlashNodes::ExposedWeakness,
                                    "exposed_weakness_damage_per_point", 0.02f) *
                            static_cast<float>(specState.exposedWeaknessPoints));
          }
          const bool isolated =
              focusedTarget != entt::null && targets.size() <= 1 && candidate.entity == focusedTarget;
          if (isolated || IsEliteOrBoss(candidate.rarity)) {
            slashDamage *= 1.0f + GetMech(kSkillId, SevenStarSlashNodes::PoJun,
                                          "po_jun_damage_per_point", 0.10f) *
                                      static_cast<float>(specState.poJunPoints);
          }
          if (specState.sevenFocus && isolated && candidate.entity == focusedTarget) {
            slashDamage *=
                1.0f + GetMech(kSkillId, SevenStarSlashNodes::SolitaryStar,
                               "solitary_star_damage_per_point", 0.12f) *
                           static_cast<float>(specState.solitaryStarPoints);
          }
          if (specState.starfall && specState.shatteredConstellationPoints > 0) {
            slashDamage *=
                1.0f + starfallFinalDamagePerPoint * static_cast<float>(specState.shatteredConstellationPoints);
          }
          if (const auto *stats = registry.try_get<CombatStats>(candidate.entity)) {
            if (stats->max_health > 0.0f && stats->health <= stats->max_health * 0.35f) {
              // 节点 1006 非 trigger 节点，契约默认 effectiveness=1.0（迁移前 getModifier 实际返回值）。
              critDamageBonus += GetMech(kSkillId, SevenStarSlashNodes::ZhanJiang,
                                         "zhan_jiang_crit_damage_per_point", 1.0f) *
                                 static_cast<float>(specState.zhanJiangPoints);
            }
          }
        } else {
          if (slashIndex < 3) {
            if (const auto *stats = registry.try_get<CombatStats>(candidate.entity)) {
              if (stats->max_health > 0.0f && stats->health >= stats->max_health * 0.99f) {
                critDamageBonus += GetMech(kSkillId, SevenStarSlashNodes::ZhanJiang,
                                           "zhan_jiang_crit_damage_per_point", 1.0f) *
                                   static_cast<float>(specState.zhanJiangPoints);
              }
            }
          }
          if (specState.poleStarOrbit && candidate.entity == orbitLastTarget) {
            orbitStreak = std::min(3, orbitStreak + 1);
          } else {
            orbitStreak = 1;
            orbitLastTarget = candidate.entity;
          }
          if (specState.poleStarOrbit) {
            slashDamage *= 1.0f +
                           static_cast<float>(orbitStreak - 1) *
                               (orbitStreakDamagePerPoint * static_cast<float>(specState.shatteredConstellationPoints));
          }
        }

        const auto damageResult = ApplySlashDamage(
            registry, owner, candidate.entity, slashDamage, kSkillId, critChanceBonus,
            critDamageBonus, false);
        ++summary.totalHits;
        if (!isFinalSlash) {
          ++summary.firstSixHits;
        }
        uniqueHits.insert(candidate.entity);
        if (candidate.entity == focusedTarget && !isFinalSlash) {
          ++summary.sameTargetHitsBeforeFinal;
        }
        if (isFinalSlash && damageResult.damage.is_crit) {
          summary.finalSlashCrit = true;
        }

        if (specState.flowReturnPoints > 0 && resourceRefunds < 4) {
          const float chance =
              GetMech(kSkillId, SevenStarSlashNodes::FlowReturn,
                      "flow_return_chance_per_point", 12.0f) *
              static_cast<float>(specState.flowReturnPoints);
          if (seven_star_shared::DeterministicRoll(
                  exec.cast_id + static_cast<uint64_t>(slashIndex * 31) +
                      static_cast<uint64_t>(entt::to_integral(candidate.entity)),
                  SevenStarSlashNodes::FlowReturn, chance)) {
            if (systems::BladeResourceService::Gain(registry, owner, 1, kSkillId)) {
              ++resourceRefunds;
            }
          }
        }

        if (specState.lingeringScarPoints > 0) {
          float scarDuration = lingeringScarDurationPerPoint * static_cast<float>(specState.lingeringScarPoints);
          scarDuration += scarRuinDurationPerPoint * static_cast<float>(specState.scarRuinPoints);
          float scarRadius = 18.0f + scarRuinRadiusPerPoint * static_cast<float>(specState.scarRuinPoints);
          float scarDamage = slashDamage * (isFinalSlash ? 0.30f : 0.15f);
          SpawnScar(registry, owner, candidate.position, scarDuration, scarRadius, scarDamage);
          if (specState.poleStarOrbit && specState.scarRuinPoints > 0) {
            const Vector2 mid = Add(candidate.position, Scale(Sub(slashCenter, candidate.position), 0.5f));
            SpawnScar(registry, owner, mid, scarDuration * 0.75f, scarRadius * 0.75f,
                      scarDamage * 0.5f);
          }
        }
      }
    }

    summary.uniqueTargetsHit = static_cast<int>(uniqueHits.size());

    if (specState.starScarFollow && resourceToSpend >= SkillConstants::DEFAULT_MAX_SWORD_INTENT &&
        focusedTarget != entt::null) {
      (void)ApplySlashDamage(
          registry, owner, focusedTarget,
          baseSlashDamage * GetMech(kSkillId, SevenStarSlashNodes::StarScarFollow,
                                    "follow_up_damage_ratio", 0.35f),
          kSkillId, 0.0f, 0.0f, false);
    }

    if (specState.quickStarPoints > 0) {
      seven_star_shared::RefundSkillCooldownPercent(
          registry, owner, kSkillId, quickStarRefundPerPoint * static_cast<float>(specState.quickStarPoints));
    }
    if (specState.chaseStepPoints > 0 && summary.totalHits >= 5) {
      seven_star_shared::RefundMovementCooldownsPercent(
          registry, owner, chaseStepRefundPerPoint * static_cast<float>(specState.chaseStepPoints));
    }
    if (specState.revolvingEdgePoints > 0) {
      GrantRevolvingEdgeWindow(registry, owner, specState.revolvingEdgePoints);
    }
    if (specState.endlessSeven) {
      GrantQiyaoWindow(registry, owner, summary.firstSixHits);
    }
    if (hadSwordStep && specState.swordStepMirage) {
      GrantSwordStepMirage(registry, owner);
    }
    if (specState.starVeilPoints > 0 && summary.totalHits >= 4) {
      GrantStarVeil(registry, owner, specState.starVeilPoints,
                    starVeilBarrierCapRatio);
    }
    if (specState.gateOfLifePoints > 0 && summary.uniqueTargetsHit >= 3) {
      const float missingHealth = std::max(0.0f, ownerStats->max_health - ownerStats->health);
      ownerStats->health = std::min(ownerStats->max_health,
                                    ownerStats->health +
                                        missingHealth * gateOfLifeHealPerPoint * specState.gateOfLifePoints);
    }
    if (specState.returningStep) {
      seven_star_shared::GrantSwordStep(registry, owner);
      GrantReturningStepWindow(registry, owner);
    }

    if (specState.starfall && specState.scarRuinPoints > 0 && summary.finalSlashCrit) {
      ExplodeScars(registry, owner, focusedTarget, baseRadius * 0.75f,
                    scarRuinExplodePerPoint * static_cast<float>(specState.scarRuinPoints));
    }

    const Vector2 landingPos = ResolveLandingPosition(
        registry, owner, castCenter, focusedTarget, specState.fallingStarSwitch);
    ownerPos->x = landingPos.x;
    ownerPos->y = landingPos.y;
  }
};

REGISTER_SKILL_BEHAVIOR(SevenStarSlash)

void RegisterSevenStarSlash() {}

} // namespace NoMoreDay::skills
