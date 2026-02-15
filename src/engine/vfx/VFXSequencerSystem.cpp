#include "engine/vfx/VFXSequencerSystem.hpp"

#include "core/logging/Logger.hpp"
#include "engine/audio/AudioSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/MotionTrailComponent.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace NoMoreDay::vfx {
namespace {

struct ActiveTrailRuntime {
  float remaining = 0.0f;
  float duration = 0.0f;
  float widthStart = 1.0f;
  float widthEnd = 1.0f;
  Color colorStart = WHITE;
  Color colorEnd = {255, 255, 255, 0};
};

struct ActiveLightRuntime {
  Vector2 position = {0.0f, 0.0f};
  LightEventParams params = {};
  float elapsed = 0.0f;
};

struct ActiveDistortionRuntime {
  Vector2 position = {0.0f, 0.0f};
  DistortionEventParams params = {};
  float elapsed = 0.0f;
};

struct ActiveMaterialSwapRuntime {
  entt::entity target = entt::null;
  int materialId = 0;
  float remaining = 0.0f;
  float duration = 0.0f;
};

enum class DispatchSkipReason : uint8_t {
  None = 0,
  TierGate = 1,
  DetailGate = 2,
};

std::vector<ActiveLightRuntime> g_activeLights;
std::vector<ActiveDistortionRuntime> g_activeDistortions;
std::vector<ActiveMaterialSwapRuntime> g_activeMaterialSwaps;
size_t g_distortionOverflowDrops = 0;
size_t g_distortionOverflowEvictions = 0;
double g_lastSkipLogAt = 0.0;

float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float RandomRange(float minValue, float maxValue) {
  if (maxValue <= minValue) {
    return minValue;
  }
  const float t = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
  return minValue + (maxValue - minValue) * t;
}

Color LerpColor(const Color &from, const Color &to, float t) {
  const float clamped = Clamp01(t);
  auto lerpChannel = [clamped](uint8_t a, uint8_t b) -> uint8_t {
    const float value =
        static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * clamped;
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
  };

  return Color{lerpChannel(from.r, to.r), lerpChannel(from.g, to.g),
               lerpChannel(from.b, to.b), lerpChannel(from.a, to.a)};
}

Color DecodeRgbaHex(uint32_t value) {
  return Color{static_cast<uint8_t>((value >> 24u) & 0xFFu),
               static_cast<uint8_t>((value >> 16u) & 0xFFu),
               static_cast<uint8_t>((value >> 8u) & 0xFFu),
               static_cast<uint8_t>(value & 0xFFu)};
}

Color MaterialBaseColor(int materialId) {
  if (materialId <= 0) {
    return WHITE;
  }

  const auto &material = render::MaterialManager::Get().GetMaterial(materialId);
  auto toByte = [](float value) -> uint8_t {
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  };

  return Color{toByte(material.baseColorR), toByte(material.baseColorG),
               toByte(material.baseColorB), toByte(material.baseColorA)};
}

Vector2 EntityPositionOr(entt::registry &registry, entt::entity entity,
                         Vector2 fallback) {
  if (!registry.valid(entity)) {
    return fallback;
  }
  if (const auto *position = registry.try_get<Position>(entity)) {
    return Vector2{position->x, position->y};
  }
  return fallback;
}

Vector2 ResolveAnchor(entt::registry &registry, entt::entity source,
                      const VFXPlayerComponent &player, AnchorType anchor) {
  const Vector2 sourcePos = EntityPositionOr(registry, source, Vector2{0.0f, 0.0f});
  const Vector2 explicitTargetPos = {player.targetWorldX, player.targetWorldY};
  const Vector2 fallbackTargetPos = player.hasTargetWorld ? explicitTargetPos : sourcePos;
  const Vector2 targetPos = EntityPositionOr(registry, player.target, fallbackTargetPos);

  switch (anchor) {
  case AnchorType::Caster:
    return sourcePos;
  case AnchorType::Target:
    return targetPos;
  case AnchorType::Impact:
    return targetPos;
  case AnchorType::World:
    return sourcePos;
  }
  return sourcePos;
}

entt::entity ResolveMaterialSwapTargetEntity(entt::registry &registry,
                                             entt::entity source,
                                             const VFXPlayerComponent &player,
                                             AnchorType anchor) {
  if ((anchor == AnchorType::Target || anchor == AnchorType::Impact) &&
      registry.valid(player.target)) {
    return player.target;
  }
  return registry.valid(source) ? source : entt::null;
}

DispatchSkipReason EvaluateDispatchGate(const VFXEvent &event,
                                        render::core::QualityTier currentTier,
                                        int detailLevel) {
  if (static_cast<int>(event.minTier) > static_cast<int>(currentTier)) {
    return DispatchSkipReason::TierGate;
  }

  if (detailLevel <= 0 &&
      event.minTier != render::core::QualityTier::Low) {
    return DispatchSkipReason::DetailGate;
  }
  if (detailLevel == 1 &&
      static_cast<int>(event.minTier) >
          static_cast<int>(render::core::QualityTier::Medium)) {
    return DispatchSkipReason::DetailGate;
  }
  return DispatchSkipReason::None;
}

const char *ToString(DispatchSkipReason reason) {
  switch (reason) {
  case DispatchSkipReason::None:
    return "none";
  case DispatchSkipReason::TierGate:
    return "tier_gate";
  case DispatchSkipReason::DetailGate:
    return "detail_gate";
  }
  return "unknown";
}

int ResolveMaterialSwapOverride(entt::entity entity) {
  for (const auto &runtime : g_activeMaterialSwaps) {
    if (runtime.target == entity && runtime.materialId > 0) {
      return runtime.materialId;
    }
  }
  return 0;
}

void UpsertMaterialSwapRuntime(entt::entity target, int materialId, float duration) {
  if (target == entt::null || materialId <= 0) {
    return;
  }

  for (auto &runtime : g_activeMaterialSwaps) {
    if (runtime.target == target) {
      runtime.materialId = materialId;
      runtime.duration = duration;
      runtime.remaining = duration;
      return;
    }
  }

  ActiveMaterialSwapRuntime runtime = {};
  runtime.target = target;
  runtime.materialId = materialId;
  runtime.duration = duration;
  runtime.remaining = duration;
  g_activeMaterialSwaps.push_back(runtime);
}

void UpdateTrailRuntimes(entt::registry &registry, float dt) {
  auto trailView = registry.view<components::MotionTrail, ActiveTrailRuntime>();
  std::vector<entt::entity> finished;
  finished.reserve(64);

  for (const entt::entity entity : trailView) {
    auto &trail = trailView.get<components::MotionTrail>(entity);
    auto &runtime = trailView.get<ActiveTrailRuntime>(entity);

    runtime.remaining = std::max(0.0f, runtime.remaining - dt);
    const float lifeT =
        runtime.duration > 1e-4f ? 1.0f - (runtime.remaining / runtime.duration) : 1.0f;

    const float width = runtime.widthStart + (runtime.widthEnd - runtime.widthStart) * lifeT;
    trail.maxWidth = std::max(0.1f, width);
    trail.color = LerpColor(runtime.colorStart, runtime.colorEnd, lifeT);

    if (runtime.remaining <= 1e-4f) {
      trail.isActive = false;
      finished.push_back(entity);
    }
  }

  for (const entt::entity entity : finished) {
    if (registry.valid(entity) && registry.all_of<ActiveTrailRuntime>(entity)) {
      registry.remove<ActiveTrailRuntime>(entity);
    }
  }
}

float ComputeLightEnvelope(float elapsed, const LightEventParams &params) {
  const float duration = std::max(0.001f, params.duration);
  const float t = Clamp01(elapsed / duration);

  float fadeIn = Clamp01(params.fadeInRatio);
  float fadeOut = Clamp01(params.fadeOutRatio);
  if ((fadeIn + fadeOut) > 1.0f) {
    const float scale = 1.0f / (fadeIn + fadeOut);
    fadeIn *= scale;
    fadeOut *= scale;
  }

  if (fadeIn > 0.0f && t < fadeIn) {
    return Clamp01(t / fadeIn);
  }
  if (fadeOut > 0.0f && t > (1.0f - fadeOut)) {
    return Clamp01((1.0f - t) / fadeOut);
  }
  return 1.0f;
}

void EmitTransientLight(const Vector2 &position, const LightEventParams &params,
                        float intensityScale) {
  components::GPULight light = {};
  light.posX = position.x;
  light.posY = position.y;
  light.radius = std::max(0.0f, params.radius);
  light.intensity = std::max(0.0f, params.intensity * std::max(0.0f, intensityScale));
  light.colorR = std::clamp(params.colorR, 0.0f, 1.0f);
  light.colorG = std::clamp(params.colorG, 0.0f, 1.0f);
  light.colorB = std::clamp(params.colorB, 0.0f, 1.0f);
  light.colorA = 1.0f;
  light.lightType = static_cast<uint32_t>(components::LightType::PointLight);
  render::lighting::LightManager::Get().AddTransientLight(light);
}

void UpdateActiveLights(float dt) {
  if (g_activeLights.empty()) {
    return;
  }

  std::vector<ActiveLightRuntime> remaining;
  remaining.reserve(g_activeLights.size());

  for (auto &runtime : g_activeLights) {
    runtime.elapsed += dt;
    if (runtime.params.duration <= 0.0f || runtime.elapsed >= runtime.params.duration) {
      continue;
    }

    const float envelope = ComputeLightEnvelope(runtime.elapsed, runtime.params);
    if (envelope > 0.0f) {
      EmitTransientLight(runtime.position, runtime.params, envelope);
    }
    remaining.push_back(runtime);
  }

  g_activeLights = std::move(remaining);
}

float ComputeDistortionRadius(const ActiveDistortionRuntime &runtime) {
  const float maxRadius = std::max(0.0f, runtime.params.radius);
  if (runtime.params.speed > 1e-4f) {
    if (maxRadius > 1e-4f) {
      return std::min(maxRadius, runtime.params.speed * runtime.elapsed);
    }
    return runtime.params.speed * runtime.elapsed;
  }

  const float duration = std::max(runtime.params.duration, 0.001f);
  const float progress = Clamp01(runtime.elapsed / duration);
  return maxRadius * progress;
}

float ComputeDistortionStrength(const ActiveDistortionRuntime &runtime) {
  const float baseStrength = std::max(0.0f, runtime.params.strength);
  if (runtime.params.duration <= 1e-4f) {
    return baseStrength;
  }
  const float progress = Clamp01(runtime.elapsed / runtime.params.duration);
  return baseStrength * (1.0f - progress);
}

void UpdateActiveDistortions(float dt) {
  if (g_activeDistortions.empty()) {
    return;
  }

  std::vector<ActiveDistortionRuntime> remaining;
  remaining.reserve(g_activeDistortions.size());

  for (auto runtime : g_activeDistortions) {
    runtime.elapsed += dt;
    if (runtime.params.duration > 0.0f && runtime.elapsed >= runtime.params.duration) {
      continue;
    }

    const float radius = ComputeDistortionRadius(runtime);
    const float strength = ComputeDistortionStrength(runtime);
    if (radius > 1e-4f && strength > 1e-4f) {
      RenderSystem::AddDistortionSource(runtime.position.x, runtime.position.y, radius,
                                        strength);
    }

    remaining.push_back(runtime);
  }

  g_activeDistortions = std::move(remaining);
}

void UpdateMaterialSwapRuntimes(float dt) {
  if (g_activeMaterialSwaps.empty()) {
    return;
  }

  std::vector<ActiveMaterialSwapRuntime> remaining;
  remaining.reserve(g_activeMaterialSwaps.size());
  for (auto runtime : g_activeMaterialSwaps) {
    runtime.remaining = std::max(0.0f, runtime.remaining - dt);
    if (runtime.remaining <= 1e-4f) {
      LOG_INFO("VFXSequencerSystem: MaterialSwap end target={}", entt::to_integral(runtime.target));
      continue;
    }
    remaining.push_back(runtime);
  }
  g_activeMaterialSwaps = std::move(remaining);
}

} // namespace

