#pragma once

#include "engine/render/core/RenderConstants.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace NoMoreDay::vfx {

enum class AnchorType : uint8_t {
  Caster = 0,
  Target,
  World,
  Impact,
};

enum class EventType : uint8_t {
  Particle = 0,
  Trail,
  Light,
  Shake,
  Distortion,
  Sound,
  MaterialSwap,
  ShadowPulse,
  LightProfileBlend,
  MaterialPhaseShift,
  Count,
};

enum class TierPolicy : uint8_t {
  Strict = 0,
  Degrade,
  Skip,
};

struct ParticleEventParams {
  int materialId = 0;
  int count = 10;
  float speed = 100.0f;
  float speedVariance = 20.0f;
  float lifetime = 0.5f;
  float scale = 1.0f;
  float spreadAngle = 360.0f;
  int16_t textureIndex = -1;
  uint8_t blendMode = 0;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

struct TrailEventParams {
  int materialId = 0;
  float duration = 0.3f;
  float widthStart = 8.0f;
  float widthEnd = 1.0f;
  uint32_t colorStart = 0xFFFFFFFFu;
  uint32_t colorEnd = 0x00000000u;
};

struct LightEventParams {
  float radius = 100.0f;
  float intensity = 2.0f;
  float colorR = 1.0f;
  float colorG = 1.0f;
  float colorB = 1.0f;
  float duration = 0.5f;
  float fadeInRatio = 0.1f;
  float fadeOutRatio = 0.3f;
};

struct ShakeEventParams {
  float intensity = 0.2f;
};

struct DistortionEventParams {
  float radius = 100.0f;
  float strength = 0.5f;
  float duration = 0.3f;
  float speed = 300.0f;
};

struct SoundEventParams {
  std::string soundId;
  float volume = 1.0f;
  float pitch = 1.0f;
};

struct MaterialSwapParams {
  int materialId = 0;
  float duration = 0.5f;
};

struct ShadowPulseParams {
  float softnessScale = 1.0f;
  float intensityScale = 1.0f;
  float duration = 0.25f;
};

struct LightProfileBlendParams {
  uint32_t profileA = 0;
  uint32_t profileB = 0;
  float blendTime = 0.5f;
};

struct MaterialPhaseShiftParams {
  float roughnessScale = 1.0f;
  float specularScale = 1.0f;
  float emissiveScale = 1.0f;
  float duration = 0.5f;
};

/**
 * @brief Component added to entities currently undergoing a material swap.
 */
struct ActiveMaterialSwap {
  int materialId = 0;
  float remaining = 0.0f;
  float duration = 0.0f;
};

using EventParams =
    std::variant<ParticleEventParams, TrailEventParams, LightEventParams,
                 ShakeEventParams, DistortionEventParams, SoundEventParams,
                 MaterialSwapParams, ShadowPulseParams, LightProfileBlendParams,
                 MaterialPhaseShiftParams>;

struct VFXEvent {
  float time = 0.0f;
  EventType type = EventType::Particle;
  AnchorType anchor = AnchorType::Caster;
  EventParams params;
  render::core::QualityTier minTier = render::core::QualityTier::Low;
  TierPolicy tierPolicy = TierPolicy::Skip;
};

struct VFXSequenceAsset {
  std::string name;
  float duration = 1.0f;
  std::vector<VFXEvent> events;
  render::core::QualityTier minTier = render::core::QualityTier::Low;
  int version = 1;
};

} // namespace NoMoreDay::vfx
