#include "engine/render/GPUSkillEffectSystem.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace NoMoreDay::systems {
namespace {

constexpr size_t kMaxQueuedSkillEvents = 4096u;
constexpr size_t kMaxQueuedDistortion = 32u;

struct SkillCapEntry {
  int high = 0;
  int medium = 0;
  int low = 0;
};

constexpr std::array<SkillCapEntry, 10> kSkillCaps = {{
    {0, 0, 0},         // 0: invalid
    {24, 16, 8},       // 1: Flowing Thrust
    {32, 24, 16},      // 2: Rending Wave
    {16, 12, 8},       // 3: Blade Formation
    {6, 5, 4},         // 4: Blade Ward
    {4096, 2048, 1024},// 5: Infinite Blades
    {4, 3, 2},         // 6: Sword Array
    {64, 48, 32},      // 7: Mind Blade
    {20, 14, 10},      // 8: Blade Boomerang
    {4, 3, 2},         // 9: Phantom Trance
}};

Color ResolveSkillColor(const uint32_t skillId) {
  switch (skillId) {
  case 1:
    return SKYBLUE;
  case 2:
    return BLUE;
  case 3:
    return Color{200, 230, 255, 255};
  case 4:
    return Color{180, 220, 255, 255};
  case 5:
    return ORANGE;
  case 6:
    return Color{120, 200, 255, 255};
  case 7:
    return GOLD;
  case 8:
    return Color{90, 180, 255, 255};
  case 9:
    return Color{240, 245, 255, 255};
  default:
    return WHITE;
  }
}

float ClampIntensity(const float value) {
  return std::clamp(value, 0.25f, 3.0f);
}

Vector2 NormalizedDirection(Vector2 from, Vector2 to) {
  Vector2 direction = Vector2Subtract(to, from);
  if (Vector2LengthSqr(direction) <= 1e-5f) {
    return Vector2{1.0f, 0.0f};
  }
  return Vector2Normalize(direction);
}

} // namespace

void GPUSkillEffectSystem::Init(ResourceManager &rm, int maxEffects) {
  (void)rm;
  if (m_shader.id != 0) {
    return;
  }

  m_maxEffects = maxEffects;
  LOG_INFO("Initializing GPUSkillEffectSystem with max {} effects...",
           maxEffects);

  m_hostBuffer.resize(m_maxEffects);
  m_currentCount = 0;
  m_pendingEvents.clear();
  m_pendingDistortion.clear();
  m_skillFrameCounts.fill(0);

  m_gpuBuffer.Create(m_maxEffects * sizeof(components::GPUSkillEffect), nullptr,
                     RL_DYNAMIC_DRAW);
  m_shader = LoadShader("assets/shaders/sh_skill_effect.vs",
                        "assets/shaders/sh_skill_effect.fs");
  m_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_shader, "mvp");

  InitRender();
}

void GPUSkillEffectSystem::InitRender() {
  float vertices[] = {
      -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
      0.5f,  -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f,
  };

  m_quadVAO = rlLoadVertexArray();
  rlEnableVertexArray(m_quadVAO);
  m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
  rlEnableVertexAttribute(0);
  rlDisableVertexArray();
}

void GPUSkillEffectSystem::Submit(const components::GPUSkillEffect &effect) {
  if (m_currentCount < m_maxEffects) {
    m_hostBuffer[m_currentCount] = effect;
    ++m_currentCount;
  }
}

void GPUSkillEffectSystem::SubmitSkillEvent(const SkillVfxEvent &event) {
  if (event.skillId == 0u) {
    return;
  }
  if (m_pendingEvents.size() >= kMaxQueuedSkillEvents) {
    return;
  }
  m_pendingEvents.push_back(event);
}

void GPUSkillEffectSystem::DrainDistortionRequests(
    std::vector<DistortionRequest> &out) {
  if (m_pendingDistortion.empty()) {
    return;
  }
  out.insert(out.end(), m_pendingDistortion.begin(), m_pendingDistortion.end());
  m_pendingDistortion.clear();
}

