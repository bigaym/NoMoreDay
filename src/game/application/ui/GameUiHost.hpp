#pragma once

#include "game/application/ui/AstrolabeController.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/MinimapController.hpp"
#include "game/application/ui/MonsterHealthBarController.hpp"
#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/PlayerHudController.hpp"
#include "game/application/ui/SkillHotbarController.hpp"
#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UICharacterController.hpp"
#include "game/application/ui/UICraftingController.hpp"
#include "game/application/ui/UIInventoryController.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UIStashController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRaylibBackend.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <entt/entt.hpp>

#include "raylib.h"

class ResourceManager;
class LevelManager;
namespace NoMoreDay {
namespace systems {
class SpatialHashGrid;
}
} // namespace NoMoreDay

namespace NoMoreDay::ui {

class WorldUiFrame;

// Composition root of the UI layer (design §4.2 lifecycle). Owns the retained
// runtime core (UiViewport / UiRuntime / UiRaylibBackend) plus the draw list
// and gameplay-session scoping. Not a singleton: the application owns exactly
// one instance and injects it into states via SharedContext.
//
// U8 final: Update/Draw own the full UI orchestration in-place (the legacy
// UISystem::Update/Draw/DrawDraggingPhantom facade functions and the static
// UIContext are gone); the panel controllers and the overlay are the
// authoritative instance state, and the host provides the cross-panel
// channels (drag session, hover, modal/pointer gates, message box).
class GameUiHost {
public:
  GameUiHost();
  ~GameUiHost() = default;

  GameUiHost(const GameUiHost &) = delete;
  GameUiHost &operator=(const GameUiHost &) = delete;

  // --- Lifecycle (design §4.2) ---
  // Called by the composition root after the window/resource system is up.
  void Initialize(ResourceManager &resourceManager);
  // Must run before resource unload / window close (see Game::cleanup order).
  void Shutdown();

  // Gameplay session scoping. Entering/leaving resets gameplay-scoped UI
  // session state so no panel/drag/tooltip state leaks into the next run.
  void EnterGameplay();
  void LeaveGameplay();

  // Per-frame update. Owns the full UI update orchestration in-place (global
  // hotkeys, animations, per-frame panel updates; see GameUiHost.cpp).
  // Compatibility overload that runs the update against an empty snapshot;
  // the gameplay Update phase should prefer the snapshot overload below.
  void Update(entt::registry &registry, const LevelManager &levelManager);

  // U6b: update against the frame-scoped read-only snapshot. Saves the
  // snapshot for later panel migrations, consumes pending intent results
  // through the compatibility bridge (failure notifications surface via the
  // hosted message box), then runs the in-place UI update.
  void Update(entt::registry &registry, const LevelManager &levelManager,
              const GameUiSnapshot &snapshot);

  // --- Pickup intent loop (U6b, design §6.2) ---
  // Render-phase code (Draw) enqueues intents with read-only detection; the
  // gameplay Update phase drains them and executes each through
  // GameUiCommandHandler. No ECS write ever happens inside the host.
  void EnqueueIntent(GameUiIntent intent);
  // Moves all pending intents out and clears the queue. Called once per
  // Update by the gameplay state after the UI update.
  [[nodiscard]] std::vector<GameUiIntent> DrainUpdateIntents();
  // Feeds a command-handler result back to the host; the next Update surfaces
  // a failure notification through the compatibility bridge.
  void Publish(const GameUiResult &result);

  // Aggregated UI input capture for gameplay input gating (U5). Starts from
  // the retained runtime capture and merges the transitional legacy gate
  // queries (U7 panel migration removes these). Call after Update so the
  // capture reflects the current frame's UI state. R8: registry-free (the
  // astrolabe/skill-tree panels report their instance visibility).
  [[nodiscard]] UiInputCapture InputCapture() const;

