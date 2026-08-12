#include "doctest.h"

#include "game/application/ui/PlayerHudController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"

#include "raylib.h"

#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {

TEST_CASE("[Unit] PlayerHudController - creates a non-modal HUD root node") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  const UiId root = controller.NodeId();
  CHECK(root != kInvalidUiId);

  const auto node = runtime.GetNode(root);
  REQUIRE(node.has_value());
  CHECK(node->id == root);
  CHECK(node->parent == kRootUiId);
  CHECK(node->visible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->focusable);
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->capturePointer);
  CHECK_FALSE(node->captureKeyboard);
  CHECK_FALSE(node->acceptsText);
  CHECK(node->zIndex == static_cast<std::int32_t>(UiDrawLayer::Hud));
  CHECK(runtime.NodeCount() == 2); // runtime root + HUD root

  // Full-viewport declarative anchor.
  CHECK(node->layout.kind == UiLayoutKind::Overlay);
  CHECK(node->layout.width.kind == UiLengthKind::Fraction);
  CHECK(node->layout.width.value == doctest::Approx(1.0f));
  CHECK(node->layout.height.kind == UiLengthKind::Fraction);
  CHECK(node->layout.height.value == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] PlayerHudController - SetVisible mirrors into the runtime node") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);
  CHECK(controller.IsVisible());

  controller.SetVisible(false);
  CHECK_FALSE(controller.IsVisible());
  const auto hidden = runtime.GetNode(controller.NodeId());
  REQUIRE(hidden.has_value());
  CHECK_FALSE(hidden->visible);

  controller.SetVisible(true);
  CHECK(controller.IsVisible());
  const auto shown = runtime.GetNode(controller.NodeId());
  REQUIRE(shown.has_value());
  CHECK(shown->visible);
}

TEST_CASE("[Unit] PlayerHudController - Enter/Leave gameplay resets session state") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  controller.EnterGameplay();
  controller.EnterGameplay(); // must be idempotent
  CHECK(controller.IsInGameplay());
  const auto inGame = runtime.GetNode(controller.NodeId());
  REQUIRE(inGame.has_value());
  CHECK(inGame->visible);

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.HasPlayerData());
  const auto left = runtime.GetNode(controller.NodeId());
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);

  // Re-entering gameplay restores the HUD node.
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  const auto reentered = runtime.GetNode(controller.NodeId());
  REQUIRE(reentered.has_value());
  CHECK(reentered->visible);
}

TEST_CASE("[Unit] PlayerHudController - Update handles empty and player snapshots") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  ui::GameUiSnapshot emptySnapshot;
  controller.Update(emptySnapshot, 60, 1.0f); // empty snapshot must not crash
  CHECK_FALSE(controller.HasPlayerData());

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.health = 80.0f;
  snapshot.player.maxHealth = 100.0f;
  snapshot.player.mana = 50.0f;
  snapshot.player.maxMana = 100.0f;
  snapshot.player.hasSwordIntent = true;
  snapshot.player.swordIntentStacks = 5;
  snapshot.player.swordIntentMaxStacks = 10;
  controller.Update(snapshot, 60, 1.0f);
  CHECK(controller.HasPlayerData());
}

TEST_CASE("[Unit] PlayerHudController - Paint executes headless without crashing") {
  // R5 adaptation: the controller no longer draws from the registry; it
  // formats a fixed snapshot into cached buffers (Update) and emits
  // draw-list commands (Paint). No raylib immediate-mode calls remain, so the
  // test drives the new contract directly against a UiDrawList.
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  UiDrawList drawList;
  const UiViewport viewport = UiViewport::Fit({2560, 1440});

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.health = 8000.0f;
  snapshot.player.maxHealth = 10000.0f;
  snapshot.player.mana = 2500.0f;
  snapshot.player.maxMana = 5000.0f;

  // Path A: blade-resource widget (primary path).
  snapshot.player.hasBladeResource = true;
  snapshot.player.bladeResourceKind =
      static_cast<std::uint8_t>(BladeResourceKind::SwordFlow);
  snapshot.player.bladeResourceCurrent = 5;
  snapshot.player.bladeResourceMax = 10;
  controller.Update(snapshot, 60, 1.0f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
  drawList.Clear();

  // Path B: sword-intent fallback widget.
  snapshot.player.hasBladeResource = false;
  snapshot.player.hasSwordIntent = true;
  snapshot.player.swordIntentStacks = 2;
  snapshot.player.swordIntentMaxStacks = 5;
  controller.Update(snapshot, 60, 1.0f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
}

TEST_CASE("[Unit] PlayerHudController - Paint reuses caches across revisions") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  UiDrawList drawList;
  const UiViewport viewport = UiViewport::Fit({2560, 1440});

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.health = 50.0f;
  snapshot.player.maxHealth = 100.0f;
  snapshot.player.mana = 25.0f;
  snapshot.player.maxMana = 100.0f;
  controller.Update(snapshot, 60, 1.0f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);

  // Same revision: Update must not re-format the text buffers (revision
  // cache), the command count stays identical.
  const std::size_t firstCount = drawList.CommandCount();
  drawList.Clear();
  snapshot.revision = 1; // unchanged
  controller.Update(snapshot, 60, 2.0f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() == firstCount);
}

TEST_CASE("[Unit] PlayerHudController - implementation declares no static mutable UI state") {
  std::ifstream source("src/game/application/ui/PlayerHudController.cpp");
  REQUIRE(source.is_open());
  const std::string contents{std::istreambuf_iterator<char>(source),
                             std::istreambuf_iterator<char>()};

  // Global invariant 4: no static mutable UI state. All animation is
  // time-driven; the .cpp must not declare any static data members.
  CHECK(contents.find("static bool") == std::string::npos);
  CHECK(contents.find("static float") == std::string::npos);
  CHECK(contents.find("static int") == std::string::npos);
  CHECK(contents.find("static uint32_t") == std::string::npos);
  CHECK(contents.find("static Texture2D") == std::string::npos);
  CHECK(contents.find("static Shader") == std::string::npos);
  CHECK(contents.find("static std::string") == std::string::npos);
}

} // namespace NoMoreDay::ui
