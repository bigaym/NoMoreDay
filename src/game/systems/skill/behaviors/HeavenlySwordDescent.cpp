#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"

#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/BladeResourceService.hpp"

#include <algorithm>

namespace NoMoreDay::skills {

namespace HeavenlySwordNodes {
constexpr uint32_t SwordCoreCalibration = 1100;
constexpr uint32_t CelestialDomain = 1101;
constexpr uint32_t SkyEdgeInfusion = 1102;
constexpr uint32_t ResidualPressure = 1103;
constexpr uint32_t WorldsplitCore = 1104;
constexpr uint32_t KingslayerIntent = 1105;
constexpr uint32_t MeteorCore = 1106;
constexpr uint32_t SkyPiercingFall = 1107;
constexpr uint32_t SkyRendAftershock = 1108;
constexpr uint32_t EdgeOffering = 1109;
constexpr uint32_t OverflowingTiers = 1110;
constexpr uint32_t SwordRainEcho = 1111;
constexpr uint32_t SpinningHeavens = 1112;
constexpr uint32_t CycleOfAllForms = 1113;
constexpr uint32_t ReturnToTheSheath = 1114;
constexpr uint32_t DomainLock = 1115;
constexpr uint32_t FieldResonance = 1116;
constexpr uint32_t ArraySynchrony = 1117;
constexpr uint32_t TideSpread = 1118;
constexpr uint32_t EnduringHeaven = 1119;
constexpr uint32_t AttunementPolarization = 1120;
constexpr uint32_t LightningTribunal = 1121;
constexpr uint32_t FrozenDominion = 1122;
constexpr uint32_t SolarIncineration = 1123;
constexpr uint32_t ElementalRazing = 1124;
} // namespace HeavenlySwordNodes

namespace {

constexpr float kCenterRadiusRatio = 0.3f;
constexpr float kSkyPiercingCenterRadiusRatio = 0.45f;
constexpr float kScarDelaySeconds = 0.25f;

SkillBehaviorRegistry::CastFunc s_originalBladeFormationCast = nullptr;
SkillBehaviorRegistry::CastFunc s_originalInfiniteBladesCast = nullptr;

bool IsEliteOrBoss(const entt::registry &registry, entt::entity target);
bool RefreshMatchingAffliction(entt::registry &registry, entt::entity target,
                               BladeAttunement attunement);
void ApplySingleHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, entt::entity source_entity,
                    BladeAttunement attunement, float damage);

Color ResolveAttunementColor(const BladeAttunement attunement) {
  switch (attunement) {
  case BladeAttunement::Lightning:
    return Color{210, 185, 255, 255};
  case BladeAttunement::Frost:
    return Color{140, 215, 255, 255};
  case BladeAttunement::Fire:
    return Color{255, 120, 70, 255};
  case BladeAttunement::None:
  default:
    return Color{170, 210, 255, 255};
  }
}

Tag ResolveAttunementTag(const BladeAttunement attunement) {
  switch (attunement) {
  case BladeAttunement::Lightning:
    return Tag::Lightning;
  case BladeAttunement::Frost:
    return Tag::Cold;
  case BladeAttunement::Fire:
    return Tag::Fire;
  case BladeAttunement::None:
  default:
    return Tag::None;
  }
}

StatType ResolveAttunementResistStat(const BladeAttunement attunement) {
  switch (attunement) {
  case BladeAttunement::Lightning:
    return StatType::ResistLightning;
  case BladeAttunement::Frost:
    return StatType::ResistCold;
  case BladeAttunement::Fire:
    return StatType::ResistFire;
  case BladeAttunement::None:
  default:
    return StatType::ResistAll;
  }
}

DamagePool BuildHeavenlyDamagePool(const BladeAttunement attunement,
                                   const float total_damage) {
  DamagePool pool;
  const Tag element = ResolveAttunementTag(attunement);
  if (element == Tag::None) {
    pool.Add(Tag::Physical, total_damage);
    return pool;
  }

  pool.Add(Tag::Physical, total_damage * 0.5f);
  pool.Add(element, total_damage * 0.5f);
  return pool;
}

Tag BuildDamageTags(const BladeAttunement attunement) {
  Tag tags = Tag::Spell | Tag::Area | Tag::SwordSkill | Tag::Hit;
  const Tag element = ResolveAttunementTag(attunement);
  if (element != Tag::None) {
    tags = tags | element;
  }
  return tags;
}

const SpecializedSkill *FindHeavenlySpec(const entt::registry &registry,
                                         const entt::entity owner) {
  const auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return nullptr;
  }

  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id == HeavenlySwordDescent::kSkillId) {
      return &spec;
    }
  }
  return nullptr;
}

