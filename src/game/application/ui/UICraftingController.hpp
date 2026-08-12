#pragma once

#include <entt/entt.hpp>

#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <cstdint>

namespace NoMoreDay::ui {

class GameUiHost; // Back-pointer injected by the host (U8 hover channel).

// Instance controller for the crafting panel (锻造 / 融合 / 分解).
//
// R7 (remediation): the legacy immediate-mode Draw(entt::registry&) is gone.
// The controller follows the remediation frame order (design §3.1), matching
// the R6 UIInventoryController pattern:
//   - Update (host Update phase, before the Escape chain): interaction phase.
//     Works from the frame snapshot + injected pointer/key state (no ECS
//     registry access). The forge/merge/salvage selections are UI-local
//     domain ids (R1: the 5 Get*DomainId getters; the internal storage holds
//     std::uint64_t domain ids, never entt::entity targets — B-01). Every
//     gameplay action (affix upgrade/chaos/refine/add, fuse, salvage, batch
//     salvage) is enqueued as a GameUiIntent and executed by the
//     GameUiCommandHandler in the NEXT gameplay Update phase (the handler
//     fetches ItemComponent briefly before each single system call and drops
//     it right after — EnTT safety).
//   - Paint (host PrepareRender phase): registry-free, input-free draw-list
//     emission. Slots/affix rows/yield preview/material bank all come from the
//     frame snapshot (GameUiCraftingView + displayedItems + salvageYield).
//
// The controller is owned by GameUiHost (back-pointer for hover routing and
// the shared drag session). The host drives EnterGameplay/LeaveGameplay around
// gameplay sessions, feeds Update once per frame with the frame snapshot +
// input, and calls Paint in PrepareRender so the stacking order (Panels layer)
// is unchanged.
class UICraftingController {
public:
  // U8: the host back-pointer routes panel hover writes through the host's
  // SetHoveredItem channel (instance hover pipeline) instead of the static
  // UiShared::HoveredItem() slot.
  explicit UICraftingController(UiRuntime& runtime, GameUiHost* uiHost);
  ~UICraftingController() = default;

  UICraftingController(const UICraftingController&) = delete;
  UICraftingController& operator=(const UICraftingController&) = delete;

  // Resets session-scoped state (forge/merge/salvage targets, salvage filter,
  // tab, alpha) and mirrors the panel node when a gameplay session begins.
  // Like the legacy UICrafting, the panel starts closed on EnterGameplay.
  // Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // R7 (remediation, design §3.1): interaction phase. Animates the instance
  // alpha (m_visible authoritative), runs the panel drag service, drops stale
  // domain-id targets that are absent from this frame's displayedItems, and
  // enqueues the intent for every gameplay action (affix upgrade/chaos/refine/
  // add, fuse, salvage, batch salvage). Reads only the frame snapshot and the
  // injected input; never touches the ECS registry.
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input);

  // R7 (remediation, design §3.4): paint step of the draw-list pipeline.
  // Appends the panel commands (tabs, forge/merge/salvage slots, affix rows,
  // yield preview, material bank, filter popup) to the host-owned draw list
  // under the panel root node. Registry-free and input-free: all display data
  // comes from the frame snapshot.
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot) const;

  // Sets the item in the forge slot and opens the panel (same semantics as the
  // legacy UICrafting::SetTargetItem, used by the item context menu). R7: the
  // entity is converted to a stable domain id at the boundary; the controller
  // never stores entt::entity targets.
  void SetTargetItem(entt::entity item);

  // Domain id of the item currently placed in the forge slot
  // (kInvalidDomainId when empty). R7: the legacy GetTargetItem() entity
  // accessor is replaced by this domain-id accessor.
  [[nodiscard]] std::uint64_t GetForgeTargetDomainId() const noexcept;

  // R1 (remediation): domain-id accessors for the snapshot options. The
  // builder receives session targets as stable integer ids (design §3.2),
  // never entity handles.
  [[nodiscard]] std::uint64_t GetMergeBaseDomainId() const noexcept;
  [[nodiscard]] std::uint64_t GetMergeFodderDomainId() const noexcept;
  [[nodiscard]] std::uint64_t GetMergeCatalystDomainId() const noexcept;
  [[nodiscard]] std::uint64_t GetSalvageItemDomainId() const noexcept;

  // Clears the forge slot without closing the panel.
  void ClearTargetItem();

  // R7: clears whichever forge/merge/salvage session target matches the given
  // domain id (called by the host result-consumption loop when a destructive
  // success reported the id through clearedDomainIds).
  void ClearConsumedTarget(std::uint64_t domainId);

  // Toggles panel visibility (and the runtime node). Idempotent.
  void Toggle();

  // R3 (remediation, design §3.6): unconditional close used by the host Escape
  // chain (Escape closes the crafting panel like any other topmost surface).
  // Mirrors the stash Close(); leaves session targets intact so the panel can
  // be reopened by K or the context menu without losing state.
  void Close();

  [[nodiscard]] bool IsVisible() const noexcept;

  // Opens the panel directly on the legendary-merge tab (used when consuming
  // a Legendary Core item).
  void OpenMergePanel();

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

