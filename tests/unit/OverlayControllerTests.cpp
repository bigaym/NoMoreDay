#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/OverlayController.hpp"
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
  // Placeholder node only: the overlays render through the controller's own
  // immediate-mode pass, so the node stays hidden.
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

TEST_CASE("[Unit] OverlayController (UI) - Open/Close context menu owns the "
          "instance state") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();

  controller.OpenContextMenu(item, true, 3, NoMoreDay::EquipmentSlot::MainHand);
  CHECK(controller.IsContextMenuVisible());
  CHECK(controller.ContextMenuItem() == item);
  CHECK(controller.IsContextFromInventory());
  CHECK(controller.ContextSourceInventoryIndex() == 3);
  CHECK(controller.ContextSourceEquipmentSlot() ==
        NoMoreDay::EquipmentSlot::MainHand);

  controller.CloseContextMenu();
  CHECK_FALSE(controller.IsContextMenuVisible());
}

TEST_CASE("[Unit] OverlayController (UI) - OpenSkillContextMenu opens a skill "
          "menu with a slot source") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;

  controller.OpenSkillContextMenu(5);
  CHECK(controller.IsContextMenuVisible());
  CHECK(controller.IsSkillContext());
  CHECK(controller.ContextSourceSkillSlot() == 5);
  CHECK_FALSE(controller.IsContextFromInventory());
  CHECK((controller.ContextMenuItem() == entt::null));

  controller.CloseContextMenu();
  CHECK_FALSE(controller.IsContextMenuVisible());
}

TEST_CASE("[Unit] OverlayController (UI) - Open/Close quantity popup owns "
          "visibility and isTyping") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();
  // DrawQuantityPopup requires a valid item (with a quantity) and a player
  // tag; otherwise it closes the popup immediately (legacy semantics).
  registry.emplace<NoMoreDay::ItemComponent>(item);
  registry.get<NoMoreDay::ItemComponent>(item).quantity = 5;
  registry.emplace<PlayerTag>(registry.create());

  controller.OpenQuantityPopup(item, 1); // 1: Destroy
  CHECK(controller.IsQuantityPopupVisible());
  // isTyping is set by the draw pass while the popup input is live (legacy
  // DrawQuantityPopup semantics), not by OpenQuantityPopup.
  CHECK_FALSE(controller.IsTyping());
  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();
  CHECK(controller.IsTyping());

  controller.CloseQuantityPopup();
  CHECK_FALSE(controller.IsQuantityPopupVisible());
  CHECK_FALSE(controller.IsTyping());
}

TEST_CASE("[Unit] OverlayController (UI) - Show/Hide message box owns the "
          "instance state") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  controller.ShowMessageBox("背包已满");
  CHECK(controller.IsMessageBoxVisible());
  CHECK(std::strcmp(controller.MessageBoxText(), "背包已满") == 0);

  controller.HideMessageBox();
  CHECK_FALSE(controller.IsMessageBoxVisible());
}

TEST_CASE("[Unit] OverlayController (UI) - UpdateMessageBox decays the timer "
          "and closes at zero") {
  ui::UiRuntime runtime;
  ui::OverlayController controller(runtime);

  // Mid-decay: a positive timer keeps the box visible (frame time in the
  // headless harness is far below the 2s lifetime).
  controller.ShowMessageBox("persist");
  controller.UpdateMessageBox();
  CHECK(controller.IsMessageBoxVisible());
  CHECK(std::strcmp(controller.MessageBoxText(), "persist") == 0);

  // HideMessageBox resets the timer so the next update stays closed
  // deterministically regardless of the frame delta.
  controller.HideMessageBox();
  controller.UpdateMessageBox();
  CHECK_FALSE(controller.IsMessageBoxVisible());
}

TEST_CASE("[Unit] OverlayController (UI) - Enter/Leave gameplay resets all "
          "overlays") {
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

  controller.EnterGameplay();
  CHECK_FALSE(controller.IsContextMenuVisible());
  CHECK_FALSE(controller.IsQuantityPopupVisible());
  CHECK_FALSE(controller.IsMessageBoxVisible());
  CHECK_FALSE(controller.IsTyping());
  CHECK((controller.ContextMenuItem() == entt::null));

  // LeaveGameplay resets again (idempotent).
  controller.OpenContextMenu(item, false, 0, NoMoreDay::EquipmentSlot::None);
  CHECK(controller.IsContextMenuVisible());
  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsContextMenuVisible());
}

TEST_CASE("[Unit] OverlayController (UI) - DrawOverlays executes headless "
          "without crashing") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works. No pointer/keyboard input
  // is delivered in the headless harness, so the menu/popup interaction
  // branches (item use/drop, confirm/cancel) do not run; only the draw paths
  // are exercised. A Consumable item keeps the craft branch (which would
  // require a SharedContext registry handle) out of the menu.
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
  // While the popup is up, the input-gating flag is active.
  CHECK(controller.IsTyping());
  CHECK(controller.IsMessageBoxVisible());

  // Invalid target: the popup closes itself instead of crashing.
  controller.CloseQuantityPopup();
  controller.OpenQuantityPopup(entt::null, 0);
  BeginDrawing();
  controller.DrawOverlays(registry);
  EndDrawing();
  CHECK_FALSE(controller.IsQuantityPopupVisible());
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
  // The legacy UISystem::Draw fallback bodies are gone with U8; the host owns
  // the overlay pass and forwards every overlay to the controller.
  const std::string source =
      ReadFileContents("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"DrawContextMenu", "DrawQuantityPopup",
                             "DrawMessageBox"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
  // The host never draws overlay primitives itself; only the drag phantom
  // uses UIRenderer::DrawSlot (a per-frame helper, not an overlay).
  CHECK(source.find("m_overlay.UpdateMessageBox()") != std::string::npos);
  CHECK(source.find("m_overlay.DrawOverlays(registry)") != std::string::npos);
}

TEST_CASE("[Unit] OverlayController - UISystem no longer draws the overlays") {
  // U8 final: UISystem::Draw and the overlay fallback bodies are gone; the
  // overlay pass lives entirely in GameUiHost. UISystem only hosts fonts and
  // stateless draw helpers.
  const std::string source =
      ReadFileContents("src/game/application/ui/UISystem.cpp");
  REQUIRE_FALSE(source.empty());
  CHECK(source.find("overlayController->DrawOverlays(registry)") ==
        std::string::npos);
  CHECK(source.find("DrawQuantityPopup") == std::string::npos);
  CHECK(source.find("DrawContextMenu") == std::string::npos);
  CHECK(source.find("DrawMessageBox") == std::string::npos);
}