bool GPUSkillEffectSystem::TrySubmitCapped(
    const uint32_t skillId, const int cap,
    const components::GPUSkillEffect &effect) {
  if (cap <= 0 || m_currentCount >= m_maxEffects) {
    return false;
  }
  if (skillId >= m_skillFrameCounts.size()) {
    return false;
  }
  if (m_skillFrameCounts[skillId] >= cap) {
    return false;
  }
  Submit(effect);
  ++m_skillFrameCounts[skillId];
  return true;
}

int GPUSkillEffectSystem::ResolveSkillCap(const uint32_t skillId,
                                          const uint8_t tier) const {
  if (skillId >= kSkillCaps.size()) {
    return 0;
  }
  const SkillCapEntry &entry = kSkillCaps[skillId];
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) {
    return entry.low;
  }
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Medium)) {
    return entry.medium;
  }
  return entry.high;
}

bool GPUSkillEffectSystem::QueueDistortion(const float worldX,
                                           const float worldY,
                                           const float radius,
                                           const float strength) {
  if (radius <= 1e-3f || strength <= 1e-3f ||
      m_pendingDistortion.size() >= kMaxQueuedDistortion) {
    return false;
  }
  m_pendingDistortion.push_back(
      DistortionRequest{worldX, worldY, radius, strength});
  return true;
}

