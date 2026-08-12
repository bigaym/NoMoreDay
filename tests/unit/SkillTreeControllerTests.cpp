#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"

#include <fstream>
#include <string>

#include <entt/entt.hpp>

using NoMoreDay::ui::GameUiSnapshot;
using NoMoreDay::ui::SkillTreeController;
using NoMoreDay::ui::UiDrawList;
using NoMoreDay::ui::UiRuntime;

TEST_CASE("[Unit] SkillTreeController registers a hidden full-screen node") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);

  const auto node = runtime.GetNode(entt::hashed_string("ui_skill_tree"));
  REQUIRE(node.has_value());
  CHECK(node->parent == 1);
  CHECK(node->zIndex ==
        static_cast<std::int32_t>(NoMoreDay::ui::UiDrawLayer::Panels));
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->visible);
  CHECK(controller.NodeId() == entt::hashed_string("ui_skill_tree"));
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(controller.IsInGameplay());
}

TEST_CASE("[Unit] SkillTreeController Toggle closes sibling panels through the "
          "host") {
  UiRuntime runtime;
  NoMoreDay::ui::GameUiHost host;
  SkillTreeController controller(runtime, nullptr, &host);
  entt::registry registry;

  // Pre-open the sibling panels on the host: Toggle must close them exactly
  // like the old KEY_S handler did (the legacy UISystem::State writes are
  // gone; sibling state now lives on the host instance).
  host.SetInventoryVisible(true);
  host.SetCharacterPanelVisible(true);
  host.OpenContextMenu(entt::null, false, 0, NoMoreDay::EquipmentSlot::None);

  controller.Toggle();
  CHECK(controller.IsVisible());
  CHECK_FALSE(host.IsInventoryVisible());
  CHECK_FALSE(host.IsCharacterPanelVisible());
  // The context menu is hosted by the overlay; the host forwards
  // CloseContextMenu, so it is closed as well.
  const auto openNode = runtime.GetNode(controller.NodeId());
  REQUIRE(openNode.has_value());
  CHECK(openNode->visible);

  controller.Toggle();
  CHECK_FALSE(controller.IsVisible());
  CHECK(controller.SelectedSkillId() == NoMoreDay::INVALID_SKILL_ID);
  const auto closedNode = runtime.GetNode(controller.NodeId());
  REQUIRE(closedNode.has_value());
  CHECK_FALSE(closedNode->visible);
}

TEST_CASE("[Unit] SkillTreeController Close and session reset") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;

  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  controller.Toggle();
  CHECK(controller.IsVisible());

  controller.Close();
  CHECK_FALSE(controller.IsVisible());

  controller.Toggle();
  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(controller.IsInGameplay());
}

TEST_CASE("[Unit] SkillTreeController UpdateAlpha drives the instance alpha") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;

  controller.Toggle();
  controller.UpdateAlpha(1.0f / 6.0f);
  CHECK(controller.IsVisible());
  // alpha = 0 + dt * 6.0f with dt = 1/6 -> 1.0, clamped.
  CHECK(controller.Alpha() == doctest::Approx(1.0f));

  controller.Toggle();
  controller.UpdateAlpha(1.0f);
  CHECK(controller.Alpha() == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillTreeController Update/Paint are headless-safe") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  UiDrawList drawList;
  NoMoreDay::ui::UiViewport viewport =
      NoMoreDay::ui::UiViewport::Fit({1280.0f, 720.0f});
  GameUiSnapshot snapshot;
  NoMoreDay::ui::UiInputFrame input;
  input.deltaSeconds = 0.016f;
  input.tooltipTarget = NoMoreDay::ui::kInvalidUiId;

  // Panel hidden: must be a no-op.
  controller.Update(snapshot, input);
  controller.Paint(drawList, viewport, snapshot);

  controller.EnterGameplay();
  controller.Toggle();
  controller.UpdateAlpha(1.0f);

  // Visible hub stage: Update + Paint run against the (empty) snapshot
  // headless-safely (the hub early-outs without host/data).
  controller.Update(snapshot, input);
  controller.Paint(drawList, viewport, snapshot);
}

TEST_CASE("[Unit] UISystem no longer calls legacy skill panels statically") {
  const std::string path = "src/game/application/ui/UISystem.cpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  const std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

  CHECK(contents.find("UISkillHub::Draw(") == std::string::npos);
  CHECK(contents.find("SkillTreeUI::Draw(") == std::string::npos);
  CHECK(contents.find("IsSkillTreeVisible") == std::string::npos);
}
