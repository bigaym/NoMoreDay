#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

// Instance controller for the three global overlays (U7 group 6): the context
// menu, the quantity popup and the message box. These used to live as static
// state and static draw functions on UISystem; the controller now owns them as
// instance members (U8 收尾: the UISystem::State mirror is gone, so the
// overlay instance is the single source of truth).
//
// UIRenderer draws the context menu from this controller's state (via the
// getters below) and routes menu actions back through it (CloseContextMenu /
// OpenQuantityPopup / ShowMessageBox); the quantity popup input/validation and
// the message box timer are fully owned here.
class OverlayController {
public:
  explicit OverlayController(UiRuntime& runtime);
  ~OverlayController() = default;

  OverlayController(const OverlayController&) = delete;
  OverlayController& operator=(const OverlayController&) = delete;

  // --- Context menu (was UISystem::OpenContextMenu / ESC / Draw) ---
  void OpenContextMenu(entt::entity item, bool fromInventory, int inventoryIndex,
                       NoMoreDay::EquipmentSlot slot);
  // U8 收尾: opens the skill-assignment context menu (hotbar right-click).
  void OpenSkillContextMenu(int skillSlot);
  void CloseContextMenu();
  [[nodiscard]] bool IsContextMenuVisible() const noexcept;

  // Context menu field getters (read by UIRenderer::DrawContextMenu).
  [[nodiscard]] entt::entity ContextMenuItem() const noexcept { return m_contextMenuItem; }
  [[nodiscard]] Vector2 ContextMenuPos() const noexcept { return m_contextMenuPos; }
  [[nodiscard]] bool IsContextFromInventory() const noexcept { return m_isContextFromInventory; }
  [[nodiscard]] int ContextSourceInventoryIndex() const noexcept { return m_contextSourceInventoryIndex; }
  [[nodiscard]] NoMoreDay::EquipmentSlot ContextSourceEquipmentSlot() const noexcept { return m_contextSourceEquipmentSlot; }
  [[nodiscard]] bool IsSkillContext() const noexcept { return m_isSkillContext; }
  [[nodiscard]] int ContextSourceSkillSlot() const noexcept { return m_contextSourceSkillSlot; }

  // --- Quantity popup (was UIRenderer open + UISystem::DrawQuantityPopup) ---
  // quantityMax seeds the popup range (UIRenderer passes the item quantity).
  void OpenQuantityPopup(entt::entity item, int actionType,
                         int quantityMax = 1);
  void CloseQuantityPopup();
  [[nodiscard]] bool IsQuantityPopupVisible() const noexcept;

  // --- Message box (was UISystem::DrawMessageBox + Update timer) ---
  void ShowMessageBox(const char* text);
  void HideMessageBox();
  [[nodiscard]] bool IsMessageBoxVisible() const noexcept;
  // U8 final: message box text (for tests/tooltips; the timer decay is owned
  // here and exposed through the visible flag only).
  [[nodiscard]] const char* MessageBoxText() const noexcept {
    return m_messageBoxText;
  }

  // True while the quantity popup text input is focused. InputCapture reads
  // this (plus the panel search-focus flags) to gate gameplay input.
  [[nodiscard]] bool IsTyping() const noexcept { return m_isTyping; }

  // Per-frame message box timer decay (was the UISystem::Update block).
  // Called by the host right after the update so the frame position matches
  // the original decay.
  void UpdateMessageBox();

  // Draws the three overlays in the legacy order (context menu -> quantity
  // popup -> message box) at the original UISystem::Draw position (after the
  // ground hover pass, before the tooltip state machine).
  void DrawOverlays(entt::registry& registry);

  void EnterGameplay();
  void LeaveGameplay();

  // Runtime node id of the overlay root (kInvalidUiId if creation failed).
  // The node is a placeholder: overlay rendering is driven by the controller's
  // legacy-compatible draw pass, so the node stays hidden.
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  void DrawContextMenu(entt::registry& registry);
  void DrawQuantityPopup(entt::registry& registry);
  void DrawMessageBox();

  void ResetOverlays();

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;

  // Context menu state.
  bool m_contextMenuVisible = false;
  entt::entity m_contextMenuItem = entt::null;
  Vector2 m_contextMenuPos = {0.0f, 0.0f};
  bool m_isContextFromInventory = false;
  int m_contextSourceInventoryIndex = -1;
  NoMoreDay::EquipmentSlot m_contextSourceEquipmentSlot = NoMoreDay::EquipmentSlot::None;
  bool m_isSkillContext = false;
  int m_contextSourceSkillSlot = -1;

  // Quantity popup state.
  bool m_quantityVisible = false;
  entt::entity m_quantityTargetItem = entt::null;
  int m_quantityActionType = 0; // 0: Drop, 1: Destroy
  int m_quantityVal = 1;
  int m_quantityMax = 1;
  char m_quantityInputBuf[16] = {0};
  bool m_isTyping = false;

  // Message box state.
  bool m_messageBoxVisible = false;
  char m_messageBoxText[64] = {0};
  float m_messageBoxTimer = 0.0f;
};

} // namespace NoMoreDay::ui