void GPUSkillEffectSystem::EmitSkillEventVisual(const SkillVfxEvent &event) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  const uint8_t runtimeTier =
      qualityManager.IsInitialized()
          ? static_cast<uint8_t>(qualityManager.GetTier())
          : static_cast<uint8_t>(render::core::QualityTier::Medium);
  const uint8_t eventTier = std::min<uint8_t>(event.qualityTier, 3u);
  const uint8_t tier = std::min(runtimeTier, eventTier);

  const bool lowOrMedium =
      tier <= static_cast<uint8_t>(render::core::QualityTier::Medium);
  const bool reduceEmission = lowOrMedium;
  const bool reduceTrailSampling = lowOrMedium;
  const bool disableSecondaryGlow = lowOrMedium;
  const bool allowDistortion =
      qualityManager.IsInitialized() && qualityManager.GetConfig().distortionEnabled &&
      tier >= static_cast<uint8_t>(render::core::QualityTier::High);

  const int cap = ResolveSkillCap(event.skillId, tier);
  if (cap <= 0) {
    return;
  }

  const Color baseColor = ResolveSkillColor(event.skillId);
  const Vector2 direction = NormalizedDirection(event.origin, event.target);
  const float intensity = ClampIntensity(event.intensity);

  auto emitEffect = [&](Vector2 pos, Vector2 vel, float radius, float angle,
                        float softness, float type, float alphaScale) {
    components::GPUSkillEffect effect = {};
    effect.position = pos;
    effect.velocity = vel;
    effect.radius = std::max(2.0f, radius);
    effect.sectorAngle = angle;
    effect.softness = std::max(0.1f, softness);
    effect.type = type;

    const float coreAlpha = std::clamp(0.9f * alphaScale, 0.25f, 1.0f);
    const float glowAlpha = disableSecondaryGlow ? 0.0f : 0.5f * alphaScale;
    effect.coreColor = ColorNormalize(ColorAlpha(baseColor, coreAlpha));
    effect.glowColor = ColorNormalize(ColorAlpha(WHITE, glowAlpha));
    TrySubmitCapped(event.skillId, cap, effect);
  };

  auto emitDistortion = [&](Vector2 center, float radius, float strength) {
    if (!allowDistortion) {
      return;
    }
    QueueDistortion(center.x, center.y, radius, strength);
  };

  switch (event.skillId) {
  case 1: {
    if (event.type == SkillVfxEventType::CastStart && !reduceTrailSampling) {
      constexpr int trailCount = 3;
      for (int i = 0; i < trailCount; ++i) {
        const float t = static_cast<float>(i + 1) /
                        static_cast<float>(trailCount + 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 260.0f), 12.0f * intensity, 24.0f,
                   0.35f, 2.0f, 0.75f);
      }
    }
    if (event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 120.0f), 20.0f * intensity,
                 360.0f, 0.45f, 1.0f, 1.0f);
    }
    if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 320.0f), 14.0f * intensity,
                 18.0f, 0.3f, 2.0f, 0.8f);
    }
    return;
  }
  case 2: {
    if (event.type == SkillVfxEventType::CastImpact) {
      const int bladeSamples = reduceTrailSampling ? 1 : 2;
      for (int i = 0; i < bladeSamples; ++i) {
        const float t = (bladeSamples == 1)
                            ? 0.82f
                            : (0.68f + 0.16f * static_cast<float>(i));
        const Vector2 samplePos = Vector2Lerp(event.origin, event.target, t);
        const float sampleRadius =
            (reduceTrailSampling ? 8.0f : (7.0f + 0.8f * static_cast<float>(i))) *
            intensity;
        emitEffect(samplePos, Vector2Scale(direction, 220.0f + 20.0f * i),
                   sampleRadius, 34.0f, 0.28f, 2.0f, 0.5f);
      }

      emitEffect(event.target, Vector2Scale(direction, 180.0f), 12.0f * intensity,
                 40.0f, 0.28f, 3.0f, 0.52f);
      emitDistortion(event.target, 18.0f * intensity, 0.10f);
    } else if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 170.0f), 10.0f * intensity,
                 36.0f, 0.28f, 3.0f, 0.42f);
      const Vector2 trailPos = Vector2Lerp(event.origin, event.target, 0.76f);
      emitEffect(trailPos, Vector2Scale(direction, 190.0f), 6.0f * intensity,
                 30.0f, 0.24f, 2.0f, 0.36f);
    }
    return;
  }
  case 3: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      const int orbitCount = reduceEmission ? 8 : 12;
      for (int i = 0; i < orbitCount; ++i) {
        const float angle = (2.0f * PI * static_cast<float>(i)) /
                            static_cast<float>(orbitCount);
        const float radius = 34.0f + ((i % 2 == 0) ? 6.0f : 0.0f);
        Vector2 pos = {event.origin.x + std::cos(angle) * radius,
                       event.origin.y + std::sin(angle) * radius};
        Vector2 vel = {std::cos(angle) * 100.0f, std::sin(angle) * 100.0f};
        emitEffect(pos, vel, 10.0f, 24.0f, 0.35f, 2.0f, 0.65f);
      }
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 80.0f), 20.0f * intensity,
                 360.0f, 0.5f, 1.0f, 0.8f);
    }
    return;
  }
  case 4: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 30.0f * intensity, 360.0f, 0.45f,
                 1.0f, 0.85f);
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 200.0f), 12.0f * intensity,
                 20.0f, 0.3f, 2.0f, 0.9f);
    }
    return;
  }
  case 5: {
    if (event.type == SkillVfxEventType::CastStart) {
      emitEffect(event.target, {0.0f, 0.0f}, 82.0f * intensity, 360.0f, 0.6f,
                 1.0f, 0.55f);
    }
    if (event.type == SkillVfxEventType::CastImpact) {
      const int rainCount = reduceEmission ? 64 : 192;
      for (int i = 0; i < rainCount; ++i) {
        const float degrees = static_cast<float>(GetRandomValue(0, 359));
        const float radians = degrees * DEG2RAD;
        const float distance = static_cast<float>(GetRandomValue(10, 120));
        Vector2 pos = {event.target.x + std::cos(radians) * distance,
                       event.target.y + std::sin(radians) * distance};
        emitEffect(pos, {0.0f, 280.0f}, 10.0f, 18.0f, 0.32f, 2.0f, 0.6f);
      }
    }
    if (event.type == SkillVfxEventType::EmpoweredConsume) {
      emitEffect(event.origin, {0.0f, 0.0f}, 36.0f * intensity, 360.0f, 0.4f,
                 1.0f, 1.0f);
      emitDistortion(event.origin, 44.0f * intensity, 0.4f);
    }
    return;
  }
  case 6: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 64.0f * intensity, 360.0f, 0.55f,
                 1.0f, 0.75f);
      emitDistortion(event.origin, 68.0f * intensity, 0.24f);
    }
    if (event.type == SkillVfxEventType::TriggerProc && !reduceEmission) {
      Vector2 flashPos = {event.target.x + static_cast<float>(GetRandomValue(-40, 40)),
                          event.target.y + static_cast<float>(GetRandomValue(-40, 40))};
      emitEffect(flashPos, Vector2Scale(direction, 180.0f), 13.0f, 22.0f, 0.3f,
                 2.0f, 0.65f);
    }
    return;
  }
  case 7: {
    if (event.type == SkillVfxEventType::CastImpact ||
        event.type == SkillVfxEventType::TriggerProc) {
      const int beamSamples = reduceTrailSampling ? 1 : 3;
      for (int i = 0; i < beamSamples; ++i) {
        const float t = (beamSamples == 1) ? 0.5f
                                            : static_cast<float>(i) /
                                                  static_cast<float>(beamSamples - 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 520.0f),
                   reduceTrailSampling ? 14.0f : 11.0f, 10.0f,
                   reduceTrailSampling ? 0.5f : 0.28f, 2.0f, 1.0f);
      }
    }
    return;
  }
  case 8: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.origin, Vector2Scale(direction, 240.0f), 18.0f * intensity,
                 360.0f, 0.45f, 1.0f, 0.95f);

      const int trailSamples = reduceTrailSampling ? 1 : 4;
      for (int i = 0; i < trailSamples; ++i) {
        const float t = static_cast<float>(i + 1) /
                        static_cast<float>(trailSamples + 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 240.0f), 10.0f, 22.0f, 0.45f,
                   2.0f, 0.55f);
      }
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::BuffExit) {
      emitEffect(event.target, {0.0f, 0.0f}, 24.0f * intensity, 360.0f, 0.55f,
                 1.0f, 0.8f);
    }
    return;
  }
  case 9: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 28.0f * intensity, 360.0f, 0.5f,
                 1.0f, 0.75f);
    }
    if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 260.0f), 16.0f * intensity,
                 20.0f, 0.34f, 2.0f, 0.9f);
    }
    if (event.type == SkillVfxEventType::CastImpact ||
        event.type == SkillVfxEventType::BuffExit) {
      emitEffect(event.origin, {0.0f, 0.0f}, 34.0f * intensity, 360.0f, 0.6f,
                 1.0f, 1.0f);
    }
    return;
  }
  default:
    return;
  }
}