  // R4 (remediation, design §3.1/§3.4): render-phase paint. Starts a fresh
  // draw list and paints the migrated surfaces (message box) into the
  // host-owned buffers, then Finalize() sorts the commands into the total draw
  // order. Call after the scene composite, before Draw (which submits the
  // finalized list through the backend). The viewport fit / runtime
  // reconcile/input/layout steps run in Update.
  void PrepareRender();

  // U8 final: draws the full UI in-place (scale fit, per-frame hover/mouse
  // resets, panel passes, overlays, tooltip state machine; see
  // GameUiHost.cpp), then submits the finalized draw list (prepared and
  // sorted by PrepareRender) through the raylib backend in a single pass.
  // Must run after the scene composite and before EndDrawing.
  // R8: the registry parameter is gone — all panel state is snapshot/intent
  // driven and painted through the draw list (A-01/B-01 closure).
  void Draw(const LevelManager &levelManager, const Camera2D &camera,
            NoMoreDay::systems::SpatialHashGrid *spatialGrid = nullptr);

  // U8 host read-side migration: binds the frame-scoped WorldUiFrame the
  // render adapter fills with visible ground-item hit proxies. The host reads
  // it for ground pickup detection (DetectPickupClick) and forwards it to the
  // tooltip controller (ground hover detection + hover highlight write-back).
  // Called by the composition root (Game) after the frame is created; the
  // host degrades gracefully (no ground pickup / hover) until bound.
  void BindWorldFrame(NoMoreDay::ui::WorldUiFrame *frame);

  // U7 group 1: panel routes. GameplayState calls these at the original
  // legacy call positions so the native UI render order is unchanged; the
  // legacy static panels remain only as fallbacks for the null-host path.
  void DrawMinimap(entt::registry &registry, const LevelManager &levelManager,
                   NoMoreDay::systems::SpatialHashGrid *grid = nullptr);
  void DrawHud(entt::registry &registry);
  // U8 final: the drag phantom + top-most tooltip pass (was
  // UISystem::DrawDraggingPhantom). GameplayState calls this at the original
  // legacy position (after the player HUD); R8: the phantom is registry-free
  // (the item/skill preview is painted from the drag session + snapshot by
  // the Draw pass, this legacy position is a no-op kept for the call order).
  void DrawDraggingPhantom();
  // Monster health bars: world-pass overhead bars + hover pick (runs inside
  // Mode2D) and the screen-pass top-center target widget.
  void RenderMonsterHealthBars(entt::registry &registry,
                               const Camera2D &camera);
  void RenderMonsterHealthBarsUI(entt::registry &registry);
  // Skill hotbar + buff strip draws in-place inside UISystem::Draw (frame
  // order coupling with the tooltip state machine; UISystem::Draw receives
  // the controller via its hotbarController parameter).
  // U7 group 2: character panel route. GameplayState calls this at the
  // original legacy position (gated by UISystem::State.showCharacterPanel /
  // characterPanelAlpha, unchanged).
  void DrawCharacter(entt::registry &registry);
  // U7 group 3: stash/crafting routes. UISystem::Update/Draw receive these
  // controllers in place of the legacy static calls (frame-order coupling
  // with the KEY_E interaction and KEY_K toggle, so they route in-place).
  // CraftingOpenMergePanel / CraftingSetTargetItem are the cross-layer
  // crafting entry points. CraftingOpenMergePanel is wired through the
  // SharedContext callback (InventorySystem Legendary Core use);
  // CraftingSetTargetItem is the host channel for the overlay context-menu
  // Craft action (OverlayController::ActivateContextMenuButton; the old
  // SharedContext craftingSetTargetItem callback was removed in R10 as it had
  // no callers). Both delegate to the hosted UICraftingController.
  void CraftingOpenMergePanel();
  void CraftingSetTargetItem(entt::entity item);
  // U7 group 5: astrolabe close entry point for the skill-tree sibling
  // coupling (wired through SharedContext.closeAstrolabe, filled by Game).
  void CloseAstrolabe();

