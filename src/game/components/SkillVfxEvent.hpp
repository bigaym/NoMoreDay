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
  TransmuterSwitch = 6,
  KeystoneActivate = 7,
};

enum class SkillVfxElementType : uint8_t {
  Physical = 0,
  Fire = 1,
  Cold = 2,
  Lightning = 3,
  Void = 4,
};

enum class SkillVfxResistDebuffType : uint8_t {
  None = 0,
  TypeA = 1,
  TypeB = 2,
  TypeC = 3,
  TypeD = 4,
  TypeE = 5,
};

namespace SkillVfxNodeRoleMask {
inline constexpr uint32_t None = 0u;
inline constexpr uint32_t Keystone = 1u << 0;
inline constexpr uint32_t Trigger = 1u << 1;
inline constexpr uint32_t Synergy = 1u << 2;
inline constexpr uint32_t Transmuter = 1u << 3;
inline constexpr uint32_t Any =
    Keystone | Trigger | Synergy | Transmuter;
} // namespace SkillVfxNodeRoleMask

inline constexpr bool HasSkillVfxNodeRole(const uint32_t mask,
                                          const uint32_t roleBit) {
  return (mask & roleBit) == roleBit;
};

struct SkillVfxEvent {
  uint32_t skillId = 0;
  uint64_t castId = 0;
  SkillVfxEventType type = SkillVfxEventType::CastStart;
  Vector2 origin = {0.0f, 0.0f};
  Vector2 target = {0.0f, 0.0f};
  Tag effectiveTags = Tag::None;
  uint32_t nodeRoleMask = SkillVfxNodeRoleMask::None;
  uint8_t qualityTier = 1;
  float intensity = 1.0f;
  uint8_t elementType = static_cast<uint8_t>(SkillVfxElementType::Physical);
  uint8_t resistDebuffType =
      static_cast<uint8_t>(SkillVfxResistDebuffType::None);
};

} // namespace NoMoreDay
