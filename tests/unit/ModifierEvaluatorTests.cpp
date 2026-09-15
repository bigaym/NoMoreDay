#include "doctest.h"

#include "game/foundation/data/TagRegistry.hpp"
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
  header.op_count = 20;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset = header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset =
      header.ops_offset + 20 * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 8005001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 20;

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

  NoMoreDay::ModifierRuntimeOp berserkerOp;
  berserkerOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE);
  berserkerOp.param_u32 = 17u;

  NoMoreDay::ModifierRuntimeOp voidZoneOp;
  voidZoneOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE);
  voidZoneOp.param_u32 = 10u;

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

  NoMoreDay::ModifierRuntimeOp suppressorOp;
  suppressorOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE);
  suppressorOp.param_u32 = 24u;

  NoMoreDay::ModifierRuntimeOp avengerOp;
  avengerOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH);
  avengerOp.param_u32 = 20u;

  NoMoreDay::ModifierRuntimeOp soulLinkOp;
  soulLinkOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE);
  soulLinkOp.param_u32 = 21u;

  NoMoreDay::ModifierRuntimeOp stormUpdateOp;
  stormUpdateOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE);
  stormUpdateOp.param_u32 = 7u;

  NoMoreDay::ModifierRuntimeOp voidOnHitOp;
  voidOnHitOp.opcode = static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT);
  voidOnHitOp.param_u32 = 9u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               20 * sizeof(NoMoreDay::ModifierRuntimeOp));
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
  AppendStructEvaluator(blob, berserkerOp);
  AppendStructEvaluator(blob, voidZoneOp);
  AppendStructEvaluator(blob, nullifierOp);
  AppendStructEvaluator(blob, entanglerOp);
  AppendStructEvaluator(blob, toxicOp);
  AppendStructEvaluator(blob, mirrorImageOp);
  AppendStructEvaluator(blob, stormStriderOp);
  AppendStructEvaluator(blob, soulEaterOp);
  AppendStructEvaluator(blob, suppressorOp);
  AppendStructEvaluator(blob, avengerOp);
  AppendStructEvaluator(blob, soulLinkOp);
  AppendStructEvaluator(blob, stormUpdateOp);
  AppendStructEvaluator(blob, voidOnHitOp);
  return blob;
}

std::vector<uint8_t> BuildManaCostRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2 * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 6007001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp globalOp;
  globalOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MANA_COST_MULT);
  globalOp.param_u32 = 0u;
  globalOp.param_f32 = 0.9f;

  NoMoreDay::ModifierRuntimeOp skillOp;
  skillOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MANA_COST_MULT);
  skillOp.param_u32 = 7u;
  skillOp.param_f32 = 0.5f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2 * sizeof(NoMoreDay::ModifierRuntimeOp));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, globalOp);
  AppendStructEvaluator(blob, skillOp);
  return blob;
}

std::vector<uint8_t> BuildPercentMultRuntimeBlob() {
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
  record.id = 4007001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  op.param_u32 = 4u;
  op.param_f32 = 0.1f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, op);
  return blob;
}

// 单记录含两个目标属性不同的乘算算子，用于验证 target_stat 精确覆写。
std::vector<uint8_t> BuildTwoStatPercentMultRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2 * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 4007002u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp healthOp;
  healthOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  healthOp.param_u32 = 4u;
  healthOp.param_f32 = 0.1f;

  NoMoreDay::ModifierRuntimeOp moveSpeedOp;
  moveSpeedOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  moveSpeedOp.param_u32 = 7u;
  moveSpeedOp.param_f32 = 0.2f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2 * sizeof(NoMoreDay::ModifierRuntimeOp));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, healthOp);
  AppendStructEvaluator(blob, moveSpeedOp);
  return blob;
}

// 单记录同时含属性算子与事件算子，用于验证类别掩码跳过无关算子组。
std::vector<uint8_t> BuildMixedCategoryRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2 * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 4007003u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp statOp;
  statOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  statOp.param_u32 = 4u;
  statOp.param_f32 = 0.1f;

  NoMoreDay::ModifierRuntimeOp eventOp;
  eventOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MONSTER_EVENT_ON_HIT);
  eventOp.param_u32 = 777u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2 * sizeof(NoMoreDay::ModifierRuntimeOp));
  AppendStructEvaluator(blob, header);
  AppendStructEvaluator(blob, record);
  AppendStructEvaluator(blob, filter);
  AppendStructEvaluator(blob, statOp);
  AppendStructEvaluator(blob, eventOp);
  return blob;
}

} // namespace

