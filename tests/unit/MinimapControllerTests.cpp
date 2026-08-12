#include "doctest.h"

#include "engine/resource/ResourceManager.hpp"
#include "game/application/ui/MinimapController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
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

TEST_CASE("[Unit] MinimapController - creates a display-only minimap root node") {
  ui::UiRuntime runtime;
  ui::MinimapController controller(runtime);
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

TEST_CASE("[Unit] MinimapController - ToggleDebugReveal flips the debug flag") {
  ui::UiRuntime runtime;
  ui::MinimapController controller(runtime);
  CHECK_FALSE(controller.DebugRevealEnabled());
  controller.ToggleDebugReveal();
  CHECK(controller.DebugRevealEnabled());
  controller.ToggleDebugReveal();
  CHECK_FALSE(controller.DebugRevealEnabled());
}

TEST_CASE("[Unit] MinimapController - session lifecycle keeps the node alive") {
  ui::UiRuntime runtime;
  ui::MinimapController controller(runtime);
  controller.EnterGameplay();
  controller.LeaveGameplay();
  controller.EnterGameplay();
  controller.LeaveGameplay();
  const auto node = runtime.GetNode(controller.NodeId());
  REQUIRE(node.has_value());
  CHECK(node->visible);
}

TEST_CASE("[Unit] MinimapController - Update and Paint render against a real level") {
  ui::UiRuntime runtime;
  ui::MinimapController controller(runtime);

  entt::registry registry;
  ResourceManager resourceManager;
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.hasWorldPosition = true;
  snapshot.player.worldX = 100.0f;
  snapshot.player.worldY = 100.0f;
  snapshot.minimap.currentMapKills = 10;
  snapshot.minimap.killRequirement = 100;

  ui::UiDrawList drawList;
  const ui::UiViewport viewport = ui::UiViewport::Fit({2560, 1440});

  // First update creates the texture lazily and uploads the partial buffer.
  controller.Update(snapshot, levelManager, nullptr, 0.2f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
  drawList.Clear();

  // Second update exercises the non-refresh path (timer below interval).
  controller.Update(snapshot, levelManager, nullptr, 0.01f);
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);

  // Shutdown is idempotent and safe to call twice.
  controller.Shutdown();
  controller.Shutdown();
}

TEST_CASE("[Unit] MinimapController - implementation declares no static "
          "mutable UI state") {
  const std::string source = ReadFileContents(
      "src/game/application/ui/MinimapController.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static Texture2D", "static std::vector",
                             "static Color", "static Shader", "static Font"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
}