void VFXSequencerSystem::Update(entt::registry &registry, float dt) {
  if (dt <= 0.0f) {
    return;
  }

  UpdateTrailRuntimes(registry, dt);
  UpdateActiveLights(dt);
  UpdateActiveDistortions(dt);
  UpdateMaterialSwapRuntimes(dt);

  const auto &qualityManager = render::core::QualityTierManager::Get();
  const render::core::QualityTier currentTier =
      qualityManager.IsInitialized() ? qualityManager.GetTier()
                                     : render::core::QualityTier::Medium;
  const int detailLevel =
      qualityManager.IsInitialized() ? qualityManager.GetConfig().vfxSequenceDetail : 2;

  auto &sequenceManager = VFXSequenceManager::Get();
  auto view = registry.view<VFXPlayerComponent>();
  std::vector<entt::entity> toStop;
  toStop.reserve(64);

  for (const entt::entity entity : view) {
    auto &player = view.get<VFXPlayerComponent>(entity);
    if (!player.active) {
      toStop.push_back(entity);
      continue;
    }

    const VFXSequenceAsset *sequence = sequenceManager.GetSequence(player.sequenceId);
    if (sequence == nullptr) {
      toStop.push_back(entity);
      continue;
    }

    if (static_cast<int>(sequence->minTier) > static_cast<int>(currentTier)) {
      toStop.push_back(entity);
      continue;
    }

    player.elapsed += dt;

    while (player.nextEventIdx >= 0 &&
           player.nextEventIdx < static_cast<int>(sequence->events.size())) {
      const VFXEvent &event = sequence->events[static_cast<size_t>(player.nextEventIdx)];
      if (event.time > player.elapsed) {
        break;
      }

      const DispatchSkipReason skipReason =
          EvaluateDispatchGate(event, currentTier, detailLevel);
      if (skipReason == DispatchSkipReason::None) {
        DispatchEvent(registry, entity, event, player);
      } else {
        const double now = GetTime();
        if ((now - g_lastSkipLogAt) >= 1.0 &&
            (event.type == EventType::MaterialSwap ||
             event.type == EventType::Distortion)) {
          LOG_INFO(
              "VFXSequencerSystem: EventSkipped reason={} sequence={} type={} tier={} "
              "detail={}",
              ToString(skipReason), sequence->name, static_cast<int>(event.type),
              static_cast<int>(currentTier), detailLevel);
          g_lastSkipLogAt = now;
        }
      }
      ++player.nextEventIdx;
    }

    if (player.elapsed >= sequence->duration) {
      if (player.loop && sequence->duration > 1e-4f) {
        player.elapsed = std::fmod(player.elapsed, sequence->duration);
        player.nextEventIdx = 0;
      } else {
        toStop.push_back(entity);
      }
    }
  }

  for (const entt::entity entity : toStop) {
    if (registry.valid(entity) && registry.all_of<VFXPlayerComponent>(entity)) {
      registry.remove<VFXPlayerComponent>(entity);
    }
  }
}

