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
