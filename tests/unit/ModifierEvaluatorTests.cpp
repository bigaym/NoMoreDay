#include "doctest.h"

#include "game/data/TagRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {

float EvalFixtureAddFlat(const float before, const NoMoreDay::ModifierEvalContext &ctx) {
  NoMoreDay::ModifierRecord record;
  record.filter.profession_mask = 1ull;
  record.filter.skill_id_whitelist = {1u};
  record.filter.required_skill_tags_all = static_cast<uint64_t>(NoMoreDay::Tag::Hit);

  NoMoreDay::ModifierOp op;
  op.opcode = NoMoreDay::ModifierOpCode::ADD_STAT_FLAT;
  op.param_u32 = 4u;
  op.param_f32 = 50.0f;
  record.ops.push_back(op);

  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      std::span<const NoMoreDay::ModifierRecord>(&record, 1), ctx);
  return NoMoreDay::ModifierEvaluator::ApplyStat(before, 4u, delta);
}

template <typename T>
void AppendStructEvaluator(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildFlatHealthRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 3001100u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.profession_mask = 1ull;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_FLAT);
  op.param_u32 = 4u;
  op.param_f32 = 50.0f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, op);
  return blob;
}

std::vector<uint8_t> BuildSkillLevelRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 1001001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_SKILL_LEVEL);
  op.param_u32 = 1u;
  op.param_f32 = 2.0f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, op);
  return blob;
}

std::vector<uint8_t> BuildMonsterEventRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 8004001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MONSTER_EVENT_ON_DEATH);
  op.param_u32 = 8u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, op);
  return blob;
}

std::vector<uint8_t> BuildMonsterBehaviorRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 13;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset = header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset =
      header.ops_offset + 13 * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 8005001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 13;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp onHitOp;
  onHitOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT);
  onHitOp.param_u32 = 16u;

  NoMoreDay::ModifierRuntimeOp teleporterOp;
  teleporterOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE);
  teleporterOp.param_u32 = 12u;

  NoMoreDay::ModifierRuntimeOp frozenOp;
  frozenOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE);
  frozenOp.param_u32 = 6u;

  NoMoreDay::ModifierRuntimeOp manaSiphonOp;
  manaSiphonOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE);
  manaSiphonOp.param_u32 = 25u;

  NoMoreDay::ModifierRuntimeOp shieldingOp;
  shieldingOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE);
  shieldingOp.param_u32 = 26u;

  NoMoreDay::ModifierRuntimeOp vortexOp;
  vortexOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE);
  vortexOp.param_u32 = 27u;

  NoMoreDay::ModifierRuntimeOp wallerOp;
  wallerOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE);
  wallerOp.param_u32 = 28u;

  NoMoreDay::ModifierRuntimeOp nullifierOp;
  nullifierOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT);
  nullifierOp.param_u32 = 13u;

  NoMoreDay::ModifierRuntimeOp entanglerOp;
  entanglerOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT);
  entanglerOp.param_u32 = 19u;

  NoMoreDay::ModifierRuntimeOp toxicOp;
  toxicOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH);
  toxicOp.param_u32 = 8u;

  NoMoreDay::ModifierRuntimeOp mirrorImageOp;
  mirrorImageOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE);
  mirrorImageOp.param_u32 = 21u;

  NoMoreDay::ModifierRuntimeOp stormStriderOp;
  stormStriderOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE);
  stormStriderOp.param_u32 = 11u;

  NoMoreDay::ModifierRuntimeOp soulEaterOp;
  soulEaterOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH);
  soulEaterOp.param_u32 = 22u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               13 * sizeof(NoMoreDay::ModifierRuntimeOp));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, onHitOp);
  AppendStructEvaluator(blob, teleporterOp);
  AppendStructEvaluator(blob, frozenOp);
  AppendStructEvaluator(blob, manaSiphonOp);
  AppendStructEvaluator(blob, shieldingOp);
  AppendStructEvaluator(blob, vortexOp);
  AppendStructEvaluator(blob, wallerOp);
  AppendStructEvaluator(blob, nullifierOp);
  AppendStructEvaluator(blob, entanglerOp);
  AppendStructEvaluator(blob, toxicOp);
  AppendStructEvaluator(blob, mirrorImageOp);
  AppendStructEvaluator(blob, stormStriderOp);
  AppendStructEvaluator(blob, soulEaterOp);
  return blob;
}

} // namespace

