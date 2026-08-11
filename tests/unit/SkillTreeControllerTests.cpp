#include "doctest.h"

#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"

#include <fstream>
#include <string>

#include <entt/entt.hpp>

using NoMoreDay::ui::SkillTreeController;
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

TEST_CASE("[Unit] SkillTreeController Toggle mirrors legacy sibling state") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;

  // Pre-set legacy sibling state: Toggle must clear it exactly like the old
  // KEY_S handler did.
  UISystem::State.showInventory = true;
  UISystem::State.showCharacterPanel = true;
  UISystem::State.showContextMenu = true;
  UISystem::State.showSkillTree = false;
  UISystem::State.selectedSkillId = 7;

  controller.Toggle(registry);
  CHECK(controller.IsVisible());
  CHECK(UISystem::State.showSkillTree);
  CHECK_FALSE(UISystem::State.showInventory);
  CHECK_FALSE(UISystem::State.showCharacterPanel);
  CHECK_FALSE(UISystem::State.showContextMenu);
  const auto openNode = runtime.GetNode(controller.NodeId());
  REQUIRE(openNode.has_value());
  CHECK(openNode->visible);

  controller.Toggle(registry);
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(UISystem::State.showSkillTree);
  CHECK(UISystem::State.selectedSkillId == NoMoreDay::INVALID_SKILL_ID);
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
  controller.Toggle(registry);
  CHECK(controller.IsVisible());

  controller.Close();
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(UISystem::State.showSkillTree);

  controller.Toggle(registry);
  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(UISystem::State.showSkillTree);
}

TEST_CASE("[Unit] SkillTreeController UpdateAlpha mirrors to State") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;

  controller.Toggle(registry);
  controller.UpdateAlpha(1.0f / 6.0f);
  CHECK(controller.IsVisible());
  // alpha = 0 + dt * 6.0f with dt = 1/6 -> 1.0, clamped.
  CHECK(UISystem::State.skillTreeAlpha == doctest::Approx(1.0f));

  controller.Toggle(registry);
  controller.UpdateAlpha(1.0f);
  CHECK(UISystem::State.skillTreeAlpha == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillTreeController Draw is headless-safe") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;

  // No player / panel hidden: must be a no-op.
  controller.Draw(registry, entt::null);

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerStats>(player);

  controller.EnterGameplay();
  controller.Toggle(registry);
  controller.UpdateAlpha(1.0f);

  BeginDrawing();
  controller.Draw(registry, player);
  EndDrawing();
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
