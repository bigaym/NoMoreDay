#include "PhantomFlash.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay::skills {

namespace PhantomFlashNodes {
// 基础分支 / Base
constexpr uint32_t Identify = 900;  // 识破 / Identify
constexpr uint32_t Aftermind = 901; // 残心 / Aftermind

// 反制分支 / Counter branch
constexpr uint32_t SwiftCounter = 910;  // 神速反制 / Swift Counter
constexpr uint32_t StunPressure = 911;  // 震慑剑压 / Stun Pressure
constexpr uint32_t QiBurst = 912;       // 气劲爆发 / Qi Burst
constexpr uint32_t ShadowCombo = 913;   // 影杀连斩 / Shadow Combo

// 隐匿分支 / Stealth branch
constexpr uint32_t ShadowHide = 930;    // 影遁 / Shadow Hide
constexpr uint32_t ShadowStay = 931;    // 幽影长存 / Shadow Stay
constexpr uint32_t FatalAmbush = 932;   // 致命奇袭 / Fatal Ambush
constexpr uint32_t ShadowTwin = 933;    // 影之双生 / Shadow Twin

// 机动分支 / Mobility branch
constexpr uint32_t AgileBody = 950;     // 灵动之躯 / Agile Body
constexpr uint32_t FlowReset = 951;     // 流光重置 / Flow Reset
constexpr uint32_t QiOverflow = 952;    // 气劲充盈 / Qi Overflow
constexpr uint32_t ShadowDance = 953;   // 影之舞 / Shadow Dance

// 元素分支 / Element branch
constexpr uint32_t ElementShield = 970; // 元素护盾 / Element Shield
constexpr uint32_t HeavenShock = 971;   // 天罚反震 / Heaven Shock
} // namespace PhantomFlashNodes

