#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"

#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Common.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cmath>

namespace NoMoreDay::skills {

namespace SevenStarSlashNodes {
constexpr uint32_t SkillId = 10;
constexpr uint32_t Base = 1000;
constexpr uint32_t FlowBonus = 1001;
constexpr uint32_t Execute = 1002;
constexpr uint32_t FollowThrough = 1003;
constexpr uint32_t FocusedArc = 1020;
constexpr uint32_t ShadowHunt = 1021;
} // namespace SevenStarSlashNodes

namespace {

struct SevenStarSlashSpecState {
  int flowBonusPoints = 0;
  bool execute = false;
  bool followThrough = false;
  bool focusedArc = false;
  bool shadowHunt = false;
};

float DistanceSquared(const Vector2 &a, const Vector2 &b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return (dx * dx) + (dy * dy);
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

SevenStarSlashSpecState ResolveSpecState(const entt::registry &registry,
                                        entt::entity owner) {
  SevenStarSlashSpecState state;

  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id != SevenStarSlashNodes::SkillId) {
        continue;
      }

      if (const auto it = spec.allocated_points.find(SevenStarSlashNodes::FlowBonus);
          it != spec.allocated_points.end()) {
        state.flowBonusPoints = it->second;
      }
      state.execute = spec.allocated_points.contains(SevenStarSlashNodes::Execute);
      state.followThrough =
          spec.allocated_points.contains(SevenStarSlashNodes::FollowThrough);
      break;
    }
  }

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, owner,
                                           SevenStarSlashNodes::SkillId);
  state.focusedArc = activeTransmuter == SevenStarSlashNodes::FocusedArc;
  state.shadowHunt = activeTransmuter == SevenStarSlashNodes::ShadowHunt;
  return state;
}

void ApplySlashDamage(entt::registry &registry, entt::entity owner,
                      entt::entity target, float damage, uint32_t skillId) {
  DamagePool pool;
  pool.Add(Tag::Physical, damage);

  DamageRequest request;
  request.attacker = owner;
  request.defender = target;
  request.skill_id = skillId;
  request.base_pool = pool;
  request.additional_tags = Tag::Melee | Tag::SwordSkill | Tag::Hit;
  request.source_entity = owner;
  (void)DamagePipeline::Execute(registry, request, owner, true);
}

} // namespace

struct SevenStarSlash : SkillBehaviorBase<SevenStarSlash> {
  static constexpr uint32_t kSkillId = SevenStarSlashNodes::SkillId;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    const auto *skillData = SkillRegistry::Get().GetSkill(kSkillId);
    const auto *ownerPos = registry.try_get<Position>(owner);
    const auto *ownerStats = registry.try_get<CombatStats>(owner);
    if (skillData == nullptr || ownerPos == nullptr || ownerStats == nullptr) {
      return;
    }

    const float radius = skillData->GetParam("radius", 96.0f);
    const int slashCount =
        static_cast<int>(skillData->GetParam("slash_count", 7.0f));
    float flowBonusPerStack =
        skillData->GetParam("flow_bonus_per_stack", 0.06f);
    float singleTargetExecuteBonus =
        skillData->GetParam("single_target_execute_bonus", 0.5f);
    const float invulnerableDuration =
        skillData->GetParam("invulnerable_duration", 0.5f);
    float effectiveRadius = radius;
    float damageMultiplier = 1.0f;

    const SevenStarSlashSpecState specState = ResolveSpecState(registry, owner);
    flowBonusPerStack += 0.03f * static_cast<float>(specState.flowBonusPoints);
    if (specState.execute) {
      singleTargetExecuteBonus += 0.75f;
    }
    if (specState.focusedArc) {
      effectiveRadius *= 0.6f;
      damageMultiplier *= 1.2f;
    }
    if (specState.shadowHunt) {
      effectiveRadius *= 1.75f;
    }

    const int resourceToSpend = GetCurrentBladeResource(registry, owner);
    if (resourceToSpend > 0) {
      (void)SkillSystem::ConsumeSwordIntent(registry, owner, resourceToSpend,
                                            kSkillId);
      exec.is_empowered = true;
    }

    registry.emplace_or_replace<InvulnerableComponent>(
        owner, InvulnerableComponent{invulnerableDuration, 0.0f, owner, SKYBLUE,
                                     effectiveRadius * 0.35f});

    std::vector<entt::entity> targets;
    const Vector2 center = exec.target_pos;
    const float radiusSq = effectiveRadius * effectiveRadius;
    auto targetView = registry.view<Position, CombatStats>();
    for (const entt::entity target : targetView) {
      if (target == owner) {
        continue;
      }

      const auto &targetPos = targetView.get<Position>(target);
      if (DistanceSquared(Vector2{targetPos.x, targetPos.y}, center) <=
          radiusSq) {
        targets.push_back(target);
      }
    }

    const float averageWeaponDamage =
        0.5f * (ownerStats->min_weapon_damage + ownerStats->max_weapon_damage);
    const float slashDamage = std::max(
        1.0f,
        averageWeaponDamage * damageMultiplier *
            (skillData->weapon_damage_mult +
             (static_cast<float>(resourceToSpend) * flowBonusPerStack)));
    const bool singleTarget = targets.size() == 1;

    for (int slashIndex = 0; slashIndex < slashCount; ++slashIndex) {
      const bool isFinalSlash = (slashIndex == slashCount - 1);
      const float finalSlashBonus =
          (singleTarget && isFinalSlash) ? (1.0f + singleTargetExecuteBonus)
                                         : 1.0f;

      for (const entt::entity target : targets) {
        ApplySlashDamage(registry, owner, target,
                         slashDamage * finalSlashBonus, kSkillId);
        if (specState.followThrough && isFinalSlash) {
          ApplySlashDamage(registry, owner, target, slashDamage * 0.35f,
                           kSkillId);
        }
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(SevenStarSlash)

void RegisterSevenStarSlash() {}

} // namespace NoMoreDay::skills