int GetAllocatedPoints(const SpecializedSkill *spec, const uint32_t node_id) {
  if (spec == nullptr) {
    return 0;
  }
  if (const auto it = spec->allocated_points.find(node_id);
      it != spec->allocated_points.end()) {
    return std::max(0, it->second);
  }
  return 0;
}

bool HasAllocated(const SpecializedSkill *spec, const uint32_t node_id) {
  return GetAllocatedPoints(spec, node_id) > 0;
}

std::vector<entt::entity> CollectTargetsInRadius(entt::registry &registry,
                                                 const Vector2 center,
                                                 const float radius) {
  std::vector<entt::entity> targets;
  const float radius_sq = radius * radius;
  auto view = registry.view<EnemyTag, Position>();
  for (const entt::entity entity : view) {
    if (registry.any_of<KilledTag>(entity)) {
      continue;
    }
    const auto &pos = view.get<Position>(entity);
    const float dx = pos.x - center.x;
    const float dy = pos.y - center.y;
    if ((dx * dx + dy * dy) <= radius_sq) {
      targets.push_back(entity);
    }
  }
  return targets;
}

void ApplyResistShred(entt::registry &registry, const entt::entity target,
                      const HeavenlySwordFieldComponent &field) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect debuff;
  debuff.id = "heavenly_sword_field_resist";
  debuff.name = "Heavenly Sword Resist Shred";
  debuff.type = BuffType::DefenseDown;
  debuff.duration = 1.25f;
  debuff.remaining = 1.25f;
  debuff.is_debuff = true;

  StatModifier modifier;
  modifier.type = ResolveAttunementResistStat(field.attunement);
  modifier.mode = ModifierMode::Flat;
  modifier.value = -(field.resist_reduction + field.extra_resist_reduction);
  modifier.required_tags = Tag::None;
  modifier.source = ModifierSource::Skill;
  debuff.modifiers.push_back(modifier);
  if (field.frozen_dominion) {
    StatModifier slow;
    slow.type = StatType::MoveSpeed;
    slow.mode = ModifierMode::PercentAdd;
    slow.value = -12.0f;
    slow.required_tags = Tag::None;
    slow.source = ModifierSource::Skill;
    debuff.modifiers.push_back(slow);
  }

  effects.AddOrRefresh(debuff);
}

void ApplyFieldDamage(entt::registry &registry, const entt::entity field_entity,
                      HeavenlySwordFieldComponent &field,
                      const std::vector<entt::entity> &targets,
                      const float base_damage) {
  if (targets.empty()) {
    return;
  }

  for (const entt::entity target : targets) {
    float damage_mult = field.field_damage_mult;
    if (field.elite_first_second_timer > 0.0f && IsEliteOrBoss(registry, target)) {
      damage_mult *= 1.0f + field.elite_field_bonus_mult;
    }
    if (field.afflicted_pressure_bonus_mult > 0.0f &&
        RefreshMatchingAffliction(registry, target, field.attunement)) {
      damage_mult *= 1.0f + field.afflicted_pressure_bonus_mult;
    }

    ApplySingleHit(registry, field.owner, target, field_entity, field.attunement,
                   base_damage * damage_mult);
    ApplyResistShred(registry, target, field);

    // Node closure: element-specific secondary effects
    if (field.solar_incineration) {
      // 炽阳焚城 (1123): apply or strengthen ignite
      systems::AilmentApplyRequest req;
      req.ailment = AilmentType::Ignite;
      req.source = field.owner;
      req.magnitude = base_damage * 0.25f; // Extra ignite magnitude
      systems::AilmentApplier::Apply(registry, target, req);
    }

    if (field.frozen_dominion) {
      // 霜星封界 (1122): chance to freeze (stagnation)
      if (utils::ThreadSafeRandom::GetFloat01() < 0.15f) {
        systems::AilmentApplyRequest req;
        req.ailment = AilmentType::Freeze;
        req.source = field.owner;
        req.duration = 1.0f;
        systems::AilmentApplier::Apply(registry, target, req);
      }
    }

    if (field.lightning_tribunal) {
      // 雷池天罚 (1121): apply shock
      systems::AilmentApplyRequest req;
      req.ailment = AilmentType::Shock;
      req.source = field.owner;
      systems::AilmentApplier::Apply(registry, target, req);
    }
  }
}

