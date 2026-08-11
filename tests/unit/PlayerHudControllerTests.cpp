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

TEST_CASE("[Unit] PlayerHudController - Update handles empty and player registries") {
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  entt::registry registry;
  controller.Update(registry); // empty registry must not crash
  CHECK_FALSE(controller.HasPlayerData());

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<CombatStats>(player);
  registry.emplace<SwordIntentComponent>(player);
  controller.Update(registry);
  CHECK(controller.HasPlayerData());
}

TEST_CASE("[Unit] PlayerHudController - Draw executes headless without crashing") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works (see
  // GameUiHostLifecycleTests). UIRenderer::DrawTextUI falls back to raylib
  // DrawText when UISystem::State.globalFont is unset, and SwordIntentWidget
  // self-initializes (its DrawTexturePro guards invalid textures), so the
  // legacy draw body is safe to run here without UISystem::Initialize.
  UiRuntime runtime;
  PlayerHudController controller(runtime);

  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& stats = registry.emplace<CombatStats>(player);
  stats.health = 8000.0f;
  stats.max_health = 10000.0f;
  stats.mana = 2500.0f;
  stats.max_mana = 5000.0f;

  // Path A: blade-resource widget (primary path).
  registry.emplace<BladeResourceComponent>(player);
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // Path B: sword-intent fallback widget.
  registry.remove<BladeResourceComponent>(player);
  auto& intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 2;
  intent.max_stacks = 5;
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();
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