void VFXSequencerSystem::DispatchEvent(entt::registry &registry, entt::entity source,
                                       const VFXEvent &event,
                                       const VFXPlayerComponent &player) {
  const Vector2 worldPos = ResolveAnchor(registry, source, player, event.anchor);
  const entt::entity materialContextEntity =
      ResolveMaterialSwapTargetEntity(registry, source, player, event.anchor);
  const int materialSwapOverride = ResolveMaterialSwapOverride(materialContextEntity);

  switch (event.type) {
  case EventType::Particle: {
    if (const auto *params = std::get_if<ParticleEventParams>(&event.params)) {
      ParticleEventParams effective = *params;
      if (materialSwapOverride > 0) {
        effective.materialId = materialSwapOverride;
      }
      Vector2 pos = worldPos;
      pos.x += effective.offsetX;
      pos.y += effective.offsetY;
      ExecuteParticle(registry, pos, effective);
    }
    break;
  }
  case EventType::Trail:
    if (const auto *params = std::get_if<TrailEventParams>(&event.params)) {
      TrailEventParams effective = *params;
      if (materialSwapOverride > 0) {
        effective.materialId = materialSwapOverride;
      }
      ExecuteTrail(registry, source, effective);
    }
    break;
  case EventType::Light:
    if (const auto *params = std::get_if<LightEventParams>(&event.params)) {
      ExecuteLight(registry, worldPos, *params);
    }
    break;
  case EventType::Shake:
    if (const auto *params = std::get_if<ShakeEventParams>(&event.params)) {
      ExecuteShake(*params);
    }
    break;
  case EventType::Distortion:
    if (const auto *params = std::get_if<DistortionEventParams>(&event.params)) {
      ExecuteDistortion(worldPos, *params);
    }
    break;
  case EventType::Sound:
    if (const auto *params = std::get_if<SoundEventParams>(&event.params)) {
      ExecuteSound(*params);
    }
    break;
  case EventType::MaterialSwap:
    if (const auto *params = std::get_if<MaterialSwapParams>(&event.params)) {
      ExecuteMaterialSwap(registry, source, player, event.anchor, *params);
    }
    break;
  case EventType::Count:
    break;
  }
}

