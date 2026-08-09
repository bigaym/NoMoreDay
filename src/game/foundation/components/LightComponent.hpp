#pragma once

#include "engine/render/GPUData.hpp"

#include <cstdint>

namespace NoMoreDay {

struct LightComponent {
  components::LightType type = components::LightType::PointLight;
  float radius = 100.0f;
  float intensity = 1.0f;
  float colorR = 1.0f;
  float colorG = 0.9f;
  float colorB = 0.7f;
  float spotAngle = 360.0f;
  float spotDirection = 0.0f;
  uint8_t priority = 128;
  bool enabled = true;
  bool flicker = false;
  float flickerSpeed = 5.0f;
  float flickerAmplitude = 0.2f;
};

} // namespace NoMoreDay
