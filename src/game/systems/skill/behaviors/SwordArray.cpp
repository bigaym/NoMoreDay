/**
 * @file SwordArray.cpp
 * @brief 剑阵·诛仙 (ID 6) - 模块化地表领域行为实现
 */
#include "SwordArray.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace SwordArrayNodes {
constexpr uint32_t SlowPressure = 630;
constexpr uint32_t ArmorIntent = 631;
constexpr uint32_t ExecuteField = 633;
constexpr uint32_t MindUnity = 652;
} // namespace SwordArrayNodes

void SwordArray::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  auto array_ent = registry.create();
  registry.emplace<Position>(array_ent, exec.target_pos.x, exec.target_pos.y);
  registry.emplace<LocalLevelTag>(array_ent);

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
  BakedSkillProfile localProfile;
  if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
      if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
    }
  }

  float dur = (profile && profile->delivery.duration > 0.0f) ? profile->delivery.duration : 6.0f;
  float rad = (profile && profile->area_radius > 0.0f) ? profile->area_radius : 120.0f;
  float interval = (profile && profile->delivery.sub_interval > 0.0f) ? profile->delivery.sub_interval : 0.3f;

  auto &array = registry.emplace<SwordArrayComponent>(array_ent);
  array.owner = owner; array.duration = dur; array.radius = rad; array.damage_interval = interval;
  array.is_empowered = exec.is_empowered; array.cast_id = exec.cast_id;

  array.has_slow = profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(SwordArrayNodes::SlowPressure % 100);
  array.has_armor_shred = profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(SwordArrayNodes::ArmorIntent % 100);
  array.has_execute = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(SwordArrayNodes::ExecuteField % 100);
  array.gain_intent_on_tick = profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(SwordArrayNodes::MindUnity % 100);

  const Tag attunement = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
  if (attunement != Tag::None) {
    auto &mods = registry.emplace_or_replace<SkillModifierComponent>(array_ent);
    mods.damage_modifiers.push_back({Tag::Physical, attunement, 0.5f, ModifierType::Convert});
  } else if (auto *ownerMods = registry.try_get<SkillModifierComponent>(owner)) {
    registry.emplace_or_replace<SkillModifierComponent>(array_ent, *ownerMods);
  }

  // 原型 5: 地表领域交付
  auto &field = registry.emplace<AreaFieldComponent>(array_ent);
  field.owner = owner; field.cast_id = exec.cast_id; field.source_skill_id = kSkillId;
  field.remaining_duration = dur; field.radius = rad; field.pulse_interval = interval;
  field.payload_count = 0;
  field.payloads[field.payload_count++] = PayloadDefinition{.type = PayloadType::Damage, .value_mult = 0.4f * (profile ? profile->more_damage_mult : 1.0f)};
  if (array.has_slow) {
    field.payloads[field.payload_count++] = PayloadDefinition{.type = PayloadType::Ailment, .ailment_id = static_cast<uint32_t>(AilmentType::Slow), .value_mult = 0.3f, .duration = 2.0f};
  }
}

void SwordArray::Update(entt::registry &registry, entt::entity entity, SwordArrayComponent &array, float dt, const systems::SpatialHashGrid &) {
  array.duration -= dt;
  if (array.duration <= 0.0f) { registry.destroy(entity); return; }
  array.damage_timer += dt;
  if (array.damage_timer >= array.damage_interval) {
    array.damage_timer = 0.0f;
    if (array.gain_intent_on_tick && registry.valid(array.owner)) {
      SkillSystem::GainSwordIntent(registry, array.owner, 1, kSkillId);
    }
  }
}

REGISTER_SKILL_BEHAVIOR(SwordArray)
void RegisterSwordArray() {}
} // namespace NoMoreDay::skills