void VFXSequencerSystem::ExecuteParticle(entt::registry &registry, Vector2 worldPos,
                                         const ParticleEventParams &params) {
  (void)registry;
  auto &particleSystem = systems::GPUParticleSystem::Get();
  if (!particleSystem.IsInitialized()) {
    return;
  }

  const int count = std::clamp(params.count, 1, 1024);
  const float spread = std::max(0.0f, params.spreadAngle);
  const float speedVariance = std::max(0.0f, params.speedVariance);
  const float life = std::max(0.01f, params.lifetime);
  const float scale = std::max(0.01f, params.scale);

  std::vector<components::GPUParticle> particles;
  particles.reserve(static_cast<size_t>(count));

  const Color baseColor = MaterialBaseColor(params.materialId);
  for (int i = 0; i < count; ++i) {
    components::GPUParticle particle = {};
    particle.position = worldPos;

    float angleDeg = RandomRange(0.0f, 360.0f);
    if (spread < 360.0f) {
      angleDeg = RandomRange(-spread * 0.5f, spread * 0.5f);
    }
    const float angleRad = angleDeg * (PI / 180.0f);
    const float speed = std::max(0.0f, params.speed + RandomRange(-speedVariance, speedVariance));
    particle.velocity = {std::cos(angleRad) * speed, std::sin(angleRad) * speed};

    particle.color = baseColor;
    particle.lifetime = life;
    particle.maxLifetime = life;
    particle.scale = scale;
    particle.flags = 0;
    particle.textureIndex = params.textureIndex;
    particle.blendMode = params.blendMode;
    particles.push_back(particle);
  }

  if (params.materialId > 0) {
    particleSystem.EmitBatch(particles, params.materialId);
  } else {
    particleSystem.EmitBatch(particles);
  }
}

