#include "doctest.h"

#include "engine/resource/ResourceManager.hpp"
#include "game/application/ui/MonsterHealthBarController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/world/LevelManager.hpp"

#include <entt/entt.hpp>

#include "raylib.h"

#include <fstream>
#include <iterator>
#include <string>

using namespace NoMoreDay;

namespace {

std::string ReadFileContents(const char* path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("[Unit] MonsterHealthBarController - creates a display-only root "
          "node") {
  ui::UiRuntime runtime;
  ui::MonsterHealthBarController controller(runtime);
  const ui::UiId nodeId = controller.NodeId();
  REQUIRE(nodeId != ui::kInvalidUiId);
  const auto node = runtime.GetNode(nodeId);
  REQUIRE(node.has_value());
  CHECK(node->parent == ui::kRootUiId);
  CHECK(node->visible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->capturePointer);
  CHECK_FALSE(node->focusable);
  CHECK(node->zIndex == static_cast<std::int32_t>(ui::UiDrawLayer::Hud));
  CHECK(runtime.NodeCount() == 2);
}

TEST_CASE("[Unit] MonsterHealthBarController - session lifecycle resets state "
          "and keeps the node alive") {
  ui::UiRuntime runtime;
  ui::MonsterHealthBarController controller(runtime);
  // Enter/Leave clears the frame-scoped hovered target; the calls are
  // idempotent and must not invalidate the registered root node.
  controller.EnterGameplay();
  controller.LeaveGameplay();
  controller.EnterGameplay();
  controller.LeaveGameplay();
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK(node->visible);
}

TEST_CASE("[Unit] MonsterHealthBarController - Render and RenderUI smoke "
          "against a real level") {
  ui::UiRuntime runtime;
  ui::MonsterHealthBarController controller(runtime);

  entt::registry registry;
  ResourceManager resourceManager;
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  // Identity camera: screen (50, 50) maps to world (50, 50).
  Camera2D camera{};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  // Damaged enemy inside the viewport: exercises the overhead bar path.
  const entt::entity enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, Position{50.0f, 50.0f});
  registry.emplace<HealthComponent>(enemy, HealthComponent{50.0f, 100.0f});

  // First frame: hover the enemy so the screen-pass target widget draws
  // (UIRenderer::DrawTextUI falls back to the default font when
  // UISystem::State.globalFont is invalid in the headless runner).
  SetMousePosition(50, 50);
  BeginDrawing();
  controller.Render(registry, camera);
  controller.RenderUI(registry);
  EndDrawing();

  // Second frame: mouse moved away; Render resets the hovered target per
  // frame so RenderUI skips the widget.
  SetMousePosition(0, 0);
  BeginDrawing();
  controller.Render(registry, camera);
  controller.RenderUI(registry);
  EndDrawing();
}

TEST_CASE("[Unit] MonsterHealthBarController - implementation declares no "
          "static mutable UI state") {
  const std::string header = ReadFileContents(
      "src/game/application/ui/MonsterHealthBarController.hpp");
  REQUIRE_FALSE(header.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static Texture2D", "static std::vector",
                             "static Color", "static Shader", "static Font",
                             "static entt::entity"}) {
    CHECK_MESSAGE(header.find(needle) == std::string::npos, needle);
  }
  const std::string source = ReadFileContents(
      "src/game/application/ui/MonsterHealthBarController.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static Texture2D", "static std::vector",
                             "static Color", "static Shader", "static Font",
                             "static entt::entity"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
}

TEST_CASE("[Unit] MonsterHealthBarController - GameplayState routes the legacy "
          "static calls behind the host fallback") {
  const std::string source = ReadFileContents(
      "src/game/application/states/GameplayState.cpp");
  REQUIRE_FALSE(source.empty());

  // The legacy static calls (Render + RenderUI) may remain only inside the
  // null-host fallback: every occurrence must be on the line right after a
  // "} else {" branch of the host route.
  int matches = 0;
  std::size_t pos = 0;
  while ((pos = source.find("MonsterHealthBarSystem::Render", pos)) !=
         std::string::npos) {
    ++matches;
    const std::size_t prevElse = source.rfind("} else {", pos);
    CHECK_MESSAGE(prevElse != std::string::npos,
                  "legacy call must be the null-host fallback (else branch)");
    if (prevElse != std::string::npos) {
      // The call must sit inside that else body: no "}" may appear between
      // the "} else {" token (skipped) and the legacy call.
      const std::string between =
          source.substr(prevElse + 8, pos - prevElse - 8);
      CHECK_MESSAGE(between.find('}') == std::string::npos,
                    "legacy call must be the null-host fallback (else branch)");
    }
    pos += 29;
  }
  REQUIRE(matches >= 2);
}