TEST_CASE("[Unit] ModifierEvaluator - MANA_COST_MULT global wildcard affects any skill") {
  NoMoreDay::ModifierRecord record;
  NoMoreDay::ModifierOp op;
  op.opcode = NoMoreDay::ModifierOpCode::MANA_COST_MULT;
  op.param_u32 = 0u; // 全局通配
  op.param_f32 = 0.9f;
  record.ops.push_back(op);

  NoMoreDay::ModifierEvalContext ctx;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      std::span<const NoMoreDay::ModifierRecord>(&record, 1), ctx);

  CHECK(delta.GetManaCostMultiplier(7u) == doctest::Approx(0.9f));
  CHECK(delta.GetManaCostMultiplier(123u) == doctest::Approx(0.9f));
  CHECK(delta.GetManaCostMultiplier(0u) == doctest::Approx(0.9f));
}

TEST_CASE("[Unit] ModifierEvaluator - MANA_COST_MULT skill specialization is isolated") {
  NoMoreDay::ModifierRecord record;
  NoMoreDay::ModifierOp op;
  op.opcode = NoMoreDay::ModifierOpCode::MANA_COST_MULT;
  op.param_u32 = 7u;
  op.param_f32 = 0.5f;
  record.ops.push_back(op);

  NoMoreDay::ModifierEvalContext ctx;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      std::span<const NoMoreDay::ModifierRecord>(&record, 1), ctx);

  CHECK(delta.GetManaCostMultiplier(7u) == doctest::Approx(0.5f));
  CHECK(delta.GetManaCostMultiplier(8u) == doctest::Approx(1.0f));
  CHECK(delta.GetManaCostMultiplier(0u) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - MANA_COST_MULT compounds global and skill") {
  NoMoreDay::ModifierRecord record;

  NoMoreDay::ModifierOp globalOp;
  globalOp.opcode = NoMoreDay::ModifierOpCode::MANA_COST_MULT;
  globalOp.param_u32 = 0u;
  globalOp.param_f32 = 0.9f;
  record.ops.push_back(globalOp);

  NoMoreDay::ModifierOp globalOp2;
  globalOp2.opcode = NoMoreDay::ModifierOpCode::MANA_COST_MULT;
  globalOp2.param_u32 = 0u;
  globalOp2.param_f32 = 0.8f;
  record.ops.push_back(globalOp2);

  NoMoreDay::ModifierOp skillOp;
  skillOp.opcode = NoMoreDay::ModifierOpCode::MANA_COST_MULT;
  skillOp.param_u32 = 7u;
  skillOp.param_f32 = 0.5f;
  record.ops.push_back(skillOp);

  NoMoreDay::ModifierEvalContext ctx;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      std::span<const NoMoreDay::ModifierRecord>(&record, 1), ctx);

  // 全局连乘 0.9 * 0.8 = 0.72，技能 7 = 0.5，合计 0.36
  CHECK(delta.GetManaCostMultiplier(7u) == doctest::Approx(0.36f));
  // skillId == 0 只取全局通配，不与自身重复相乘
  CHECK(delta.GetManaCostMultiplier(0u) == doctest::Approx(0.72f));
  CHECK(delta.GetManaCostMultiplier(9u) == doctest::Approx(0.72f));
}

TEST_CASE("[Unit] ModifierEvaluator - runtime registry applies MANA_COST_MULT") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildManaCostRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 6007001u;
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const uint32_t>(&recordId, 1), ctx);

  CHECK(delta.GetManaCostMultiplier(7u) == doctest::Approx(0.45f));
  CHECK(delta.GetManaCostMultiplier(3u) == doctest::Approx(0.9f));
  CHECK(delta.GetManaCostMultiplier(0u) == doctest::Approx(0.9f));
}

TEST_CASE("[Unit] ModifierEvaluator - record request overrides ADD_STAT_PERCENT_MULT value") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildPercentMultRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 4007001u;
  const std::span<const uint32_t> recordIds(&recordId, 1);

  // 无 override：使用离线模板值 0.1
  const auto templateDelta = NoMoreDay::ModifierEvaluator::Evaluate(runtime, recordIds, ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, templateDelta) ==
        doctest::Approx(110.0f));

  // 命中 override：以运行时滚值 0.5 替代
  const NoMoreDay::ModifierRecordRequest request{
      4007001u, true, 0.5f, NoMoreDay::kAllStatTargets};
  const auto overrideDelta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const NoMoreDay::ModifierRecordRequest>(&request, 1),
      ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, overrideDelta) ==
        doctest::Approx(150.0f));

  // 同一 record_id 出现两次、滚值不同：应各施加一次 -> 1.5 * 1.2 = 1.8。
  // 锁死“按 id 只取首条 override”的回归（旧实现会得到 1.5 * 1.5 = 2.25）。
  const NoMoreDay::ModifierRecordRequest repeated[2] = {
      {4007001u, true, 0.5f, NoMoreDay::kAllStatTargets},
      {4007001u, true, 0.2f, NoMoreDay::kAllStatTargets}};
  const auto stackedDelta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const NoMoreDay::ModifierRecordRequest>(repeated, 2),
      ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, stackedDelta) ==
        doctest::Approx(180.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - record request override only targets matching stat") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildTwoStatPercentMultRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 4007002u;
  const std::span<const uint32_t> recordIds(&recordId, 1);

  // 记录含两个乘算 op：Stat 4 模板 0.1、Stat 7 模板 0.2。
  const auto templateDelta = NoMoreDay::ModifierEvaluator::Evaluate(runtime, recordIds, ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, templateDelta) ==
        doctest::Approx(110.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 7u, templateDelta) ==
        doctest::Approx(120.0f));

  // target_stat = 4 只覆写 Stat 4，Stat 7 保留模板值。
  const NoMoreDay::ModifierRecordRequest request{4007002u, true, 0.5f, 4u};
  const auto delta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const NoMoreDay::ModifierRecordRequest>(&request, 1),
      ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, delta) ==
        doctest::Approx(150.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 7u, delta) ==
        doctest::Approx(120.0f));

  // target_stat 未命中任何算子时不覆写任何属性。
  const NoMoreDay::ModifierRecordRequest mismatch{4007002u, true, 0.5f, 99u};
  const auto mismatchDelta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, std::span<const NoMoreDay::ModifierRecordRequest>(&mismatch, 1),
      ctx);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, mismatchDelta) ==
        doctest::Approx(110.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 7u, mismatchDelta) ==
        doctest::Approx(120.0f));
}