bool IsLinkableSkill(const uint32_t skill_id) {
  return skill_id == 2 || skill_id == 3 || skill_id == 5 || skill_id == 6 ||
         skill_id == 7;
}

bool IsInsideField(const Position &target_pos, const Position &field_pos,
                   const float radius) {
  const float dx = target_pos.x - field_pos.x;
  const float dy = target_pos.y - field_pos.y;
  return (dx * dx + dy * dy) <= (radius * radius);
}

bool IsEliteOrBoss(const entt::registry &registry, const entt::entity target) {
  const auto *rarity = registry.try_get<EnemyRarityComponent>(target);
  if (rarity == nullptr) {
    return false;
  }
  return rarity->rarity == EnemyRarityComponent::ELITE ||
         rarity->rarity == EnemyRarityComponent::BOSS ||
         rarity->rarity == EnemyRarityComponent::NEMESIS;
}

float ComputeCenterRadius(const float impact_radius, const bool has_sky_piercing) {
  return impact_radius *
         (has_sky_piercing ? kSkyPiercingCenterRadiusRatio : kCenterRadiusRatio);
}

bool IsInsideImpactCenter(const Position &target_pos, const Vector2 center,
                         const float center_radius) {
  const float dx = target_pos.x - center.x;
  const float dy = target_pos.y - center.y;
  return (dx * dx + dy * dy) <= (center_radius * center_radius);
}

void ApplySingleHit(entt::registry &registry, const entt::entity attacker,
                    const entt::entity target, const entt::entity source_entity,
                    const BladeAttunement attunement, const float damage) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = target;
  request.skill_id = HeavenlySwordDescent::kSkillId;
  request.base_pool = BuildHeavenlyDamagePool(attunement, damage);
  request.additional_tags = BuildDamageTags(attunement);
  request.source_entity = source_entity;
  (void)DamagePipeline::Execute(registry, request, attacker, true);
}

void ApplyMeteorCoreSlow(entt::registry &registry, const entt::entity target,
                         const int points) {
  if (points <= 0) {
    return;
  }

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect debuff;
  debuff.id = "heavenly_sword_meteor_core";
  debuff.name = "Heavenly Sword Meteor Core";
  debuff.type = BuffType::SpeedDown;
  debuff.duration = 2.0f;
  debuff.remaining = 2.0f;
  debuff.is_debuff = true;

  StatModifier modifier;
  modifier.type = StatType::MoveSpeed;
  modifier.mode = ModifierMode::PercentAdd;
  modifier.value = -10.0f * static_cast<float>(points);
  modifier.required_tags = Tag::None;
  modifier.source = ModifierSource::Skill;
  debuff.modifiers.push_back(modifier);
  effects.AddOrRefresh(debuff);
}

BuffType ResolveAttunementAilmentType(const BladeAttunement attunement) {
  switch (attunement) {
  case BladeAttunement::Fire:
    return BuffType::Burn;
  case BladeAttunement::Frost:
    return BuffType::Freeze;
  case BladeAttunement::Lightning:
    return BuffType::Shock;
  case BladeAttunement::None:
  default:
    return BuffType::None;
  }
}

bool RefreshMatchingAffliction(entt::registry &registry, const entt::entity target,
                               const BladeAttunement attunement) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(target);
  if (effects == nullptr) {
    return false;
  }

  const BuffType ailment_type = ResolveAttunementAilmentType(attunement);
  if (ailment_type == BuffType::None) {
    return false;
  }

  bool refreshed = false;
  for (auto &effect : effects->effects) {
    if (effect.type != ailment_type || effect.duration <= 0.0f) {
      continue;
    }
    effect.remaining = effect.duration;
    refreshed = true;
  }
  return refreshed;
}

bool ConsumeReturnToSheathBonus(entt::registry &registry, const entt::entity owner,
                                 float &bonus_mult) {
  auto view = registry.view<HeavenlySwordFieldComponent>();
  for (const entt::entity field_entity : view) {
    auto &field = view.get<HeavenlySwordFieldComponent>(field_entity);
    if (field.owner != owner || field.return_to_sheath_timer <= 0.0f ||
        field.return_to_sheath_bonus_mult <= 0.0f ||
        !field.return_to_sheath_ready) {
      continue;
    }
    bonus_mult = field.return_to_sheath_bonus_mult;
    field.return_to_sheath_timer = 0.0f;
    field.return_to_sheath_ready = false;
    return true;
  }
  return false;
}

