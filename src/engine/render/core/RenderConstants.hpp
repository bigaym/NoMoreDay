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
