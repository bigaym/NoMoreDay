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
// instance members.
//
// The corresponding UIContext fields (showContextMenu / contextMenuItem /
// contextMenuPos / showQuantityPopup / quantity* / showMessageBox /
// messageBox* / isTyping) remain as a *write-back mirror* because cross-layer
// readers still touch UISystem::State during the transition: UIRenderer opens
// the context menu and the quantity popup (and picks menu items), the host
// compatibility bridge surfaces notifications through the message box, and
// input gating reads State.isTyping / State.showQuantityPopup. Draw/Update
// re-adopt externally opened state each frame and every controller mutation
// mirrors it back; the mirror is removed when the overlays are rewired in U8.
class OverlayController {
public:
  explicit OverlayController(UiRuntime& runtime);
  ~OverlayController() = default;

  OverlayController(const OverlayController&) = delete;
  OverlayController& operator=(const OverlayController&) = delete;

  // --- Context menu (was UISystem::OpenContextMenu / ESC / Draw) ---
  void OpenContextMenu(entt::entity item, bool fromInventory, int inventoryIndex,
                       NoMoreDay::EquipmentSlot slot);
  void CloseContextMenu();
  [[nodiscard]] bool IsContextMenuVisible() const noexcept;

  // --- Quantity popup (was UIRenderer open + UISystem::DrawQuantityPopup) ---
  void OpenQuantityPopup(entt::entity item, int actionType);
  void CloseQuantityPopup();
  [[nodiscard]] bool IsQuantityPopupVisible() const noexcept;

  // --- Message box (was UISystem::DrawMessageBox + Update timer) ---
  void ShowMessageBox(const char* text);
  void HideMessageBox();
  [[nodiscard]] bool IsMessageBoxVisible() const noexcept;

  // Per-frame message box timer decay (was the UISystem::Update block).
  // Called by the host right after the legacy update so the frame position
  // matches the original decay.
  void UpdateMessageBox();

  // Draws the three overlays in the legacy order (context menu -> quantity
  // popup -> message box) at the original UISystem::Draw position (after the
  // ground hover pass, before the tooltip state machine).
  void DrawOverlays(entt::registry& registry);

  void EnterGameplay();
  void LeaveGameplay();

  // Runtime node id of the overlay root (kInvalidUiId if creation failed).
  // The node is a placeholder during the U7 transition: overlay rendering is
  // still driven by the legacy mirrors, so the node stays hidden until U8.
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // Cross-layer writers still open/close overlays by writing UISystem::State
  // directly during the transition (UIRenderer right-click flow, GameUiHost
  // compatibility bridge, sibling-panel closes). These helpers re-adopt that
  // state into the instance members (adopt) and write the members back to the
  // legacy fields (mirror) so both views stay consistent.
  void AdoptContextMenuFromState();
  void MirrorContextMenuToState();
  void AdoptQuantityFromState();
  void MirrorQuantityToState();
  void AdoptMessageBoxFromState();
  void MirrorMessageBoxToState();

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
  bool m_isTyping = false; // Mirrors UISystem::State.isTyping (input gating).

  // Message box state.
  bool m_messageBoxVisible = false;
  char m_messageBoxText[64] = {0};
  float m_messageBoxTimer = 0.0f;
};

} // namespace NoMoreDay::ui
