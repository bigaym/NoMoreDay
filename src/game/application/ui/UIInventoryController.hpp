#pragma once

#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include <array>
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
// R6 (remediation): the legacy immediate-mode Draw is gone. The controller now
// follows the remediation frame order (design §3.1):
//   - Update  (host Update phase, before the Escape chain): interaction phase.
//     Works from the frame snapshot + injected pointer/key state (no ECS
//     registry access). Every gameplay action is enqueued as a GameUiIntent
//     (EquipItem/UnequipItem/UseItem/DropItem/DestroyItem/SocketRune/
//     SwapItems/BagEquip/BagUnequip/StashWithdraw/OrganizeInventory) and
//     executed by GameUiCommandHandler in the NEXT gameplay Update phase. The
//     panel-local state (tabs, scroll, search, drag session) is updated here.
//   - Paint (host PrepareRender phase): registry-free, input-free draw-list
//     emission. Slots/icons/quantities come from the frame snapshot; the
//     material filter uses a revision-keyed cache (C-01: no per-frame
//     filteredList/lowercase allocations).
//   - Hover/context-menu routing goes through the host domain-id channels
//     (SetHoveredItemDomain / OpenContextMenuDomain) so the controller never
//     touches entt::entity handles.
//
// The controller is owned by GameUiHost (back-pointer for hover/context-menu
// routing, same pattern as UIStashController/UICraftingController). The host
// drives EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update
// once per frame with the frame snapshot + input, and calls Paint in
// PrepareRender so the stacking order (Panels layer) is unchanged.
class UIInventoryController {
public:
  explicit UIInventoryController(UiRuntime& runtime, GameUiHost* uiHost);
  ~UIInventoryController() = default;

  UIInventoryController(const UIInventoryController&) = delete;
  UIInventoryController& operator=(const UIInventoryController&) = delete;

  // Resets session-scoped state (page/tab/scroll/search/drag) and reveals the
  // panel root node when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // R6 (remediation, design §3.1): interaction phase. Animates the instance
  // alpha (m_visible authoritative), runs the panel drag service, updates the
  // hover/context-menu channels, and enqueues the intent for every gameplay
  // action the user performs on the panel. Reads only the frame snapshot and
  // the injected input; never touches the ECS registry. The GameUiCommandHandler
  // executes the enqueued intents in the next gameplay Update phase.
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input,
              float mouseWheel, const LevelManager& levelManager);

  // R6 (remediation, design §3.4): paint step of the draw-list pipeline.
  // Appends the panel commands (equipment slots, item grid, material list, bag
  // slots) to the host-owned draw list under the panel root node. Registry-free
  // and input-free: all display data comes from the frame snapshot plus the
  // Update-phase caches (drag mirror, scroll offsets, material filter cache).
  // Called by the host PrepareRender before Finalize.
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot) const;

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

  // R6: count of entries in the revision-keyed material filter cache
  // (C-01). Exposed for tests that assert the cache rebuild contract.
  [[nodiscard]] std::size_t MaterialFilterCount() const noexcept {
    return m_materialFilterCache.size();
  }

private:
  // Panel layout derived from the (draggable) panel origin. Shared by Update
  // (hit-testing) and Paint (drawing) so both phases see the same rects.
  struct Layout {
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelW = 0.0f;
    float panelH = 0.0f;
    float leftPanelX = 0.0f;
    float leftPanelW = 0.0f;
    float rightPanelX = 0.0f;
    float rightPanelW = 0.0f;
    float equipX = 0.0f;
    float equipY = 0.0f;
    float equipW = 0.0f;
    float equipH = 0.0f;
    float invX = 0.0f;
    float invY = 0.0f;
    float invW = 0.0f;
    float invH = 0.0f;
    float tabY = 0.0f;
    float bottomY = 0.0f;
    float bagSlotsY = 0.0f;
  };

  Layout ComputeLayout() const noexcept;

  // R6 (C-01): rebuilds the material filter cache when the snapshot revision,
  // the search query or the selected category changed. The cache stores copies
  // of the matching GameUiMaterialView (no pointers into the frame snapshot,
  // no per-frame filteredList/lowercase allocations; the lowercase query is
  // cached in m_cachedLowerSearch and substring matching is case-insensitive
  // without allocating per-material strings).
  void RebuildMaterialFilter(const GameUiSnapshot& snapshot);

  // Returns the first free socket index in [0, socketCount) or -1.
  [[nodiscard]] static int FreeSocketIndex(const GameUiItemView& item) noexcept;
  [[nodiscard]] static int FreeSocketIndex(
      const GameUiEquippedSlotView& item) noexcept;

  // Resolves the dragged item's view inside the snapshot's displayed-items
  // section (the builder resolved options.draggedItem there). Returns null
  // when the item is not part of this frame's displayed items.
  [[nodiscard]] static const GameUiItemView* FindDisplayedItem(
      const GameUiSnapshot& snapshot, std::uint64_t domainId) noexcept;

  // Routes an intent to the host (no-op in headless tests without a host).
  void EnqueueIntent(GameUiIntent intent);

  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  // U8 drag session accessor (same pattern as UIStashController): routes to
  // the host-owned session when the host is present (gameplay), otherwise to
  // a local fallback (headless tests, where no cross-panel drag can occur).
  UIDragSession& DragSession() noexcept;

  UiRuntime& m_runtime;
  // U8 back-pointer (same pattern as UIStashController/UICraftingController):
  // routes panel hover writes to the host hover channel (SetHoveredItemDomain),
  // right-click opens to the hosted overlay controller (OpenContextMenuDomain),
  // drag state to the host-owned UIDragSession, and intents to the command
  // handler queue. Null in headless unit tests, where those interactions are
  // not exercised.
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
  // R6: panel origin is now instance state (the legacy Draw recomputed the
  // centered origin every frame, so dragging never persisted).
  float m_panelX = 0.0f;
  float m_panelY = 0.0f;

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
  // R6: grid scroll offset moved out of InventoryComponent::scrollOffset (the
  // legacy Draw mutated the component from the paint path).
  float m_inventoryScrollOffset = 0.0f;
  char m_searchBuffer[64] = {0};
  // Defaults to MaterialCategory::Count ("All") once the ctor runs.
  NoMoreDay::MaterialCategory m_selectedCategory;
  bool m_isSearchFocused = false;

  // R6 (C-01): revision-keyed material filter cache (see RebuildMaterialFilter).
  std::vector<GameUiMaterialView> m_materialFilterCache;
  std::uint64_t m_filterCacheRevision = 0;
  char m_cachedLowerSearch[64] = {0};
  NoMoreDay::MaterialCategory m_cachedCategory =
      static_cast<NoMoreDay::MaterialCategory>(0);

  // R6: per-frame lookup caches built in Update from the snapshot (the paint
  // maps slot -> item view index through these; -1 = empty slot).
  std::vector<int> m_slotToItemIndex; // grid slot -> items[] index
  std::array<int, 16> m_equipSlotIndex{}; // EquipmentSlot -> equipment[] index

  // R6: drag mirror updated in Update so Paint can hide the item under the
  // cursor in its source slot (paint stays input-free).
  std::uint64_t m_draggedItemDomainId = 0;
};

} // namespace NoMoreDay::ui
