#include "doctest.h"

#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include "raylib.h"

#include <cstring>
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

// UISystem::State is a process-wide legacy singleton; restore the fields this
// suite touches to their defaults so each case starts from a clean slate.
void ResetLegacyState() {
  UISystem::State.showContextMenu = false;
  UISystem::State.contextMenuItem = entt::null;
  UISystem::State.contextMenuPos = {0.0f, 0.0f};
  UISystem::State.isContextFromInventory = false;
  UISystem::State.contextSourceInventoryIndex = -1;
  UISystem::State.contextSourceEquipmentSlot = NoMoreDay::EquipmentSlot::None;
  UISystem::State.isSkillContext = false;
  UISystem::State.contextSourceSkillSlot = -1;

  UISystem::State.showQuantityPopup = false;
  UISystem::State.quantityTargetItem = entt::null;
  UISystem::State.quantityActionType = 0;
  UISystem::State.quantityVal = 1;
  UISystem::State.quantityMax = 1;
  UISystem::State.quantityInputBuf[0] = '\0';
  UISystem::State.isTyping = false;

  UISystem::State.showMessageBox = false;
  UISystem::State.messageBoxText[0] = '\0';
  UISystem::State.messageBoxTimer = 0.0f;
}

} // namespace

TEST_CASE("[Unit] OverlayController (UI) - creates a hidden placeholder "
          "overlay root node") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  const ui::UiId root = controller.NodeId();
  CHECK(root != ui::kInvalidUiId);

  const auto node = runtime.GetNode(root);
  REQUIRE(node.has_value());
  CHECK(node->id == root);
  CHECK(node->parent == ui::kRootUiId);
  // Placeholder node only: the overlays still render through the legacy
  // mirrors during the U7 transition, so the node stays hidden until U8.
  CHECK_FALSE(node->visible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->focusable);
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->capturePointer);
  CHECK_FALSE(node->captureKeyboard);
  CHECK_FALSE(node->acceptsText);
  CHECK(node->zIndex == static_cast<std::int32_t>(ui::UiDrawLayer::Panels));
  CHECK(runtime.NodeCount() == 2); // runtime root + overlay root

  // Full-viewport declarative anchor.
  CHECK(node->layout.kind == ui::UiLayoutKind::Overlay);
  CHECK(node->layout.width.kind == ui::UiLengthKind::Fraction);
  CHECK(node->layout.width.value == doctest::Approx(1.0f));
  CHECK(node->layout.height.kind == ui::UiLengthKind::Fraction);
  CHECK(node->layout.height.value == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] OverlayController (UI) - Open/Close context menu mirrors "
          "into UISystem::State") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();

  controller.OpenContextMenu(item, true, 3, NoMoreDay::EquipmentSlot::MainHand);
  CHECK(controller.IsContextMenuVisible());
  CHECK(UISystem::State.showContextMenu);
  CHECK(UISystem::State.contextMenuItem == item);
  CHECK(UISystem::State.isContextFromInventory);
  CHECK(UISystem::State.contextSourceInventoryIndex == 3);
  CHECK(UISystem::State.contextSourceEquipmentSlot ==
        NoMoreDay::EquipmentSlot::MainHand);

  controller.CloseContextMenu();
  CHECK_FALSE(controller.IsContextMenuVisible());
  CHECK_FALSE(UISystem::State.showContextMenu);
}

TEST_CASE("[Unit] OverlayController (UI) - context menu re-adopts state opened "
          "by legacy writers") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item).type = ItemType::Consumable;

  // Legacy flow: the right-click menu is opened by a cross-layer writer that
  // still writes UISystem::State directly (e.g. UIRenderer or the hotbar).
  UISystem::State.showContextMenu = true;
  UISystem::State.contextMenuItem = item;
  UISystem::State.isContextFromInventory = true;
  UISystem::State.contextSourceInventoryIndex = 1;

  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();

  CHECK(controller.IsContextMenuVisible());
  CHECK(UISystem::State.showContextMenu);
}

TEST_CASE("[Unit] OverlayController (UI) - Open/Close quantity popup mirrors "
          "visibility and isTyping") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();

  controller.OpenQuantityPopup(item, 1); // 1: Destroy
  CHECK(controller.IsQuantityPopupVisible());
  CHECK(UISystem::State.showQuantityPopup);
  CHECK(UISystem::State.quantityTargetItem == item);
  CHECK(UISystem::State.quantityActionType == 1);
  CHECK_FALSE(UISystem::State.isTyping);

  controller.CloseQuantityPopup();
  CHECK_FALSE(controller.IsQuantityPopupVisible());
  CHECK_FALSE(UISystem::State.showQuantityPopup);
  const bool quantityTargetReset =
      (UISystem::State.quantityTargetItem == entt::null);
  CHECK(quantityTargetReset);
  CHECK_FALSE(UISystem::State.isTyping);
}

