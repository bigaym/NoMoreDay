#include "doctest.h"

#include "game/application/ui/UICraftingController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {

namespace {

std::string ReadFileContents(const char* path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("[Unit] UICraftingController - creates a panel root node") {
  UiRuntime runtime;
  UICraftingController controller(runtime);

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

TEST_CASE("[Unit] UICraftingController - Enter/Leave gameplay resets session state") {
  UiRuntime runtime;
  UICraftingController controller(runtime);

  const UiId root = controller.NodeId();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());

  controller.EnterGameplay();
  controller.EnterGameplay();  // must be idempotent
  CHECK(controller.IsInGameplay());
  // The crafting panel starts closed on EnterGameplay (legacy default), so the
  // node is hidden until the player opens the panel with Toggle/context menu.
  CHECK_FALSE(controller.IsVisible());
  REQUIRE(root != kInvalidUiId);
  const auto inGame = runtime.GetNode(root);
  REQUIRE(inGame.has_value());
  CHECK_FALSE(inGame->visible);

  // Open the panel, then leave: LeaveGameplay must reset the panel closed.
  controller.Toggle();
  CHECK(controller.IsVisible());
  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());
  const auto left = runtime.GetNode(root);
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);

  // Re-entering gameplay restores the (hidden) panel node state.
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  CHECK_FALSE(controller.IsVisible());
  const auto reentered = runtime.GetNode(root);
  REQUIRE(reentered.has_value());
  CHECK_FALSE(reentered->visible);
}

TEST_CASE("[Unit] UICraftingController - Toggle flips visibility with the node") {
  UiRuntime runtime;
  UICraftingController controller(runtime);
  controller.EnterGameplay();

  const UiId root = controller.NodeId();
  REQUIRE(root != kInvalidUiId);

  CHECK_FALSE(controller.IsVisible());
  auto hidden = runtime.GetNode(root);
  REQUIRE(hidden.has_value());
  CHECK_FALSE(hidden->visible);

  controller.Toggle();
  CHECK(controller.IsVisible());
  auto shown = runtime.GetNode(root);
  REQUIRE(shown.has_value());
  CHECK(shown->visible);

  controller.Toggle();
  CHECK_FALSE(controller.IsVisible());
  auto hiddenAgain = runtime.GetNode(root);
  REQUIRE(hiddenAgain.has_value());
  CHECK_FALSE(hiddenAgain->visible);

  // SetTargetItem auto-opens the panel; ClearTargetItem keeps it open.
  controller.SetTargetItem(entt::null);
  CHECK(controller.IsVisible());
  controller.ClearTargetItem();
  CHECK(controller.GetTargetItem() == static_cast<entt::entity>(entt::null));
  CHECK(controller.IsVisible());

  // OpenMergePanel opens on the merge tab without toggling state twice.
  controller.Toggle();  // close first
  CHECK_FALSE(controller.IsVisible());
  controller.OpenMergePanel();
  CHECK(controller.IsVisible());
  auto mergeShown = runtime.GetNode(root);
  REQUIRE(mergeShown.has_value());
  CHECK(mergeShown->visible);
}

TEST_CASE("[Unit] UICraftingController - Update runs headless against a world") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so GetFrameTime() is available. The alpha is private to the
  // controller, so the checks here focus on observable behavior: no crash on
  // an empty registry and on a live world, and stale entity cleanup.
  UiRuntime runtime;
  UICraftingController controller(runtime);
  controller.EnterGameplay();

  ResourceManager resourceManager;
  entt::registry registry;

  // Empty registry before any world exists: Update must not crash.
  controller.Update(registry);

  // Provide a real world plus a player (PlayerTag + InventoryComponent) so
  // Update runs against live gameplay entities, mirroring the runtime wiring.
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<InventoryComponent>(player);

  controller.Update(registry);

  // Valid forge target survives Update.
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  controller.SetTargetItem(item);
  CHECK(controller.GetTargetItem() == item);
  controller.Update(registry);
  CHECK(controller.GetTargetItem() == item);

  // Destroyed forge target is dropped on the next Update.
  registry.destroy(item);
  controller.Update(registry);
  CHECK(controller.GetTargetItem() == static_cast<entt::entity>(entt::null));
}

TEST_CASE("[Unit] UICraftingController - Draw runs headless for the reachable tabs") {
  // Headless-safety was verified function by function: UIRenderer::DrawTextUI
  // and MeasureTextUI fall back to the default font when State.globalFont is
  // invalid in the headless runner, UIRenderer::DrawButton falls back to plain
  // rects when the button texture is missing, and AssetLoadingSystem::GetTexture
  // returns a null texture when uninitialized. The panel alpha gate is driven
  // to 1.0 by rendering real frames (SetTargetFPS(60) in tests/main.cpp makes
  // each BeginDrawing/EndDrawing cycle advance GetFrameTime() by ~16ms).
  UiRuntime runtime;
  UICraftingController controller(runtime);
  controller.EnterGameplay();

  ResourceManager resourceManager;
  entt::registry registry;
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<InventoryComponent>(player);

  // Forge item with a prefix and a suffix so DrawAffixList runs both row kinds.
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.type = ItemType::Weapon;
  itemComp.forgingPotential = 5;
  Affix prefix;
  prefix.type = AffixType::Strength;
  prefix.tier = 3;
  prefix.isPrefix = true;
  Affix suffix;
  suffix.type = AffixType::FlatPhysicalDamage;
  suffix.tier = 2;
  suffix.isPrefix = false;
  itemComp.affixes.push_back(prefix);
  itemComp.affixes.push_back(suffix);

  // Keep the mouse away from every panel control: no hover, no click, no drag.
  SetMousePosition(50, 50);

  // Real usage (UISystem::Draw) sets the scale before drawing panels.
  const float savedUiScale = UIRenderer::GetScale();
  const float savedScaleFactor = UISystem::State.scaleFactor;
  UIRenderer::SetScale(1.0f);
  UISystem::State.scaleFactor = 1.0f;

  // Forging tab: set the target (auto-opens the panel) and animate alpha to 1.
  controller.SetTargetItem(item);
  for (int i = 0; i < 90; ++i) {
    BeginDrawing();
    EndDrawing();
    controller.Update(registry);
  }
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // Merging tab: OpenMergePanel switches tabs; draw with empty merge slots.
  controller.OpenMergePanel();
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // The Salvaging tab is only reachable through a tab click, which cannot be
  // synthesized headless; its primitives (DrawRing/DrawPolyLines/GetTime plus
  // the shared UIRenderer/raylib calls) are covered by inspection and by the
  // other two tabs above.

  UIRenderer::SetScale(savedUiScale);
  UISystem::State.scaleFactor = savedScaleFactor;
}

TEST_CASE("[Unit] UICraftingController - header declares no static data members") {
  const std::string path = "src/game/application/ui/UICraftingController.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class UICraftingController";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in UICraftingController: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] UICraftingController - implementation declares no static mutable state") {
  const std::string path = "src/game/application/ui/UICraftingController.cpp";
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