  // U8 panel-hover convergence channel: forwards the hovered item to the
  // tooltip controller's hover source (TooltipController::SetHoveredItemDomain).
  // R8: the entity overload is gone (dead code, no callers); panel hover write
  // points route through SetHoveredItemDomain instead, so hover flows into the
  // instance hover pipeline (UpdateState write-back) as a stable domain id.
  // Pass 0 to clear the hover source.
  void SetHoveredItemDomain(std::uint64_t domainId) noexcept {
    m_hoveredItemDomainId = domainId;
  }

  // R1 (remediation): collects the UI session display requests (hover/drag/
  // crafting targets/active stash tab) the snapshot builder needs to resolve
  // the displayed-items cache. Pure read of UI-local state; never writes
  // gameplay (design §3.2).
  [[nodiscard]] GameUiSnapshotOptions SnapshotOptions() const;

  // U8 inventory takeover: host routes for the inventory panel instance
  // visibility (the panel is an instance controller now; the legacy
  // GameplayState PushState<InventoryState> path is gone). GameplayState uses
  // these for the KEY_I/Esc gating and the anyPanelOpen check; InputCapture
  // uses IsInventoryVisible to capture pointer input while the panel is open.
  void SetInventoryVisible(bool visible);
  [[nodiscard]] bool IsInventoryVisible() const noexcept;

  // U8 final: opens the item context menu through the hosted
  // OverlayController. The inventory controller routes its right-click
  // interaction through this host channel.
  void OpenContextMenu(entt::entity item, bool fromInventory, int inventoryIndex,
                       NoMoreDay::EquipmentSlot slot);

  // R6: domain-id context-menu channel. The snapshot-driven inventory
  // controller only carries stable domain ids (never entt::entity handles on
  // the interaction path); the host re-resolves the entity and forwards to the
  // overlay. The overlay validates the entity before building its menu, so an
  // unresolvable id degrades to a closed menu (no crash).
  void OpenContextMenuDomain(std::uint64_t domainId, bool fromInventory,
                             int inventoryIndex,
                             NoMoreDay::EquipmentSlot slot);

  // --- U8 host read-side migration: aggregated panel visibility ---
  // True while any hosted panel/overlay is open (inventory, skill tree,
  // character, stash, crafting, astrolabe). Replaces the legacy
  // State.showSkillTree || State.showCharacterPanel anyPanelOpen check in
  // GameplayState. R8: registry-free; each hosted controller reports its own
  // instance visibility (character also counts the fading-out alpha).
  [[nodiscard]] bool IsAnyPanelOpen() const;

  // U8 final: character-panel visibility channel. The KEY_C/ESC writers and
  // GameplayState route through this host channel; the controller owns the
  // instance visibility and alpha animation.
  void SetCharacterPanelVisible(bool visible);
  [[nodiscard]] bool IsCharacterPanelVisible() const noexcept;

  // R6: opens the attribute-confirm focus surface (test seam mirroring the
  // UpdateInput confirm-click path; the Escape chain closes it before the
  // panel).
  void ShowCharacterConfirmPopup() { m_character.ShowConfirmPopup(); }

  // U8 astrolabe sibling-close channel (was AstrolabeController writing
  // State.showInventory/showCharacterPanel/showSkillTree=false on open).
  void CloseInventory();
  void CloseCharacterPanel();
  void CloseSkillTree();
  // U8 astrolabe sibling-close channel for the context menu (was the same
  // legacy State.write; forwards to the hosted OverlayController).
  void CloseContextMenu();

  // U8 final: quantity popup and skill-tree toggle forwarded to the hosted
  // overlay / skill-tree controller (tests drive the modal input gate through
  // these).
  void OpenQuantityPopup(entt::entity item, int actionType,
                         int quantityMax = 1) {
    m_overlay.OpenQuantityPopup(item, actionType, quantityMax);
  }
  void CloseQuantityPopup() { m_overlay.CloseQuantityPopup(); }
  void ToggleSkillTree() {
    m_skillTree.Toggle();
  }

