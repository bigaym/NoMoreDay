#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

namespace NoMoreDay::ui {

class GameUiHost;

// Instance controller for the character attribute panel.
//
// R6 (remediation, design §3.1/§3.4/§3.6): the panel is split into two
// phases. UpdateInput (host Update phase, after the snapshot build) edits the
// controller-local draft attribute points and enqueues the
// ConfirmAttributeAllocation intent when the confirm popup is accepted; the
// command handler re-validates and delegates to
// StatsSystem::AllocateAttributePoints on the next Update, and failures
// surface through the standard result notification channel (message box).
// Paint (PrepareRender phase) emits draw-list commands from the frame
// snapshot + the draft state — no registry access, no gameplay writes, no
// input on the paint path.
//
// The legacy Draw phase is gone: the controller never touches
// PrimaryStats / StatsDirty / AttributeUIComponent (B-01 remediation). The
// confirm popup visibility is controller state (host Escape chain closes it
// via CloseConfirmPopup before closing the panel).
class UICharacterController {
public:
  explicit UICharacterController(UiRuntime& runtime,
                                 GameUiHost* uiHost = nullptr);
  ~UICharacterController() = default;

  UICharacterController(const UICharacterController&) = delete;
  UICharacterController& operator=(const UICharacterController&) = delete;

  // Resets session-scoped state (tab, scroll, draft points) and hides the
  // panel root node (a gameplay session starts with the panel closed; KEY_C
  // opens it). Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Per-frame instance alpha update towards the authoritative visibility flag
  // (m_visible; host KEY_C/ESC route through SetVisible). Does not draw
  // anything. Called by the host right after the legacy update.
  void Update(float dt);

  // R6 interaction phase (host Update, after the snapshot build): drags the
  // panel header, edits the draft attribute points (+/- buttons), switches
  // tabs, scrolls the content and handles the confirm popup. The confirm
  // action enqueues a ConfirmAttributeAllocation intent; no gameplay write
  // happens here. Reads only the frame snapshot and the logical mouse.
  void UpdateInput(const GameUiSnapshot& snapshot);

  // R6 paint phase (host PrepareRender): emits draw-list commands for the
  // panel from the frame snapshot + the controller draft state. Registry-free
  // and input-free; the only non-const side effect is the content-height
  // measurement cache used for scroll clamping (UI-local state).
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot) const;

  // Toggles the panel root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;

  // Sets the panel opacity used by Paint (1.0 = fully opaque).
  void SetAlpha(float alpha);
  [[nodiscard]] float Alpha() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  // --- R6 host seams (Escape chain / KEY_C close) ---

  // True while the attribute-confirm popup is open (focus surface above the
  // panel; the host closes it before the panel on Escape).
  [[nodiscard]] bool IsConfirmPopupVisible() const noexcept;

  // Dismisses the confirm popup (Escape cancel). The draft points stay, so
  // the user can resume editing (legacy ESC behaviour).
  void CloseConfirmPopup();

  // Opens the confirm popup (host test seam + UpdateInput confirm-click path
  // share this entry point). No-op while the panel is hidden.
  void ShowConfirmPopup();

  // Clears the draft attribute points (host KEY_C close / session boundary;
  // mirrors the legacy AttributeUIComponent temp reset).
  void ResetDraftPoints();

private:
  // Shared panel geometry (computed in both phases so the interaction hit
  // tests and the paint layout can never drift apart).
  struct CharacterLayout {
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelW = 450.0f;
    float panelH = 780.0f;
    float padding = 25.0f;
    float avatarSize = 90.0f;
    float avatarX = 0.0f;
    float avatarY = 0.0f;
    float infoX = 0.0f;
    float primaryY = 0.0f;      // "基础属性" header row.
    float attrRowY = 0.0f;      // First draft attribute row.
    float attrBtnX = 0.0f;      // +/- button column.
    float tabY = 0.0f;
    float tabH = 32.0f;
    float tabW = 0.0f;
    float contentY = 0.0f;
    float contentH = 0.0f;
    UiRect contentRect{};       // Scissor/view area for the tab rows.
    UiRect confirmButtonRect{}; // 确认加点 (only when draft > 0).
    UiRect popupBox{};          // Confirm popup frame.
    UiRect popupYesRect{};
    UiRect popupNoRect{};
  };

  CharacterLayout ComputeLayout() const;

  // Enqueues the ConfirmAttributeAllocation intent with the current drafts.
  void EnqueueAllocationIntent(int strength, int dexterity, int intelligence,
                               int vitality);

  // Restores the panel session state to its defaults.
  void ResetSessionState();

  UiRuntime& m_runtime;
  GameUiHost* m_uiHost = nullptr;

  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = false;
  bool m_inGameplay = false;  // Session state set by Enter/LeaveGameplay.
  float m_alpha = 0.0f;       // Panel opacity, animated by Update.

  // Instance panel-drag state (same pattern as UIStashController).
  NoMoreDay::PanelState m_panelState;
  UIPanelID m_activeDragPanel = UIPanelID::None;
  // Drag output (logical space), stored so Paint shares the exact geometry
  // the interaction phase hit-tested against.
  float m_panelX = 40.0f;
  float m_panelY = 0.0f;

  // Panel session state (reset by Enter/LeaveGameplay).
  int m_activeCharTab = 0;  // 0: attack, 1: defense, 2: summon, 3: other.
  float m_charPanelScroll = 0.0f;
  mutable float m_lastContentHeight = 0.0f;

  // R6: controller-local draft attribute points (was the legacy
  // AttributeUIComponent temp fields). Committed via the
  // ConfirmAttributeAllocation intent; reset by ResetDraftPoints.
  int m_draftStrength = 0;
  int m_draftDexterity = 0;
  int m_draftIntelligence = 0;
  int m_draftVitality = 0;
  bool m_showConfirmPopup = false;
};

} // namespace NoMoreDay::ui