void VFXSequencerSystem::ExecuteTrail(entt::registry &registry, entt::entity source,
                                      const TrailEventParams &params) {
  if (!registry.valid(source)) {
    return;
  }

  const Color decodedStart = DecodeRgbaHex(params.colorStart);
  const Color decodedEnd = DecodeRgbaHex(params.colorEnd);
  Color startColor = decodedStart;
  Color endColor = decodedEnd;
  if (params.materialId > 0) {
    const Color materialColor = MaterialBaseColor(params.materialId);
    startColor = materialColor;
    endColor = materialColor;
    startColor.a = decodedStart.a;
    endColor.a = decodedEnd.a;
  }

  auto &trail = registry.get_or_emplace<components::MotionTrail>(source);
  trail.isActive = true;
  trail.useParticles = false;
  trail.useGPUTrail = true;
  trail.maxWidth = std::max(0.1f, params.widthStart);
  trail.lifetime = std::max(0.05f, params.duration);
  trail.minDistance = 2.0f;
  trail.color = startColor;
  trail.points.clear();

  if (params.duration > 0.0f) {
    ActiveTrailRuntime runtime = {};
    runtime.remaining = params.duration;
    runtime.duration = params.duration;
    runtime.widthStart = std::max(0.1f, params.widthStart);
    runtime.widthEnd = std::max(0.1f, params.widthEnd);
    runtime.colorStart = startColor;
    runtime.colorEnd = endColor;
    registry.emplace_or_replace<ActiveTrailRuntime>(source, runtime);
  } else if (registry.all_of<ActiveTrailRuntime>(source)) {
    registry.remove<ActiveTrailRuntime>(source);
  }
}

void VFXSequencerSystem::ExecuteLight(entt::registry &registry, Vector2 worldPos,
                                      const LightEventParams &params) {
  (void)registry;
  EmitTransientLight(worldPos, params, ComputeLightEnvelope(0.0f, params));

  if (params.duration > 0.0f) {
    ActiveLightRuntime runtime = {};
    runtime.position = worldPos;
    runtime.params = params;
    runtime.elapsed = 0.0f;
    g_activeLights.push_back(runtime);
  }
}

void VFXSequencerSystem::ExecuteShake(const ShakeEventParams &params) {
  RenderSystem::AddScreenShake(std::max(0.0f, params.intensity));
}

