#include "doctest.h"

#include "game/foundation/components/Stats.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"
#include "game/systems/modifier/TalentModifierAdapter.hpp"

#include <cstdint>
#include <vector>

namespace {

template <typename T>
void AppendStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildTalentRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 1;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 3099001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.node_whitelist_offset = 0;
  filter.node_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_FLAT);
  op.param_u32 = static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth);
  op.param_f32 = 37.0f;

  constexpr uint32_t kNodeWhitelist = 2200u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               sizeof(kNodeWhitelist));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  AppendStruct(blob, kNodeWhitelist);
  return blob;
}

float RunTalentFixture(const std::vector<uint32_t> &nodeIds) {
  constexpr float kBase = 100.0f;
  return NoMoreDay::TalentModifierAdapter::ApplyFlatHealthBonus(kBase, nodeIds);
}

} // namespace

TEST_CASE("[Unit] TalentModifierAdapter - evaluates runtime-record-driven health bonus") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildTalentRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  const float base = RunTalentFixture({});
  const float withMappedNode = RunTalentFixture({2200u});
  const float withLegacyNode = RunTalentFixture({1100u});

  CHECK(base == doctest::Approx(100.0f));
  CHECK(withMappedNode == doctest::Approx(137.0f));
  CHECK(withLegacyNode == doctest::Approx(100.0f));

  const std::vector<uint32_t> mappedNode = {2200u};
  const std::vector<uint32_t> missingNode = {9999u};
  CHECK(NoMoreDay::TalentModifierAdapter::EvaluateFlatHealthBonus(mappedNode) ==
        doctest::Approx(37.0f));
  CHECK(NoMoreDay::TalentModifierAdapter::EvaluateFlatHealthBonus(missingNode) ==
        doctest::Approx(0.0f));
}
