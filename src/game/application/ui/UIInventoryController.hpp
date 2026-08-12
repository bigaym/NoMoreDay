#pragma once

#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

// Forward declarations. The controller only passes these by reference, so
// declarations suffice in this header.
class LevelManager;
namespace NoMoreDay {
enum class MaterialCategory : std::uint8_t;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

class GameUiHost; // Back-pointer injected by the host (U8 hover/context-menu
                  // channel; same pattern as UIStashController).

// Instance controller for the inventory panel.
//
// Ports the legacy static panel UIInventory into a hostable instance (U8
// "inventory panel takeover"): the controller owns a UiRuntime root node
// (created in the ctor) plus the panel's instance visibility/alpha state, and
// performs the full legacy UIInventory::Draw (panel, equipment slots, item
// grid, material list, bag slots) through the hosted Draw. It holds no static
// mutable UI state; the legacy static members were migrated into instance
// members and are reset by EnterGameplay/LeaveGameplay so no session state
// leaks into the next run.
//
// Visibility is an instance flag (m_visible); SetVisible/Toggle write it and
// the runtime node only. Cross-layer writers (host KEY_C/KEY_K,
// UIStashController::Open) go through the host API, so no legacy mirror or
// re-adopt remains (U8 final narrowing).
//
// The controller is owned by GameUiHost (back-pointer for hover/context-menu
// routing, same pattern as UIStashController/UICraftingController). The host
// drives EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update
// once per frame, and calls Draw in the legacy UISystem::Draw frame position
// (after stash/crafting, before skill tree/overlays) so the stacking order is
// unchanged.
class UIInventoryController {
public:
  explicit UIInventoryController(UiRuntime& runtime, GameUiHost* uiHost);
  ~UIInventoryController() = default;

  UIInventoryController(const UIInventoryController&) = delete;
  UIInventoryController& operator=(const UIInventoryController&) = delete;

  // Resets session-scoped state (page/tab/scroll/search state) and reveals the
  // panel root node when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: animates the instance alpha towards the visibility flag
  // (m_visible is authoritative; no legacy mirror remains). Does not draw
  // anything.
  void Update(entt::registry& registry, const LevelManager& levelManager);

  // Full legacy UIInventory::Draw ported into the hosted controller: equipment
  // slots (hover/drag/quick-unequip/socketing/right-click), item grid
  // (drag/socketing/drop), material list (search/category/scroll), and the bag
  // extension slots. Hover writes and context-menu opens route through the
  // GameUiHost back-pointer (tooltip controller / overlay controller); when
  // the host is null (headless tests) those interactions are skipped.
  void Draw(entt::registry& registry);

  // --- Visibility (instance state, authoritative) ---
  // Opens (true) or closes (false) the panel. Closing resets the page and
  // closes any context menu opened from the panel (via the hosted overlay),
  // matching the legacy UIInventory::Toggle close path.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
  // U8: instance alpha (authoritative; the legacy State.inventoryAlpha pair
  // is gone). Animated by Update towards the visibility flag.
  [[nodiscard]] float Alpha() const noexcept { return m_alpha; }
  void Toggle();

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  // U8 typing-gate source: true while the inventory search box is focused
  // (host InputCapture aggregates this instead of State.isTyping).
  [[nodiscard]] bool IsSearchFocused() const noexcept {
    return m_isSearchFocused;
  }

  [[nodiscard]] bool IsInGameplay() const noexcept;

private:
  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  // U8 drag session accessor (same pattern as UIStashController): routes to
  // the host-owned session when the host is present (gameplay), otherwise to
  // a local fallback (headless tests, where no cross-panel drag can occur).
  UIDragSession& DragSession() noexcept;

  UiRuntime& m_runtime;
  // U8 back-pointer (same pattern as UIStashController/UICraftingController):
  // routes panel hover writes to the host hover channel (SetHoveredItem),
  // right-click opens to the hosted overlay controller, and drag state to the
  // host-owned UIDragSession. Null in headless unit tests, where those
  // interactions are not exercised.
  GameUiHost* m_uiHost = nullptr;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_inGameplay = false; // Session state set by Enter/LeaveGameplay.

  // U8: instance visibility + alpha animation (authoritative; the legacy
  // State.showInventory / State.inventoryAlpha pair is gone).
  bool m_visible = false;
  float m_alpha = 0.0f;
  // Headless-test fallback for the drag session (see DragSession).
  UIDragSession m_localDragSession;

  // U8: instance panel-drag state (was UISystem::State.panelStates + the
  // UISystem::UpdatePanelDrag helper; same pattern as UIStashController).
  NoMoreDay::PanelState m_panelState;
  UIPanelID m_activeDragPanel = UIPanelID::None;

  // U8: equipment-slot hover animation (was UISystem::State.equipmentSlotAnims;
  // sized in the ctor to the Draw loop bound).
  struct ElementAnim {
    float hoverValue = 0.0f;
  };
  std::vector<ElementAnim> m_equipmentSlotAnims;

  // Session-scoped panel state migrated from the legacy static members of
  // UIInventory (U7 cleanup: static mutable state -> instance members).
  int m_inventoryPage = 0;
  int m_activeTab = 0; // 0: Items, 1: Materials
  float m_materialScrollOffset = 0.0f;
  char m_searchBuffer[64] = {0};
  // Defaults to MaterialCategory::Count ("All") once the ctor runs.
  NoMoreDay::MaterialCategory m_selectedCategory;
  bool m_isSearchFocused = false;
};

} // namespace NoMoreDay::ui