void GPUSkillEffectSystem::StageSkillEvents() {
  if (m_pendingEvents.empty()) {
    return;
  }
  for (const SkillVfxEvent &event : m_pendingEvents) {
    EmitSkillEventVisual(event);
  }
  m_pendingEvents.clear();
}

void GPUSkillEffectSystem::Render(const Camera2D &camera) {
  (void)camera;
  m_skillFrameCounts.fill(0);
  StageSkillEvents();
  if (m_currentCount == 0 || m_shader.id == 0) {
    m_currentCount = 0;
    return;
  }

  m_gpuBuffer.Update(m_hostBuffer.data(),
                     m_currentCount * sizeof(components::GPUSkillEffect));

  rlDrawRenderBatchActive();
  Matrix mvp = rlGetMatrixModelview();
  Matrix projection = rlGetMatrixProjection();
  Matrix finalMvp = MatrixMultiply(mvp, projection);

  BeginBlendMode(BLEND_ALPHA);
  rlDisableDepthTest();
  rlDisableBackfaceCulling();

  rlEnableShader(m_shader.id);
  rlSetUniformMatrix(m_shader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);

  using NoMoreDay::RenderConstants::Binding;
  m_gpuBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_SKILL_EFFECTS));

  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 6, m_currentCount);
  rlDisableVertexArray();

  rlDisableShader();
  EndBlendMode();
  m_currentCount = 0;
}

void GPUSkillEffectSystem::Shutdown() {
  LOG_INFO("Shutting down GPUSkillEffectSystem...");
  m_gpuBuffer.Release();
  UnloadShader(m_shader);
  rlUnloadVertexArray(m_quadVAO);
  rlUnloadVertexBuffer(m_quadVBO);

  m_shader.id = 0;
  m_quadVAO = 0;
  m_quadVBO = 0;
  m_maxEffects = 0;
  m_currentCount = 0;
  m_hostBuffer.clear();
  m_pendingEvents.clear();
  m_pendingDistortion.clear();
  m_skillFrameCounts.fill(0);
}

} // namespace NoMoreDay::systems
