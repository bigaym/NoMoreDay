#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"

#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include "core/logging/Logger.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/BladeResourceService.hpp"

#include <algorithm>

namespace NoMoreDay::skills {

namespace HeavenlySwordNodes {
constexpr uint32_t CelestialDomain = 1101;
constexpr uint32_t SkyEdgeInfusion = 1102;
constexpr uint32_t ResidualPressure = 1103;
constexpr uint32_t SkyPiercingFall = 1107;
constexpr uint32_t EdgeOffering = 1109;
constexpr uint32_t OverflowingTiers = 1110;
constexpr uint32_t SwordRainEcho = 1111;
constexpr uint32_t CycleOfAllForms = 1113;
constexpr uint32_t DomainLock = 1115;
constexpr uint32_t FieldResonance = 1116;
constexpr uint32_t ArraySynchrony = 1117;
constexpr uint32_t EnduringHeaven = 1119;
constexpr uint32_t AttunementPolarization = 1120;
constexpr uint32_t LightningTribunal = 1121;
constexpr uint32_t FrozenDominion = 1122;
constexpr uint32_t SolarIncineration = 1123;
constexpr uint32_t ElementalRazing = 1124;
} // namespace HeavenlySwordNodes

namespace {

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

  DamagePipeline::CalculateBatch(registry, field.owner, targets,
                                 HeavenlySwordDescent::kSkillId,
                                 BuildHeavenlyDamagePool(
                                     field.attunement,
                                     base_damage * field.field_damage_mult),
                                 BuildDamageTags(field.attunement), field_entity);

  for (const entt::entity target : targets) {
    ApplyResistShred(registry, target, field);
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
  impact_damage_mult += 0.04f * static_cast<float>(GetAllocatedPoints(
                            spec, HeavenlySwordNodes::SkyEdgeInfusion)) *
                        static_cast<float>(spent_tiers);
  if (HasAllocated(spec, HeavenlySwordNodes::CycleOfAllForms)) {
    impact_damage_mult *= 0.8f;
  }

  float field_radius =
      base_field_radius + static_cast<float>(spent_tiers) * tier_radius_bonus;
  field_radius *= 1.0f +
                  0.08f * static_cast<float>(GetAllocatedPoints(
                              spec, HeavenlySwordNodes::CelestialDomain));
  if (HasAllocated(spec, HeavenlySwordNodes::SkyPiercingFall)) {
    field_radius *= 0.7f;
    impact_damage_mult *= 1.35f;
  }

  const std::vector<entt::entity> targets = CollectTargetsInRadius(
      registry, exec.target_pos, base_impact_radius + field_radius * 0.25f);
  if (!targets.empty()) {
    DamagePipeline::CalculateBatch(
        registry, owner, targets, kSkillId,
        BuildHeavenlyDamagePool(attunement,
                                (skill ? skill->base_damage : 120.0f) *
                                    impact_damage_mult),
        BuildDamageTags(attunement), owner);
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
      DamagePipeline::CalculateBatch(
          registry, owner, targets, kSkillId,
          BuildHeavenlyDamagePool(attunement,
                                  (skill ? skill->base_damage : 120.0f) * 0.25f *
                                      static_cast<float>(spent_tiers)),
          BuildDamageTags(attunement), field_entity);
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
  if (field.duration <= 0.0f) {
    registry.destroy(entity);
    return;
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
    }
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

void RegisterHeavenlySwordDescent() {}

} // namespace NoMoreDay::skills
