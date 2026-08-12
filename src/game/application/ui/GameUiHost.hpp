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
  // hotkeys, animations, the one-shot test-item grant, per-frame panel
  // updates; see GameUiHost.cpp). Compatibility overload that runs the update
  // against an empty snapshot; the gameplay Update phase should prefer the
  // snapshot overload below.
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
  // capture reflects the current frame's UI state. Requires the registry to
  // resolve the player entity (astrolabe visibility).
  [[nodiscard]] UiInputCapture InputCapture(entt::registry &registry) const;

  // Re-fit the viewport to the current framebuffer and start a fresh draw
  // list. Call after the scene composite, before Draw.
  void PrepareRender();

  // U8 final: draws the full UI in-place (scale fit, per-frame hover/mouse
  // resets, panel passes, overlays, tooltip state machine; see
  // GameUiHost.cpp), then submits the new draw list through the raylib
  // backend. Must run after the scene composite and before EndDrawing.
  void Draw(entt::registry &registry, const LevelManager &levelManager,
            const Camera2D &camera,
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
  // legacy position (after the player HUD); the phantom draws directly from
  // the host-owned drag session and the tooltip routes through the hosted
  // TooltipController.
  void DrawDraggingPhantom(entt::registry &registry);
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
  // entry points wired through SharedContext callbacks (InventorySystem
  // Legendary Core use; UIRenderer context-menu craft).
  void CraftingOpenMergePanel();
  void CraftingSetTargetItem(entt::entity item);
  // U7 group 5: astrolabe close entry point for the skill-tree sibling
  // coupling (wired through SharedContext.closeAstrolabe, filled by Game).
  void CloseAstrolabe();

  // U8 panel-hover convergence channel: forwards the hovered item to the
  // tooltip controller's hover source (TooltipController::SetHoveredItem).
  // Panel hover write points (stash/crafting controllers, states) route
  // through this host channel instead of the static UiShared::HoveredItem()
  // slot, so hover flows into the instance hover pipeline (UpdateState
  // write-back) and the static slot stays write-free except for the U8
  // fallback branches. Pass entt::null to clear the hover source.
  void SetHoveredItem(entt::entity entity);

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

  // --- U8 host read-side migration: aggregated panel visibility ---
  // True while any hosted panel/overlay is open (inventory, skill tree,
  // character, stash, crafting, astrolabe). Replaces the legacy
  // State.showSkillTree || State.showCharacterPanel anyPanelOpen check in
  // GameplayState. Requires the registry to resolve the player entity
  // (astrolabe visibility); each hosted controller reports its own instance
  // visibility (character also counts the fading-out alpha).
  [[nodiscard]] bool IsAnyPanelOpen(entt::registry &registry) const;

  // U8 final: character-panel visibility channel. The KEY_C/ESC writers and
  // GameplayState route through this host channel; the controller owns the
  // instance visibility and alpha animation.
  void SetCharacterPanelVisible(bool visible);
  [[nodiscard]] bool IsCharacterPanelVisible() const noexcept;

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
  void ToggleSkillTree(entt::registry& registry) {
    m_skillTree.Toggle(registry);
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
  // the visible item cache, player/item distance and entity validity, then
  // enqueues a PickupItem intent. Never mutates the ECS.
  void DetectPickupClick(entt::registry &registry, const Camera2D &camera);

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
  // update. Enter/LeaveGameplay reset all overlay state.
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
  // U8 final: one-shot debug/test item grant per gameplay session (was the
  // legacy UISystem::s_hasGivenTestItems static).
  bool m_hasGivenTestItems = false;

  bool m_initialized = false;
  bool m_inGameplay = false;
};

} // namespace NoMoreDay::ui
