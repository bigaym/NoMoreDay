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

TEST_CASE("[Unit] UIStashController - Update runs headless against a snapshot") {
  // R7: the controller no longer takes a registry — it consumes the snapshot
  // view model and the input frame (the same contract as UIInventoryController).
  // The alpha is animated from input.deltaSeconds towards the visibility flag,
  // so the checks use clamped-range + monotonicity invariants instead of exact
  // deltas.
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);
  controller.EnterGameplay();

  GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.stash.unlockedTabs = 1;
  snapshot.stash.nextUnlockCost = 5000;
  GameUiStashTabView tab;
  tab.tabType = 0;
  snapshot.stash.tabs.push_back(tab);

  UiInputFrame input;
  input.deltaSeconds = 1.0f / 60.0f;

  // Empty snapshot before any world exists: Update must not crash.
  controller.SetVisible(true);
  controller.Update(snapshot, input);
  CHECK(controller.Alpha() >= 0.0f);
  CHECK(controller.Alpha() <= 1.0f);

  // Branch A: stash closed. Alpha must never increase and stays clamped
  // within [0, 1].
  controller.SetVisible(false);
  controller.Update(snapshot, input);
  const float closedFirst = controller.Alpha();
  controller.Update(snapshot, input);
  const float closedSecond = controller.Alpha();
  CHECK(closedFirst >= 0.0f);
  CHECK(closedFirst <= 1.0f);
  CHECK(closedSecond >= 0.0f);
  CHECK(closedSecond <= 1.0f);
  CHECK(closedSecond <= closedFirst);

  // Branch B: stash open. Alpha must never decrease and stays clamped
  // within [0, 1].
  controller.SetVisible(true);
  controller.Update(snapshot, input);
  const float openFirst = controller.Alpha();
  controller.Update(snapshot, input);
  const float openSecond = controller.Alpha();
  CHECK(openFirst >= 0.0f);
  CHECK(openFirst <= 1.0f);
  CHECK(openSecond >= 0.0f);
  CHECK(openSecond <= 1.0f);
  CHECK(openSecond >= openFirst);
}

TEST_CASE("[Unit] UIStashController - Paint executes headless without crashing") {
  // R7: the panel is painted from the snapshot view model into a UiDrawList
  // (no registry, no raylib immediate mode, no component references). The
  // viewport scales the logical 2560x1440 layout down to the test window.
  UiRuntime runtime;
  UIStashController controller(runtime, nullptr);
  controller.EnterGameplay();

  UiViewport viewport = UiViewport::Fit({800, 600});

  // Empty stash snapshot: tabs and grid are skipped, but the panel frame,
  // close button, search bar and footer still paint.
  // U8/R7: alpha is instance state animated by Update; raise it to 1 by
  // running the animation loop, then paint.
  GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.stash.unlockedTabs = 0;
  snapshot.stash.nextUnlockCost = 5000;

  UiInputFrame input;
  input.deltaSeconds = 1.0f / 60.0f;
  controller.SetVisible(true);
  for (int i = 0; i < 90; ++i) {
    controller.Update(snapshot, input);
  }
  UiDrawList emptyList;
  controller.Paint(emptyList, viewport, snapshot);

  // A player-owned personal stash with one tab and a valid item slot view.
  snapshot.stash.unlockedTabs = 1;
  GameUiStashTabView tab;
  tab.tabType = 0;
  GameUiStashSlotView slot;
  slot.slotIndex = 0;
  slot.domainId = 9001;  // stable domain id, not an entt::entity
  slot.textureId = 0;    // headless: no texture registered
  slot.rarity = static_cast<std::uint8_t>(NoMoreDay::Rarity::Rare);
  slot.quantity = 1;
  slot.matchesSearch = true;
  tab.slots.push_back(slot);
  snapshot.stash.tabs.push_back(tab);

  // Hidden panel (alpha 0): Paint must early-out without emitting commands.
  controller.SetVisible(false);
  for (int i = 0; i < 90; ++i) {
    controller.Update(snapshot, input);
  }
  UiDrawList hiddenList;
  controller.Paint(hiddenList, viewport, snapshot);
  CHECK(hiddenList.CommandCount() == 0);

  // Visible panel with a populated tab: full paint path (tabs, unlock button,
  // grid with one rendered item, search bar, footer buttons).
  controller.SetVisible(true);
  for (int i = 0; i < 90; ++i) {
    controller.Update(snapshot, input);
  }
  UiDrawList fullList;
  controller.Paint(fullList, viewport, snapshot);
  CHECK(fullList.CommandCount() > 0);
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
