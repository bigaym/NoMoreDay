#include "doctest.h"

#include "game/application/ui/AstrolabeController.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"

#include <fstream>
#include <iterator>
#include <string>

#include <entt/entt.hpp>

using NoMoreDay::ui::AstrolabeController;
using NoMoreDay::ui::GameUiSnapshot;
using NoMoreDay::ui::SkillTreeController;
using NoMoreDay::ui::UiDrawList;
using NoMoreDay::ui::UiInputFrame;
using NoMoreDay::ui::UiRuntime;
using NoMoreDay::ui::UiViewport;

namespace {

// R8: a frame-scoped empty snapshot + input for headless interaction smokes
// (the controllers early-exit on hidden panels / empty data before touching
// gameplay or the renderer).
UiInputFrame MakeEmptyInput() {
  UiInputFrame input;
  input.deltaSeconds = 0.016f;
  input.tooltipTarget = NoMoreDay::ui::kInvalidUiId;
  return input;
}

UiViewport MakeViewport() {
  return UiViewport::Fit({1280.0f, 720.0f});
}

} // namespace

TEST_CASE("[Unit] AstrolabeController registers a hidden full-screen node") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);

  const auto node = runtime.GetNode(entt::hashed_string("ui_astrolabe"));
  REQUIRE(node.has_value());
  CHECK(node->parent == 1);
  CHECK(node->zIndex ==
        static_cast<std::int32_t>(NoMoreDay::ui::UiDrawLayer::Panels));
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->visible);
  CHECK(controller.NodeId() == entt::hashed_string("ui_astrolabe"));
  CHECK_FALSE(controller.IsVisible());
  CHECK_FALSE(controller.IsInGameplay());
}

TEST_CASE("[Unit] AstrolabeController Toggle flips visibility") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);

  // R8: the sibling-panel close coupling moved to the host KEY_N handler (the
  // host owns the sibling controllers; the panel controller is registry-free
  // and only flips its own visibility), so this test now asserts the panel
  // flip only.
  CHECK_FALSE(controller.IsVisible());

  controller.Toggle();
  CHECK(controller.IsVisible());
  const auto openNode = runtime.GetNode(controller.NodeId());
  REQUIRE(openNode.has_value());
  CHECK(openNode->visible);

  controller.Toggle();
  CHECK_FALSE(controller.IsVisible());
  const auto closedNode = runtime.GetNode(controller.NodeId());
  REQUIRE(closedNode.has_value());
  CHECK_FALSE(closedNode->visible);
}

TEST_CASE("[Unit] AstrolabeController Close hides the panel") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);

  controller.Toggle();
  CHECK(controller.IsVisible());

  controller.Close();
  CHECK_FALSE(controller.IsVisible());
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK_FALSE(node->visible);
}

TEST_CASE("[Unit] AstrolabeController Enter/LeaveGameplay reset session state") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);

  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());

  controller.Toggle();
  CHECK(controller.IsVisible());

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK_FALSE(node->visible);
}

TEST_CASE("[Unit] AstrolabeController Capture/RestoreVisibilityState round-trip") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);

  controller.Toggle();
  const auto captured = controller.CaptureVisibilityState();
  CHECK(captured.visible);
  CHECK(captured.alpha == doctest::Approx(0.0f));

  // Mutate state, then restore: the fade behavior must be exactly as captured.
  controller.Hide();
  CHECK_FALSE(controller.IsVisible());
  controller.RestoreVisibilityState(captured);
  const auto restored = controller.CaptureVisibilityState();
  CHECK(restored.visible == captured.visible);
  CHECK(restored.alpha == doctest::Approx(captured.alpha));
}

TEST_CASE("[Unit] AstrolabeController Update/Paint are headless-safe early exits") {
  UiRuntime runtime;
  AstrolabeController controller(runtime);
  UiDrawList drawList;
  UiViewport viewport = MakeViewport();

  // Hidden panel: Update and Paint are no-ops (no GL work).
  GameUiSnapshot snapshot;
  controller.Update(snapshot, MakeEmptyInput());
  controller.Paint(drawList, viewport);

  // Visible panel without loaded data: Update still early-outs before touching
  // the renderer. Opening loads the data/renderer headless-safely
  // (AssetLoadingSystem is uninitialized in tests and returns zero shaders).
  controller.Toggle();
  CHECK(controller.IsVisible());
  controller.Update(snapshot, MakeEmptyInput());
  controller.Paint(drawList, viewport);

  controller.Close();
}

TEST_CASE("[Unit] SkillTreeController Toggle closes astrolabe via host channel") {
  // R8: the legacy SharedContext closeAstrolabe callback is gone; the
  // SkillTreeController Toggle routes the sibling close through the host
  // channel (m_uiHost->CloseAstrolabe()). The test drives the host seam:
  // ShowAstrolabe() opens the hosted astrolabe, then the skill-tree toggle
  // must close it.
  UiRuntime runtime;
  NoMoreDay::ui::GameUiHost host;
  SkillTreeController controller(runtime, nullptr, &host);

  host.ShowAstrolabe();
  CHECK(host.IsAstrolabeVisible());

  // Opening the skill tree closes the hosted astrolabe (replaces the legacy
  // SharedContext closeAstrolabe coupling).
  controller.Toggle();
  CHECK_FALSE(host.IsAstrolabeVisible());

  controller.Toggle(); // close: no coupling callback expected
  CHECK_FALSE(host.IsAstrolabeVisible());
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
