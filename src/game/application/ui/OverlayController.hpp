#pragma once

#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include <array>
#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

class GameUiHost;

// Instance controller for the three global overlays (U7 group 6): the context
// menu, the quantity popup and the message box. These used to live as static
// state and static draw functions on UISystem; the controller now owns them as
// instance members (U8 收尾: the UISystem::State mirror is gone, so the
// overlay instance is the single source of truth).
//
// R6 (remediation): all three surfaces are now painted through the draw list
// (Paint) and no gameplay mutator runs in a paint path. Interactions happen in
// the host Update phase (UpdateOverlays): registry reads refresh the display
// caches, and every gameplay action is enqueued as a GameUiIntent executed by
// the GameUiCommandHandler in the next gameplay Update phase. The only
// remaining direct gameplay write is the hotbar skill assignment from the
// skill context menu (no skill-assign intent kind exists yet; the write moved
// from the Draw phase to the Update phase, see R6 evidence).
enum class OverlayContextAction : std::uint8_t {
  Equip,
  Use,
  Unequip,
  Craft,
  Drop,
  Lock,
  Cancel,
};

class OverlayController {
public:
  // uiHost is the intent enqueue channel (R6). Null in headless/controller
  // tests: ConfirmQuantityPopup / ActivateContextMenuButton then only mutate
  // the session state (no gameplay effect).
  explicit OverlayController(UiRuntime& runtime, GameUiHost* uiHost = nullptr);
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

  // Context menu field getters (kept for host SnapshotOptions + tests).
  [[nodiscard]] entt::entity ContextMenuItem() const noexcept { return m_contextMenuItem; }
  [[nodiscard]] std::uint64_t ContextMenuItemDomainId() const noexcept {
    return entt::to_integral(m_contextMenuItem);
  }
  [[nodiscard]] Vector2 ContextMenuPos() const noexcept { return m_contextMenuPos; }
  [[nodiscard]] bool IsContextFromInventory() const noexcept { return m_isContextFromInventory; }
  [[nodiscard]] int ContextSourceInventoryIndex() const noexcept { return m_contextSourceInventoryIndex; }
  [[nodiscard]] NoMoreDay::EquipmentSlot ContextSourceEquipmentSlot() const noexcept { return m_contextSourceEquipmentSlot; }
  [[nodiscard]] bool IsSkillContext() const noexcept { return m_isSkillContext; }
  [[nodiscard]] int ContextSourceSkillSlot() const noexcept { return m_contextSourceSkillSlot; }

  // R6: item-menu button model (built in UpdateOverlays while the menu is
  // visible; paint consumes the same entries so hit-testing and rendering
  // share one layout). Exposed for tests.
  [[nodiscard]] int ContextMenuButtonCount() const noexcept;
  // Activates the n-th context-menu entry. Routes gameplay actions through the
  // GameUiCommandHandler (intent) and UI-local actions through the host.
  void ActivateContextMenuButton(int index);

  // --- Quantity popup (was UIRenderer open + UISystem::DrawQuantityPopup) ---
  // quantityMax seeds the popup range (UIRenderer passes the item quantity).
  void OpenQuantityPopup(entt::entity item, int actionType,
                         int quantityMax = 1);
  void CloseQuantityPopup();
  [[nodiscard]] bool IsQuantityPopupVisible() const noexcept;
  // R6: confirm/cancel entry points (shared by the UpdateOverlays hit-test
  // path and tests). Confirm enqueues the DropItem/DestroyItem intent with the
  // popup state; the command handler re-validates target/quantity in the next
  // Update phase. Registry-free: works on the cached session state alone.
  void ConfirmQuantityPopup();
  void CancelQuantityPopup();

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

  // R4 (remediation, design §3.1/§3.4): reconcile step of the runtime UI
  // pipeline. Synchronizes the overlay root node with the session state so the
  // retained tree reflects what will be painted this frame (the message box is
  // the R4 migrated surface, so the node visibility tracks the message box).
  // Called by the host Update before UpdateInput/Arrange.
  void ReconcileRuntime();

  // R6 (remediation, design §3.1): interaction phase of the overlay surfaces,
  // called by the host Update right before HandleEscape. Registry reads only:
  // validates the context menu item / quantity target and refreshes the
  // display caches (item type/lock/quantity, popup name/max). All gameplay
  // actions are enqueued as intents (executed by the command handler in the
  // next Update phase); the only exception is the hotbar skill assignment
  // (no intent kind exists yet; kept as an Update-phase write).
  void UpdateOverlays(entt::registry& registry, const UiViewport& viewport);

  // R6 (remediation, design §3.4): paint step of the draw-list pipeline.
  // Appends the context menu, the quantity popup and the message box commands
  // to the host-owned draw list under the overlay root node. Registry-free and
  // raylib-free: display values come from the Update-phase caches, resources
  // are referenced through the shared resource ids the host registered with
  // the backend at Initialize time. Called by the host PrepareRender before
  // Finalize.
  void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

  // R4 (remediation, design §3.4): paint step for the message box surface
  // (kept public for the R4 draw-list pipeline contract; Paint() calls it).
  void PaintMessageBox(UiDrawList& drawList, const UiViewport& viewport) const;

  void EnterGameplay();
  void LeaveGameplay();

  // Runtime node id of the overlay root (kInvalidUiId if creation failed).
  // R4: the node is the message box carrier — it is reconciled per frame
  // (visible while the message box is up) and carries the draw-list commands.
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  struct ContextMenuEntry {
    OverlayContextAction action = OverlayContextAction::Cancel;
    const char* label = "";
    UiColor color{255, 255, 255, 255};
    UiRect rect{};
  };

  void RefreshContextMenuDisplay(entt::registry& registry);
  void BuildContextMenuEntries();
  void RefreshQuantityTarget(entt::registry& registry);
  void UpdateContextMenuInteraction(entt::registry& registry,
                                    const UiViewport& viewport);
  void UpdateQuantityPopupInteraction(entt::registry& registry,
                                      const UiViewport& viewport);
  void PaintContextMenu(UiDrawList& drawList, const UiViewport& viewport) const;
  void PaintQuantityPopup(UiDrawList& drawList, const UiViewport& viewport) const;

  void ResetOverlays();

  UiRuntime& m_runtime;
  GameUiHost* m_uiHost = nullptr;
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
  // R6: display cache refreshed in UpdateOverlays while the menu is visible
  // (paint is registry-free; values come from the Update-phase refresh).
  std::uint8_t m_contextItemType = 0;
  bool m_contextItemLocked = false;
  std::int32_t m_contextItemQuantity = 0;
  bool m_contextItemValid = false;
  std::array<ContextMenuEntry, 7> m_contextEntries{};
  int m_contextEntryCount = 0;
  int m_contextHoverEntry = -1;
  UiRect m_skillMenuRect{};
  float m_skillEntryHeight = 40.0f;
  int m_skillHoverIndex = -1;

  // Quantity popup state.
  bool m_quantityVisible = false;
  entt::entity m_quantityTargetItem = entt::null;
  int m_quantityActionType = 0; // 0: Drop, 1: Destroy
  int m_quantityVal = 1;
  int m_quantityMax = 1;
  char m_quantityInputBuf[16] = {0};
  char m_quantityItemName[64] = {0};
  bool m_isTyping = false;
  // R6: hover state is computed in the Update phase (paint stays input-free).
  bool m_popupConfirmHovered = false;
  bool m_popupCancelHovered = false;

  // Message box state.
  bool m_messageBoxVisible = false;
  char m_messageBoxText[64] = {0};
  float m_messageBoxTimer = 0.0f;
};

} // namespace NoMoreDay::ui