  // R3 test seams: forward to the hosted stash/astrolabe controllers so the
  // Escape truth-table tests can open every closeable surface without
  // simulating the KEY_E / KEY_N interaction routes (same pattern as the
  // OpenQuantityPopup / ToggleSkillTree seams above). ShowAstrolabe uses the
  // controller's visibility-only Show() (the KEY_N route would run the full
  // Toggle including asset/shader loading, which is not available headless).
  void OpenStash(NoMoreDay::StashType type) { m_stash.Open(type); }
  void ShowAstrolabe() { m_astrolabe.Show(); }

  // R3 read seams: surface visibility forwards used by the Escape truth-table
  // tests (and by the host itself for the close-policy chain).
  [[nodiscard]] bool IsQuantityPopupVisible() const noexcept {
    return m_overlay.IsQuantityPopupVisible();
  }
  [[nodiscard]] bool IsContextMenuVisible() const noexcept {
    return m_overlay.IsContextMenuVisible();
  }
  [[nodiscard]] bool IsSkillTreeVisible() const noexcept {
    return m_skillTree.IsVisible();
  }
  [[nodiscard]] bool IsAstrolabeVisible() const noexcept {
    return m_astrolabe.IsVisible();
  }
  [[nodiscard]] bool IsStashVisible() const noexcept {
    return m_stash.IsVisible();
  }
  [[nodiscard]] bool IsCraftingVisible() const noexcept {
    return m_crafting.IsVisible();
  }

  // U8 message box channel: forwards to the hosted OverlayController. Wired
  // to SharedContext.showMessageBox by Game so gameplay-layer systems
  // (InventorySystem) surface notifications without including UI headers.
  void ShowMessageBox(const char *text);

  // U8 final: message box visibility/text (for tests and tech guards; the
  // hosted OverlayController owns the state).
  [[nodiscard]] bool IsMessageBoxVisible() const noexcept {
    return m_overlay.IsMessageBoxVisible();
  }
  [[nodiscard]] const char* MessageBoxText() const noexcept {
    return m_overlay.MessageBoxText();
  }
  // U8 final: dismiss the hosted message box (forwarded; tests use this to
  // reset the overlay between cases).
  void ClearMessageBox() { m_overlay.HideMessageBox(); }

  // U8 drag session: the single instance owner of the cross-panel drag state
  // (replaces the legacy State.draggedItem/isDragging*/dragSource* fields).
  // Inventory/stash controllers and the skill hotbar/hub read and write this
  // session through the host back-pointer; GameplayState's drag cleanup calls
  // ClearDragSession.
  [[nodiscard]] UIDragSession &DragSession() noexcept { return m_dragSession; }
  void ClearDragSession() noexcept { m_dragSession.Clear(); }
  [[nodiscard]] bool IsDragging() const noexcept {
    return m_dragSession.IsDragging();
  }

  // U8 skill-hover channel: forwards the hovered skill id to the tooltip
  // controller (the skill hub/tree hover writes route through this host
  // channel instead of the static State.hoveredSkillId slot; F2 removes the
  // slot).
  void SetHoveredSkillId(uint32_t skillId);

  // U8 typing-gate aggregation: true while any text input is focused
  // (inventory/stash search, quantity popup). Replaces the legacy
  // State.isTyping read in InputCapture; the per-frame write in GameplayState
  // is removed with it (the controllers own their focus flags).
  [[nodiscard]] bool IsTyping() const noexcept;

  // --- U8 final: host-owned per-frame UI gates (legacy static state gone) ---
  // Per-frame pointer-capture flag written by the panel controllers (hotbar /
  // stash / inventory hover) through this host channel; InputCapture reads it
  // (replaces the legacy UISystem::State.isMouseOverUI slot).
  void SetMouseOverUI(bool v) noexcept { m_mouseOverUI = v; }
  // Modal surfaces (quantity popup, skill tree) capture all input. Replaces
  // the legacy UISystem::IsModalInputCaptured() query.
  [[nodiscard]] bool IsModalInputCaptured() const noexcept {
    return m_overlay.IsQuantityPopupVisible() || m_skillTree.IsVisible();
  }
  // Skill-slot right-click opens the skill context menu through the hosted
  // overlay (replaces the legacy State field writes in the hotbar).
  void OpenSkillContextMenu(int skillSlot) {
    m_overlay.OpenSkillContextMenu(skillSlot);
  }

