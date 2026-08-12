#include "doctest.h"

#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"

#include <fstream>
#include <iterator>
#include <string>

using namespace NoMoreDay;

using NoMoreDay::ui::GameUiSnapshot;
using NoMoreDay::ui::kInvalidDomainId;

namespace {

// R8: the state machine consumes a frame-scoped snapshot. Skill/buff/item
// hovers surface as stable ids/indices, and the hotbar-slot hover resolves
// against snapshot.skillBar.slots — so the tests feed a minimal snapshot
// instead of a registry with an ActiveSkillsComponent. The builder emits one
// slot entry per populated hotbar slot, so a slot-hover test must size the
// vector explicitly (R8 adaptation: skillBar.slots is a vector, not a fixed
// array).
GameUiSnapshot MakeSnapshotWithSlot(std::size_t slotIndex, uint32_t skillId) {
  GameUiSnapshot snapshot;
  if (snapshot.skillBar.slots.size() <= slotIndex) {
    snapshot.skillBar.slots.resize(slotIndex + 1);
  }
  snapshot.skillBar.slots[slotIndex].skillId = skillId;
  return snapshot;
}

constexpr std::uint64_t kItemDomain = 0x1234u;

} // namespace

TEST_CASE("[Unit] TooltipController (UI) - hover starts: delay decays then alpha rises") {
  ui::TooltipController tooltip;
  const GameUiSnapshot snapshot;

  const uint32_t skillId = 100;
  tooltip.SetHoveredSkill(skillId);
  tooltip.UpdateState(snapshot, 0.1f);

  // New target while the alpha is still zero -> the long initial delay.
  CHECK(tooltip.ActiveTooltipSkillId() == skillId);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.02f)); // 0.12 - 0.10
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK_FALSE(tooltip.TooltipInitialized());

  // Enough hover to drain the delay.
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));

  // Delay is zero: alpha rises at 10/s.
  tooltip.UpdateState(snapshot, 0.05f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.5f));

  // Alpha clamps at 1.
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));
  CHECK(tooltip.HoveredLastFrame());
}

TEST_CASE("[Unit] TooltipController (UI) - target switch resets the delay") {
  ui::TooltipController tooltip;
  const GameUiSnapshot snapshot;

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(snapshot, 0.12f); // delay drained
  tooltip.UpdateState(snapshot, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // Switching the hovered skill while the tooltip is visible re-arms the
  // short re-entry delay and unlocks the position for the new target.
  tooltip.SetHoveredSkill(200);
  tooltip.UpdateState(snapshot, 0.0f);
  CHECK(tooltip.ActiveTooltipSkillId() == 200);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.05f)); // alpha > 0.01
  CHECK_FALSE(tooltip.TooltipInitialized());
}

TEST_CASE("[Unit] TooltipController (UI) - no hover: alpha decays and active clears") {
  ui::TooltipController tooltip;
  const GameUiSnapshot snapshot;

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(snapshot, 0.12f); // delay drained
  tooltip.UpdateState(snapshot, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // Mouse leaves: the next frame arms the 0.08s exit delay...
  tooltip.ResetFrame();
  tooltip.UpdateState(snapshot, 0.0f);
  CHECK_FALSE(tooltip.HoveredLastFrame());
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.08f));
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // ...the exit delay drains...
  tooltip.UpdateState(snapshot, 0.08f);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // ...then the alpha fades at 8/s.
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.2f));

  // Alpha hits zero: the active tooltip is cleared.
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK_FALSE(tooltip.TooltipInitialized());
}

TEST_CASE("[Unit] TooltipController (UI) - hover priority: item > skillId > slot > buff") {
  ui::TooltipController tooltip;
  const uint32_t slotSkillId = 300;
  const GameUiSnapshot snapshot = MakeSnapshotWithSlot(2, slotSkillId);

  // 1. Item hover wins over every other hover input. R8: item hovers are
  //    stable domain ids (never entt::entity values).
  tooltip.SetHoveredBuff(1);
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredSkill(200);
  tooltip.SetHoveredItemDomain(kItemDomain);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipItemDomain() == kItemDomain);
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 2. Direct skill-id hover wins over the hotbar slot.
  tooltip.ResetFrame();
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredSkill(200);
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == 200);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 3. Hotbar slot hover resolves to the assigned skill id at state-machine
  //    time (R8: from the snapshot's skillBar.slots view model).
  tooltip.ResetFrame();
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == slotSkillId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 4. Buff index wins when nothing else is hovered.
  tooltip.ResetFrame();
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipBuffIdx() == 1);
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
}

TEST_CASE("[Unit] TooltipController (UI) - adopts the skill-hub hover via SetHoveredSkill") {
  ui::TooltipController tooltip;
  const GameUiSnapshot snapshot;

  // U8: the skill hub / talent tree route their hovered node through
  // SetHoveredSkill (the legacy UISystem::State.hoveredSkillId slot is gone);
  // UpdateState uses the cached hover as the direct skill-id source.
  tooltip.SetHoveredSkill(400);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == 400);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  tooltip.SetHoveredSkill(NoMoreDay::INVALID_SKILL_ID);
}

TEST_CASE("[Unit] TooltipController (UI) - Enter/LeaveGameplay clear all state") {
  ui::TooltipController tooltip;
  const GameUiSnapshot snapshot;

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(snapshot, 0.12f); // delay drained
  tooltip.UpdateState(snapshot, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  tooltip.EnterGameplay();
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.ActiveTooltipItemDomain() == kInvalidDomainId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK_FALSE(tooltip.TooltipInitialized());
  CHECK_FALSE(tooltip.HoveredLastFrame());

  // A fresh hover still works after the reset...
  tooltip.SetHoveredSkill(200);
  tooltip.UpdateState(snapshot, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == 200);

  // ...and LeaveGameplay clears again.
  tooltip.LeaveGameplay();
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] TooltipController (UI) migration sources keep no static tooltip state") {
  // U7 group 6-B source guards: the tooltip state machine left UISystem::Draw
  // and the hotbar hover writes route through the controller now.
  const std::string uiSystemPath = "src/game/application/ui/UISystem.cpp";
  std::ifstream uiSystem(uiSystemPath);
  REQUIRE_MESSAGE(uiSystem.good(), "cannot open ", uiSystemPath);
  const std::string uiSystemContents((std::istreambuf_iterator<char>(uiSystem)),
                                     std::istreambuf_iterator<char>());
  CHECK(uiSystemContents.find("// Update Hover Targets") == std::string::npos);
  CHECK(uiSystemContents.find("tooltipHoveredLastFrame = isAnythingHovered") ==
        std::string::npos);

  const std::string hotbarPath =
      "src/game/application/ui/SkillHotbarController.cpp";
  std::ifstream hotbar(hotbarPath);
  REQUIRE_MESSAGE(hotbar.good(), "cannot open ", hotbarPath);
  const std::string hotbarContents((std::istreambuf_iterator<char>(hotbar)),
                                   std::istreambuf_iterator<char>());
  CHECK(hotbarContents.find("State.hoveredSkillSlot") == std::string::npos);
  CHECK(hotbarContents.find("State.hoveredBuffIdx") == std::string::npos);

  const std::string headerPath =
      "src/game/application/ui/TooltipController.hpp";
  std::ifstream header(headerPath);
  REQUIRE_MESSAGE(header.good(), "cannot open ", headerPath);
  const std::string headerContents((std::istreambuf_iterator<char>(header)),
                                   std::istreambuf_iterator<char>());
  CHECK(headerContents.find("static ") == std::string::npos);
}
