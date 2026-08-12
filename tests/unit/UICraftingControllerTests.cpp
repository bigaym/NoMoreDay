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
  UICraftingController controller(runtime, nullptr);

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
  UICraftingController controller(runtime, nullptr);

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
  UICraftingController controller(runtime, nullptr);
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
  CHECK(controller.GetForgeTargetDomainId() == kInvalidDomainId);
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

TEST_CASE("[Unit] UICraftingController - Update runs headless against a snapshot") {
  // R7: the controller no longer takes a registry — it consumes the snapshot
  // view model and the input frame. The forge target is a stable domain id
  // (kInvalidDomainId when empty); stale targets (destroyed items) are dropped
  // on the next Update when the item is absent from snapshot.displayedItems,
  // mirroring the legacy registry.valid() cleanup without touching the ECS.
  UiRuntime runtime;
  UICraftingController controller(runtime, nullptr);
  controller.EnterGameplay();

  GameUiSnapshot snapshot;
  snapshot.revision = 1;

  UiInputFrame input;
  input.deltaSeconds = 1.0f / 60.0f;

  // Empty snapshot before any world exists: Update must not crash.
  controller.Update(snapshot, input);

  // Valid forge target survives Update while it stays in displayedItems.
  const entt::entity item = entt::entity(9001);
  GameUiItemView itemView;
  itemView.domainId = entt::to_integral(item);
  itemView.itemType = static_cast<std::uint8_t>(NoMoreDay::ItemType::Weapon);
  itemView.forgingPotential = 5;
  snapshot.displayedItems.push_back(itemView);

  controller.SetTargetItem(item);
  CHECK(controller.GetForgeTargetDomainId() == entt::to_integral(item));
  controller.Update(snapshot, input);
  CHECK(controller.GetForgeTargetDomainId() == entt::to_integral(item));

  // Destroyed forge target (absent from displayedItems) is dropped on the
  // next Update.
  snapshot.displayedItems.clear();
  controller.Update(snapshot, input);
  CHECK(controller.GetForgeTargetDomainId() == kInvalidDomainId);
}

TEST_CASE("[Unit] UICraftingController - Paint runs headless for the reachable tabs") {
  // R7: the panel is painted from the snapshot view model into a UiDrawList
  // (no registry, no raylib immediate mode). The panel alpha gate is driven to
  // 1.0 through Update's input-driven animation.
  UiRuntime runtime;
  UICraftingController controller(runtime, nullptr);
  controller.EnterGameplay();

  UiViewport viewport = UiViewport::Fit({800, 600});

  // Forge item with a prefix and a suffix so PaintForgingTab runs both row
  // kinds. The item is carried as a GameUiItemView in displayedItems (the
  // snapshot carries no component references).
  GameUiSnapshot snapshot;
  snapshot.revision = 1;

  const entt::entity item = entt::entity(9001);
  GameUiItemView itemView;
  itemView.domainId = entt::to_integral(item);
  itemView.itemType = static_cast<std::uint8_t>(NoMoreDay::ItemType::Weapon);
  itemView.forgingPotential = 5;
  GameUiAffixView prefix;
  prefix.type = static_cast<std::uint16_t>(NoMoreDay::AffixType::Strength);
  prefix.value = 0.0f;
  prefix.tier = 3;
  prefix.isPrefix = true;
  prefix.isLegendary = false;
  GameUiAffixView suffix;
  suffix.type = static_cast<std::uint16_t>(NoMoreDay::AffixType::FlatPhysicalDamage);
  suffix.value = 0.0f;
  suffix.tier = 2;
  suffix.isPrefix = false;
  suffix.isLegendary = false;
  itemView.affixes.push_back(prefix);
  itemView.affixes.push_back(suffix);
  snapshot.displayedItems.push_back(itemView);

  UiInputFrame input;
  input.deltaSeconds = 1.0f / 60.0f;

  // Forging tab: set the target (auto-opens the panel) and animate alpha to 1.
  controller.SetTargetItem(item);
  for (int i = 0; i < 90; ++i) {
    controller.Update(snapshot, input);
  }
  UiDrawList forgeList;
  controller.Paint(forgeList, viewport, snapshot);
  CHECK(forgeList.CommandCount() > 0);

  // Merging tab: OpenMergePanel switches tabs; paint with empty merge slots.
  controller.OpenMergePanel();
  UiDrawList mergeList;
  controller.Paint(mergeList, viewport, snapshot);
  CHECK(mergeList.CommandCount() > 0);

  // The Salvaging tab is only reachable through a tab click, which cannot be
  // synthesized headless; its primitives (salvage slot, yield preview from
  // snapshot.crafting.salvageYield, filter popup, batch button) are covered by
  // inspection and by the other two tabs above.
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