TEST_CASE("[Unit] OverlayController (UI) - Show/Hide message box mirrors into "
          "UISystem::State") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  controller.ShowMessageBox("背包已满");
  CHECK(controller.IsMessageBoxVisible());
  CHECK(UISystem::State.showMessageBox);
  CHECK(std::strcmp(UISystem::State.messageBoxText, "背包已满") == 0);
  CHECK(UISystem::State.messageBoxTimer == doctest::Approx(2.0f));

  controller.HideMessageBox();
  CHECK_FALSE(controller.IsMessageBoxVisible());
  CHECK_FALSE(UISystem::State.showMessageBox);
  CHECK(UISystem::State.messageBoxTimer == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] OverlayController (UI) - UpdateMessageBox decays the timer "
          "and closes at zero") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  // Mid-decay: a positive timer keeps the box visible (frame time in the
  // headless harness is far below the 2s lifetime).
  controller.ShowMessageBox("persist");
  controller.UpdateMessageBox();
  CHECK(controller.IsMessageBoxVisible());
  CHECK(UISystem::State.showMessageBox);
  CHECK(UISystem::State.messageBoxTimer <= doctest::Approx(2.0f));

  // Legacy writer sets a zero timer: the next update closes deterministically
  // regardless of the frame delta.
  UISystem::State.showMessageBox = true;
  UISystem::State.messageBoxTimer = 0.0f;
  controller.UpdateMessageBox();
  CHECK_FALSE(controller.IsMessageBoxVisible());
  CHECK_FALSE(UISystem::State.showMessageBox);
  CHECK(UISystem::State.messageBoxTimer == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] OverlayController (UI) - Enter/Leave gameplay resets all "
          "overlays and mirrors") {
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();

  controller.OpenContextMenu(item, false, 0, NoMoreDay::EquipmentSlot::None);
  controller.OpenQuantityPopup(item, 1);
  controller.ShowMessageBox("stale");
  CHECK(controller.IsContextMenuVisible());
  CHECK(controller.IsQuantityPopupVisible());
  CHECK(controller.IsMessageBoxVisible());
  CHECK(UISystem::State.showContextMenu);
  CHECK(UISystem::State.showQuantityPopup);
  CHECK(UISystem::State.showMessageBox);

  controller.EnterGameplay();
  CHECK_FALSE(controller.IsContextMenuVisible());
  CHECK_FALSE(controller.IsQuantityPopupVisible());
  CHECK_FALSE(controller.IsMessageBoxVisible());
  CHECK_FALSE(UISystem::State.showContextMenu);
  CHECK_FALSE(UISystem::State.showQuantityPopup);
  CHECK_FALSE(UISystem::State.showMessageBox);
  CHECK_FALSE(UISystem::State.isTyping);
  const bool contextItemReset = (UISystem::State.contextMenuItem == entt::null);
  const bool quantityTargetReset = (UISystem::State.quantityTargetItem == entt::null);
  CHECK(contextItemReset);
  CHECK(quantityTargetReset);

  // LeaveGameplay resets again (idempotent).
  controller.OpenContextMenu(item, false, 0, NoMoreDay::EquipmentSlot::None);
  CHECK(UISystem::State.showContextMenu);
  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsContextMenuVisible());
  CHECK_FALSE(UISystem::State.showContextMenu);
}

TEST_CASE("[Unit] OverlayController (UI) - DrawOverlays executes headless "
          "without crashing") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works. No pointer/keyboard input
  // is delivered in the headless harness, so the menu/popup interaction
  // branches (item use/drop, confirm/cancel) do not run; only the draw paths
  // are exercised. A Consumable item keeps the craft branch (which would
  // require a SharedContext registry handle) out of the menu.
  ResetLegacyState();
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.name = "Test Potion";
  itemComp.type = ItemType::Consumable;
  itemComp.quantity = 5;

  // Empty registry: every overlay early-outs.
  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();

  // All three overlays open at once (legacy draw order: context menu ->
  // quantity popup -> message box).
  controller.OpenContextMenu(item, true, 0, NoMoreDay::EquipmentSlot::None);
  controller.OpenQuantityPopup(item, 0); // 0: Drop
  controller.ShowMessageBox("hello");
  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();

  CHECK(controller.IsContextMenuVisible());
  CHECK(controller.IsQuantityPopupVisible());
  CHECK(UISystem::State.showQuantityPopup);
  // While the popup is up, the input-gating mirror is active.
  CHECK(UISystem::State.isTyping);
  CHECK(controller.IsMessageBoxVisible());
  CHECK(UISystem::State.showMessageBox);

  // Invalid target: the popup closes itself instead of crashing.
  controller.CloseQuantityPopup();
  controller.OpenQuantityPopup(entt::null, 0);
  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();
  CHECK_FALSE(controller.IsQuantityPopupVisible());
  CHECK_FALSE(UISystem::State.showQuantityPopup);
}

TEST_CASE("[Unit] OverlayController - implementation declares no static "
          "mutable UI state") {
  const std::string source =
      ReadFileContents("src/game/application/ui/OverlayController.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static uint32_t", "static Texture2D",
                             "static Font", "static Shader",
                             "static std::string", "static std::vector",
                             "static Color"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
}

TEST_CASE("[Unit] OverlayController - GameUiHost routes the overlays through "
          "the controller") {
  // The legacy UISystem::Draw fallback bodies stay behind as the null-pointer
  // path, so instead of guarding UISystem this checks that the host never
  // draws the overlays itself and forwards them to the controller.
  const std::string source =
      ReadFileContents("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"DrawContextMenu", "DrawQuantityPopup",
                             "DrawMessageBox", "UIRenderer::Draw"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
  CHECK(source.find("&m_overlay") != std::string::npos);
  CHECK(source.find("m_overlay.UpdateMessageBox()") != std::string::npos);
}

TEST_CASE("[Unit] OverlayController - UISystem routes the overlays through "
          "the controller") {
  const std::string source =
      ReadFileContents("src/game/application/ui/UISystem.cpp");
  REQUIRE_FALSE(source.empty());
  CHECK(source.find("overlayController->DrawOverlays(registry)") !=
        std::string::npos);
  CHECK(source.find("overlayController->CloseQuantityPopup()") !=
        std::string::npos);
  CHECK(source.find("overlayController->CloseContextMenu()") !=
        std::string::npos);
}
