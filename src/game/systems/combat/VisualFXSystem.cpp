#include "game/systems/combat/VisualFXSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FrameRateUtils.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "raymath.h"
#include <algorithm>
#include <string>

namespace NoMoreDay::systems {
namespace {

std::string ResolveHitSequenceName(uint32_t skillId) {
  switch (skillId) {
  case 1:
    return "SwordSlash";
  case 2:
    return "LightningStrike";
  case 7:
    return "BladeFormation";
  default:
    return "";
  }
}

bool TryPlaySequence(entt::registry &registry, entt::entity source, entt::entity target,
                     const std::string &sequenceName) {
  if (sequenceName.empty() || !registry.valid(source)) {
    return false;
  }

  auto &manager = vfx::VFXSequenceManager::Get();
  if (manager.GetSequence(sequenceName) == nullptr) {
    return false;
  }

  manager.Play(registry, source, sequenceName,
               registry.valid(target) ? target : entt::null, false);
  return true;
}

} // namespace

void VisualFXSystem::Initialize(entt::registry &registry) {
  // 1. On Hit VFX
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [](entt::registry &r, const CombatEvent &evt) {
        if (!r.valid(evt.target) || !r.all_of<Position>(evt.target))
          return;
        const auto &pos = r.get<Position>(evt.target);
        Vector2 p = {pos.x, pos.y};

        const std::string seqName = ResolveHitSequenceName(evt.skill_id);
        if (TryPlaySequence(r, evt.source, evt.target, seqName)) {
          return;
        }

        auto &particleSys = GPUParticleSystem::Get();

        // Skill Specific
        if (evt.skill_id == 2) { // Rending Wave
          auto splash = InkEffectHelper::CreateInkSplash(p, 8, 15.0f, 150.0f);
          particleSys.EmitBatch(splash);
        } else if (evt.skill_id == 7) { // Mind Blade
          auto splash = InkEffectHelper::CreateInkSplash(p, 8, 15.0f, 150.0f);
          for (auto &part : splash) {
            part.color = GOLD;
            particleSys.Emit(part);
          }
        }
        // General Hit (if no specific logic, or always?)
        else if (CombatEventFactory::GetReportedDamage(evt) > 0.0f) {
          auto splash = InkEffectHelper::CreateInkSplash(p, 3, 10.0f, 80.0f);
          for (auto &part : splash)
            particleSys.Emit(part);
        }
      },
      100);

  // 2. On Crit VFX
  CombatEventDispatcher::Register(
      CombatEventType::OnCrit,
      [](entt::registry &r, const CombatEvent &evt) {
        if (!r.valid(evt.target) || !r.all_of<Position>(evt.target))
          return;
        const auto &pos = r.get<Position>(evt.target);

        if (TryPlaySequence(r, evt.source, evt.target, "CriticalHit")) {
          return;
        }

        auto &particleSys = GPUParticleSystem::Get();

        // Gold Spark
        particleSys.Emit(InkEffectHelper::CreateSpark(
            {pos.x, pos.y}, {0, -100.0f}, GOLD, 2.0f));

        // Screen Shake (Small)
        RenderSystem::AddScreenShake(0.15f);
      },
      100);
}

