#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "game/foundation/data/TagRegistry.hpp"

namespace NoMoreDay {

enum class SkillDisplayDamageMode { Hit, PerSecond, Total, ChannelWindow };

struct SkillDisplayPreview {
  bool has_duration = false;
  float display_duration_seconds = 0.0f;
  bool has_estimated_damage = false;
  float estimated_damage_value = 0.0f;
  SkillDisplayDamageMode estimated_damage_mode = SkillDisplayDamageMode::Hit;

  float display_mana_cost = 0.0f;
  float display_cooldown = 0.0f;
  int display_projectiles = 1;
  Tag display_tags = Tag::None;
};

class SkillDisplayPreviewService
{
public:
    [[nodiscard]] static SkillDisplayPreview Build(entt::registry& registry,
                                                   entt::entity player,
                                                   uint32_t skillId);
};

} // namespace NoMoreDay