TEST_CASE("[Unit] ModifierEvaluator - op category mask skips unrelated op groups") {
  NoMoreDay::ModifierRuntimeRegistry runtime;
  const auto blob = BuildMixedCategoryRuntimeBlob();
  REQUIRE(runtime.LoadFromBytes(blob));

  NoMoreDay::ModifierEvalContext ctx;
  const uint32_t recordId = 4007003u;
  const std::span<const uint32_t> recordIds(&recordId, 1);
  const NoMoreDay::ModifierRecordRequest request{
      4007003u, false, 0.0f, NoMoreDay::kAllStatTargets};
  const std::span<const NoMoreDay::ModifierRecordRequest> requests(&request, 1);

  // 默认 All：属性与事件同时产出。
  const auto allDelta = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, requests, ctx, NoMoreDay::ModifierOpCategory::All);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, allDelta) ==
        doctest::Approx(110.0f));
  CHECK(allDelta.monster_event_on_hit_affix_ids.contains(777u));

  // 只要 Stats：事件集合必须为空（窄入口不为无关组做插入）。
  const auto statsOnly = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, requests, ctx, NoMoreDay::ModifierOpCategory::Stats);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, 4u, statsOnly) ==
        doctest::Approx(110.0f));
  CHECK(statsOnly.monster_event_on_hit_affix_ids.empty());

  // 只要 Events：属性乘算必须为空。
  const auto eventsOnly = NoMoreDay::ModifierEvaluator::Evaluate(
      runtime, requests, ctx, NoMoreDay::ModifierOpCategory::Events);
  CHECK(eventsOnly.percent_mult.empty());
  CHECK(eventsOnly.monster_event_on_hit_affix_ids.contains(777u));
}

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

  NoMoreDay::ModifierOp berserkerBehaviorOp;
  berserkerBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE;
  berserkerBehaviorOp.param_u32 = 17u;
  record.ops.push_back(berserkerBehaviorOp);

  NoMoreDay::ModifierOp voidZoneBehaviorOp;
  voidZoneBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE;
  voidZoneBehaviorOp.param_u32 = 10u;
  record.ops.push_back(voidZoneBehaviorOp);

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

  NoMoreDay::ModifierOp suppressorBehaviorOp;
  suppressorBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE;
  suppressorBehaviorOp.param_u32 = 24u;
  record.ops.push_back(suppressorBehaviorOp);

  NoMoreDay::ModifierOp avengerBehaviorOp;
  avengerBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH;
  avengerBehaviorOp.param_u32 = 20u;
  record.ops.push_back(avengerBehaviorOp);

  NoMoreDay::ModifierOp soulLinkBehaviorOp;
  soulLinkBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE;
  soulLinkBehaviorOp.param_u32 = 21u;
  record.ops.push_back(soulLinkBehaviorOp);

  NoMoreDay::ModifierOp stormUpdateBehaviorOp;
  stormUpdateBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE;
  stormUpdateBehaviorOp.param_u32 = 7u;
  record.ops.push_back(stormUpdateBehaviorOp);

  NoMoreDay::ModifierOp voidOnHitBehaviorOp;
  voidOnHitBehaviorOp.opcode =
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT;
  voidOnHitBehaviorOp.param_u32 = 9u;
  record.ops.push_back(voidOnHitBehaviorOp);

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
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE)));
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
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH)));
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
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE)));
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE)));
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
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH)));
  CHECK(delta.monster_behavior_on_death_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH)));
}