private:
  // Batch salvage filter (ported from the legacy namespace-scope SalvageFilter
  // struct). Defaults are applied in the ctor and on every session reset; the
  // Rarity enumerators they need are only complete in the .cpp.
  struct SalvageFilter {
    std::uint32_t rarityMask = 0;
    std::uint32_t categoryMask = 0;
    bool keepIfTier6Plus = true;
    bool excludeLocked = true;
  };

  enum class CraftingTab { Forging, Merging, Salvaging };

  // Panel layout derived from the (draggable) panel origin. Shared by Update
  // (hit-testing) and Paint (drawing) so both phases see the same rects.
  struct Layout {
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelW = 0.0f;
    float panelH = 0.0f;
    float tabY = 0.0f;
  };

  Layout ComputeLayout() const noexcept;

  // R7: tab-scoped interaction phases (called from Update).
  void UpdateForgingTab(const GameUiSnapshot& snapshot, const UiInputFrame& input,
                        const Layout& layout, UIDragSession& drag);
  void UpdateMergingTab(const GameUiSnapshot& snapshot, const UiInputFrame& input,
                        const Layout& layout, UIDragSession& drag);
  void UpdateSalvagingTab(const GameUiSnapshot& snapshot, const UiInputFrame& input,
                          const Layout& layout, UIDragSession& drag);

  // R7: tab-scoped paint phases (called from Paint; registry-free).
  void PaintForgingTab(UiDrawList& drawList, const GameUiSnapshot& snapshot,
                       const Layout& layout, float alpha) const;
  void PaintMergingTab(UiDrawList& drawList, const GameUiSnapshot& snapshot,
                       const Layout& layout, float alpha) const;
  void PaintSalvagingTab(UiDrawList& drawList, const GameUiSnapshot& snapshot,
                         const Layout& layout, float alpha) const;

  // Resolves the target's view inside the snapshot's displayed-items section
  // (the builder resolved the crafting targets there). Returns null when the
  // item is not part of this frame's displayed items.
  [[nodiscard]] static const GameUiItemView* FindDisplayedItem(
      const GameUiSnapshot& snapshot, std::uint64_t domainId) noexcept;

  // Routes an intent to the host (no-op in headless tests without a host).
  void EnqueueIntent(GameUiIntent intent);

  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  // U8 drag session accessor (same pattern as UIInventoryController): routes
  // to the host-owned session when the host is present (gameplay), otherwise
  // to a local fallback (headless tests, where no cross-panel drag can occur).
  UIDragSession& DragSession() noexcept;

  UiRuntime& m_runtime;
  // U8: borrowed back-pointer to the owning GameUiHost; used to forward panel
  // hover writes to the tooltip controller's hover source and to route the
  // drag session reads (was UISystem::State.draggedItem).
  GameUiHost* m_uiHost = nullptr;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_inGameplay = false; // Session state set by Enter/LeaveGameplay.

  // U8: instance panel-drag state (was UISystem::State.panelStates + the
  // UISystem::UpdatePanelDrag helper; same pattern as UIStashController).
  NoMoreDay::PanelState m_panelState;
  UIPanelID m_activeDragPanel = UIPanelID::None;
  // Headless-test fallback for the drag session (see DragSession).
  UIDragSession m_localDragSession;
  // R7: panel origin is instance state so dragging persists across frames
  // (same pattern as the R6 inventory controller).
  float m_panelX = 0.0f;
  float m_panelY = 0.0f;

  // Session-scoped panel state migrated from the legacy static members of
  // UICrafting (U7 cleanup: static mutable state -> instance members).
  // R7 (B-01): targets are stable integer domain ids (kInvalidDomainId = 0
  // when empty); no entt::entity target is stored across operations.
  std::uint64_t m_forgeTarget = kInvalidDomainId;
  std::uint64_t m_mergeBase = kInvalidDomainId;
  std::uint64_t m_mergeFodder = kInvalidDomainId;
  std::uint64_t m_mergeCatalyst = kInvalidDomainId;
  std::uint64_t m_salvageItem = kInvalidDomainId;
  int m_selectedAffixIndex = -1;
  SalvageFilter m_salvageFilter;
  bool m_showSalvageFilter = false;
  CraftingTab m_currentTab = CraftingTab::Forging;
  float m_craftingAlpha = 0.0f;
  bool m_visible = false; // Mirrored to the runtime node visibility.
};

} // namespace NoMoreDay::ui
