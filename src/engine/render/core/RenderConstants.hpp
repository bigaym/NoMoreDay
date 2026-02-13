#pragma once

#include <cstdint>

namespace NoMoreDay::render::core {

enum class QualityTier : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3,
};

struct RenderConfig {
  bool bloomEnabled = false;
  bool dynamicLightingEnabled = false;
  int maxParticles = 20000;
  int shadowResolution = 0;
  int maxLights = 0;
  float ambientIntensity = 0.3f;
  float ambientColorR = 0.15f;
  float ambientColorG = 0.15f;
  float ambientColorB = 0.2f;

  int bloomMipLevels = 0;
  float bloomThreshold = 1.0f;
  float bloomIntensity = 0.8f;
  float bloomKnee = 0.1f;

  bool fxaaEnabled = false;
  bool vignetteEnabled = false;
  float vignetteIntensity = 0.3f;
  float vignetteRadius = 0.75f;

  bool particleTexturesEnabled = false;
  bool subEmitterEnabled = false;
  bool forceFieldEnabled = false;
  int maxForceFields = 0;

  bool trailEnabled = false;
  int trailMaxPoints = 0;
  int maxTrails = 0;
};

inline const char *ToString(QualityTier tier) {
  switch (tier) {
  case QualityTier::Low:
    return "Low";
  case QualityTier::Medium:
    return "Medium";
  case QualityTier::High:
    return "High";
  case QualityTier::Ultra:
    return "Ultra";
  default:
    return "Unknown";
  }
}

} // namespace NoMoreDay::render::core