void VFXSequencerSystem::ExecuteDistortion(Vector2 worldPos,
                                           const DistortionEventParams &params) {
  if (params.strength <= 1e-4f ||
      (params.radius <= 1e-4f && params.speed <= 1e-4f)) {
    return;
  }

  ActiveDistortionRuntime runtime = {};
  runtime.position = worldPos;
  runtime.params = params;
  runtime.elapsed = 0.0f;

  constexpr size_t kMaxActiveDistortions =
      static_cast<size_t>(render::passes::DistortionPass::MAX_DISTORTION_SOURCES);
  if (g_activeDistortions.size() >= kMaxActiveDistortions) {
    auto weakest = std::min_element(
        g_activeDistortions.begin(), g_activeDistortions.end(),
        [](const ActiveDistortionRuntime &lhs, const ActiveDistortionRuntime &rhs) {
          if (lhs.params.strength != rhs.params.strength) {
            return lhs.params.strength < rhs.params.strength;
          }
          return lhs.elapsed > rhs.elapsed;
        });
    if (weakest != g_activeDistortions.end() &&
        params.strength > weakest->params.strength) {
      ++g_distortionOverflowEvictions;
      LOG_INFO(
          "VFXSequencerSystem: DistortionOverflow action=evict active={} cap={} "
          "incomingStrength={:.3f} replacedStrength={:.3f}",
          g_activeDistortions.size(), kMaxActiveDistortions, params.strength,
          weakest->params.strength);
      *weakest = runtime;
    } else {
      ++g_distortionOverflowDrops;
      LOG_INFO(
          "VFXSequencerSystem: DistortionOverflow action=drop active={} cap={} "
          "incomingStrength={:.3f}",
          g_activeDistortions.size(), kMaxActiveDistortions, params.strength);
      return;
    }
  } else {
    g_activeDistortions.push_back(runtime);
  }

  float initialRadius = 1.0f;
  if (params.speed > 1e-4f) {
    initialRadius = std::max(1.0f, std::min(params.radius, params.speed / 60.0f));
  } else {
    initialRadius = std::max(1.0f, params.radius * 0.1f);
  }
  RenderSystem::AddDistortionSource(worldPos.x, worldPos.y, initialRadius,
                                    params.strength);
}

void VFXSequencerSystem::ExecuteMaterialSwap(entt::registry &registry,
                                             entt::entity source,
                                             const VFXPlayerComponent &player,
                                             AnchorType anchor,
                                             const MaterialSwapParams &params) {
  const auto &qualityManager = render::core::QualityTierManager::Get();
  if (qualityManager.IsInitialized()) {
    const auto &config = qualityManager.GetConfig();
    if (!config.materialSystemEnabled || config.vfxSequenceDetail <= 0) {
      LOG_INFO(
          "VFXSequencerSystem: MaterialSwap skipped reason=detail_gate tier={} "
          "detail={} source={}",
          static_cast<int>(qualityManager.GetTier()), config.vfxSequenceDetail,
          entt::to_integral(source));
      return;
    }
  }

  if (params.materialId <= 0) {
    LOG_WARN("VFXSequencerSystem: MaterialSwap skipped invalid materialId={}",
             params.materialId);
    return;
  }

  const entt::entity target =
      ResolveMaterialSwapTargetEntity(registry, source, player, anchor);
  if (target == entt::null) {
    return;
  }

  const float duration = (params.duration > 1e-4f) ? params.duration : (1.0f / 60.0f);
  UpsertMaterialSwapRuntime(target, params.materialId, duration);
  LOG_INFO("VFXSequencerSystem: MaterialSwap begin target={} materialId={} duration={:.3f}",
           entt::to_integral(target), params.materialId, duration);
}

void VFXSequencerSystem::ExecuteSound(const SoundEventParams &params) {
  if (params.soundId.empty()) {
    return;
  }

  auto &audio = AudioSystem::Get();
  if (!audio.HasSound(params.soundId)) {
    LOG_WARN("VFXSequencerSystem: missing sound '{}'", params.soundId);
    return;
  }

  audio.PlaySound(params.soundId, AudioChannel::SFX);
}

void VFXSequencerSystem::ResetRuntimeStateForTesting() {
  g_activeLights.clear();
  g_activeDistortions.clear();
  g_activeMaterialSwaps.clear();
  g_distortionOverflowDrops = 0;
  g_distortionOverflowEvictions = 0;
}

size_t VFXSequencerSystem::GetActiveDistortionCountForTesting() {
  return g_activeDistortions.size();
}

size_t VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() {
  return g_activeMaterialSwaps.size();
}

size_t VFXSequencerSystem::GetDistortionOverflowDropCountForTesting() {
  return g_distortionOverflowDrops;
}

size_t VFXSequencerSystem::GetDistortionOverflowEvictCountForTesting() {
  return g_distortionOverflowEvictions;
}

} // namespace NoMoreDay::vfx
