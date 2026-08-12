#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/world/LevelManager.hpp"

namespace NoMoreDay::ui {

// R3 (remediation, design §3.6): the GameUiHost is the sole owner of the UI
// Escape key. HandleEscape closes exactly ONE topmost surface per the close
// policy chain (quantity popup -> character confirm/panel -> context menu ->
// skill tree -> astrolabe -> inventory -> stash -> crafting) and reports
// whether the key was consumed, so GameplayState only pushes PauseState on an
// unconsumed Escape edge (H-02: the same key is never consumed twice).
//
// These tests drive the chain headlessly: surfaces are opened through the
// host's public channels (the same channels GameplayState / panel controllers
// use) and HandleEscape is called directly — the host Update path only adds
// the per-frame flag reset and the IsKeyPressed gate around it.

namespace {

// Minimal registry with a PlayerTag entity (required by the character-confirm
// and astrolabe close policies).
entt::registry MakePlayerRegistry() {
  entt::registry registry;
  registry.emplace<PlayerTag>(registry.create());
  return registry;
}

} // namespace

TEST_CASE("[Unit] GameUiHost - Escape with nothing open is not consumed") {
  GameUiHost host;
  entt::registry registry;

  CHECK_FALSE(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.HandleEscape());
  CHECK_FALSE(host.EscapeConsumedThisFrame());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the quantity popup first") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();

  host.OpenQuantityPopup(entt::null, 1);
  REQUIRE(host.IsModalInputCaptured());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsQuantityPopupVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the character confirm popup before "
          "the panel") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();
  // The confirm popup only exists while the character panel is open and the
  // player has pending attribute draft points (R6: the confirm state lives in
  // the controller, not in an AttributeUIComponent).
  host.SetCharacterPanelVisible(true);
  host.ShowCharacterConfirmPopup();

  // First Escape: dismisses the confirm popup only, the panel stays open.
  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK(host.IsCharacterPanelVisible());

  // Second Escape: closes the panel.
  CHECK(host.HandleEscape());
  CHECK_FALSE(host.IsCharacterPanelVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the character panel without a "
          "confirm popup") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();
  host.SetCharacterPanelVisible(true);
  REQUIRE(host.IsCharacterPanelVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsCharacterPanelVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the context menu") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();

  host.OpenContextMenu(entt::null, true, 0, NoMoreDay::EquipmentSlot::None);
  REQUIRE(host.IsContextMenuVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsContextMenuVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the skill tree") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();
  host.ToggleSkillTree();
  REQUIRE(host.IsModalInputCaptured());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsSkillTreeVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the astrolabe") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();
  const auto player = registry.view<PlayerTag>().front();

  host.ShowAstrolabe();
  REQUIRE(host.IsAstrolabeVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsAstrolabeVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the inventory") {
  GameUiHost host;
  entt::registry registry;
  host.SetInventoryVisible(true);
  REQUIRE(host.IsInventoryVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsInventoryVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the stash") {
  GameUiHost host;
  entt::registry registry;
  // OpenStash also reveals the inventory panel (legacy sibling semantics), so
  // the close chain consumes the topmost open surface first: inventory, then
  // stash.
  host.OpenStash(NoMoreDay::StashType::Personal);
  REQUIRE(host.IsStashVisible());
  REQUIRE(host.IsInventoryVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsInventoryVisible());
  // The stash is still open: the chain closed exactly one (topmost) surface.
  CHECK(host.IsStashVisible());

  CHECK(host.HandleEscape());
  CHECK_FALSE(host.IsStashVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes the crafting panel") {
  GameUiHost host;
  entt::registry registry;
  host.CraftingOpenMergePanel();
  REQUIRE(host.IsCraftingVisible());

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsCraftingVisible());
}

TEST_CASE("[Unit] GameUiHost - Escape closes exactly one topmost surface "
          "(priority chain)") {
  GameUiHost host;
  entt::registry registry = MakePlayerRegistry();
  const auto player = registry.view<PlayerTag>().front();

  // Open inventory + character + context menu + quantity popup. Design §3.6
  // priority: quantity popup -> character confirm/panel -> context menu ->
  // skill tree -> astrolabe -> inventory -> stash -> crafting. The popup wins.
  host.SetInventoryVisible(true);
  host.SetCharacterPanelVisible(true);
  host.OpenContextMenu(entt::null, true, 0, NoMoreDay::EquipmentSlot::None);
  host.OpenQuantityPopup(entt::null, 1);

  CHECK(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());
  CHECK_FALSE(host.IsQuantityPopupVisible());
  // The other surfaces are untouched.
  CHECK(host.IsInventoryVisible());
  CHECK(host.IsCharacterPanelVisible());
  CHECK(host.IsContextMenuVisible());

  // Next Escape: the character panel (no confirm popup pending).
  CHECK(host.HandleEscape());
  CHECK_FALSE(host.IsCharacterPanelVisible());
  CHECK(host.IsInventoryVisible());
  CHECK(host.IsContextMenuVisible());

  // Context menu next.
  CHECK(host.HandleEscape());
  CHECK_FALSE(host.IsContextMenuVisible());
  CHECK(host.IsInventoryVisible());

  // Inventory last.
  CHECK(host.HandleEscape());
  CHECK_FALSE(host.IsInventoryVisible());

  // Nothing left: the key is not consumed (gameplay may push PauseState).
  CHECK_FALSE(host.HandleEscape());
  CHECK_FALSE(host.EscapeConsumedThisFrame());
}

TEST_CASE("[Unit] GameUiHost - EscapeConsumedThisFrame is reset by Update each "
          "frame (H-02)") {
  GameUiHost host;
  entt::registry registry;
  host.SetInventoryVisible(true);
  REQUIRE(host.HandleEscape());
  CHECK(host.EscapeConsumedThisFrame());

  // A subsequent Update (no Escape key in the real input path) resets the
  // flag so a later Escape in the same frame is evaluated fresh. The host is
  // not initialized here: the reset runs before the initialization guard so
  // the frame contract holds even headless.
  LevelManager levelManager;
  host.Update(registry, levelManager);
  CHECK_FALSE(host.EscapeConsumedThisFrame());
}

} // namespace NoMoreDay::ui
