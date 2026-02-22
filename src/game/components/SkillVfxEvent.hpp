#pragma once

#include "game/data/TagRegistry.hpp"
#include "raylib.h"

#include <cstdint>

namespace NoMoreDay {

enum class SkillVfxEventType : uint8_t {
  CastStart = 0,
  CastImpact = 1,
  TriggerProc = 2,
  EmpoweredConsume = 3,
  BuffEnter = 4,
  BuffExit = 5,
};

struct SkillVfxEvent {
  uint32_t skillId = 0;
  uint64_t castId = 0;
  SkillVfxEventType type = SkillVfxEventType::CastStart;
  Vector2 origin = {0.0f, 0.0f};
  Vector2 target = {0.0f, 0.0f};
  Tag effectiveTags = Tag::None;
  uint32_t nodeRoleMask = 0;
  uint8_t qualityTier = 1;
  float intensity = 1.0f;
};

} // namespace NoMoreDay
