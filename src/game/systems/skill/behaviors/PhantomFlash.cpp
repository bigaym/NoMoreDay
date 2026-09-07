#include "PhantomFlash.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

namespace PhantomFlashNodes {
constexpr uint32_t ShadowHide = 930;
constexpr uint32_t AgileBody = 950;
constexpr uint32_t FlowReset = 951;
constexpr uint32_t QiOverflow = 952;
constexpr uint32_t ElementShield = 970;
} // namespace PhantomFlashNodes

void PhantomFlash::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  auto *pos = registry.try_get<Position>(owner); if (!pos) return;
  const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, true, exec.cast_id);
  const auto *sd = SkillRegistry::Get().GetSkill(kSkillId);
  float speed = (sd ? sd->GetParam("dash_speed", 500.0f) : 500.0f) * link.damage_multiplier;
  float dist = sd ? sd->GetParam("dash_dist", 50.0f) : 50.0f;
  Vector2 dir = Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));

  // 原型 8: 位移交付
  auto &mob = registry.emplace_or_replace<MobilityComponent>(owner);
  mob.owner = owner; mob.skill_id = kSkillId; mob.cast_id = exec.cast_id;
  mob.type = MobilityType::Dash; mob.direction = dir; mob.speed = speed;
  mob.duration = dist / std::max(speed, 1.0f); mob.has_invulnerability = true;
  if (auto *vel = registry.try_get<Velocity>(owner)) { vel->vx = dir.x * speed; vel->vy = dir.y * speed; }
  if (auto *dash = registry.try_get<DashComponent>(owner)) { dash->isDashing = true; dash->dashTimer = mob.duration; dash->dirX = dir.x; dash->dirY = dir.y; dash->dashSpeed = speed; }

  // 反制状态与原型 11: 响应式护盾
  auto &pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
  pf.counter_window = 0.5f + 0.10f * (float)link.qiyao_stacks;
  pf.knockback_bonus = 0.25f * (float)link.qiyao_stacks;

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
  BakedSkillProfile localProfile;
  if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
      if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
    }
  }

  pf.synergy_shadow_hide = profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(PhantomFlashNodes::ShadowHide % 100);
  if (pf.synergy_shadow_hide) pf.counter_window += 0.2f;
  pf.flow_reset = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(PhantomFlashNodes::FlowReset % 100);
  pf.intent_overflow = profile ? profile->delivery.sub_count : (exec.active_nodes.test(PhantomFlashNodes::QiOverflow % 100) ? 1 : 0);
  pf.enchant_tag = (profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(PhantomFlashNodes::ElementShield % 100)) ? Tag::Cold : ((profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(PhantomFlashNodes::AgileBody % 100)) ? Tag::Lightning : Tag::None);

  auto &rw = registry.emplace_or_replace<ReactiveWardComponent>(owner);
  rw.owner = owner; rw.ward_duration = pf.counter_window; rw.counter_window = pf.counter_window; rw.counter_skill_id = kSkillId;

  if (pf.intent_overflow > 0) SkillSystem::GainSwordIntent(registry, owner, std::min(3, pf.intent_overflow), kSkillId);
  if (pf.synergy_shadow_hide) {
    BuffEffect b; b.id = std::string(BuffIdToString(BuffId::PhantomFlashShadowHide)); b.name = "Shadow Hide"; b.type = BuffType::SpeedUp; b.duration = pf.counter_window; b.remaining = pf.counter_window;
    b.modifiers.push_back({.value = 20.0f, .type = StatType::MoveSpeed, .mode = ModifierMode::PercentAdd});
    b.modifiers.push_back({.value = 10.0f, .type = StatType::DodgeChance, .mode = ModifierMode::Flat});
    registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(b);
  }
  if (pf.enchant_tag != Tag::None) {
    auto &mods = registry.get_or_emplace<SkillModifierComponent>(owner);
    std::erase_if(mods.damage_modifiers, [](const auto &m) { return m.type == ModifierType::GainExtra && m.source_tag == Tag::Physical; });
    mods.damage_modifiers.push_back({Tag::Physical, pf.enchant_tag, 0.5f, ModifierType::GainExtra});
  }
  if (link.consume_returning_step) seven_star_shared::ApplyReturningStepOverride(registry, owner, kSkillId);

  auto &trig = registry.get_or_emplace<TriggerRuleComponent>(owner);
  trig.RemoveRule(kSkillId);
  TriggerRule rule; rule.rule_id = kSkillId; rule.listen_event = CombatEventType::OnTakeDamage; rule.target_mode = TriggerTargetPolicy::Attacker;
  rule.cast_skill_id = kSkillId; rule.base_chance = 1.0f; rule.internal_cooldown = pf.counter_window; rule.use_proc_scaling = false;
  rule.effectiveness = pf.synergy_shadow_hide ? 1.2f : 1.0f;
  if (pf.flow_reset) { rule.cooldown_refund_skill_id = 8; rule.cooldown_refund_amount = 1.5f; }
  trig.AddRule(rule);
}

bool PhantomFlash::Update(entt::registry &registry, entt::entity entity, PhantomFlashComponent &pf, float dt) {
  pf.counter_window -= dt;
  if (pf.counter_window <= 0.0f || pf.triggered) {
    if (auto *tc = registry.try_get<TriggerRuleComponent>(entity)) tc->RemoveRule(kSkillId);
    return true;
  }
  return false;
}

REGISTER_SKILL_BEHAVIOR(PhantomFlash)
void RegisterPhantomFlash() {}
} // namespace NoMoreDay::skills
