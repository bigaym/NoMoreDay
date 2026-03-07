#include "game/systems/skill/behaviors/BloodSea.hpp"

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

namespace BloodSeaNodes {
constexpr uint32_t BottomlessPurgatory = 1207;
constexpr uint32_t FreshBloodReturn = 1211;
constexpr uint32_t DrinkTheSeaAndLive = 1213;
constexpr uint32_t PhantomDevour = 1217;
constexpr uint32_t VoidErosionMiasma = 1220;
constexpr uint32_t CrimsonTorrent = 1221;
constexpr uint32_t BloodRingDevour = 1222;
constexpr uint32_t MiasmaShred = 1224;
} // namespace BloodSeaNodes

namespace {

const SpecializedSkill *FindBloodSeaSpecialization(const entt::registry &registry,
                                                   const entt::entity owner) {
  const auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return nullptr;
  }

  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id == BloodSea::kSkillId) {
      return &spec;
    }
  }
  return nullptr;
}

int GetBloodSeaAllocatedPoints(const SpecializedSkill *spec,
                               const uint32_t node_id) {
  if (spec == nullptr) {
    return 0;
  }
  if (const auto it = spec->allocated_points.find(node_id);
      it != spec->allocated_points.end()) {
    return std::max(0, it->second);
  }
  return 0;
}

bool HasBloodSeaNode(const SpecializedSkill *spec, const uint32_t node_id) {
  return GetBloodSeaAllocatedPoints(spec, node_id) > 0;
}

bool IsBloodSeaLinkableSkill(const uint32_t skill_id) {
  return skill_id == 1 || skill_id == 4 || skill_id == 7 || skill_id == 8 ||
         skill_id == 9;
}

bool IsInsideBloodSeaField(const Position &target_pos, const Position &field_pos,
                           const float radius) {
  const float dx = target_pos.x - field_pos.x;
  const float dy = target_pos.y - field_pos.y;
  return (dx * dx + dy * dy) <= (radius * radius);
}

DamagePool BuildBloodSeaDamagePool(const BloodSeaFieldComponent &field,
                                   const float total_damage) {
  DamagePool pool;
  float physical_ratio = field.has_void_keystone ? 0.45f : 0.6f;
  if (field.ring_form) {
    physical_ratio -= 0.05f;
  }
  physical_ratio = std::clamp(physical_ratio, 0.25f, 0.8f);
  pool.Add(Tag::Physical, total_damage * physical_ratio);
  pool.Add(Tag::Void, total_damage * (1.0f - physical_ratio));
  return pool;
}

Tag BuildBloodSeaDamageTags() {
  return Tag::Spell | Tag::Area | Tag::DamageOverTime | Tag::SwordSkill |
         Tag::Physical | Tag::Void;
}

std::vector<entt::entity> CollectBloodSeaTargets(entt::registry &registry,
                                                 const Position &field_pos,
                                                 const float radius,
                                                 const entt::entity owner,
                                                 const entt::entity field_entity) {
  std::vector<entt::entity> targets;
  auto view = registry.view<EnemyTag, Position>();
  for (const entt::entity target : view) {
    if (target == owner || target == field_entity ||
        registry.any_of<KilledTag>(target)) {
      continue;
    }
    const auto &target_pos = view.get<Position>(target);
    if (IsInsideBloodSeaField(target_pos, field_pos, radius)) {
      targets.push_back(target);
    }
  }
  return targets;
}

void ApplyResistShred(entt::registry &registry, const entt::entity target,
                      const BloodSeaFieldComponent &field) {
  if (field.resist_shred <= 0.0f) {
    return;
  }

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect debuff;
  debuff.id = "blood_sea_miasma";
  debuff.name = "Blood Sea Miasma";
  debuff.type = BuffType::DefenseDown;
  debuff.duration = 1.0f;
  debuff.remaining = 1.0f;
  debuff.is_debuff = true;

  debuff.modifiers.push_back({.value = -field.resist_shred,
                              .type = StatType::ResistPhysical,
                              .mode = ModifierMode::Flat});
  debuff.modifiers.push_back({.value = -field.resist_shred,
                              .type = StatType::ResistShadow,
                              .mode = ModifierMode::Flat});
  effects.AddOrRefresh(debuff);
}

float ApplyHealing(entt::registry &registry, const entt::entity owner,
                   const float attempted_heal) {
  auto *stats = registry.try_get<CombatStats>(owner);
  if (stats == nullptr || attempted_heal <= 0.0f) {
    return 0.0f;
  }

  const float previous = stats->health;
  stats->health = std::min(stats->max_health, stats->health + attempted_heal);
  const float actual = stats->health - previous;
  if (actual > 0.0f) {
    registry.emplace_or_replace<StatsDirty>(owner);
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateOnHeal(owner, owner, actual));
  }
  return actual;
}

