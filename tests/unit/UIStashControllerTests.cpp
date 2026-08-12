#include "doctest.h"

#include "game/application/ui/UIStashController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/StashComponent.hpp"
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

TEST_CASE("[Unit] UIStashController - creates a panel root node") {
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);

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

TEST_CASE("[Unit] UIStashController - Enter/Leave gameplay resets session state") {
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);

  const UiId root = controller.NodeId();
  CHECK_FALSE(controller.IsInGameplay());

  controller.EnterGameplay();
  controller.EnterGameplay();  // must be idempotent
  CHECK(controller.IsInGameplay());
  REQUIRE(root != kInvalidUiId);
  const auto inGame = runtime.GetNode(root);
  REQUIRE(inGame.has_value());
  CHECK(inGame->visible);

  // Session state is reset on leave: the active type returns to the default
  // and the active tab index to the first tab.
  controller.Open(NoMoreDay::StashType::Shared);
  CHECK(controller.GetActiveType() == NoMoreDay::StashType::Shared);

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  const auto left = runtime.GetNode(root);
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);
  CHECK(controller.GetActiveType() == NoMoreDay::StashType::Personal);
  CHECK(controller.GetActiveTabIndex() == 0);

  // Re-entering gameplay restores the panel node.
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  const auto reentered = runtime.GetNode(root);
  REQUIRE(reentered.has_value());
  CHECK(reentered->visible);
}

TEST_CASE("[Unit] UIStashController - Open/Close/Toggle flip instance visibility") {
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);

  // U8: visibility is instance state (the legacy State.showStash mirror is
  // gone); opening the stash also opens the inventory as drag target, which
  // routes through the hosted GameUiHost (covered by GameUiHost tests) and is
  // a no-op here because the test controller has no host.
  CHECK_FALSE(controller.IsVisible());

  controller.Open(NoMoreDay::StashType::Shared);
  CHECK(controller.IsVisible());
  CHECK(controller.GetActiveType() == NoMoreDay::StashType::Shared);
  CHECK(controller.GetActiveTabIndex() == 0);

  controller.Toggle();  // visible -> close
  CHECK_FALSE(controller.IsVisible());

  controller.Toggle();  // hidden -> reopen with the last active type
  CHECK(controller.IsVisible());
  CHECK(controller.GetActiveType() == NoMoreDay::StashType::Shared);

  controller.Close();
  CHECK_FALSE(controller.IsVisible());
}

TEST_CASE("[Unit] UIStashController - Update runs headless against a world") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so GetFrameTime() is available; the alpha is animated towards the
  // visibility flag exactly like the legacy UIStash::Update. GetFrameTime may
  // be 0 in the harness, so the checks use clamped-range + monotonicity
  // invariants instead of exact deltas.
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);
  controller.EnterGameplay();

  ResourceManager resourceManager;
  entt::registry registry;
  LevelManager levelManager;

  // Empty registry before any world exists: Update must not crash.
  controller.SetVisible(true);
  controller.Update(registry);
  CHECK(controller.Alpha() >= 0.0f);
  CHECK(controller.Alpha() <= 1.0f);

  // Provide a real world so Update runs against live gameplay systems.
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  // Branch A: stash closed. Alpha must never increase and stays clamped
  // within [0, 1].
  controller.SetVisible(false);
  controller.Update(registry);
  const float closedFirst = controller.Alpha();
  controller.Update(registry);
  const float closedSecond = controller.Alpha();
  CHECK(closedFirst >= 0.0f);
  CHECK(closedFirst <= 1.0f);
  CHECK(closedSecond >= 0.0f);
  CHECK(closedSecond <= 1.0f);
  CHECK(closedSecond <= closedFirst);

  // Branch B: stash open. Alpha must never decrease and stays clamped
  // within [0, 1].
  controller.SetVisible(true);
  controller.Update(registry);
  const float openFirst = controller.Alpha();
  controller.Update(registry);
  const float openSecond = controller.Alpha();
  CHECK(openFirst >= 0.0f);
  CHECK(openFirst <= 1.0f);
  CHECK(openSecond >= 0.0f);
  CHECK(openSecond <= 1.0f);
  CHECK(openSecond >= openFirst);
}

TEST_CASE("[Unit] UIStashController - Draw executes headless without crashing") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works. UIRenderer falls back to
  // raylib DrawText when the font is unset, AssetLoadingSystem::GetTexture
  // returns a zeroed texture headless (DrawButton/DrawSlot guard on id > 0),
  // and raylib MeasureTextEx early-outs on a zeroed font, so the full panel
  // body is safe to run here without UISystem::Initialize.
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);
  controller.EnterGameplay();

  entt::registry registry;

  // Empty registry: no personal stash yet, so the tabs and grid are skipped,
  // but the panel frame, close button, search bar and footer still draw.
  // U8: alpha is instance state animated by Update (the legacy
  // State.showStash/stashAlpha writes are gone); raise it to 1 by running the
  // animation loop, then draw.
  controller.SetVisible(true);
  for (int i = 0; i < 90; ++i) {
    BeginDrawing();
    EndDrawing();
    controller.Update(registry);
  }
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // A player-owned personal stash with one tab and a valid item entity.
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& stash = registry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 1;

  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.name = "Test Item";
  itemComp.type = ItemType::Weapon;
  itemComp.rarity = Rarity::Rare;
  stash.tabs[0].items[0] = item;

  // Hidden panel (alpha 0): Draw must early-out without touching the world.
  controller.SetVisible(false);
  for (int i = 0; i < 90; ++i) {
    BeginDrawing();
    EndDrawing();
    controller.Update(registry);
  }
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // Visible panel with a populated tab: full draw path (tabs, unlock button,
  // grid with one rendered item, search bar, footer buttons).
  controller.SetVisible(true);
  for (int i = 0; i < 90; ++i) {
    BeginDrawing();
    EndDrawing();
    controller.Update(registry);
  }
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();
}

TEST_CASE("[Unit] UIStashController - header declares no static data members") {
  const std::string path = "src/game/application/ui/UIStashController.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class UIStashController";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in UIStashController: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] UIStashController - implementation declares no static mutable state") {
  const std::string path = "src/game/application/ui/UIStashController.cpp";
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
