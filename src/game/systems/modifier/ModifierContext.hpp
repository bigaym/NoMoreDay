#pragma once

#include "game/data/TagRegistry.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay {

enum class ModifierOpCode : uint16_t {
  ADD_STAT_FLAT = 0,
  ADD_STAT_PERCENT_ADD = 1,
  ADD_STAT_PERCENT_MULT = 2,
  ADD_SKILL_LEVEL = 3,
  MANA_COST_MULT = 4,
  MONSTER_EVENT_ON_UPDATE = 5,
  MONSTER_EVENT_ON_HIT = 6,
  MONSTER_EVENT_ON_DEATH = 7,
  MONSTER_BEHAVIOR_MOLTEN_UPDATE = 8,
  MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT = 9,
  MONSTER_BEHAVIOR_TELEPORTER_UPDATE = 10,
  MONSTER_BEHAVIOR_FROZEN_UPDATE = 11,
  MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE = 12,
  MONSTER_BEHAVIOR_SHIELDING_UPDATE = 13,
  MONSTER_BEHAVIOR_VORTEX_UPDATE = 14,
  MONSTER_BEHAVIOR_WALLER_UPDATE = 15,
  MONSTER_BEHAVIOR_NULLIFIER_ON_HIT = 16,
  MONSTER_BEHAVIOR_ENTANGLER_ON_HIT = 17,
  MONSTER_BEHAVIOR_TOXIC_ON_DEATH = 18,
  MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE = 19,
  MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE = 20,
  MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH = 21,
  MONSTER_BEHAVIOR_BERSERKER_UPDATE = 22,
  MONSTER_BEHAVIOR_VOIDZONE_UPDATE = 23,
  MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE = 24,
  MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH = 25,
  MONSTER_BEHAVIOR_SOUL_LINK_UPDATE = 26,
};

struct ModifierFilter {
  uint64_t profession_mask = 0;
  std::vector<uint32_t> skill_id_whitelist;
  uint64_t required_skill_tags_all = 0;
  uint64_t forbidden_skill_tags_any = 0;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0;
  std::vector<uint32_t> node_id_whitelist;
};

struct ModifierOp {
  ModifierOpCode opcode = ModifierOpCode::ADD_STAT_FLAT;
  uint32_t param_u32 = 0;
  float param_f32 = 0.0f;
};

struct ModifierRecord {
  ModifierFilter filter;
  std::vector<ModifierOp> ops;
};

struct ModifierEvalContext {
  uint32_t profession_id = 0;
  uint32_t skill_id = 0;
  Tag skill_tags = Tag::None;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0xFFFFFFFFu;
  std::vector<uint32_t> active_node_ids;
};

} // namespace NoMoreDay
