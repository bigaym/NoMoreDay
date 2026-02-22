#include "game/systems/vfx/SwordIntentVisualSystem.hpp"

#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/vfx/SwordIntentVisualComponent.hpp"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace NoMoreDay::systems {
namespace {

struct RuntimeVisualState {
  int prevIntentStacks = 0;
  float intentCarry = 0.0f;
  float peakDistortionTimer = 0.0f;
  bool swordStepActive = false;
  int swordStepTrailId = -1;
  float swordStepCarry = 0.0f;
  Vector2 lastStepPos = {0.0f, 0.0f};
  bool hasLastStepPos = false;
};

std::unordered_map<entt::entity, RuntimeVisualState> g_runtimeState;

bool HasSwordStepBuff(const ActiveEffectsComponent *effects) {
  if (!effects) {
    return false;
  }
  for (const auto &effect : effects->effects) {
    if (effect.id == "flowing_thrust_swift" && effect.remaining > 0.0f) {
      return true;
    }
  }
  return false;
}

uint32_t PackColorRGBA8(const Color &c) {
  return static_cast<uint32_t>(c.r) | (static_cast<uint32_t>(c.g) << 8u) |
         (static_cast<uint32_t>(c.b) << 16u) |
         (static_cast<uint32_t>(c.a) << 24u);
}

Color LerpColor(const Color &a, const Color &b, float t) {
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  auto lerpChannel = [clamped](unsigned char x, unsigned char y) {
    return static_cast<unsigned char>(
        std::clamp(x + static_cast<float>(y - x) * clamped, 0.0f, 255.0f));
  };
  return Color{
      lerpChannel(a.r, b.r),
      lerpChannel(a.g, b.g),
      lerpChannel(a.b, b.b),
      lerpChannel(a.a, b.a),
  };
}

Color ResolveIntentColor(const int stacks, const int maxStacks) {
  const Color base = NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT;
  const Color peak = Color{255, 235, 120, 255};
  if (maxStacks <= 0) {
    return base;
  }
  const float ratio =
      std::clamp(static_cast<float>(stacks) / static_cast<float>(maxStacks), 0.0f, 1.0f);
  return LerpColor(base, peak, ratio * 0.7f);
}

int ResolveIntentParticleBudget(const uint8_t tier) {
  using NoMoreDay::render::core::QualityTier;
  if (tier <= static_cast<uint8_t>(QualityTier::Low)) {
    return 6;
  }
  if (tier <= static_cast<uint8_t>(QualityTier::Medium)) {
    return 12;
  }
  if (tier <= static_cast<uint8_t>(QualityTier::High)) {
    return 20;
  }
  return 28;
}

float ResolveIntentTierScale(const uint8_t tier) {
  using NoMoreDay::render::core::QualityTier;
  if (tier <= static_cast<uint8_t>(QualityTier::Low)) {
    return 0.35f;
  }
  if (tier <= static_cast<uint8_t>(QualityTier::Medium)) {
    return 0.55f;
  }
  if (tier <= static_cast<uint8_t>(QualityTier::High)) {
    return 0.85f;
  }
  return 1.1f;
}

float ResolveSwordStepTierScale(const uint8_t tier) {
  using NoMoreDay::render::core::QualityTier;
  if (tier <= static_cast<uint8_t>(QualityTier::Low)) {
    return 0.45f;
  }
  if (tier <= static_cast<uint8_t>(QualityTier::Medium)) {
    return 0.7f;
  }
  return 1.0f;
}

int ResolveTrailSampleStride(const uint8_t tier, const int vfxDetail,
                             const int autoDegradeLevel) {
  using NoMoreDay::render::core::QualityTier;
  int stride = 1;
  if (tier <= static_cast<uint8_t>(QualityTier::Low)) {
    stride = 3;
  } else if (tier <= static_cast<uint8_t>(QualityTier::Medium)) {
    stride = 2;
  }
  if (vfxDetail == 0) {
    stride = std::max(stride, 3);
  } else if (vfxDetail == 1) {
    stride = std::max(stride, 2);
  }
  if (autoDegradeLevel >= 3) {
    stride = std::max(stride, 2);
  }
  return stride;
}

void EmitAmbientParticle(const Vector2 center, const Color color, const float speedMin,
                         const float speedMax, int &budget) {
  if (budget <= 0 || !GPUParticleSystem::Get().IsInitialized()) {
    return;
  }
  components::GPUParticle p = {};
  p.position = {center.x + static_cast<float>(GetRandomValue(-16, 16)),
                center.y + static_cast<float>(GetRandomValue(-26, 10))};
  p.velocity = {static_cast<float>(GetRandomValue(-20, 20)),
                -speedMin - static_cast<float>(GetRandomValue(
                                0, static_cast<int>(std::max(1.0f, speedMax - speedMin))))};
  p.acceleration = {0.0f, 4.0f};
  p.maxLifetime = 0.45f;
  p.lifetime = p.maxLifetime;
  p.scale = 1.4f;
  p.growthRate = -2.2f;
  p.color = color;
  p.blendMode = 1;
  GPUParticleSystem::Get().Emit(p);
  --budget;
}

void EmitBurst(const Vector2 center, const Color color, int count, float speedMin,
               float speedMax, float scale, int &budget) {
  if (budget <= 0 || !GPUParticleSystem::Get().IsInitialized()) {
    return;
  }
  const int maxEmit = std::min(count, budget);
  for (int i = 0; i < maxEmit; ++i) {
    components::GPUParticle p = {};
    p.position = {center.x + static_cast<float>(GetRandomValue(-12, 12)),
                  center.y + static_cast<float>(GetRandomValue(-12, 12))};
    const float angleDeg = static_cast<float>(GetRandomValue(0, 359));
    const float angle = angleDeg * DEG2RAD;
    const float speed = speedMin +
                        static_cast<float>(GetRandomValue(
                            0, static_cast<int>(std::max(1.0f, speedMax - speedMin))));
    p.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
    p.acceleration = {0.0f, 0.0f};
    p.maxLifetime = 0.24f + static_cast<float>(GetRandomValue(0, 8)) * 0.01f;
    p.lifetime = p.maxLifetime;
    p.scale = scale;
    p.growthRate = -scale * 3.0f;
    p.color = color;
    p.blendMode = 1;
    GPUParticleSystem::Get().Emit(p);
  }
  budget -= maxEmit;
}

void FreeSwordStepTrail(RuntimeVisualState &state,
                        NoMoreDay::render::GPUTrailRenderer &trailRenderer) {
  if (state.swordStepTrailId >= 0 && trailRenderer.IsInitialized()) {
    trailRenderer.FreeTrail(state.swordStepTrailId);
  }
  state.swordStepTrailId = -1;
  state.hasLastStepPos = false;
}

} // namespace

