#pragma once

#include <cstdint>

namespace NoMoreDay {

struct ModifierRuntimeHeader {
  static constexpr uint32_t kMagic = 0x4D444D4Eu;

  uint32_t magic = kMagic;
  uint16_t format_version = 2;
  uint16_t endian = 1;
  uint32_t record_count = 0;
  uint32_t filter_count = 0;
  uint32_t op_count = 0;
  uint32_t index_count = 0;
  uint32_t records_offset = 0;
  uint32_t filters_offset = 0;
  uint32_t ops_offset = 0;
  uint32_t index_offset = 0;
  uint32_t crc32 = 0;
  uint8_t reserved[20] = {};
};

static_assert(sizeof(ModifierRuntimeHeader) == 64,
              "ModifierRuntimeHeader must remain 64 bytes");

struct ModifierRuntimeRecord {
  uint32_t id = 0;
  uint32_t priority = 0;
  uint32_t filter_index = 0;
  uint32_t op_offset = 0;
  uint32_t op_count = 0;
  uint32_t reserved = 0;
};

struct ModifierRuntimeFilter {
  uint64_t profession_mask = 0;
  uint64_t required_skill_tags_all = 0;
  uint64_t forbidden_skill_tags_any = 0;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0;
  uint32_t skill_whitelist_offset = 0;
  uint32_t skill_whitelist_count = 0;
  uint32_t node_whitelist_offset = 0;
  uint32_t node_whitelist_count = 0;
};

struct ModifierRuntimeOp {
  uint16_t opcode = 0;
  uint16_t reserved = 0;
  uint32_t param_u32 = 0;
  float param_f32 = 0.0f;
};

static_assert(sizeof(ModifierRuntimeRecord) == 24,
              "ModifierRuntimeRecord layout must remain stable");
static_assert(sizeof(ModifierRuntimeFilter) == 48,
              "ModifierRuntimeFilter layout must remain stable");
static_assert(sizeof(ModifierRuntimeOp) == 12,
              "ModifierRuntimeOp layout must remain stable");

} // namespace NoMoreDay