float DealPulse(entt::registry &registry, const entt::entity field_entity,
                BloodSeaFieldComponent &field,
                const std::vector<entt::entity> &targets,
                const float base_damage) {
  float total_applied_damage = 0.0f;
  for (const entt::entity target : targets) {
    DamageRequest request;
    request.attacker = field.owner;
    request.defender = target;
    request.skill_id = BloodSea::kSkillId;
    request.base_pool = BuildBloodSeaDamagePool(
        field, base_damage * field.bonus_damage_mult);
    request.additional_tags = BuildBloodSeaDamageTags();
    request.source_entity = field_entity;
    const auto result = DamagePipeline::Execute(registry, request, field.owner, true);
    total_applied_damage += result.damage.total_damage;
    ApplyResistShred(registry, target, field);
  }

  const float attempted_heal = total_applied_damage * field.leech_ratio;
  const float actual_heal = ApplyHealing(registry, field.owner, attempted_heal);
  if (field.has_recovery_keystone) {
    (void)systems::BladeResourceService::TryGainBloodthirstFromOverflowHeal(
        registry, field.owner, attempted_heal, actual_heal, BloodSea::kSkillId);
  }
  return total_applied_damage;
}

} // namespace

void BloodSea::DoCast(entt::registry &registry, entt::entity owner,
                      SkillExecution &exec) {
  const SpecializedSkill *spec = FindBloodSeaSpecialization(registry, owner);
  const int consumed = systems::BladeResourceService::ConsumeAll(registry, owner, kSkillId);
  const int effective_consumed = std::max(1, consumed);
  const auto *skill = SkillRegistry::Get().GetSkill(kSkillId);

  const entt::entity field_entity = registry.create();
  registry.emplace<LocalLevelTag>(field_entity);
  if (const auto *owner_pos = registry.try_get<Position>(owner)) {
    registry.emplace<Position>(field_entity, owner_pos->x, owner_pos->y);
  } else {
    registry.emplace<Position>(field_entity, exec.target_pos.x, exec.target_pos.y);
  }
  registry.emplace<ColorComponent>(field_entity, Color{150, 24, 32, 220});
  registry.emplace<SkillComponent>(field_entity, kSkillId, owner);

  auto &field = registry.emplace<BloodSeaFieldComponent>(field_entity);
  field.owner = owner;
  field.cast_id = exec.cast_id;
  field.consumed_bloodthirst = effective_consumed;
  field.duration = (skill ? skill->GetParam("field_duration", 4.8f) : 4.8f) +
                   0.2f * static_cast<float>(effective_consumed);
  field.radius = (skill ? skill->GetParam("field_radius", 120.0f) : 120.0f) +
                 static_cast<float>(effective_consumed) * 6.0f;
  field.damage_interval =
      skill ? skill->GetParam("field_tick", 0.25f) : 0.25f;
  field.bonus_damage_mult =
      1.0f + static_cast<float>(effective_consumed) *
                 (skill ? skill->GetParam("bloodthirst_damage_bonus", 0.12f)
                        : 0.12f);
  field.leech_ratio = skill ? skill->GetParam("leech_ratio", 0.12f) : 0.12f;
  field.resist_shred =
      2.0f * static_cast<float>(GetBloodSeaAllocatedPoints(
          spec, BloodSeaNodes::MiasmaShred));
  field.has_trigger_burst = HasBloodSeaNode(spec, BloodSeaNodes::FreshBloodReturn);
  field.has_linked_synergy = HasBloodSeaNode(spec, BloodSeaNodes::PhantomDevour);
  field.has_recovery_keystone =
      HasBloodSeaNode(spec, BloodSeaNodes::DrinkTheSeaAndLive);
  field.has_void_keystone = HasBloodSeaNode(spec, BloodSeaNodes::VoidErosionMiasma);
  field.torrent_form = HasBloodSeaNode(spec, BloodSeaNodes::CrimsonTorrent);
  field.ring_form = HasBloodSeaNode(spec, BloodSeaNodes::BloodRingDevour);

  if (field.has_recovery_keystone) {
    field.leech_ratio += 0.08f;
  }
  if (field.has_void_keystone) {
    field.bonus_damage_mult *= 1.18f;
    field.resist_shred += 4.0f;
  }
  if (field.torrent_form) {
    field.move_follow_speed = 14.0f;
    field.radius *= 1.15f;
    field.damage_interval *= 0.85f;
  }
  if (field.ring_form) {
    field.radius *= 0.8f;
    field.leech_ratio += 0.1f;
    field.bonus_damage_mult *= 1.15f;
  }
  if (HasBloodSeaNode(spec, BloodSeaNodes::BottomlessPurgatory)) {
    field.bonus_damage_mult *= 1.1f;
  }

  if (field.has_trigger_burst) {
    std::vector<entt::entity> burst_targets;
    auto view = registry.view<EnemyTag, Position>();
    const auto &field_pos = registry.get<Position>(field_entity);
    const float burst_radius = field.radius * 0.7f;
    for (const entt::entity target : view) {
      if (registry.any_of<KilledTag>(target)) {
        continue;
      }
      const auto &target_pos = view.get<Position>(target);
      if (IsInsideBloodSeaField(target_pos, field_pos, burst_radius)) {
        burst_targets.push_back(target);
      }
    }
    if (!burst_targets.empty()) {
      ++field.pulses_triggered;
      (void)DealPulse(registry, field_entity, field, burst_targets,
                      16.0f + 6.0f * static_cast<float>(effective_consumed));
    }
  }

  LOG_INFO("Blood Sea cast: consumed={} radius={:.1f}", effective_consumed,
           field.radius);
}

