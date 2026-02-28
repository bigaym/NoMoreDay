#include "doctest.h"

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> BuildBlobWithMagic(const uint32_t magic) {
  NoMoreDay::ModifierRuntimeHeader header;
  header.magic = magic;

  std::vector<uint8_t> blob(sizeof(header));
  std::memcpy(blob.data(), &header, sizeof(header));
  return blob;
}

template <typename T>
void AppendStructRegistry(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildRuntimeBlobForLookup() {
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
  AppendStructRegistry(blob, header);
  AppendStructRegistry(blob, record);
  AppendStructRegistry(blob, filter);
  AppendStructRegistry(blob, op);
  return blob;
}

} // namespace

TEST_CASE("[Unit] ModifierRuntimeRegistry - Rejects bad magic") {
  auto blob = BuildBlobWithMagic(0x12345678u);
  NoMoreDay::ModifierRuntimeRegistry reg;
  CHECK_FALSE(reg.LoadFromBytes(blob));
}

TEST_CASE("[Unit] ModifierRuntimeRegistry - exposes record by id") {
  const auto blob = BuildRuntimeBlobForLookup();
  NoMoreDay::ModifierRuntimeRegistry reg;
  REQUIRE(reg.LoadFromBytes(blob));

  const auto *record = reg.FindRecordById(3001100u);
  REQUIRE(record != nullptr);
  CHECK(record->id == 3001100u);

  const auto *filter = reg.GetFilter(*record);
  REQUIRE(filter != nullptr);
  CHECK(filter->profession_mask == 1ull);

  const auto ops = reg.GetOps(*record);
  REQUIRE(ops.size() == 1u);
  CHECK(ops[0].param_u32 == 4u);
  CHECK(ops[0].param_f32 == doctest::Approx(50.0f));
}
