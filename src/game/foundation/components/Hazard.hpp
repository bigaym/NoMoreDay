#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay {

struct HoveringHazardComponent {
  float duration = 1.0f;
  float tick_interval = 0.2f;
  float tick_timer = 0.0f;
  float radius = 50.0f;
  float damage_mult = 0.3f;
  entt::entity owner = entt::null;
  uint32_t skill_id = 0;

  // For storing damage snapshot if needed, or reference owner
  // We can store CombatStats snapshot to be safe
  // For now, minimal.
};

} // namespace NoMoreDay