  // --- R3 (remediation, design §3.6): UI Escape single ownership ---
  // The host is the sole owner of the UI Escape key. HandleEscape runs the
  // close-policy chain (modal -> focus -> z-order/open order) and closes
  // exactly ONE topmost surface; EscapeConsumedThisFrame reports whether the
  // key was consumed by the last Update so GameplayState only pauses when no
  // UI surface consumed it (H-02: the same key is never consumed twice).
  // Public so tests can drive the chain without raylib input.
  [[nodiscard]] bool EscapeConsumedThisFrame() const noexcept {
    return m_escapeConsumedThisFrame;
  }
  // Closes exactly one topmost surface per priority order (design §3.6):
  // quantity popup -> character confirm/panel -> context menu -> skill tree ->
  // astrolabe -> inventory -> stash -> crafting. Returns true when a surface
  // was closed (the key was consumed); false when nothing was open (the key
  // is left for gameplay to consume, e.g. PushState<PauseState>).
  bool HandleEscape();

  // --- New runtime core access (migration stages U5+) ---
  [[nodiscard]] UiViewport &Viewport() noexcept { return m_viewport; }
  [[nodiscard]] const UiViewport &Viewport() const noexcept {
    return m_viewport;
  }
  [[nodiscard]] UiRuntime &Runtime() noexcept { return m_runtime; }
  [[nodiscard]] const UiRuntime &Runtime() const noexcept { return m_runtime; }
  [[nodiscard]] UiRaylibBackend &Backend() noexcept { return m_backend; }
  [[nodiscard]] const UiRaylibBackend &Backend() const noexcept {
    return m_backend;
  }
  [[nodiscard]] UiDrawList &DrawList() noexcept { return m_drawList; }
  [[nodiscard]] const UiDrawList &DrawList() const noexcept {
    return m_drawList;
  }