void SwordIntentVisualSystem::Update(entt::registry &registry, float dt) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  const bool qualityReady = qualityManager.IsInitialized();
  const auto &cfg =
      qualityReady ? qualityManager.GetConfig() : render::core::RenderConfig{};
  const uint8_t tier = qualityReady
                           ? static_cast<uint8_t>(qualityManager.GetTier())
                           : static_cast<uint8_t>(render::core::QualityTier::Medium);
  const int vfxDetail = std::clamp(cfg.vfxSequenceDetail, 0, 2);
  const int autoDegradeLevel =
      qualityReady ? std::clamp(qualityManager.GetAutoDegradeLevel(), 0, 6) : 0;
  const bool allowDistortion = cfg.distortionEnabled &&
                               tier >= static_cast<uint8_t>(render::core::QualityTier::Ultra);
  const bool allowTrail = cfg.trailEnabled;
  const int trailStride = ResolveTrailSampleStride(tier, vfxDetail, autoDegradeLevel);

  int intentParticleBudget = ResolveIntentParticleBudget(tier);
  std::unordered_set<entt::entity> alive;
  auto &trailRenderer = render::GPUTrailRenderer::Get();

  auto view = registry.view<SwordIntentComponent, Position>();
  for (auto entity : view) {
    alive.insert(entity);

    auto &intent = view.get<SwordIntentComponent>(entity);
    const auto &pos = view.get<Position>(entity);
    auto &visual = registry.get_or_emplace<components::SwordIntentVisual>(entity);
    RuntimeVisualState &state = g_runtimeState[entity];

    visual.currentLevel = intent.stacks;
    const float targetIntensity =
        (intent.max_stacks > 0)
            ? static_cast<float>(intent.stacks) / static_cast<float>(intent.max_stacks)
            : 0.0f;
    visual.intensity = Lerp(visual.intensity, targetIntensity, dt * 6.0f);
    visual.pulseSpeed = 1.0f + visual.intensity * 2.4f;
    visual.pulseTime += dt * visual.pulseSpeed;
    visual.auraColor = ResolveIntentColor(intent.stacks, intent.max_stacks);

    if (intent.stacks > 0) {
      const float intentRate =
          (0.8f + static_cast<float>(intent.stacks) * 0.9f) * ResolveIntentTierScale(tier);
      state.intentCarry += intentRate * dt;
      while (state.intentCarry >= 1.0f && intentParticleBudget > 0) {
        state.intentCarry -= 1.0f;
        EmitAmbientParticle({pos.x, pos.y},
                            ColorAlpha(visual.auraColor, 0.42f + 0.25f * visual.intensity),
                            28.0f, 62.0f, intentParticleBudget);
      }

      if (intent.stacks >= intent.max_stacks) {
        state.peakDistortionTimer += dt;
        if (allowDistortion && state.peakDistortionTimer >= 0.28f) {
          const float distortionRadius = 26.0f + 8.0f * visual.intensity;
          RenderSystem::AddDistortionSource(pos.x, pos.y, distortionRadius, 0.07f);
          state.peakDistortionTimer = 0.0f;
        }
      } else {
        state.peakDistortionTimer = 0.0f;
      }
    } else {
      state.intentCarry = 0.0f;
      state.peakDistortionTimer = 0.0f;
    }

    if (state.prevIntentStacks >= intent.max_stacks &&
        intent.stacks < state.prevIntentStacks) {
      const int burstCount =
          (tier <= static_cast<uint8_t>(render::core::QualityTier::Low))
              ? 4
              : (tier <= static_cast<uint8_t>(render::core::QualityTier::Medium) ? 7 : 10);
      EmitBurst({pos.x, pos.y}, ColorAlpha(visual.auraColor, 0.85f), burstCount,
                85.0f, 190.0f, 2.4f, intentParticleBudget);
      RenderSystem::AddScreenShake(0.04f);
      if (allowDistortion) {
        RenderSystem::AddDistortionSource(pos.x, pos.y, 34.0f, 0.12f);
      }
    }
    state.prevIntentStacks = intent.stacks;

    const ActiveEffectsComponent *effects = registry.try_get<ActiveEffectsComponent>(entity);
    const bool swordStepActive = HasSwordStepBuff(effects);
    if (swordStepActive && !state.swordStepActive) {
      const int enterCount =
          (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) ? 3 : 6;
      EmitBurst({pos.x, pos.y}, Color{180, 240, 255, 220}, enterCount, 90.0f, 220.0f,
                2.2f, intentParticleBudget);

      if (allowTrail && trailRenderer.IsInitialized()) {
        components::GPUTrailHeader header = {};
        header.maxPoints = std::max(4, cfg.trailMaxPoints / std::max(1, trailStride));
        header.maxLifetime = 0.16f;
        header.widthStart = 10.0f;
        header.widthEnd = 3.0f;
        header.colorStart = PackColorRGBA8(Color{160, 225, 255, 220});
        header.colorEnd = PackColorRGBA8(Color{120, 190, 255, 0});
        state.swordStepTrailId = trailRenderer.AllocateTrail(header);
      }
      state.swordStepCarry = 0.0f;
      state.lastStepPos = {pos.x, pos.y};
      state.hasLastStepPos = true;
    }

    if (swordStepActive) {
      const float sustainRate = 8.0f * ResolveSwordStepTierScale(tier);
      state.swordStepCarry += sustainRate * dt;
      while (state.swordStepCarry >= 1.0f && intentParticleBudget > 0) {
        state.swordStepCarry -= 1.0f;
        EmitAmbientParticle({pos.x, pos.y}, Color{140, 210, 255, 170}, 50.0f, 95.0f,
                            intentParticleBudget);
      }

      if (state.swordStepTrailId >= 0 && trailRenderer.IsInitialized()) {
        const Vector2 currentPos = {pos.x, pos.y};
        bool appendPoint = !state.hasLastStepPos;
        if (!appendPoint) {
          const float minDist = 8.0f * static_cast<float>(trailStride);
          appendPoint = Vector2Distance(state.lastStepPos, currentPos) >= minDist;
        }
        if (appendPoint) {
          Vector2 direction = {1.0f, 0.0f};
          if (state.hasLastStepPos) {
            direction = Vector2Subtract(currentPos, state.lastStepPos);
          }
          const float trailWidth = 8.0f + 2.0f * visual.intensity;
          trailRenderer.AppendPoint(
              state.swordStepTrailId, currentPos, direction, trailWidth,
              PackColorRGBA8(Color{160, 225, 255, 220}));
          state.lastStepPos = currentPos;
          state.hasLastStepPos = true;
        }
      }
    } else if (state.swordStepActive) {
      const int exitCount =
          (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) ? 2 : 5;
      EmitBurst({pos.x, pos.y}, Color{130, 190, 255, 190}, exitCount, 45.0f, 120.0f,
                1.8f, intentParticleBudget);
      FreeSwordStepTrail(state, trailRenderer);
      state.swordStepCarry = 0.0f;
    }
    state.swordStepActive = swordStepActive;
  }

  for (auto it = g_runtimeState.begin(); it != g_runtimeState.end();) {
    const entt::entity entity = it->first;
    if (!registry.valid(entity) || alive.find(entity) == alive.end()) {
      FreeSwordStepTrail(it->second, trailRenderer);
      it = g_runtimeState.erase(it);
    } else {
      ++it;
    }
  }
}

