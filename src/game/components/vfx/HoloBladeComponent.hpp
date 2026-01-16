#pragma once
#include "raylib.h"
#include <nlohmann/json.hpp>

namespace NoMoreDay::components {

struct HoloBlade {
  Color holoColor = {100, 200, 255, 200};
  float rimStrength = 0.8f;
  float noiseSpeed = 0.5f;
  float scale = 1.0f;
  bool isVisible = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HoloBlade, holoColor, rimStrength,
                                   noiseSpeed, scale, isVisible)

} // namespace NoMoreDay::components