  [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
  [[nodiscard]] bool IsInGameplay() const noexcept { return m_inGameplay; }

private:
  // Clears gameplay-scoped session state (host-owned drag session; the panel
  // controllers reset their own state on Enter/LeaveGameplay).
  void ResetSessionState();

  // Render-phase ground-item pickup click detection (U6b). Read-only: checks
  // the visible item cache, player/item distance (player from the frame
  // snapshot) and entity validity, then enqueues a PickupItem intent. Never
  // mutates the ECS.
  void DetectPickupClick(const Camera2D &camera);

  // R8: drag-phantom paint (called by Draw right before the tooltip command,
  // after PrepareRender's Finalize). Paints the item/skill preview from the
  // host-owned drag session + the frame snapshot through the draw list
  // (DragPreview layer); never re-resolves entities.
  void PaintDragPhantom(UiDrawList& drawList);

  UiViewport m_viewport;
  UiRuntime m_runtime;
  UiRaylibBackend m_backend;
  UiDrawList m_drawList;

  // U7 group 1: panel controllers owned by the host. Created in the ctor
  // (they register their root nodes with m_runtime). EnterGameplay /
  // LeaveGameplay / Draw routes delegate to them; the legacy static panels
  // keep their output as the null-host fallback only.
  PlayerHudController m_playerHud;
  MinimapController m_minimap;
  // U7 group 6-B: tooltip state machine. Declared before m_skillHotbar so the
  // hotbar can inject it (hover writes route through the controller instead of
  // UISystem::State). ResetFrame/UpdateState run inside Draw; the tooltip draw
  // routes through DrawDraggingPhantom.
  TooltipController m_tooltip;
  SkillHotbarController m_skillHotbar;
  MonsterHealthBarController m_monsterHealthBars;
  // U7 group 2: inventory alpha animation (legacy UIInventory::Update) and
  // character panel route through the host.
  UIInventoryController m_inventory;
  UICharacterController m_character;
  // U7 group 3: stash and crafting panels route in-place through
  // UISystem::Update/Draw parameters (KEY_E/KEY_K frame coupling).
  UIStashController m_stash;
  UICraftingController m_crafting;
  // U7 group 4: skill specialization UI (mastery hub + talent tree). KEY_S /
  // ESC / alpha animation route in-place through UISystem::Update; the draw
  // stage routes through UISystem::Draw.
  SkillTreeController m_skillTree;
  // U7 group 5: astrolabe panel. KEY_N / ESC / per-frame Update/Draw route
  // in-place through UISystem::Update/Draw; Enter/LeaveGameplay reset the
  // session-scoped panel state.
  AstrolabeController m_astrolabe;
  // U7 group 6: the three global overlays (context menu, quantity popup,
  // message box). Draw/Update route in-place through UISystem::Draw/Update
  // (frame-order coupling with the tooltip state machine and ESC handling);
  // the message box timer decays via UpdateMessageBox right after the legacy
  // update. R4: the message box is the first surface painted through the draw
  // list (PrepareRender -> PaintMessageBox -> Finalize -> backend submit);
  // its runtime node is reconciled by ReconcileRuntime each Update.
  // Enter/LeaveGameplay reset all overlay state.
  OverlayController m_overlay;

  // U8: cross-panel drag session (single instance owner; see DragSession).
  UIDragSession m_dragSession;

  // Frame-scoped read model handed in by the Update phase (U6b). Retained so
  // later panel migrations (U7) can read it without re-querying the ECS.
  GameUiSnapshot m_snapshot{};
  // Intents queued by render-time detection; executed by the gameplay Update
  // phase via GameUiCommandHandler. Cleared by DrainUpdateIntents.
  std::vector<GameUiIntent> m_pendingIntents;
  // Results published back by the gameplay Update phase; consumed (and
  // cleared) on the next Update through the compatibility bridge.
  std::vector<GameUiResult> m_pendingNotifications;

  // U8: frame-scoped world UI bridge (bound via BindWorldFrame by the
  // composition root; null until then). Read side: ground pickup detection
  // and tooltip ground hover / highlight write-back.
  NoMoreDay::ui::WorldUiFrame *m_worldFrame = nullptr;

  // U8 final: per-frame UI pointer-capture flag (see SetMouseOverUI).
  bool m_mouseOverUI = false;

  // R6: hovered item reported by the snapshot-driven inventory controller as
  // a stable domain id (SetHoveredItemDomain); resolved to an entity inside
  // Draw before the tooltip state machine runs. 0 = no hover.
  std::uint64_t m_hoveredItemDomainId = 0;

  // R3 (remediation, design §3.6): Escape key consumption this frame. Reset at
  // the start of each Update; set when HandleEscape closed a surface (or
  // handled a confirm/cancel) so gameplay never consumes the same key.
  bool m_escapeConsumedThisFrame = false;

  // R5: registers the texture asset ids referenced by the snapshot-driven
  // panels (hotbar skill icons, buff icons, summon icons) with the backend.
  // Identity mapping (asset id == UiResourceId); IsRegistered short-circuits
  // the steady state so the per-frame pass is find-only (zero allocation).
  void RegisterSnapshotIconTextures(const GameUiSnapshot& snapshot);

  // R5: id of the controller-owned minimap texture last registered with the
  // backend (the texture is recreated when the fog grid resizes).
  unsigned int m_registeredMinimapTextureId = 0;

  // R6: raw GL id of the player avatar texture last registered with the
  // backend under kPlayerAvatarTextureResourceId (the character panel paints
  // the avatar through the draw list; re-register when the sprite changes).
  unsigned int m_registeredAvatarTextureId = 0;

  bool m_initialized = false;
  bool m_inGameplay = false;

}; // class GameUiHost

} // namespace NoMoreDay::ui