void PhantomFlash::DoCast(entt::registry &registry, entt::entity owner,
                          SkillExecution &exec) {
  auto *pos = registry.try_get<Position>(owner);
  if (!pos)
    return;

  const auto sevenStarLink = seven_star_shared::ConsumeLinkBuffs(
      registry, owner, PhantomFlash::kSkillId, true, exec.cast_id);

  const auto *skillData = SkillRegistry::Get().GetSkill(PhantomFlash::kSkillId);
  float dashSpeed =
      skillData ? skillData->GetParam("dash_speed", 500.0f) : 500.0f;
  float dashDist = skillData ? skillData->GetParam("dash_dist", 50.0f) : 50.0f;
  dashSpeed *= sevenStarLink.damage_multiplier;

  // Dash backwards
  Vector2 dir =
      Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));

  if (auto *vel = registry.try_get<Velocity>(owner)) {
    vel->vx = dir.x * dashSpeed;
    vel->vy = dir.y * dashSpeed;
  }

  if (auto *dash = registry.try_get<DashComponent>(owner)) {
    dash->isDashing = true;
    dash->dashTimer = dashDist / dashSpeed;
    dash->dirX = dir.x;
    dash->dirY = dir.y;
    dash->dashSpeed = dashSpeed;
  }

  // VFX
  auto &particleSys = systems::GPUParticleSystem::Get();
  Vector2 startPos = {pos->x, pos->y};

  auto dashParticles = systems::InkEffectHelper::CreateDashEffect(
      startPos, dir, systems::InkEffectHelper::COLOR_SHADOW_CORE, dashDist, 20);
  particleSys.EmitBatch(dashParticles);

  for (int i = 0; i < 8; ++i) {
    Vector2 gVel = {(float)GetRandomValue(-80, 80),
                    (float)GetRandomValue(-80, 80)};
    particleSys.Emit(systems::InkEffectHelper::CreateSpark(
        startPos, gVel, systems::InkEffectHelper::COLOR_GOLD_CORE, 1.5f));
  }

  // Counter State
  auto &pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
  pf.counter_window = 0.5f;
  pf.triggered = false;
  pf.flow_reset = false;
  pf.synergy_shadow_hide = false;
  pf.intent_overflow = 0;
  pf.enchant_tag = Tag::None;
  pf.counter_window += 0.10f * static_cast<float>(sevenStarLink.qiyao_stacks);
  pf.knockback_bonus = 0.25f * static_cast<float>(sevenStarLink.qiyao_stacks);

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, owner, PhantomFlash::kSkillId);
  if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id != PhantomFlash::kSkillId) {
        continue;
      }

      if (auto it = spec.allocated_points.find(PhantomFlashNodes::ShadowHide);
          it != spec.allocated_points.end() && it->second > 0) {
        pf.synergy_shadow_hide = true;
        pf.counter_window += 0.2f;
      }
      if (auto it = spec.allocated_points.find(PhantomFlashNodes::FlowReset);
          it != spec.allocated_points.end() && it->second > 0) {
        pf.flow_reset = true;
      }
      if (auto it = spec.allocated_points.find(PhantomFlashNodes::QiOverflow);
          it != spec.allocated_points.end() && it->second > 0) {
        pf.intent_overflow = it->second;
      }

      if (activeTransmuter == PhantomFlashNodes::ElementShield &&
          spec.allocated_points.contains(PhantomFlashNodes::ElementShield) &&
          spec.allocated_points.at(PhantomFlashNodes::ElementShield) > 0) {
        pf.enchant_tag = Tag::Cold;
      } else if (activeTransmuter == PhantomFlashNodes::AgileBody &&
                 spec.allocated_points.contains(PhantomFlashNodes::AgileBody) &&
                 spec.allocated_points.at(PhantomFlashNodes::AgileBody) > 0) {
        pf.enchant_tag = Tag::Lightning;
      }
      break;
    }
  }

  if (pf.intent_overflow > 0) {
    SkillSystem::GainSwordIntent(registry, owner, std::min(3, pf.intent_overflow),
                                 PhantomFlash::kSkillId);
  }

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  if (pf.synergy_shadow_hide) {
    BuffEffect shadowHide;
    shadowHide.id =
        std::string(BuffIdToString(BuffId::PhantomFlashShadowHide));
    shadowHide.name = "Shadow Hide";
    shadowHide.type = BuffType::SpeedUp;
    shadowHide.duration = pf.counter_window;
    shadowHide.remaining = pf.counter_window;
    shadowHide.modifiers.push_back({.value = 20.0f,
                                    .type = StatType::MoveSpeed,
                                    .mode = ModifierMode::PercentAdd});
    shadowHide.modifiers.push_back({.value = 10.0f,
                                    .type = StatType::DodgeChance,
                                    .mode = ModifierMode::Flat});
    effects.AddOrRefresh(shadowHide);
  }

  if (pf.enchant_tag != Tag::None) {
    auto &mods = registry.get_or_emplace<SkillModifierComponent>(owner);
    mods.damage_modifiers.erase(
        std::remove_if(mods.damage_modifiers.begin(), mods.damage_modifiers.end(),
                       [](const DamageModifier &mod) {
                         return mod.type == ModifierType::GainExtra &&
                                mod.source_tag == Tag::Physical;
                       }),
        mods.damage_modifiers.end());
    mods.damage_modifiers.push_back(
        DamageModifier{Tag::Physical, pf.enchant_tag, 0.5f, ModifierType::GainExtra});
  }

  if (sevenStarLink.consume_returning_step) {
    seven_star_shared::ApplyReturningStepOverride(registry, owner,
                                                  PhantomFlash::kSkillId);
  }

  LOG_INFO("Phantom Flash: Counter state active for entity {}",
           (uint32_t)owner);
}

bool PhantomFlash::Update(entt::registry &registry, entt::entity entity,
                          PhantomFlashComponent &pf, float dt) {
  pf.counter_window -= dt;
  if (pf.counter_window <= 0.0f || pf.triggered) {
    return true;
  }

  // Optional: Visual effect for "Counter Ready" state?
  if (GetRandomValue(0, 10) == 0) { // Low freq
    if (registry.all_of<Position>(entity)) {
      const auto &pos = registry.get<Position>(entity);
      auto &particleSys = systems::GPUParticleSystem::Get();
      components::GPUParticle p;
      p.position = {pos.x + GetRandomValue(-10, 10),
                    pos.y + GetRandomValue(-20, 0)};
      p.velocity = {0, -10};
      p.color = ColorAlpha(GRAY, 0.5f);
      p.lifetime = 0.3f;
      p.maxLifetime = 0.3f;
      p.scale = 1.0f;
      p.flags = 0;
      particleSys.Emit(p);
    }
  }

  return false;
}

REGISTER_SKILL_BEHAVIOR(PhantomFlash)

void RegisterPhantomFlash() {}

} // namespace NoMoreDay::skills
