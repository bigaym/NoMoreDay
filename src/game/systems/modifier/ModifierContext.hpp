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
