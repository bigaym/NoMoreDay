#include "doctest.h"

#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

#include <entt/entt.hpp>

#include <fstream>
#include <iterator>
#include <string>

using namespace NoMoreDay;

namespace {

// Ground-item hover lives in the legacy UiShared singleton and the direct
// skill-id hover in the UISystem::State mirror (skill hub / talent tree
// writes); restore both so each case starts from a clean slate.
void ResetLegacyHover() {
  UiShared::HoveredItem() = entt::null;
  UISystem::State.hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
}

// Player with the default empty hotbar slots; a slot can be assigned by
// writing ActiveSkillsComponent::slots[i].id.
entt::entity MakePlayer(entt::registry &registry) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<ActiveSkillsComponent>(player);
  return player;
}

} // namespace

TEST_CASE("[Unit] TooltipController (UI) - hover starts: delay decays then alpha rises") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();

  const uint32_t skillId = 100;
  tooltip.SetHoveredSkill(skillId);
  tooltip.UpdateState(registry, 0.1f);

  // New target while the alpha is still zero -> the long initial delay.
  CHECK(tooltip.ActiveTooltipSkillId() == skillId);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.02f)); // 0.12 - 0.10
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK_FALSE(tooltip.TooltipInitialized());

  // Enough hover to drain the delay.
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));

  // Delay is zero: alpha rises at 10/s.
  tooltip.UpdateState(registry, 0.05f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.5f));

  // Alpha clamps at 1.
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));
  CHECK(tooltip.HoveredLastFrame());
}

TEST_CASE("[Unit] TooltipController (UI) - target switch resets the delay") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(registry, 0.12f); // delay drained
  tooltip.UpdateState(registry, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // Switching the hovered skill while the tooltip is visible re-arms the
  // short re-entry delay and unlocks the position for the new target.
  tooltip.SetHoveredSkill(200);
  tooltip.UpdateState(registry, 0.0f);
  CHECK(tooltip.ActiveTooltipSkillId() == 200);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.05f)); // alpha > 0.01
  CHECK_FALSE(tooltip.TooltipInitialized());
}

TEST_CASE("[Unit] TooltipController (UI) - no hover: alpha decays and active clears") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(registry, 0.12f); // delay drained
  tooltip.UpdateState(registry, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // Mouse leaves: the next frame arms the 0.08s exit delay...
  tooltip.ResetFrame();
  tooltip.UpdateState(registry, 0.0f);
  CHECK_FALSE(tooltip.HoveredLastFrame());
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.08f));
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // ...the exit delay drains...
  tooltip.UpdateState(registry, 0.08f);
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  // ...then the alpha fades at 8/s.
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.2f));

  // Alpha hits zero: the active tooltip is cleared.
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK_FALSE(tooltip.TooltipInitialized());
}

TEST_CASE("[Unit] TooltipController (UI) - hover priority: item > skillId > slot > buff") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();
  const entt::entity player = MakePlayer(registry);
  auto &active = registry.get<ActiveSkillsComponent>(player);
  const uint32_t slotSkillId = 300;
  active.slots[2].id = slotSkillId; // hotbar slot 2 resolves to skill 300
  const entt::entity item = registry.create();

  // 1. Item hover wins over every other hover input.
  tooltip.SetHoveredBuff(1);
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredSkill(200);
  tooltip.SetHoveredItem(item);
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.ActiveTooltipItem() == item);
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 2. Direct skill-id hover wins over the hotbar slot.
  tooltip.ResetFrame();
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredSkill(200);
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == 200);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 3. Hotbar slot hover resolves to the assigned skill id at state-machine
  //    time (ActiveSkillsComponent::slots[slot].id).
  tooltip.ResetFrame();
  tooltip.SetHoveredSkillSlot(2);
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == slotSkillId);
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  // 4. Buff index wins when nothing else is hovered.
  tooltip.ResetFrame();
  tooltip.SetHoveredBuff(1);
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.ActiveTooltipBuffIdx() == 1);
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
}

TEST_CASE("[Unit] TooltipController (UI) - adopts the skill-hub hover from the State mirror") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();

  // The skill hub / talent tree still write UISystem::State.hoveredSkillId
  // during their draw; UpdateState adopts it as the direct skill-id source.
  UISystem::State.hoveredSkillId = 400;
  tooltip.UpdateState(registry, 0.1f);
  CHECK(tooltip.ActiveTooltipSkillId() == 400);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);

  UISystem::State.hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
}

TEST_CASE("[Unit] TooltipController (UI) - Enter/LeaveGameplay clear all state") {
  entt::registry registry;
  ui::TooltipController tooltip;
  ResetLegacyHover();

  tooltip.SetHoveredSkill(100);
  tooltip.UpdateState(registry, 0.12f); // delay drained
  tooltip.UpdateState(registry, 0.12f); // alpha rises to 1
  CHECK(tooltip.Alpha() == doctest::Approx(1.0f));

  tooltip.EnterGameplay();
  CHECK(tooltip.ActiveTooltipSkillId() == NoMoreDay::INVALID_SKILL_ID);
  CHECK((tooltip.ActiveTooltipItem() == entt::null));
  CHECK(tooltip.ActiveTooltipBuffIdx() == -1);
  CHECK(tooltip.Alpha() == doctest::Approx(0.0f));
  CHECK(tooltip.DelayTimer() == doctest::Approx(0.0f));
  CHECK_FALSE(tooltip.TooltipInitialized());
  CHECK_FALSE(tooltip.HoveredLastFrame());
  // The mirror is cleared for the legacy readers too.
  CHECK(UISystem::State.activeTooltipSkillId == NoMoreDay::INVALID_SKILL_ID);
  CHECK((UISystem::State.activeTooltipItem == entt::null));
  CHECK(UISystem::State.activeTooltipBuffIdx == -1);
  CHECK(UISystem::State.tooltipAlpha == doctest::Approx(0.0f));
  CHECK_FALSE(UISystem::State.tooltipInitialized);
  CHECK_FALSE(UISystem::State.tooltipHoveredLastFrame);

  // A fresh hover still works after the reset...
  tooltip.SetHoveredSkill(200);
  tooltip.UpdateState(registry, 0.1f);
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