float GetHeavenlySwordSpinningBonus(const entt::registry &registry,
                                    const entt::entity owner,
                                    const entt::entity exclude = entt::null) {
  float bonus = 0.0f;
  const auto view = registry.view<HeavenlySwordFieldComponent>();
  for (const entt::entity field_entity : view) {
    if (field_entity == exclude) {
      continue;
    }
    const auto &field = view.get<HeavenlySwordFieldComponent>(field_entity);
    if (field.owner != owner || field.spinning_heavens_bonus <= 0.0f) {
      continue;
    }
    bonus = std::max(bonus, field.spinning_heavens_bonus);
  }
  return bonus;
}

struct SpinningCadenceBaselines {
  float formation_attack_interval = 1.0f;
  float channel_tick_interval = 0.5f;
};

SpinningCadenceBaselines ResolveSpinningCadenceBaselines(
    const entt::registry &registry, const entt::entity owner,
    const entt::entity exclude = entt::null,
    const float fallback_formation_interval = 1.0f,
    const float fallback_channel_tick_interval = 0.5f) {
  SpinningCadenceBaselines baselines{fallback_formation_interval,
                                     fallback_channel_tick_interval};

  const auto view = registry.view<HeavenlySwordFieldComponent>();
  for (const entt::entity field_entity : view) {
    if (field_entity == exclude) {
      continue;
    }
    const auto &field = view.get<HeavenlySwordFieldComponent>(field_entity);
    if (field.owner != owner) {
      continue;
    }
    if (field.original_formation_attack_interval > 0.0f) {
      baselines.formation_attack_interval = field.original_formation_attack_interval;
    }
    if (field.original_channel_tick_interval > 0.0f) {
      baselines.channel_tick_interval = field.original_channel_tick_interval;
    }
    break;
  }

  return baselines;
}

void ApplyHeavenlySwordSpinningBonus(entt::registry &registry,
                                     const entt::entity owner,
                                     const float bonus,
                                     const float base_formation_attack_interval,
                                     const float base_channel_tick_interval) {
  const float interval_mult = 1.0f + std::max(0.0f, bonus);
  if (auto *formation = registry.try_get<BladeFormationComponent>(owner)) {
    formation->attack_interval = base_formation_attack_interval / interval_mult;

    auto sword_view = registry.view<SpiritSwordTag, SummonComponent, SpiritSwordAI>();
    for (const entt::entity sword : sword_view) {
      const auto &summon = sword_view.get<SummonComponent>(sword);
      if (summon.owner != owner || summon.skill_id != 3u) {
        continue;
      }
      auto &ai = sword_view.get<SpiritSwordAI>(sword);
      ai.attack_interval = formation->attack_interval;
    }
  }

  if (auto *chan = registry.try_get<ChannelingComponent>(owner);
      chan != nullptr && chan->skill_id == 5u) {
    chan->tick_interval = base_channel_tick_interval / interval_mult;
    chan->tick_timer = std::min(chan->tick_timer, chan->tick_interval);
  }
}

void CastBladeFormationWithHeavenlyFollowUp(entt::registry &registry,
                                            entt::entity owner,
                                            SkillExecution &exec) {
  if (s_originalBladeFormationCast != nullptr) {
    s_originalBladeFormationCast(registry, owner, exec);
  }

  float bonus_mult = 0.0f;
  if (!ConsumeReturnToSheathBonus(registry, owner, bonus_mult)) {
    return;
  }

  auto sword_view = registry.view<SpiritSwordTag, SummonComponent, SummonCombatProfile>();
  for (const entt::entity sword : sword_view) {
    const auto &summon = sword_view.get<SummonComponent>(sword);
    if (summon.owner != owner || summon.skill_id != 3u) {
      continue;
    }
    auto &combat = sword_view.get<SummonCombatProfile>(sword);
    combat.damage_scale *= 1.0f + bonus_mult;
  }

  if (auto *formation = registry.try_get<BladeFormationComponent>(owner)) {
    formation->is_empowered = true;
  }
}

void CastInfiniteBladesWithHeavenlyFollowUp(entt::registry &registry,
                                            entt::entity owner,
                                            SkillExecution &exec) {
  if (s_originalInfiniteBladesCast != nullptr) {
    s_originalInfiniteBladesCast(registry, owner, exec);
  }

  float bonus_mult = 0.0f;
  if (!ConsumeReturnToSheathBonus(registry, owner, bonus_mult)) {
    return;
  }

  auto *chan = registry.try_get<ChannelingComponent>(owner);
  if (chan == nullptr || chan->skill_id != 5u) {
    return;
  }
  chan->bonus_damage_mult *= 1.0f + bonus_mult;
  chan->is_empowered = true;
}

} // namespace

