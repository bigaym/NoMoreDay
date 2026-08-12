#include "doctest.h"

#include "game/application/ui/UICharacterController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Stats.hpp"

#include "raylib.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {

TEST_CASE("[Unit] UICharacterController - creates a panel root node") {
  UiRuntime runtime;
  UICharacterController controller(runtime);

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
  CHECK(node->zIndex == static_cast<std::int32_t>(UiDrawLayer::Panels));
  CHECK(runtime.NodeCount() == 2);  // runtime root + panel root

  CHECK(node->layout.kind == UiLayoutKind::Overlay);
  CHECK(node->layout.width.kind == UiLengthKind::Fraction);
  CHECK(node->layout.width.value == doctest::Approx(1.0f));
  CHECK(node->layout.height.kind == UiLengthKind::Fraction);
  CHECK(node->layout.height.value == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] UICharacterController - SetVisible mirrors into the runtime node") {
  UiRuntime runtime;
  UICharacterController controller(runtime);
  // U8: the character panel starts closed (sessions begin with the panel
  // hidden), matching the post-EnterGameplay semantics.
  CHECK_FALSE(controller.IsVisible());

  controller.SetVisible(true);
  CHECK(controller.IsVisible());
  const auto shown = runtime.GetNode(controller.NodeId());
  REQUIRE(shown.has_value());
  CHECK(shown->visible);

  controller.SetVisible(false);
  CHECK_FALSE(controller.IsVisible());
  const auto hidden = runtime.GetNode(controller.NodeId());
  REQUIRE(hidden.has_value());
  CHECK_FALSE(hidden->visible);
}

TEST_CASE("[Unit] UICharacterController - SetAlpha drives opacity") {
  UiRuntime runtime;
  UICharacterController controller(runtime);
  // U8: the instance alpha starts at 0.0 (matching the legacy
  // State.characterPanelAlpha at startup) and is animated by Update.
  CHECK(controller.Alpha() == doctest::Approx(0.0f));

  controller.SetAlpha(0.5f);
  CHECK(controller.Alpha() == doctest::Approx(0.5f));

  controller.SetAlpha(1.0f);
  CHECK(controller.Alpha() == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] UICharacterController - Enter/Leave gameplay resets session state") {
  UiRuntime runtime;
  UICharacterController controller(runtime);

  controller.EnterGameplay();
  controller.EnterGameplay();  // must be idempotent
  CHECK(controller.IsInGameplay());
  // U8: a session starts with the character panel closed (host KEY_C opens
  // it); the instance flag is authoritative.
  CHECK_FALSE(controller.IsVisible());
  const auto inGame = runtime.GetNode(controller.NodeId());
  REQUIRE(inGame.has_value());
  CHECK_FALSE(inGame->visible);

  // The panel can still be opened mid-session through SetVisible (host KEY_C).
  controller.SetVisible(true);
  CHECK(controller.IsVisible());

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());
  const auto left = runtime.GetNode(controller.NodeId());
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);

  // Re-entering gameplay keeps the panel closed (no stale session state).
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());
  const auto reentered = runtime.GetNode(controller.NodeId());
  REQUIRE(reentered.has_value());
  CHECK_FALSE(reentered->visible);
}

TEST_CASE("[Unit] UICharacterController - Draw executes headless without crashing") {
  UiRuntime runtime;
  UICharacterController controller(runtime);

  // Empty registry with no player must early-out (no PlayerTag view hit).
  {
    entt::registry registry;
    BeginDrawing();
    controller.Draw(registry, entt::null);
    EndDrawing();
  }

  // Full panel smoke: a player with stats, drawn with an explicit entity and
  // again through the legacy PlayerTag lookup (entt::null fallback).
  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);

  auto& stats = registry.emplace<CombatStats>(player);
  stats.health = 8000.0f;
  stats.max_health = 10000.0f;
  stats.mana = 2500.0f;
  stats.max_mana = 5000.0f;
  stats.armor = 120.0f;
  stats.dodge_rating = 30.0f;
  stats.block_rating = 40.0f;
  stats.thorns = 15.0f;
  stats.pickup_range = 3.0f;

  auto& pStats = registry.emplace<PlayerStats>(player);
  pStats.level = 50;
  pStats.current_xp = 500.0f;
  pStats.required_xp = 1000.0f;
  pStats.available_attribute_points = 3;

  registry.emplace<PrimaryStats>(player);

  BeginDrawing();
  controller.Draw(registry, player);
  EndDrawing();

  BeginDrawing();
  controller.Draw(registry, entt::null);  // PlayerTag fallback path
  EndDrawing();
}

TEST_CASE("[Unit] UICharacterController - header declares no static data members") {
  const std::string path = "src/game/application/ui/UICharacterController.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class UICharacterController";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in UICharacterController: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] UICharacterController - implementation declares no static mutable state") {
  const std::string path = "src/game/application/ui/UICharacterController.cpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  const std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

  CHECK(contents.find("static bool") == std::string::npos);
  CHECK(contents.find("static float") == std::string::npos);
  CHECK(contents.find("static int") == std::string::npos);
  CHECK(contents.find("static uint32_t") == std::string::npos);
  CHECK(contents.find("static Texture2D") == std::string::npos);
  CHECK(contents.find("static Shader") == std::string::npos);
  CHECK(contents.find("static std::string") == std::string::npos);
}

} // namespace NoMoreDay::ui
