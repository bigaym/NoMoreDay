#include "doctest.h"

#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstdint>

// 真实资产冒烟：显式 Reload 生成产物，而不是依赖单例里可能被其它用例
// 通过 LoadFromBytes 注入的合成 blob，否则本用例会恒定通过、失去守护意义。
// 4001004 是 assets/data/modifier_v2/map_modifiers.json 中真实存在的 map 词缀记录。
TEST_CASE("[Integration] ModifierRuntimeV2 - boot loads binary and evaluates sample") {
  auto &registry = NoMoreDay::ModifierRuntimeRegistry::Get();
  REQUIRE(registry.Reload("assets/generated/modifier_runtime_v2.bin"));
  CHECK(registry.RecordCount() > 0);

  constexpr uint32_t kRealMapRecordId = 4001004u;
  const NoMoreDay::ModifierRuntimeRecord *record =
      registry.FindRecordById(kRealMapRecordId);
  REQUIRE(record != nullptr);
  CHECK(record->id == kRealMapRecordId);
}
