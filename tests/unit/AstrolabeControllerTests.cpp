#include "doctest.h"

#include "game/application/ui/AstrolabeController.hpp"
#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Common.hpp"

#include <fstream>
#include <iterator>
#include <string>

#include <entt/entt.hpp>

using NoMoreDay::ui::AstrolabeController;
using NoMoreDay::ui::SkillTreeController;
using NoMoreDay::ui::UiRuntime;

TEST_CASE("[Unit] AstrolabeController registers a hidden full-screen node") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  const auto node = runtime.GetNode(entt::hashed_string("ui_astrolabe"));
  REQUIRE(node.has_value());
  CHECK(node->parent == 1);
  CHECK(node->zIndex ==
        static_cast<std::int32_t>(NoMoreDay::ui::UiDrawLayer::Panels));
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->visible);
  CHECK(controller.NodeId() == entt::hashed_string("ui_astrolabe"));
  CHECK_FALSE(controller.IsVisible(registry, entt::null));
  CHECK_FALSE(controller.IsInGameplay());
}

TEST_CASE("[Unit] AstrolabeController Toggle flips visibility and mirrors siblings") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  // Pre-set legacy sibling state: opening the astrolabe must clear it exactly
  // like the old KEY_N handler did.
  UISystem::State.showInventory = true;
  UISystem::State.showCharacterPanel = true;
  UISystem::State.showContextMenu = true;
  UISystem::State.showSkillTree = true;

  controller.Toggle(registry, entt::null);
  CHECK(controller.IsVisible(registry, entt::null));
  CHECK_FALSE(UISystem::State.showInventory);
  CHECK_FALSE(UISystem::State.showCharacterPanel);
  CHECK_FALSE(UISystem::State.showContextMenu);
  CHECK_FALSE(UISystem::State.showSkillTree);
  const auto openNode = runtime.GetNode(controller.NodeId());
  REQUIRE(openNode.has_value());
  CHECK(openNode->visible);

  controller.Toggle(registry, entt::null);
  CHECK_FALSE(controller.IsVisible(registry, entt::null));
  const auto closedNode = runtime.GetNode(controller.NodeId());
  REQUIRE(closedNode.has_value());
  CHECK_FALSE(closedNode->visible);
}

TEST_CASE("[Unit] AstrolabeController Close hides the panel") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  controller.Toggle(registry, entt::null);
  CHECK(controller.IsVisible(registry, entt::null));

  controller.Close();
  CHECK_FALSE(controller.IsVisible(registry, entt::null));
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK_FALSE(node->visible);
}

TEST_CASE("[Unit] AstrolabeController Enter/LeaveGameplay reset session state") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible(registry, entt::null));

  controller.Toggle(registry, entt::null);
  CHECK(controller.IsVisible(registry, entt::null));

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible(registry, entt::null));
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK_FALSE(node->visible);
}

TEST_CASE("[Unit] AstrolabeController Capture/RestoreVisibilityState round-trip") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  controller.Toggle(registry, entt::null);
  const auto captured = controller.CaptureVisibilityState();
  CHECK(captured.visible);
  CHECK(captured.alpha == doctest::Approx(0.0f));

  // Mutate state, then restore: the fade behavior must be exactly as captured.
  controller.Hide();
  CHECK_FALSE(controller.CaptureVisibilityState().visible);
  controller.RestoreVisibilityState(captured);
  const auto restored = controller.CaptureVisibilityState();
  CHECK(restored.visible == captured.visible);
  CHECK(restored.alpha == doctest::Approx(captured.alpha));
}

TEST_CASE("[Unit] AstrolabeController Draw/Update are headless-safe early exits") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  entt::registry registry;

  // Hidden panel: Update and Draw are no-ops (no GL work).
  controller.Update(registry);
  controller.Draw(registry);

  // Visible panel without any player entity: Draw still early-outs before
  // touching the renderer. Opening loads the data/renderer headless-safely
  // (AssetLoadingSystem is uninitialized in tests and returns zero shaders).
  controller.Toggle(registry, entt::null);
  CHECK(controller.IsVisible(registry, entt::null));
  controller.Draw(registry);

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  controller.Close();
}

TEST_CASE("[Unit] SkillTreeController Toggle closes astrolabe via SharedContext") {
  UiRuntime runtime;
  SkillTreeController controller(runtime);
  entt::registry registry;
  NoMoreDay::SharedContext shared;
  bool astrolabeClosed = false;
  shared.closeAstrolabe = [&astrolabeClosed]() { astrolabeClosed = true; };
  registry.ctx().emplace<NoMoreDay::SharedContext *>(&shared);

  // Opening the skill tree must invoke the host astrolabe close callback
  // (replaces the legacy static UIAstrolabe::Toggle coupling).
  controller.Toggle(registry);
  CHECK(astrolabeClosed);

  astrolabeClosed = false;
  controller.Toggle(registry); // close: no coupling callback expected
  CHECK_FALSE(astrolabeClosed);
}

TEST_CASE("[Unit] Astrolabe migration sources keep no static panel state") {
  // U7 group 5 source guards: the legacy static UIAstrolabe facade is gone
  // from the routing layer and the controller keeps no static mutable state.
  const std::string uiSystemPath = "src/game/application/ui/UISystem.cpp";
  std::ifstream uiSystem(uiSystemPath);
  REQUIRE_MESSAGE(uiSystem.good(), "cannot open ", uiSystemPath);
  const std::string uiSystemContents((std::istreambuf_iterator<char>(uiSystem)),
                                     std::istreambuf_iterator<char>());
  CHECK(uiSystemContents.find("UIAstrolabe") == std::string::npos);

  const std::string hostPath = "src/game/application/ui/GameUiHost.cpp";
  std::ifstream host(hostPath);
  REQUIRE_MESSAGE(host.good(), "cannot open ", hostPath);
  const std::string hostContents((std::istreambuf_iterator<char>(host)),
                                 std::istreambuf_iterator<char>());
  CHECK(hostContents.find("UIAstrolabe") == std::string::npos);

  const std::string headerPath = "src/game/application/ui/AstrolabeController.hpp";
  std::ifstream header(headerPath);
  REQUIRE_MESSAGE(header.good(), "cannot open ", headerPath);
  const std::string headerContents((std::istreambuf_iterator<char>(header)),
                                   std::istreambuf_iterator<char>());
  // The only allowed static is the constexpr vow hold duration.
  CHECK(headerContents.find("static bool") == std::string::npos);
  CHECK(headerContents.find("static float") == std::string::npos);
  CHECK(headerContents.find("static std::string") == std::string::npos);
  CHECK(headerContents.find("static AstrolabeView") == std::string::npos);
  CHECK(headerContents.find("static ProfessionID") == std::string::npos);
}
