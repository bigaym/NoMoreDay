#include "doctest.h"

#include "game/components/Stats.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {

template <typename T>
void AppendStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildSkillSpecRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 2;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 2099001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.skill_whitelist_offset = 0;
  filter.skill_whitelist_count = 1;
  filter.node_whitelist_offset = 1;
  filter.node_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  op.param_u32 = static_cast<uint32_t>(NoMoreDay::StatType::PhysicalDamage);
  op.param_f32 = 0.35f;

  constexpr uint32_t kSkillWhitelist = 77u;
  constexpr uint32_t kNodeWhitelist = 999u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               sizeof(kSkillWhitelist) + sizeof(kNodeWhitelist));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  AppendStruct(blob, kSkillWhitelist);
  AppendStruct(blob, kNodeWhitelist);
  return blob;
}

float RunSpecFixture(const uint32_t skillId, const std::vector<uint32_t> &nodes) {
  constexpr float kBaseline = 100.0f;
  return NoMoreDay::SkillSpecModifierAdapter::ApplyHeavyMomentum(
      kBaseline, skillId, NoMoreDay::Tag::None, nodes);
}

} // namespace

TEST_CASE("[Unit] SkillSpecModifierAdapter - evaluates runtime-record-driven scalar") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildSkillSpecRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  const float baseline = RunSpecFixture(77u, {});
  const float boosted = RunSpecFixture(77u, {999u});
  CHECK(baseline == doctest::Approx(100.0f));
  CHECK(boosted == doctest::Approx(135.0f));
  CHECK(RunSpecFixture(77u, {213u}) == doctest::Approx(100.0f));
  CHECK(RunSpecFixture(99u, {999u}) == doctest::Approx(100.0f));
}

TEST_CASE("[Unit] SkillSpecModifierAdapter - runtime multiplier only affects physical damage") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildSkillSpecRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  std::array<float, 6> multipliers = {1.0f, 1.5f, 0.8f, 1.2f, 0.9f, 1.1f};
  const std::vector<uint32_t> heavyMomentumNodes = {999u};

  NoMoreDay::SkillSpecModifierAdapter::ApplyHeavyMomentumToDamageMultipliers(
      multipliers, 77u, NoMoreDay::Tag::None, heavyMomentumNodes);

  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Physical)] ==
        doctest::Approx(1.35f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Fire)] ==
        doctest::Approx(1.5f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Cold)] ==
        doctest::Approx(0.8f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Lightning)] ==
        doctest::Approx(1.2f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Poison)] ==
        doctest::Approx(0.9f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Shadow)] ==
        doctest::Approx(1.1f));
}