void SwordIntentVisualSystem::Render(entt::registry &registry) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  const uint8_t tier = qualityManager.IsInitialized()
                           ? static_cast<uint8_t>(qualityManager.GetTier())
                           : static_cast<uint8_t>(render::core::QualityTier::Medium);

  auto view = registry.view<Position, components::SwordIntentVisual>();
  for (auto entity : view) {
    const auto &pos = view.get<Position>(entity);
    const auto &visual = view.get<components::SwordIntentVisual>(entity);
    if (!visual.showAura || visual.currentLevel <= 0 || visual.intensity <= 0.01f) {
      continue;
    }

    const float pulse = (std::sin(visual.pulseTime) + 1.0f) * 0.5f;
    const float baseRadius =
        16.0f + static_cast<float>(visual.currentLevel) * 1.4f + pulse * 2.0f;
    const float auraAlpha = std::clamp(0.18f + visual.intensity * 0.35f, 0.1f, 0.6f);
    const Color aura = ColorAlpha(visual.auraColor, auraAlpha);

    DrawCircleLinesV({pos.x, pos.y}, baseRadius, aura);

    if (tier >= static_cast<uint8_t>(render::core::QualityTier::Medium)) {
      DrawRing({pos.x, pos.y}, baseRadius * 0.72f, baseRadius, 0.0f, 360.0f, 24,
               Fade(aura, 0.35f));
    }

    if (visual.currentLevel >= 10) {
      const Color peakColor = ColorAlpha(LerpColor(visual.auraColor, GOLD, 0.55f),
                                         std::clamp(0.22f + 0.25f * pulse, 0.2f, 0.55f));
      DrawPolyLinesEx({pos.x, pos.y}, 6, baseRadius * 1.18f + pulse * 4.0f,
                      pulse * 45.0f, 2.0f, peakColor);
      if (tier >= static_cast<uint8_t>(render::core::QualityTier::High)) {
        DrawRing({pos.x, pos.y}, baseRadius * 1.05f, baseRadius * 1.24f,
                 pulse * 120.0f, pulse * 120.0f + 140.0f, 20, Fade(peakColor, 0.35f));
      }
    }
  }
}

} // namespace NoMoreDay::systems
