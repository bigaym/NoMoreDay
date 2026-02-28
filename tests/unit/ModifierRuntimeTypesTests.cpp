#include "doctest.h"

#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

TEST_CASE("[Unit] ModifierRuntimeTypes - Header layout is stable") {
  CHECK(sizeof(NoMoreDay::ModifierRuntimeHeader) == 64);
  CHECK(NoMoreDay::ModifierRuntimeHeader::kMagic == 0x4D444D4Eu);
  CHECK(sizeof(NoMoreDay::ModifierRuntimeRecord) == 24);
  CHECK(sizeof(NoMoreDay::ModifierRuntimeFilter) == 48);
  CHECK(sizeof(NoMoreDay::ModifierRuntimeOp) == 12);
}