void HeavenlySwordDescent::DoCast(entt::registry &registry, entt::entity owner,
                                  SkillExecution &exec) {
  const SpecializedSkill *spec = FindHeavenlySpec(registry, owner);
  const int extra_spend_cap = std::min(
      2, GetAllocatedPoints(spec, HeavenlySwordNodes::OverflowingTiers));
  const int spend_cap = 5 + extra_spend_cap;
  const int spent_tiers = systems::BladeResourceService::ConsumeUpTo(
      registry, owner, spend_cap, kSkillId);

  const BladeAttunement attunement =
      systems::BladeResourceService::GetHeavenlyAttunement(registry, owner);
  const auto *skill = SkillRegistry::Get().GetSkill(kSkillId);
  const float base_impact_radius =
      skill ? skill->GetParam("impact_radius", 90.0f) : 90.0f;
  const float base_field_radius =
      skill ? skill->GetParam("field_radius", 140.0f) : 140.0f;
  const float base_field_duration =
      skill ? skill->GetParam("field_duration", 5.0f) : 5.0f;
  const float tier_damage_bonus =
      skill ? skill->GetParam("tier_damage_bonus", 0.18f) : 0.18f;
  const float tier_radius_bonus =
      skill ? skill->GetParam("tier_radius_bonus", 14.0f) : 14.0f;

  float impact_damage_mult = 1.0f + static_cast<float>(spent_tiers) *
                                         tier_damage_bonus;
  impact_damage_mult += getModifier(HeavenlySwordNodes::SkyEdgeInfusion, "effectiveness", 0.04f) *
                        static_cast<float>(GetAllocatedPoints(
                            spec, HeavenlySwordNodes::SkyEdgeInfusion)) *
                        static_cast<float>(spent_tiers);
  if (HasAllocated(spec, HeavenlySwordNodes::CycleOfAllForms)) {
    impact_damage_mult *= 0.8f;
  }

  const float impact_stability_mult =
      1.0f + getModifier(HeavenlySwordNodes::SwordCoreCalibration, "effectiveness", 0.10f) * static_cast<float>(GetAllocatedPoints(
                        spec, HeavenlySwordNodes::SwordCoreCalibration));
  const float center_bonus_mult =
      getModifier(HeavenlySwordNodes::WorldsplitCore, "effectiveness", 0.10f) * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::WorldsplitCore));
  const float elite_impact_bonus_mult =
      getModifier(HeavenlySwordNodes::KingslayerIntent, "effectiveness", 0.08f) * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::KingslayerIntent));
  const float elite_field_bonus_mult =
      getModifier(HeavenlySwordNodes::KingslayerIntent, "field_effectiveness", 0.04f) * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::KingslayerIntent));
  const int meteor_core_points =
      GetAllocatedPoints(spec, HeavenlySwordNodes::MeteorCore);
  const int scar_points =
      GetAllocatedPoints(spec, HeavenlySwordNodes::SkyRendAftershock);
  const float spinning_heavens_bonus =
      0.15f * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::SpinningHeavens));
  const float return_to_sheath_bonus_mult =
      0.08f * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::ReturnToTheSheath));
  const float afflicted_pressure_bonus_mult =
      0.10f * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::TideSpread));

  float field_radius =
      base_field_radius + static_cast<float>(spent_tiers) * tier_radius_bonus;
  field_radius *= 1.0f +
                  getModifier(HeavenlySwordNodes::CelestialDomain, "range_mult", 0.08f) *
                  static_cast<float>(GetAllocatedPoints(
                              spec, HeavenlySwordNodes::CelestialDomain));
  if (HasAllocated(spec, HeavenlySwordNodes::SkyPiercingFall)) {
    field_radius *= getModifier(HeavenlySwordNodes::SkyPiercingFall, "range_mult", 0.7f);
    impact_damage_mult *=
        1.0f + getModifier(HeavenlySwordNodes::SkyPiercingFall, "effectiveness", 0.35f);
  }

  const float impact_radius = (base_impact_radius + field_radius * 0.25f) *
                              impact_stability_mult;
  const float center_radius = ComputeCenterRadius(
      impact_radius, HasAllocated(spec, HeavenlySwordNodes::SkyPiercingFall));
  const std::vector<entt::entity> targets =
      CollectTargetsInRadius(registry, exec.target_pos, impact_radius);
  if (!targets.empty()) {
    const float impact_base_damage =
        (skill ? skill->base_damage : 120.0f) * impact_damage_mult;
    for (const entt::entity target : targets) {
      const auto *target_pos = registry.try_get<Position>(target);
      const bool is_center_hit =
          target_pos != nullptr &&
          IsInsideImpactCenter(*target_pos, exec.target_pos, center_radius);

      float total_mult = 1.0f;
      if (is_center_hit) {
        total_mult += center_bonus_mult;
        ApplyMeteorCoreSlow(registry, target, meteor_core_points);
      }
      if (IsEliteOrBoss(registry, target)) {
        total_mult += elite_impact_bonus_mult;
      }

      ApplySingleHit(registry, owner, target, owner, attunement,
                     impact_base_damage * total_mult);
    }
  }

  const entt::entity field_entity = registry.create();
  registry.emplace<LocalLevelTag>(field_entity);
  registry.emplace<Position>(field_entity, exec.target_pos.x, exec.target_pos.y);
  registry.emplace<ColorComponent>(field_entity, ResolveAttunementColor(attunement));
  registry.emplace<SkillComponent>(field_entity, kSkillId, owner);

  auto &field = registry.emplace<HeavenlySwordFieldComponent>(field_entity);
  field.owner = owner;
  field.duration = base_field_duration +
                   0.5f * static_cast<float>(GetAllocatedPoints(
                       spec, HeavenlySwordNodes::EnduringHeaven));
  field.radius = field_radius;
  field.cast_id = exec.cast_id;
  field.spent_tiers = spent_tiers;
  field.attunement = attunement;
  field.impact_damage_mult = impact_damage_mult;
  field.field_damage_mult =
      1.0f + 0.08f * static_cast<float>(spent_tiers) +
      0.04f * static_cast<float>(GetAllocatedPoints(
                 spec, HeavenlySwordNodes::EdgeOffering)) *
          static_cast<float>(spent_tiers);
  field.damage_interval *=
      1.0f - 0.05f * static_cast<float>(GetAllocatedPoints(
                 spec, HeavenlySwordNodes::ResidualPressure));
  field.damage_interval *=
      1.0f - 0.08f * static_cast<float>(GetAllocatedPoints(
                 spec, HeavenlySwordNodes::FieldResonance));
  field.damage_interval = std::clamp(field.damage_interval, 0.18f, 0.75f);
  field.resist_reduction = 6.0f;
  field.extra_resist_reduction = std::min(
      12.0f, 2.0f * static_cast<float>(GetAllocatedPoints(
                   spec, HeavenlySwordNodes::ElementalRazing)));
  field.elite_first_second_timer = elite_field_bonus_mult > 0.0f ? 1.0f : 0.0f;
  field.elite_impact_bonus_mult = elite_impact_bonus_mult;
  field.elite_field_bonus_mult = elite_field_bonus_mult;
  field.pending_scar_strikes = scar_points;
  field.scar_delay_timer = scar_points > 0 ? kScarDelaySeconds : 0.0f;
  field.scar_interval = scar_points > 0 ? 0.15f : 0.0f;
  field.scar_damage_mult = 0.18f * static_cast<float>(scar_points);
  field.spinning_heavens_bonus = spinning_heavens_bonus;
  const float current_formation_attack_interval =
      registry.all_of<BladeFormationComponent>(owner)
          ? registry.get<BladeFormationComponent>(owner).attack_interval
          : 1.0f;
  const float current_channel_tick_interval =
      registry.all_of<ChannelingComponent>(owner) &&
              registry.get<ChannelingComponent>(owner).skill_id == 5u
          ? registry.get<ChannelingComponent>(owner).tick_interval
          : 0.5f;
  const auto baselines = ResolveSpinningCadenceBaselines(
      registry, owner, entt::null, current_formation_attack_interval,
      current_channel_tick_interval);
  field.original_formation_attack_interval = baselines.formation_attack_interval;
  field.original_channel_tick_interval = baselines.channel_tick_interval;
  field.return_to_sheath_bonus_mult = return_to_sheath_bonus_mult;
  field.return_to_sheath_ready = false;
  field.afflicted_pressure_bonus_mult = afflicted_pressure_bonus_mult;
  field.has_trigger_echo = HasAllocated(spec, HeavenlySwordNodes::SwordRainEcho);
  field.has_cycle = HasAllocated(spec, HeavenlySwordNodes::CycleOfAllForms);
  field.has_domain_lock = HasAllocated(spec, HeavenlySwordNodes::DomainLock);
  field.has_array_synchrony = HasAllocated(spec, HeavenlySwordNodes::ArraySynchrony);
  field.has_polarization =
      HasAllocated(spec, HeavenlySwordNodes::AttunementPolarization);
  field.lightning_tribunal =
      field.has_polarization && attunement == BladeAttunement::Lightning &&
      HasAllocated(spec, HeavenlySwordNodes::LightningTribunal);
  field.frozen_dominion =
      field.has_polarization && attunement == BladeAttunement::Frost &&
      HasAllocated(spec, HeavenlySwordNodes::FrozenDominion);
  field.solar_incineration =
      field.has_polarization && attunement == BladeAttunement::Fire &&
      HasAllocated(spec, HeavenlySwordNodes::SolarIncineration);

  if (field.lightning_tribunal) {
    field.damage_interval = std::min(0.75f, field.damage_interval * 1.2f);
    field.field_damage_mult *= 1.3f;
    field.radius *= 1.2f; // Low frequency but wider area
  }
  if (field.frozen_dominion) {
    field.field_damage_mult *= 1.12f;
  }
  if (field.solar_incineration) {
    field.field_damage_mult *= 1.18f;
  }

  if (field.has_trigger_echo && spent_tiers > 0) {
    field.echo_strikes_triggered += spent_tiers;
    if (!targets.empty()) {
      for (const entt::entity target : targets) {
        ApplySingleHit(registry, owner, target, field_entity, attunement,
                        (skill ? skill->base_damage : 120.0f) *
                           getModifier(HeavenlySwordNodes::SwordRainEcho, "effectiveness", 0.10f) *
                           static_cast<float>(spent_tiers));
      }
    }
  }

  LOG_INFO("Heavenly Sword Descent cast: spent={} radius={:.1f}", spent_tiers,
           field.radius);
}

