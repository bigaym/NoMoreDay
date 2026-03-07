#pragma once

#include <cstdint>
#include <string>

namespace NoMoreDay {

enum class ProfessionID : uint8_t;

enum class BladeMasteryId : uint8_t {
  None = 0,
  SwordSaint = 1,
  HeavenlySword = 2,
  DemonBlade = 3,
};

enum class BladeResourceKind : uint8_t {
  None = 0,
  SwordIntent = 1,
  SwordFlow = 2,
  SpiritBladeTier = 3,
  Bloodthirst = 4,
};

enum class BladeAttunement : uint8_t {
  None = 0,
  Lightning = 1,
  Frost = 2,
  Fire = 3,
};

struct BladeMasteryProfile {
  BladeMasteryId id = BladeMasteryId::None;
  ProfessionID profession = static_cast<ProfessionID>(0);
  std::string name = "";
  std::string description = "";
  BladeResourceKind resource_kind = BladeResourceKind::None;
  int unlock_level = 50;
  int debug_unlock_level_override = 5;
  uint32_t signature_skill_id = 0;
  int max_resource = 10;
  float grace_period = 5.0f;
  float decay_interval = 0.5f;
};

} // namespace NoMoreDay