void VisualFXSystem::Update(entt::registry &registry, float dt) {
  // Sword Intent global visuals are owned by SwordIntentVisualSystem to avoid
  // duplicate emissions and to keep quality-tier fallback centralized.
  auto &skillFx = GPUSkillEffectSystem::Get();

  // 2. Blade Ward Visuals (Option A refined: persistent elliptical shield)
  auto ward_view = registry.view<BladeWardComponent, Position>();
  static float wardTimer = 0.0f;
  wardTimer += dt;
  int wardCount = 0;
  int submittedEffects = 0;
  float sampleRemaining = 0.0f;
  float sampleVisibility = 0.0f;
  for (auto entity : ward_view) {
    const auto &ward = ward_view.get<BladeWardComponent>(entity);
    const auto &pos = ward_view.get<Position>(entity);
    ++wardCount;

    const float wardDuration = std::max(0.01f, ward.duration);
    const float fadeIn = std::clamp((wardDuration - ward.remaining) / 0.24f,
                                    0.0f, 1.0f);
    const float fadeOut = std::clamp(ward.remaining / 0.35f, 0.0f, 1.0f);
    const float visibility = std::min(fadeIn, fadeOut);
    const float shimmer = 0.5f + 0.5f * std::sin(wardTimer * 1.8f);

    components::GPUSkillEffect coreShield = {};
    coreShield.position = {pos.x, pos.y};
    coreShield.velocity = {0.0f, 0.0f};
    coreShield.radius = 50.0f;
    coreShield.sectorAngle = 360.0f;
    coreShield.type = 5.0f;
    coreShield.flags = 0u;
    coreShield.coreColor = {0.30f, 0.72f, 1.00f, 0.42f * visibility};
    coreShield.glowColor = {0.52f, 0.88f, 1.00f, 0.36f * visibility};
    skillFx.Submit(coreShield);
    ++submittedEffects;

    components::GPUSkillEffect outerShield = coreShield;
    outerShield.radius = 58.0f;
    outerShield.coreColor = {0.36f, 0.78f, 1.00f, 0.24f * visibility};
    outerShield.glowColor = {0.64f, 0.93f, 1.00f, 0.24f * visibility};
    skillFx.Submit(outerShield);
    ++submittedEffects;

    // Elliptical flow shell: low-alpha moving glint, no sector/fan artifact.
    components::GPUSkillEffect flowShield = coreShield;
    const float flowAngle = wardTimer * 2.8f;
    flowShield.velocity = {std::cos(flowAngle), std::sin(flowAngle)};
    flowShield.radius = 54.0f;
    flowShield.coreColor = {0.70f, 0.93f, 1.00f, 0.24f * visibility};
    flowShield.glowColor = {0.86f, 0.98f, 1.00f,
                            (0.30f + 0.10f * shimmer) * visibility};
    skillFx.Submit(flowShield);
    ++submittedEffects;

    components::GPUSkillEffect counterFlowShield = flowShield;
    const float counterFlowAngle = -wardTimer * 2.1f + 1.3f;
    counterFlowShield.velocity = {std::cos(counterFlowAngle),
                                  std::sin(counterFlowAngle)};
    counterFlowShield.radius = 52.0f;
    counterFlowShield.coreColor = {0.66f, 0.90f, 1.00f, 0.14f * visibility};
    counterFlowShield.glowColor = {0.84f, 0.97f, 1.00f,
                                   (0.22f + 0.07f * shimmer) * visibility};
    skillFx.Submit(counterFlowShield);
    ++submittedEffects;

    sampleRemaining = ward.remaining;
    sampleVisibility = visibility;
  }

  if (wardCount > 0) {
    LOG_LIMITED_INFO(
        1.0f,
        "[BladeWardVFX] wards={} submitted={} sampleRemaining={:.2f} visibility={:.2f}",
        wardCount, submittedEffects, sampleRemaining, sampleVisibility);
  }

  // 3. Sword Array Visuals (Type 6: Magic Grid Formation)
  auto array_view = registry.view<SwordArrayComponent, Position>();
  for (auto entity : array_view) {
    const auto &arrayInfo = array_view.get<SwordArrayComponent>(entity);
    const auto &pos = array_view.get<Position>(entity);

    // Fade in / out based on duration (from SwordArray :: Update we see duration decreases to 0)
    // Assume max duration was 5.0f. Use simple crossfade based on remaining time.
    const float remaining = arrayInfo.duration;
    const float fadeIn = std::clamp((5.0f - remaining) / 0.3f, 0.0f, 1.0f);
    const float fadeOut = std::clamp(remaining / 0.4f, 0.0f, 1.0f);
    const float visibility = std::min(fadeIn, fadeOut);
    
    // Slower rotation for the array
    float rotateAngle = wardTimer * 1.5f;

    components::GPUSkillEffect formation = {};
    formation.position = {pos.x, pos.y};
    formation.velocity = {std::cos(rotateAngle), std::sin(rotateAngle)};
    formation.radius = arrayInfo.radius;
    formation.sectorAngle = 360.0f;
    formation.type = 6.0f;
    formation.flags = 0u;
    
    // Core and glow colors pulled from the component, which supports elemental conversion
    Color coreC = arrayInfo.core_color;
    Color glowC = arrayInfo.glow_color;
    
    formation.coreColor = {
        coreC.r / 255.0f, 
        coreC.g / 255.0f, 
        coreC.b / 255.0f, 
        0.85f * visibility
    };
    formation.glowColor = {
        glowC.r / 255.0f, 
        glowC.g / 255.0f, 
        glowC.b / 255.0f, 
        0.65f * visibility
    };

    skillFx.Submit(formation);

    // Optional: add a counter-rotating inner layer for complexity
    components::GPUSkillEffect innerFormation = formation;
    float counterAngle = -wardTimer * 2.2f + 1.0f;
    innerFormation.velocity = {std::cos(counterAngle), std::sin(counterAngle)};
    innerFormation.radius = arrayInfo.radius * 0.75f;
    innerFormation.coreColor.w *= 0.7f;
    innerFormation.glowColor.w *= 0.5f;
    skillFx.Submit(innerFormation);
  }
}

} // namespace NoMoreDay::systems