void HeavenlySwordDescent::UpdateField(entt::registry &registry,
                                       entt::entity entity,
                                       HeavenlySwordFieldComponent &field,
                                       float dt,
                                       const systems::SpatialHashGrid &grid) {
  auto *pos = registry.try_get<Position>(entity);
  if (pos == nullptr || !registry.valid(field.owner)) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
    return;
  }

  field.duration -= dt;
  field.damage_timer -= dt;
  field.linked_cut_cooldown = std::max(0.0f, field.linked_cut_cooldown - dt);
  field.cycle_refund_timer -= dt;
  field.elite_first_second_timer =
      std::max(0.0f, field.elite_first_second_timer - dt);
  field.scar_delay_timer -= dt;
  field.return_to_sheath_timer =
      std::max(0.0f, field.return_to_sheath_timer - dt);
  if (field.duration <= 0.0f) {
    if (registry.valid(field.owner)) {
      const auto baselines = ResolveSpinningCadenceBaselines(
          registry, field.owner, entity, field.original_formation_attack_interval,
          field.original_channel_tick_interval);
      ApplyHeavenlySwordSpinningBonus(
          registry, field.owner,
          GetHeavenlySwordSpinningBonus(registry, field.owner, entity),
          baselines.formation_attack_interval, baselines.channel_tick_interval);
    }
    registry.destroy(entity);
    return;
  }

  if (field.spinning_heavens_bonus > 0.0f && registry.valid(field.owner)) {
    const auto baselines = ResolveSpinningCadenceBaselines(
        registry, field.owner, entt::null, field.original_formation_attack_interval,
        field.original_channel_tick_interval);
    ApplyHeavenlySwordSpinningBonus(
        registry, field.owner,
        GetHeavenlySwordSpinningBonus(registry, field.owner),
        baselines.formation_attack_interval, baselines.channel_tick_interval);
  }

  std::vector<entt::entity> targets;
  grid.query(*pos, field.radius, [&](entt::entity target, const Position &) {
    if (target == field.owner || target == entity || registry.any_of<KilledTag>(target) ||
        !registry.any_of<EnemyTag>(target)) {
      return;
    }
    targets.push_back(target);
  });

  if (field.has_domain_lock) {
    for (const entt::entity target : targets) {
      auto *target_pos = registry.try_get<Position>(target);
      if (target_pos == nullptr) {
        continue;
      }
      target_pos->x = Lerp(target_pos->x, pos->x, dt * 0.6f);
      target_pos->y = Lerp(target_pos->y, pos->y, dt * 0.6f);
    }
  }

  if (field.has_cycle && field.cycle_refund_timer <= 0.0f && !targets.empty() &&
      field.cycle_refunds_granted < 2) {
    field.cycle_refund_timer = 1.0f;
    if (systems::BladeResourceService::Gain(registry, field.owner, 1, kSkillId)) {
      ++field.cycle_refunds_granted;
      if (field.return_to_sheath_bonus_mult > 0.0f) {
        field.return_to_sheath_timer = 3.0f;
        field.return_to_sheath_ready = true;
      }
    }
  }

  if (field.pending_scar_strikes > 0 && field.scar_delay_timer <= 0.0f) {
    const float scar_radius = std::max(24.0f, field.radius * 0.25f);
    auto scar_targets = CollectTargetsInRadius(registry, {pos->x, pos->y}, scar_radius);
    if (!scar_targets.empty()) {
      ++field.echo_strikes_triggered;
      const float base_scar_damage =
          32.0f * (1.0f + static_cast<float>(field.spent_tiers) * 0.08f) *
          (1.0f + field.scar_damage_mult);
      for (const entt::entity target : scar_targets) {
        float scar_damage = base_scar_damage;
        if (IsEliteOrBoss(registry, target)) {
          scar_damage *= 1.0f + 0.20f * static_cast<float>(field.pending_scar_strikes);
        }
        ApplySingleHit(registry, field.owner, target, entity, field.attunement,
                       scar_damage);
      }
    }
    --field.pending_scar_strikes;
    field.scar_delay_timer =
        field.pending_scar_strikes > 0 ? field.scar_interval : 1.0f;
  }

  if (field.damage_timer > 0.0f || targets.empty()) {
    return;
  }
  field.damage_timer = field.damage_interval;

  float base_damage = 28.0f + 6.0f * static_cast<float>(field.spent_tiers);
  if (field.frozen_dominion) {
    base_damage *= 1.08f;
  }
  if (field.solar_incineration) {
    base_damage *= 1.10f;
  }
  ApplyFieldDamage(registry, entity, field, targets, base_damage);
}