void BloodSea::UpdateField(entt::registry &registry, entt::entity entity,
                           BloodSeaFieldComponent &field, float dt,
                           const systems::SpatialHashGrid &grid) {
  auto *field_pos = registry.try_get<Position>(entity);
  auto *owner_pos = registry.try_get<Position>(field.owner);
  if (field_pos == nullptr || owner_pos == nullptr || !registry.valid(field.owner)) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
    return;
  }

  field.duration -= dt;
  field.damage_timer -= dt;
  field.linked_pulse_cooldown = std::max(0.0f, field.linked_pulse_cooldown - dt);
  if (field.duration <= 0.0f) {
    registry.destroy(entity);
    return;
  }

  field_pos->x = Lerp(field_pos->x, owner_pos->x,
                      std::clamp(dt * field.move_follow_speed, 0.0f, 1.0f));
  field_pos->y = Lerp(field_pos->y, owner_pos->y,
                      std::clamp(dt * field.move_follow_speed, 0.0f, 1.0f));

  if (field.damage_timer > 0.0f) {
    return;
  }
  field.damage_timer = field.damage_interval;

  std::vector<entt::entity> targets;
  grid.query(*field_pos, field.radius, [&](entt::entity target, const Position &) {
    if (target == field.owner || target == entity || registry.any_of<KilledTag>(target) ||
        !registry.any_of<EnemyTag>(target)) {
      return;
    }
    targets.push_back(target);
  });
  if (targets.empty()) {
    targets = CollectBloodSeaTargets(registry, *field_pos, field.radius,
                                     field.owner, entity);
  }
  if (targets.empty()) {
    return;
  }

  ++field.pulses_triggered;
  float base_damage = 14.0f +
                      4.0f * static_cast<float>(field.consumed_bloodthirst);
  if (field.has_void_keystone) {
    base_damage *= 1.08f;
  }
  if (field.ring_form) {
    base_damage *= 1.1f;
  }
  (void)DealPulse(registry, entity, field, targets, base_damage);
}

void BloodSea::HandleLinkedHit(entt::registry &registry, const CombatEvent &evt) {
  if (!IsBloodSeaLinkableSkill(evt.skill_id) || !registry.valid(evt.source) ||
      !registry.valid(evt.target)) {
    return;
  }

  const auto *target_pos = registry.try_get<Position>(evt.target);
  if (target_pos == nullptr) {
    return;
  }

  auto view = registry.view<BloodSeaFieldComponent, Position>();
  for (const entt::entity field_entity : view) {
    auto &field = view.get<BloodSeaFieldComponent>(field_entity);
    const auto &field_pos = view.get<Position>(field_entity);
    if (field.owner != evt.source ||
        !IsInsideBloodSeaField(*target_pos, field_pos, field.radius)) {
      continue;
    }

    ++field.linked_hit_count;
    if (!field.has_linked_synergy || field.linked_pulse_cooldown > 0.0f) {
      continue;
    }

    field.linked_pulse_cooldown = field.torrent_form ? 0.12f : 0.2f;
    std::vector<entt::entity> targets = {evt.target};
    ++field.pulses_triggered;
    (void)DealPulse(registry, field_entity, field, targets,
                    12.0f + static_cast<float>(field.consumed_bloodthirst) * 2.0f);
  }
}

REGISTER_SKILL_BEHAVIOR(BloodSea)

void RegisterBloodSea() {}

} // namespace NoMoreDay::skills
