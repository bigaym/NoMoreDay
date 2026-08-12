#pragma once

#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <cstdint>
#include <utility>
#include <vector>

// Forward declaration. The controller exposes the active stash type by value
// and stores it as an instance member, so a declaration suffices here.
namespace NoMoreDay {
enum class StashType : std::uint8_t;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

class GameUiHost; // Back-pointer injected by the host (U8 hover channel).

// Instance controller for the stash panel.
//
// R7 (remediation): the legacy immediate-mode Draw(entt::registry&) is gone.
// The controller follows the remediation frame order (design §3.1), matching
// the R6 UIInventoryController pattern:
//   - Update (host Update phase, before the Escape chain): interaction phase.
//     Works from the frame snapshot + injected pointer/key state (no ECS
//     registry access). Tab/lock/search state is UI-local; every gameplay
//     action (unlock tab, ctrl+click quick withdraw, drag transfer/deposit,
//     sort, auto-deposit) is enqueued as a GameUiIntent and executed by the
//     GameUiCommandHandler in the NEXT gameplay Update phase. No StashTab*
//     pointer is kept across operations (B-01: the handler re-fetches the tab
//     per execution); the drag session stores only domain ids + source
//     metadata (R1: UIDragSession).
//   - Paint (host PrepareRender phase): registry-free, input-free draw-list
//     emission. Tabs/slots/lock-cost/search-matches all come from the frame
//     snapshot (GameUiStashView; the builder computes the per-slot
//     matchesSearch flags from the UI search query).
//
// The controller is owned by GameUiHost (back-pointer for hover/context-menu
// routing and the shared drag session). The host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update once per
// frame with the frame snapshot + input, and calls Paint in PrepareRender so
// the stacking order (Panels layer) is unchanged.
class UIStashController {
public:
  explicit UIStashController(UiRuntime& runtime, GameUiHost* uiHost);
  ~UIStashController() = default;

  UIStashController(const UIStashController&) = delete;
  UIStashController& operator=(const UIStashController&) = delete;

  // Resets session-scoped state (active type/tab, search buffer, focus) and
  // reveals the panel root node when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // R7 (remediation, design §3.1): interaction phase. Animates the instance
  // alpha (m_visible authoritative), runs the panel drag service, updates the
  // hover channel and the search focus/input, and enqueues the intent for
  // every gameplay action (unlock tab / quick withdraw / transfer / deposit /
  // sort / auto-deposit). Reads only the frame snapshot and the injected
  // input; never touches the ECS registry.
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input);

  // R7 (remediation, design §3.4): paint step of the draw-list pipeline.
  // Appends the panel commands (title, tabs, unlock button, grid slots, search
  // bar, footer buttons) to the host-owned draw list under the panel root
  // node. Registry-free and input-free: all display data comes from the frame
  // snapshot (GameUiStashView) plus the Update-phase UI-local state (active
  // tab, search buffer).
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot) const;

  // Opens/closes/toggles the stash panel. Mirrors the legacy UIStash API:
  // Open also reveals the inventory panel (drag destination) and resets the
  // active tab to the first one, Toggle flips the current visibility and
  // Close hides the stash only.
  void Open(NoMoreDay::StashType type);
  void Close();
  void Toggle();

  // U8: instance visibility/alpha (replacing the legacy State.showStash /
  // stashAlpha pair).
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] float Alpha() const noexcept { return m_alpha; }
  // U8 typing-gate source: true while the stash search box is focused
  // (InputCapture aggregates this instead of State.isTyping).
  [[nodiscard]] bool IsSearchFocused() const noexcept {
    return m_isSearchFocused;
  }
  [[nodiscard]] NoMoreDay::StashType GetActiveType() const noexcept;
  [[nodiscard]] int GetActiveTabIndex() const noexcept;

  // R7: the search query buffer consumed by the host SnapshotOptions (the
  // builder computes the per-slot matchesSearch flags from it).
  [[nodiscard]] const char* SearchQuery() const noexcept { return m_searchBuffer; }

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

private:
  // Panel layout derived from the (draggable) panel origin. Shared by Update
  // (hit-testing) and Paint (drawing) so both phases see the same rects.
  struct Layout {
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelW = 0.0f;
    float panelH = 0.0f;
    float tabY = 0.0f;
    float gridX = 0.0f;
    float gridY = 0.0f;
    float footerY = 0.0f;
  };

  Layout ComputeLayout() const noexcept;

  // R7: first free inventory slot index for the ctrl+click quick-withdraw
  // (computed from the snapshot inventory, never the registry); -1 when full.
  [[nodiscard]] static int FirstFreeInventorySlot(
      const GameUiSnapshot& snapshot) noexcept;

  // Routes an intent to the host (no-op in headless tests without a host).
  void EnqueueIntent(GameUiIntent intent);

  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  // U8 drag session accessor: routes to the host-owned session when the host
  // is present (gameplay), otherwise to a local fallback (headless tests,
  // where no cross-panel drag can occur anyway).
  UIDragSession& DragSession() noexcept;

  UiRuntime& m_runtime;
  // U8: borrowed back-pointer to the owning GameUiHost; used to forward panel
  // hover writes to the tooltip controller's hover source, to access the
  // host-owned drag session and to enqueue intents.
  GameUiHost* m_uiHost = nullptr;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_inGameplay = false; // Session state set by Enter/LeaveGameplay.

  // U8: instance visibility + alpha animation + panel drag position,
  // replacing the legacy State.showStash / stashAlpha / panelStates[Stash]
  // triple.
  bool m_visible = false;
  float m_alpha = 0.0f;
  PanelState m_panelState;
  UIPanelID m_activeDragPanel = UIPanelID::None;
  // Headless-test fallback for the drag session (see DragSession).
  UIDragSession m_localDragSession;
  // R7: panel origin is instance state so dragging persists across frames
  // (same pattern as the R6 inventory controller).
  float m_panelX = 0.0f;
  float m_panelY = 0.0f;

  // Session-scoped panel state migrated from the legacy static members of
  // UIStash (U7 cleanup: static mutable state -> instance members).
  // Defaults to StashType::Personal once the ctor runs.
  NoMoreDay::StashType m_activeType;
  int m_activeTabIndex = 0;
  // R7: only the query buffer + focus flag remain UI-local; the search
  // matching moved into the builder (options.stashSearchQuery -> per-slot
  // matchesSearch). The legacy m_lastSearchBuffer/m_cachedSearchResults cache
  // is gone.
  char m_searchBuffer[64] = {0};
  bool m_isSearchFocused = false;
  // R7: unlock-button hover state computed in Update so Paint can draw the
  // cost tooltip without reading input (paint stays input-free).
  bool m_unlockButtonHovered = false;
  UiVec2 m_unlockTooltipPos{};
};

} // namespace NoMoreDay::ui