void HeavenlySwordDescent::HandleLinkedHit(entt::registry &registry,
                                           const CombatEvent &evt) {
  if (!IsLinkableSkill(evt.skill_id) || !registry.valid(evt.source) ||
      !registry.valid(evt.target)) {
    return;
  }

  const auto *target_pos = registry.try_get<Position>(evt.target);
  if (target_pos == nullptr) {
    return;
  }

  auto view = registry.view<HeavenlySwordFieldComponent, Position>();
  for (const entt::entity field_entity : view) {
    auto &field = view.get<HeavenlySwordFieldComponent>(field_entity);
    const auto &field_pos = view.get<Position>(field_entity);
    if (field.owner != evt.source ||
        !IsInsideField(*target_pos, field_pos, field.radius)) {
      continue;
    }

    ++field.linked_hit_count;
    ApplyResistShred(registry, evt.target, field);

    if (!field.has_array_synchrony || field.linked_cut_cooldown > 0.0f) {
      continue;
    }

    field.linked_cut_cooldown = 0.15f;
    ++field.echo_strikes_triggered;

    DamageRequest request;
    request.attacker = field.owner;
    request.defender = evt.target;
    request.skill_id = kSkillId;
    request.base_pool = BuildHeavenlyDamagePool(field.attunement, 18.0f);
    request.additional_tags = BuildDamageTags(field.attunement);
    request.source_entity = field_entity;
    (void)DamagePipeline::Execute(registry, request, field.owner, true);
  }
}

REGISTER_SKILL_BEHAVIOR(HeavenlySwordDescent)

void RegisterHeavenlySwordDescent() {
  if (const auto current = SkillBehaviorRegistry::GetCast(3);
      current != &CastBladeFormationWithHeavenlyFollowUp) {
    s_originalBladeFormationCast = current;
  }
  if (s_originalBladeFormationCast != nullptr) {
    SkillBehaviorRegistry::RegisterCast(3, &CastBladeFormationWithHeavenlyFollowUp);
  }

  if (const auto current = SkillBehaviorRegistry::GetCast(5);
      current != &CastInfiniteBladesWithHeavenlyFollowUp) {
    s_originalInfiniteBladesCast = current;
  }
  if (s_originalInfiniteBladesCast != nullptr) {
    SkillBehaviorRegistry::RegisterCast(5, &CastInfiniteBladesWithHeavenlyFollowUp);
  }
}

} // namespace NoMoreDay::skills
