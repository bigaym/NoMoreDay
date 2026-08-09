#pragma once
#include "raylib.h"
#include <nlohmann/json.hpp>

namespace NoMoreDay::components {

struct SwordIntentVisual {
  int currentLevel = 0;   // Level from 0 to 10
  int thresholdTier = 0;  // 0 none, 1 = 5+, 2 = 8+, 3 = 10
  float intensity = 0.0f; // Visual intensity [0.0, 1.0]
  float pulseSpeed = 1.0f;
  float pulseTime = 0.0f;
  float critFeedbackPulse = 0.0f;
  Color auraColor = {100, 200, 255, 255};
  bool showAura = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwordIntentVisual, currentLevel, thresholdTier,
                                   intensity, pulseSpeed, critFeedbackPulse,
                                   showAura)

} // namespace NoMoreDay::components
