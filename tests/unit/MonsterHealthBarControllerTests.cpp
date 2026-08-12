#include "doctest.h"

#include "game/application/ui/MonsterHealthBarController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"

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

TEST_CASE("[Unit] MonsterHealthBarController - Update and Paint smoke against "
          "a fixed snapshot") {
  // R5 adaptation: Render/RenderUI(registry) are gone; the controller now
  // takes a frame snapshot + plain camera data (Update) and emits draw-list
  // commands (Paint). The test drives the new contract directly.
  ui::UiRuntime runtime;
  ui::MonsterHealthBarController controller(runtime);

  // Identity camera: screen (50, 50) maps to world (50, 50) with zoom 1,
  // target (0,0) and offset (0,0); the default logical viewport is 2560x1440.
  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.worldX = 0.0f;
  snapshot.player.worldY = 0.0f;

  // Damaged enemy inside the viewport: exercises the overhead bar path.
  ui::GameUiMonsterHealthView enemy;
  enemy.domainId = 7;
  enemy.current = 50.0f;
  enemy.max = 100.0f;
  enemy.worldX = 50.0f;
  enemy.worldY = 50.0f;
  snapshot.monsters.push_back(enemy);

  ui::UiDrawList drawList;
  const ui::UiViewport viewport = ui::UiViewport::Fit({2560, 1440});

  // First frame: hover the enemy so the screen-pass target widget paints.
  controller.Update(snapshot, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                    50.0f, 50.0f, 2560, 1440);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
  drawList.Clear();

  // Second frame: mouse moved away; the hovered target resets so only the
  // overhead bars paint (no target widget).
  controller.Update(snapshot, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                    0.0f, 0.0f, 2560, 1440);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
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

TEST_CASE("[Unit] MonsterHealthBarController - GameplayState routes the "
          "controller directly (U8)") {
  const std::string source = ReadFileContents(
      "src/game/application/states/GameplayState.cpp");
  REQUIRE_FALSE(source.empty());

  // U8 final: the null-host fallback branch is gone, so the legacy static
  // calls must not appear anywhere; the hosted controller is called directly.
  CHECK(source.find("MonsterHealthBarSystem::Render") == std::string::npos);
  CHECK(source.find("m_uiHost->RenderMonsterHealthBars") != std::string::npos);
}
