#pragma once

#include <entt/entt.hpp>

namespace NoMoreDay::vfx {

struct VFXPlayerComponent {
  int sequenceId = -1;
  float elapsed = 0.0f;
  int nextEventIdx = 0;
  entt::entity target = entt::null;
  float targetWorldX = 0.0f;
  float targetWorldY = 0.0f;
  bool hasTargetWorld = false;
  bool loop = false;
  bool active = true;
};

} // namespace NoMoreDay::vfx
