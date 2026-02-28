#include "doctest.h"

#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

TEST_CASE("[Integration] ModifierRuntimeV2 - boot loads binary and evaluates sample") {
  CHECK(NoMoreDay::ModifierRuntimeRegistry::Get().EnsureLoaded());
  CHECK(NoMoreDay::ModifierRuntimeRegistry::Get().RecordCount() > 0);
}
