#pragma once

#include <cstdint>
#include <entt/entt.hpp>

namespace NoMoreDay {

enum class SkillDisplayDamageMode { Hit, PerSecond, Total, ChannelWindow };

struct SkillDisplayPreview {
  bool has_duration = false;
  float display_duration_seconds = 0.0f;
  bool has_estimated_damage = false;
  float estimated_damage_value = 0.0f;
  SkillDisplayDamageMode estimated_damage_mode = SkillDisplayDamageMode::Hit;
};

class SkillDisplayPreviewService
{
public:
    [[nodiscard]] static SkillDisplayPreview Build(entt::registry& registry,
                                                   entt::entity player,
                                                   uint32_t skillId);
};

} // namespace NoMoreDay