TEST_CASE("[Unit] ModifierEvaluator - Applies ADD_STAT_FLAT when filters match") {
  NoMoreDay::ModifierEvalContext ctx;
  ctx.profession_id = 0;
  ctx.skill_id = 1;
  ctx.skill_tags = NoMoreDay::Tag::Hit;

  const float before = 100.0f;
  const float after = EvalFixtureAddFlat(before, ctx);
  CHECK(after == doctest::Approx(150.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - runtime registry evaluate applies ops") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildFlatHealthRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  ctx.profession_id = 0;

  const uint32_t recordId = 3001100u;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const uint32_t>(&recordId, 1), ctx);
  const float after = NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, delta);
  CHECK(after == doctest::Approx(150.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - runtime registry evaluates ADD_SKILL_LEVEL") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildSkillLevelRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  ctx.skill_id = 1u;

  const uint32_t recordId = 1001001u;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const uint32_t>(&recordId, 1), ctx);
  CHECK(delta.GetSkillLevelBonus(1u) == doctest::Approx(2.0f));
  CHECK(delta.GetSkillLevelBonus(9u) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - captures monster event ops in delta") {
  NoMoreDay::ModifierRecord record;
  NoMoreDay::ModifierOp updateOp;
  updateOp.opcode = NoMoreDay::ModifierOpCode::MONSTER_EVENT_ON_UPDATE;
  updateOp.param_u32 = 5u;
  record.ops.push_back(updateOp);

  NoMoreDay::ModifierOp onHitOp;
  onHitOp.opcode = NoMoreDay::ModifierOpCode::MONSTER_EVENT_ON_HIT;
  onHitOp.param_u32 = 16u;
  record.ops.push_back(onHitOp);

  NoMoreDay::ModifierOp moltenBehaviorOp;
  moltenBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE;
  moltenBehaviorOp.param_u32 = 5u;
  record.ops.push_back(moltenBehaviorOp);

  NoMoreDay::ModifierOp vampiricBehaviorOp;
  vampiricBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT;
  vampiricBehaviorOp.param_u32 = 16u;
  record.ops.push_back(vampiricBehaviorOp);

  NoMoreDay::ModifierOp teleporterBehaviorOp;
  teleporterBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE;
  teleporterBehaviorOp.param_u32 = 12u;
  record.ops.push_back(teleporterBehaviorOp);

  NoMoreDay::ModifierOp frozenBehaviorOp;
  frozenBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE;
  frozenBehaviorOp.param_u32 = 6u;
  record.ops.push_back(frozenBehaviorOp);

  NoMoreDay::ModifierOp manaSiphonBehaviorOp;
  manaSiphonBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE;
  manaSiphonBehaviorOp.param_u32 = 25u;
  record.ops.push_back(manaSiphonBehaviorOp);

  NoMoreDay::ModifierOp shieldingBehaviorOp;
  shieldingBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE;
  shieldingBehaviorOp.param_u32 = 26u;
  record.ops.push_back(shieldingBehaviorOp);

  NoMoreDay::ModifierOp vortexBehaviorOp;
  vortexBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE;
  vortexBehaviorOp.param_u32 = 27u;
  record.ops.push_back(vortexBehaviorOp);

  NoMoreDay::ModifierOp wallerBehaviorOp;
  wallerBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE;
  wallerBehaviorOp.param_u32 = 28u;
  record.ops.push_back(wallerBehaviorOp);

  NoMoreDay::ModifierOp nullifierBehaviorOp;
  nullifierBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT;
  nullifierBehaviorOp.param_u32 = 13u;
  record.ops.push_back(nullifierBehaviorOp);

  NoMoreDay::ModifierOp entanglerBehaviorOp;
  entanglerBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT;
  entanglerBehaviorOp.param_u32 = 19u;
  record.ops.push_back(entanglerBehaviorOp);

  NoMoreDay::ModifierOp toxicBehaviorOp;
  toxicBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH;
  toxicBehaviorOp.param_u32 = 8u;
  record.ops.push_back(toxicBehaviorOp);

  NoMoreDay::ModifierOp mirrorImageBehaviorOp;
  mirrorImageBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE;
  mirrorImageBehaviorOp.param_u32 = 21u;
  record.ops.push_back(mirrorImageBehaviorOp);

  NoMoreDay::ModifierOp stormStriderBehaviorOp;
  stormStriderBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE;
  stormStriderBehaviorOp.param_u32 = 11u;
  record.ops.push_back(stormStriderBehaviorOp);

  NoMoreDay::ModifierOp soulEaterBehaviorOp;
  soulEaterBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH;
  soulEaterBehaviorOp.param_u32 = 22u;
  record.ops.push_back(soulEaterBehaviorOp);

  NoMoreDay::ModifierEvalContext ctx;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      std::span<const NoMoreDay::ModifierRecord>(&record, 1), ctx);

  CHECK(delta.monster_event_on_update_affix_ids.contains(5u));
  CHECK(delta.monster_event_on_hit_affix_ids.contains(16u));
  CHECK(delta.monster_event_on_death_affix_ids.empty());
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH)));
}

TEST_CASE("[Unit] ModifierEvaluator - runtime registry captures monster event ops") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildMonsterEventRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 8004001u;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const uint32_t>(&recordId, 1), ctx);

  CHECK(delta.monster_event_on_death_affix_ids.contains(8u));
}

TEST_CASE("[Unit] ModifierEvaluator - runtime registry captures monster behavior ops") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildMonsterBehaviorRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 8005001u;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const uint32_t>(&recordId, 1), ctx);

  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH)));
}
