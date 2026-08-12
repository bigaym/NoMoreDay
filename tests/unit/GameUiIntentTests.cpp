#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace NoMoreDay {
namespace {

// --- R1 POD boundary source guards ----------------------------------------
// The contract headers are pure data (design §3.2/§3.3): no EnTT, raylib or
// gameplay headers may leak into them. Comment text may mention the rules, so
// the guard inspects non-comment lines only.

std::vector<std::string> NonCommentLines(const std::string& contents) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= contents.size()) {
    const std::size_t end = contents.find('\n', start);
    std::string line =
        contents.substr(start, end == std::string::npos ? end : end - start);
    // Strip leading whitespace, then skip // and * comment lines (and the
    // #pragma once include guard line which only repeats the file name).
    std::size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos) {
      line = line.substr(first);
      if (line.rfind("//", 0) != 0 && line.rfind("*", 0) != 0) {
        lines.push_back(line);
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

TEST_CASE("[Unit] GameUiIntent - contract headers stay gameplay-free") {
  const std::string files[] = {"src/game/application/ui/GameUiIntent.hpp",
                               "src/game/application/ui/GameUiSnapshot.hpp"};
  for (const auto& path : files) {
    std::ifstream source(path);
    REQUIRE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source),
                               std::istreambuf_iterator<char>()};
    const auto lines = NonCommentLines(contents);

    bool hasEntt = false;
    bool hasRaylib = false;
    bool hasGameplayInclude = false;
    for (const auto& line : lines) {
      if (line.find("entt") != std::string::npos) {
        hasEntt = true;
      }
      if (line.find("raylib") != std::string::npos) {
        hasRaylib = true;
      }
      if (line.rfind("#include", 0) == 0) {
        const bool uiInternal =
            line.find("\"game/application/ui/") != std::string::npos;
        const bool gameHeader =
            (line.find("\"game/") != std::string::npos && !uiInternal) ||
            line.find("\"engine/") != std::string::npos ||
            line.find("<entt") != std::string::npos ||
            line.find("\"raylib") != std::string::npos;
        if (gameHeader) {
          hasGameplayInclude = true;
        }
      }
    }
    CAPTURE(path);
    CHECK_FALSE(hasEntt);
    CHECK_FALSE(hasRaylib);
    CHECK_FALSE(hasGameplayInclude);
  }
}

TEST_CASE("[Unit] GameUiIntent - payload is a tagged POD round trip") {
  ui::GameUiIntent intent;
  intent.sourceNode = 7;
  intent.kind = ui::GameUiIntentKind::StashDeposit;
  intent.payload.sourceDomainId = 100;
  intent.payload.targetDomainId = 200;
  intent.payload.catalystDomainId = 300;
  intent.payload.sourceSlot = 1;
  intent.payload.targetSlot = 2;
  intent.payload.sourceTab = 3;
  intent.payload.targetTab = 4;
  intent.payload.equipmentSlot = 5;
  intent.payload.socketIndex = 6;
  intent.payload.quantity = 7;
  intent.payload.affixIndex = 8;
  intent.payload.itemSource =
      static_cast<std::uint8_t>(ui::GameUiItemSource::Stash);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(ui::GameUiStashTarget::Personal);
  intent.payload.bagAction =
      static_cast<std::uint8_t>(ui::GameUiBagAction::Unequip);
  intent.payload.sortMode = 2;
  intent.payload.affixType = 1001; // Legendary affix range.
  intent.payload.isPrefix = true;
  intent.payload.salvageRarityMask = 1u << 2; // Rare.
  intent.payload.keepIfTier6Plus = true;
  intent.payload.excludeLocked = false;
  intent.payload.allocationStrength = 1;
  intent.payload.allocationDexterity = 2;
  intent.payload.allocationIntelligence = 3;
  intent.payload.allocationVitality = 4;

  // Trivial copy keeps every field (POD semantics).
  const ui::GameUiIntent copy = intent;
  CHECK(copy.sourceNode == 7);
  CHECK(copy.kind == ui::GameUiIntentKind::StashDeposit);
  CHECK(copy.payload.sourceDomainId == 100);
  CHECK(copy.payload.targetDomainId == 200);
  CHECK(copy.payload.catalystDomainId == 300);
  CHECK(copy.payload.sourceSlot == 1);
  CHECK(copy.payload.targetSlot == 2);
  CHECK(copy.payload.sourceTab == 3);
  CHECK(copy.payload.targetTab == 4);
  CHECK(copy.payload.equipmentSlot == 5);
  CHECK(copy.payload.socketIndex == 6);
  CHECK(copy.payload.quantity == 7);
  CHECK(copy.payload.affixIndex == 8);
  CHECK(copy.payload.itemSource ==
        static_cast<std::uint8_t>(ui::GameUiItemSource::Stash));
  CHECK(copy.payload.stashTarget ==
        static_cast<std::uint8_t>(ui::GameUiStashTarget::Personal));
  CHECK(copy.payload.bagAction ==
        static_cast<std::uint8_t>(ui::GameUiBagAction::Unequip));
  CHECK(copy.payload.sortMode == 2);
  CHECK(copy.payload.affixType == 1001);
  CHECK(copy.payload.isPrefix);
  CHECK(copy.payload.salvageRarityMask == (1u << 2));
  CHECK(copy.payload.keepIfTier6Plus);
  CHECK_FALSE(copy.payload.excludeLocked);
  CHECK(copy.payload.allocationStrength == 1);
  CHECK(copy.payload.allocationDexterity == 2);
  CHECK(copy.payload.allocationIntelligence == 3);
  CHECK(copy.payload.allocationVitality == 4);
}

TEST_CASE("[Unit] GameUiIntent - intent kinds cover every gameplay domain") {
  // The R1 contract enumerates all gameplay actions; ensure the sentinel
  // exists and the individual domains are reachable.
  CHECK(static_cast<int>(ui::GameUiIntentKind::PickupItem) == 0);
  CHECK(static_cast<int>(ui::GameUiIntentKind::ConfirmAttributeAllocation) >
        static_cast<int>(ui::GameUiIntentKind::CraftBatchSalvage));
  CHECK(static_cast<int>(ui::GameUiIntentKind::Count) >
        static_cast<int>(ui::GameUiIntentKind::ConfirmAttributeAllocation));
}

TEST_CASE("[Unit] GameUiSnapshot - result carries a failure code") {
  ui::GameUiResult failure;
  failure.success = false;
  failure.code = ui::GameUiResultCode::TooFarAway;
  failure.notification = "Too far away";
  CHECK_FALSE(failure.success);
  CHECK(failure.code == ui::GameUiResultCode::TooFarAway);
  CHECK(failure.notification == "Too far away");
  CHECK(failure.clearedDomainIds.empty());
}

} // namespace
} // namespace NoMoreDay
