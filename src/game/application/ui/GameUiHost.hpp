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

// Composition root of the UI layer (design §4.2 lifecycle). Owns the retained
// runtime core (UiViewport / UiRuntime / UiRaylibBackend) plus the draw list
// and gameplay-session scoping. Not a singleton: the application owns exactly
// one instance and injects it into states via SharedContext.
//
// Transitional (U4): Update/Draw forward to the legacy panel renderer
// (UISystem) unchanged so the ownership path change does not alter panel
// output. The new runtime core is owned here and is driven from U5 on.
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

  // Per-frame update. Transitional: forwards to the legacy panel renderer.
  // Compatibility overload that runs the update against an empty snapshot;
  // the gameplay Update phase should prefer the snapshot overload below.
  void Update(entt::registry &registry, const LevelManager &levelManager);

  // U6b: update against the frame-scoped read-only snapshot. Saves the
  // snapshot for later panel migrations (U7), consumes pending intent results
  // through the compatibility bridge (failure notifications surface via the
  // legacy message box; U7 removes this bridge), then forwards to the legacy
  // panel renderer unchanged.
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

  // Transitional: forwards to the legacy panel renderer (UISystem), then
  // submits the new draw list through the raylib backend. Must run after the
  // scene composite and before EndDrawing.
  void Draw(entt::registry &registry, const LevelManager &levelManager,
            const Camera2D &camera,
            NoMoreDay::systems::SpatialHashGrid *spatialGrid = nullptr);

  // U7 group 1: panel routes. GameplayState calls these at the original
  // legacy call positions so the native UI render order is unchanged; the
  // legacy static panels remain only as fallbacks for the null-host path.
  void DrawMinimap(entt::registry &registry, const LevelManager &levelManager,
                   NoMoreDay::systems::SpatialHashGrid *grid = nullptr);
  void DrawHud(entt::registry &registry);
  // U7 group 6-B: the drag phantom + top-most tooltip pass (was
  // UISystem::DrawDraggingPhantom). GameplayState calls this at the original
  // legacy position (after the player HUD); the tooltip routes through the
  // hosted TooltipController, the legacy static path remains as the null-host
  // fallback only.
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
  // Clears gameplay-scoped session state (panels, drag, tooltip, message box,
  // quantity popup). Delegates to the legacy facade's internal reset so no new
  // static-state writes are introduced elsewhere.
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

  // Frame-scoped read model handed in by the Update phase (U6b). Retained so
  // later panel migrations (U7) can read it without re-querying the ECS.
  GameUiSnapshot m_snapshot{};
  // Intents queued by render-time detection; executed by the gameplay Update
  // phase via GameUiCommandHandler. Cleared by DrainUpdateIntents.
  std::vector<GameUiIntent> m_pendingIntents;
  // Results published back by the gameplay Update phase; consumed (and
  // cleared) on the next Update through the compatibility bridge.
  std::vector<GameUiResult> m_pendingNotifications;

  bool m_initialized = false;
  bool m_inGameplay = false;
};

} // namespace NoMoreDay::ui
