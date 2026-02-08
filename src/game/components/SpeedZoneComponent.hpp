#pragma once

#include "game/components/Common.hpp"

struct SpeedZoneComponent {
  Position center = {};
  float radius = 45.0f;
  float speedMultiplier = 1.25f;
  bool isActive = true;
};
