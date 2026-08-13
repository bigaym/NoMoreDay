#include "game/application/ui/GameUiHost.hpp"

#include "game/application/ui/UIAnimationSystem.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/WorldUiFrame.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

// R10 (B-R0-1): Tracy instrumentation, active only when TRACY_PROFILING=ON
// (no-op macros otherwise).
#include <tracy/Tracy.hpp>

namespace NoMoreDay::ui {

namespace {
// R4 (remediation, design §3.4): fixed capacities for the host-owned draw
// list. Reserved once at Initialize time; overflow is recorded as telemetry
// (assertable in tests) instead of growing on the frame path. R6 raised the
// initial values to cover the migrated inventory/character/overlay surfaces
// (R0.4 baseline: 64 commands per frame; a full inventory frame with all
// slots + overlays stays well under these bounds; R9 tunes from real
// measurements).
constexpr std::size_t kUiCommandCapacity = 1024;
constexpr std::size_t kUiTextArenaBytes = 16384;
} // namespace

GameUiHost::GameUiHost()
    : m_playerHud(m_runtime), m_minimap(m_runtime),
      // U8: host back-pointer so the hotbar routes its skill-drag reads
      // (drop target) through the host-owned drag session.
      m_skillHotbar(m_runtime, &m_tooltip, this), m_monsterHealthBars(m_runtime),
      // U8: back-pointer to the host so the inventory controller routes its
      // panel hover writes and context-menu opens through the host channels
      // (same pattern as m_stash/m_crafting below).
      m_inventory(m_runtime, this), m_character(m_runtime, this),
      // U8: back-pointer to the host so the panel controllers route their
      // hover writes through the host channel (SetHoveredItem) instead of the
      // static UiShared::HoveredItem() slot.
      m_stash(m_runtime, this), m_crafting(m_runtime, this),
      // U8: back-pointers so the skill tree routes its sibling closes and
      // the tooltip bind happens through the host (the tree also injects the
      // tooltip/host into its hub + talent tree instances).
      m_skillTree(m_runtime, &m_tooltip, this), m_astrolabe(m_runtime),
      // R6: back-pointer so the overlay's context-menu Craft action routes
      // through the host channel (CraftingSetTargetItem) instead of the
      // legacy SharedContext callback.
      m_overlay(m_runtime, this) {
  // U8: the tooltip controller's modal gate (DetectGroundHover) consults the
  // host instance instead of the legacy UISystem::IsModalInputCaptured query.
  m_tooltip.BindHost(this);
}

void GameUiHost::Initialize(ResourceManager &resourceManager) {
  if (m_initialized) {
    return;
  }

  // Transitional (U4): the legacy facade (UISystem) still owns font/texture
  // resources. Register its font handle (and the message box frame texture)
  // with the backend so draw-list commands resolve real resources during the
  // single-pass submit; controllers only reference the shared resource ids
  // (R4, design §3.4: raylib types stay inside the backend).
  UISystem::Initialize(resourceManager);
  m_backend.RegisterFont(kGlobalFontResourceId, UISystem::GetFont());
  m_backend.RegisterTexture(
      kMessageBoxTextureResourceId,
      AssetLoadingSystem::GetTexture(
          assets::ui::textures::Button_Frost_Rect.id));
  // R5: the blade-resource widget icon has no asset-registry entry (the legacy
  // SwordIntentWidget loaded the file directly); load it once here so the
  // draw-list Image command (kSwordIntentIconResourceId) resolves during the
  // single-pass submit. If the file is missing the texture id stays 0 and the
  // backend skips the command (same graceful behaviour as the legacy widget).
  if (FileExists("assets/textures/ui/ui_sword_icon.png")) {
    m_backend.RegisterTexture(kSwordIntentIconResourceId,
                              LoadTexture("assets/textures/ui/ui_sword_icon.png"));
  }
  // U7 group 5: astrolabe initialization moved out of UISystem::Initialize
  // into the hosted controller (same load order: assets are up by now).
  m_astrolabe.Initialize();

  // R8: register the custom painters of the migrated skill/astrolabe/tooltip
  // surfaces. The backend invokes them during Render for the matching Custom
  // commands; userData is the controller-owned frame-scoped paint state (the
  // canvas draw functions stay behind these registered callbacks, so the
  // controller/paint paths never call raylib directly — design §3.4).
  m_backend.RegisterPainter(kTooltipPainterResourceId,
                            &TooltipPaintCallback, &m_tooltip.PaintState());
  m_backend.RegisterPainter(kSkillHubPainterResourceId,
                            &SkillHubPaintCallback, &m_skillTree.Hub());
  m_backend.RegisterPainter(kSkillTreePainterResourceId,
                            &SkillTreePaintCallback, &m_skillTree.Tree());
  m_backend.RegisterPainter(kAstrolabePainterResourceId,
                            &AstrolabePaintCallback, &m_astrolabe);

  m_runtime.Reserve(64);
  // R4: reserve the host-owned draw-list buffers once at Initialize time.
  // Capacities are measurable (DrawList().CommandCapacity()/TextCapacity())
  // and overflow surfaces as telemetry, never as hot-path reallocation.
  m_drawList.Reserve(kUiCommandCapacity);
  m_drawList.ReserveText(kUiTextArenaBytes);
  m_viewport = UiViewport::Fit(
      {static_cast<float>(GetScreenWidth()),
       static_cast<float>(GetScreenHeight())});

  m_initialized = true;
  LOG_INFO("GameUiHost: initialized");
}

void GameUiHost::Shutdown() {
  if (!m_initialized) {
    return;
  }

  // Release registered resources before the legacy facade tears down its
  // fonts. Game::cleanup runs Shutdown before resource unload/window close.
  m_backend.UnregisterAll();
  m_minimap.Shutdown();
  m_runtime.Reset();
  m_drawList.Clear();
  UISystem::Shutdown();

  m_initialized = false;
  m_inGameplay = false;
  LOG_INFO("GameUiHost: shutdown");
}

void GameUiHost::EnterGameplay() {
  if (!m_initialized) {
    return;
  }
  ResetSessionState();
  // U7: runtime node tree now carries panel roots, so EnterGameplay no longer
  // resets the runtime; the panel controllers reset their own session state.
  m_playerHud.EnterGameplay();
  m_minimap.EnterGameplay();
  m_skillHotbar.EnterGameplay();
  m_tooltip.EnterGameplay();
  m_monsterHealthBars.EnterGameplay();
  m_inventory.EnterGameplay();
  m_character.EnterGameplay();
  m_stash.EnterGameplay();
  m_crafting.EnterGameplay();
  m_skillTree.EnterGameplay();
  m_astrolabe.EnterGameplay();
  m_overlay.EnterGameplay();
  m_inGameplay = true;
  LOG_INFO("GameUiHost: entered gameplay");
}

void GameUiHost::LeaveGameplay() {
  if (!m_initialized) {
    return;
  }
  ResetSessionState();
  m_playerHud.LeaveGameplay();
  m_minimap.LeaveGameplay();
  m_skillHotbar.LeaveGameplay();
  m_tooltip.LeaveGameplay();
  m_monsterHealthBars.LeaveGameplay();
  m_inventory.LeaveGameplay();
  m_character.LeaveGameplay();
  m_stash.LeaveGameplay();
  m_crafting.LeaveGameplay();
  m_skillTree.LeaveGameplay();
  m_astrolabe.LeaveGameplay();
  m_overlay.LeaveGameplay();
  m_inGameplay = false;
  LOG_INFO("GameUiHost: left gameplay");
}

void GameUiHost::Update(entt::registry &registry,
                        const LevelManager &levelManager) {
  Update(registry, levelManager, GameUiSnapshot{});
}

void GameUiHost::Update(entt::registry &registry,
                        const LevelManager &levelManager,
                        const GameUiSnapshot &snapshot) {
  ZoneScopedN("GameUiHost::Update");
  // R3 (remediation, design §3.6): per-frame reset of the Escape consumption
  // flag. Runs before the initialization guard so every Update call starts a
  // fresh frame for the Escape ownership contract; HandleEscape below sets it
  // only when the key was actually consumed by a UI surface. GameplayState
  // reads it after Update to decide whether to pause.
  m_escapeConsumedThisFrame = false;

  if (!m_initialized) {
    return;
  }
  m_snapshot = snapshot;

  // U6b: consume results published by the gameplay Update phase. Failed
  // intents surface their user-facing message through the hosted message box
  // (was the legacy State.showMessageBox bridge; U8 routes it through the
  // overlay controller).
  for (const GameUiResult &result : m_pendingNotifications) {
    if (!result.success && !result.notification.empty()) {
      m_overlay.ShowMessageBox(result.notification.c_str());
    }
    // R1: destructive successes (drop/destroy/socket/salvage/fuse) publish the
    // domain ids that are no longer valid in the UI session; clear the
    // matching drag/crafting sessions so no stale handle is rendered or
    // dropped twice.
    for (const std::uint64_t clearedId : result.clearedDomainIds) {
      if (clearedId == 0) {
        continue;
      }
      if (m_dragSession.draggedItemDomainId == clearedId) {
        m_dragSession.draggedItemDomainId = 0;
      }
      // R7: salvage/merge successes consume the forge/merge/salvage session
      // targets, not just the forge target (the single ClearConsumedTarget
      // covers forge target + merge base/fodder/catalyst + salvage item).
      m_crafting.ClearConsumedTarget(clearedId);
    }
  }
  m_pendingNotifications.clear();

  // R4 (remediation, design §3.1/§3.4): the runtime UI pipeline runs on every
  // Update. Design OnUpdate step 3 — fit the viewport to the current
  // framebuffer, reconcile the retained node tree with the session state,
  // feed the per-frame pointer input, then re-layout the tree. The backend
  // submit happens later in the render phase (PrepareRender paints, Draw
  // submits); this Update block is the single place the runtime receives
  // input and produces the frame's layout (design §3.4: UpdateInput/Arrange
  // run every UI update).
  const float dt = GetFrameTime();
  m_viewport = UiViewport::Fit(
      {static_cast<float>(GetScreenWidth()),
       static_cast<float>(GetScreenHeight())});
  m_overlay.ReconcileRuntime();
  UiInputFrame uiInput;
  const Vector2 mousePos = GetMousePosition();
  uiInput.pointer.logicalPosition =
      m_viewport.ToLogical(UiVec2{mousePos.x, mousePos.y});
  uiInput.pointer.pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  uiInput.pointer.released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
  uiInput.pointer.pressedRight = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
  // R8: sustained button state + wheel delta must reach the UiInputFrame
  // consumers (astrolabe camera pan/zoom, talent-tree pan/zoom, vow
  // hold-to-confirm) — they read pointer.down/rightDown/mouseWheel instead of
  // touching raylib inside Update.
  uiInput.pointer.down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
  uiInput.pointer.rightDown = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
  uiInput.pointer.mouseWheel = GetMouseWheelMove();
  uiInput.deltaSeconds = dt;
  // R4: the runtime tooltip controller stays unused until the tooltip surface
  // migrates (R6/R8); no target is fed this frame.
  uiInput.tooltipTarget = kInvalidUiId;
  m_runtime.UpdateInput(uiInput);
  m_runtime.Arrange({{0.0f, 0.0f}, m_viewport.LogicalSize()});

  // R6: interaction phases of the migrated panels/overlays run right after the
  // frame input is settled and before the Escape chain below. They read only
  // the frame snapshot + logical mouse and enqueue intents (executed by the
  // handler on the next gameplay Update); the character confirm state and the
  // quantity popup state they produce are exactly what HandleEscape reads.
  // R10 (收尾): UpdateOverlays is registry-free — the overlay validates its
  // targets against the frame snapshot's displayed-items cache (the builder
  // includes the context-menu target every frame via SnapshotOptions).
  m_character.UpdateInput(m_snapshot);
  m_overlay.UpdateOverlays(m_snapshot, m_viewport);

  // U8 inventory takeover: the KEY_I toggle moved out of GameplayState
  // (PushState<InventoryState> is gone) into the host update, at the original
  // frame position (before the legacy update) so the toggle is applied before
  // UISystem::Update reads the mirrored State.showInventory this frame.
  if (IsKeyPressed(KEY_I)) {
    m_inventory.Toggle();
  }

  // Transitional: the legacy panel renderer stays as the per-frame update
  // path for the surfaces that have not migrated to the runtime pipeline yet;
  // behaviour is unchanged for them. The runtime pipeline above (reconcile /
  // input / layout) already carries the R4 migrated surface (message box).
  // U7 group 3: stash/crafting route in-place through the parameters (KEY_E
  // interaction and KEY_K toggle keep their original frame position).
  // U7 group 4: skill tree KEY_S/ESC/alpha animation route in-place through
  // the skillTreeController parameter.
  // U7 group 5: astrolabe KEY_N/ESC/Update route through the controller.
  // U7 group 6: the global overlays (context menu, quantity popup, message
  // box) route through the overlay controller; its ESC handling and the
  // message box timer run in-place.
  // U8 final: the legacy UISystem::Update orchestration moved into the host
  // (UISystem::Update is gone). Frame order and behaviour are preserved:
  // animations, global hotkeys and the per-frame panel updates all run at
  // their original positions.

  // 0. Update Animation System
  UIAnimationSystem::Update(registry, dt);

  // Skill tree alpha animation (was the legacy UISystem::Update alpha block;
  // inventory/character alphas animate in their controllers below).
  m_skillTree.UpdateAlpha(dt);

  // U7 group 4: the skill tree interaction phase (hub/talent-tree input +
  // frame-scoped paint-state capture). R8 moved the hub canvas behind a
  // registered backend painter that reads m_paint.snapshot; without this call
  // the painter's snapshot stays null and the tree renders nothing while its
  // visible flag still blocks gameplay input (modal capture).
  m_skillTree.Update(m_snapshot, uiInput);

  // 1. Global Hotkeys

  // Character Panel (C): toggles the hosted controller; closing also resets
  // the attribute-confirm draft points and the context menu (legacy
  // behaviour; the confirm state now lives in the controller, not in the
  // AttributeUIComponent).
  if (IsKeyPressed(KEY_C)) {
    m_character.SetVisible(!m_character.IsVisible());
    if (!m_character.IsVisible()) {
      m_character.ResetDraftPoints();
    }
    m_overlay.CloseContextMenu();
  }

  // Quick Sort (Z): routes through the command handler as an intent (R6); the
  // handler re-validates the player and delegates to InventorySystem::organize
  // in the gameplay Update phase.
  if (IsKeyPressed(KEY_Z)) {
    GameUiIntent intent;
    intent.sourceNode = kInvalidUiId;
    intent.kind = GameUiIntentKind::OrganizeInventory;
    EnqueueIntent(intent);
  }

  // Astrolabe (N): first press opens (resetting the view), repeat presses
  // reset the camera instead of closing (legacy semantics). Opening closes
  // the sibling panels through the hosted controllers. R8: the controller is
  // registry-free; the player lookup is gone.
  if (IsKeyPressed(KEY_N)) {
    if (!m_astrolabe.IsVisible()) {
      m_astrolabe.Toggle();
    } else {
      m_astrolabe.ResetView();
    }
    if (m_astrolabe.IsVisible()) {
      m_inventory.SetVisible(false);
      m_character.SetVisible(false);
      m_overlay.CloseContextMenu();
      m_skillTree.Close();
    }
  }

  // Skill Tree (S)
  if (IsKeyPressed(KEY_S)) {
    m_skillTree.Toggle();
  }

  // Stash Interaction (E): R8 reads the player position from the frame
  // snapshot (the registry read is gone; Update stays registry-free for the
  // gameplay-state queries).
  if (IsKeyPressed(KEY_E)) {
    if (m_snapshot.player.hasPlayer) {
      const float pPosX = m_snapshot.player.worldX;
      const float pPosY = m_snapshot.player.worldY;

      auto stashView = registry.view<StashInteractableComponent, Position>();
      for (auto entity : stashView) {
        const auto &iPos = stashView.get<Position>(entity);
        float dx = iPos.x - pPosX;
        float dy = iPos.y - pPosY;
        if (dx * dx + dy * dy < 100.0f * 100.0f) {
          const auto &interact =
              stashView.get<StashInteractableComponent>(entity);
          m_stash.Open(interact.type);
          break;
        }
      }
    }
  }

  // Quick Pickup (F): R6 routes every candidate through the command handler as
  // a PickupItem intent (the handler re-validates distance/capacity in the
  // gameplay Update phase). Only entity validity is checked here (read-only);
  // the "背包已满" (bag full) feedback surfaces via the handler result
  // notification on the next Update. R8: player position from the snapshot.
  if (IsKeyPressed(KEY_F)) {
    if (m_snapshot.player.hasPlayer) {
      const float pPosX = m_snapshot.player.worldX;
      const float pPosY = m_snapshot.player.worldY;

      auto groundItemView = registry.view<ItemComponent, Position>();
      float pickupRangeSq = 200.0f * 200.0f; // Max pickup range (Increased)
      int attemptCount = 0;

      for (auto entity : groundItemView) {
        const auto &iPos = groundItemView.get<Position>(entity);
        float dx = iPos.x - pPosX;
        float dy = iPos.y - pPosY;
        float distSq = dx * dx + dy * dy;

        if (distSq < pickupRangeSq && registry.valid(entity)) {
          attemptCount++;
          GameUiIntent intent;
          intent.sourceNode = kInvalidUiId;
          intent.kind = GameUiIntentKind::PickupItem;
          intent.payload.sourceDomainId = entt::to_integral(entity);
          EnqueueIntent(intent);
        }
      }

      if (attemptCount > 0) {
        LOG_LIMITED_INFO(1.0f, "批量拾取: 范围内的物品 {} 个",
                         attemptCount);
      }
    }
  }

  // R3 (remediation, design §3.6): the host is the sole owner of the UI
  // Escape key. The close-policy chain closes exactly one topmost surface per
  // press (modal -> focus -> z-order/open order); when no surface is open the
  // key is left unconsumed so GameplayState can pause. This replaces the old
  // split chain (here + the inventory Esc block below) that consumed one key
  // twice — once for the UI and once for PauseState (H-02).
  //
  // R6: the overlay and character confirm state are settled before the chain
  // runs (their interaction phases are above, right after the frame input is
  // built), so HandleEscape reads the frame-accurate modal/confirm state.
  if (IsKeyPressed(KEY_ESCAPE)) {
    // HandleEscape owns the consumption flag (single writer) and returns the
    // consumption for testability; the flag is reset at the top of Update.
    (void)HandleEscape();
  }

  // Per-frame panel updates (was the legacy UISystem::Update tail).
  // R7: stash/crafting now run the snapshot-driven Update (frame input +
  // frame snapshot; the interaction phase enqueues intents for the next
  // gameplay Update phase). Same frame position so the per-frame animation
  // timing is equivalent.
  m_stash.Update(m_snapshot, uiInput);
  m_astrolabe.Update(m_snapshot, uiInput);
  m_crafting.Update(m_snapshot, uiInput);

  // Crafting toggle (K): opens the inventory as a drag source.
  if (IsKeyPressed(KEY_K)) {
    m_crafting.Toggle();
    if (m_crafting.IsVisible()) {
      m_inventory.SetVisible(true); // Open inventory to drag items
    }
  }

  // U7 group 6: message box timer decay runs right after the legacy update
  // (the legacy UISystem::Update timer block moved here; same frame
  // position).
  m_overlay.UpdateMessageBox();

  // U8 inventory takeover: TAB closes the inventory overlay (matches the
  // legacy InventoryState close-on-TAB behaviour). ESC is handled once, by
  // the unified chain above (R3, design §3.6); the inventory close policy
  // lives in HandleEscape so the key is consumed exactly once per press.
  if (IsKeyPressed(KEY_TAB) && m_inventory.IsVisible()) {
    m_inventory.SetVisible(false);
  }

  // R6: inventory alpha animation + interaction phase moved out of the legacy
  // registry-based Update into the snapshot-driven Update (frame input +
  // wheel + level manager for material display). The interaction runs at the
  // original frame position so the per-frame animation timing is equivalent;
  // GetFrameTime() is frame-scoped, so this ordering is behaviour-preserving.
  m_inventory.Update(m_snapshot, uiInput, GetMouseWheelMove(), levelManager);

  // U8 character panel: instance visibility re-adopt + alpha animation (was
  // the legacy UISystem::Update alpha block; KEY_C/ESC still write the legacy
  // flag until F2). Runs after the legacy update, same frame position as the
  // inventory animation above.
  m_character.Update(GetFrameTime());

  // U7 group 1: minimap debug toggle (F1) moved out of the legacy Update
  // into the host route. Same frame-position semantics as before.
  if (IsKeyPressed(KEY_F1)) {
    m_minimap.ToggleDebugReveal();
  }

  // R5 (remediation, design §3.1/§3.7): the HUD / hotbar / minimap /
  // monster-health panels are snapshot-only paints. Their controllers resolve
  // the frame snapshot into fixed controller-owned caches here (host Update,
  // after the runtime pipeline so the frame input is settled), and Paint
  // emits draw-list commands during PrepareRender. No registry reads on the
  // paint path; gameplay writes remain in the gameplay Update phase.
  m_playerHud.Update(m_snapshot, GetFPS(), GetTime());
  m_skillHotbar.Update(m_snapshot, uiInput);

  // R5: the hotbar / buff / summon icons reference texture asset ids directly
  // as UiResourceIds (identity mapping). Register each referenced id with the
  // backend once (IsRegistered short-circuits the steady state, so this loop
  // is a bounded find-only pass after the first frame — zero allocation).
  RegisterSnapshotIconTextures(m_snapshot);

  // R6: sync the player avatar texture under the fixed resource id the
  // character panel paints with (kPlayerAvatarTextureResourceId). The
  // snapshot carries the raw GL id (a GL id cannot cross the draw-list
  // boundary), so the host re-resolves the actual Texture2D from the player
  // SpriteComponent and keeps the backend handle in sync the same way the
  // minimap texture is synced (DrawMinimap). The backend skips the avatar
  // Image command while no player sprite is present.
  if (m_snapshot.player.avatarTextureId != 0 &&
      m_registeredAvatarTextureId != m_snapshot.player.avatarTextureId) {
    const auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() != playerView.end()) {
      if (const auto *sprite =
              registry.try_get<const SpriteComponent>(playerView.front())) {
        m_backend.RegisterTexture(kPlayerAvatarTextureResourceId,
                                  sprite->texture);
        m_registeredAvatarTextureId = m_snapshot.player.avatarTextureId;
      }
    }
  }
}

bool GameUiHost::HandleEscape() {
  // R3 (remediation, design §3.6): UI Escape close policy — closes exactly ONE
  // topmost surface per press, in priority order (modal -> focus -> z-order /
  // open order). Returns true when a surface was closed (the key was
  // consumed); false when nothing was open (the key is left for gameplay,
  // e.g. PauseState). The consumption flag is written here (single owner) and
  // reset by Update at the start of every frame; GameplayState reads it after
  // Update to decide whether to pause.
  //
  // Close policy per surface:
  //   quantity popup      : modal text-input surface, topmost (close).
  //   character confirm   : focus surface above the character panel (dismiss
  //                         the confirm dialog; the panel stays open).
  //   character panel     : closes the panel (and resets its temp points).
  //   context menu        : closes the popup menu.
  //   skill tree          : closes the tree (keyboard capture released).
  //   astrolabe           : closes the full-screen panel.
  //   inventory           : closes the overlay panel.
  //   stash               : closes the stash panel.
  //   crafting            : closes the crafting panel.
  bool consumed = false;
  if (m_overlay.IsQuantityPopupVisible()) {
    m_overlay.CloseQuantityPopup();
    consumed = true;
  } else if (m_character.IsVisible()) {
    // Character confirm popup has priority over the panel itself: dismiss the
    // confirm dialog (focus surface) without closing the panel. The confirm
    // state lives in the controller (R6); the draft points are kept so the
    // user can resume editing (legacy ESC behaviour).
    if (m_character.IsConfirmPopupVisible()) {
      m_character.CloseConfirmPopup();
      consumed = true;
    } else {
      m_character.SetVisible(false);
      consumed = true;
    }
  } else if (m_overlay.IsContextMenuVisible()) {
    m_overlay.CloseContextMenu();
    consumed = true;
  } else if (m_skillTree.IsVisible()) {
    m_skillTree.Close();
    consumed = true;
  } else if (m_astrolabe.IsVisible()) {
    m_astrolabe.Close();
    consumed = true;
  } else if (m_inventory.IsVisible()) {
    m_inventory.SetVisible(false);
    consumed = true;
  } else if (m_stash.IsVisible()) {
    m_stash.Close();
    consumed = true;
  } else if (m_crafting.IsVisible()) {
    m_crafting.Close();
    consumed = true;
  }
  m_escapeConsumedThisFrame = consumed;
  // No UI surface consumed the key: leave it unconsumed so GameplayState may
  // push PauseState (the old IsInventoryVisible() proxy is gone).
  return consumed;
}

void GameUiHost::EnqueueIntent(GameUiIntent intent) {
  m_pendingIntents.push_back(intent);
}

std::vector<GameUiIntent> GameUiHost::DrainUpdateIntents() {
  std::vector<GameUiIntent> drained;
  drained.swap(m_pendingIntents);
  m_pendingIntents.clear();
  return drained;
}

void GameUiHost::Publish(const GameUiResult &result) {
  m_pendingNotifications.push_back(result);
}

UiInputCapture GameUiHost::InputCapture() const {
  // U5: gameplay input gating consumes this aggregate instead of touching
  // UISystem static state. The retained runtime capture is authoritative;
  // the legacy queries below are transitional and removed by the U7 panel
  // migration.
  UiInputCapture capture = m_runtime.InputCapture();

  // A visible astrolabe is a full-screen surface: treat it as modal so
  // gameplay input is fully blocked, matching the pre-U5 gate. R8: the
  // controller is registry-free.
  if (m_astrolabe.IsVisible()) {
    capture.modal = true;
  }
  // U7 group 4: skill tree visibility is owned by the controller now.
  capture.keyboard = capture.keyboard || m_skillTree.IsVisible();
  // U8 typing gate: aggregated instance text-input focus (inventory/stash
  // search, quantity popup) replaces the legacy State.isTyping read; the
  // per-frame write in GameplayState is removed with it.
  capture.text = capture.text || IsTyping();
  capture.modal = capture.modal || IsModalInputCaptured();
  capture.pointer = capture.pointer || m_mouseOverUI;
  // U8 inventory takeover: the open inventory panel captures pointer input
  // (mouse movement/attacks/skills are gated by InputSystem, matching the
  // legacy InventoryState push semantics; keyboard movement stays enabled,
  // consistent with the other hosted overlay panels).
  capture.pointer = capture.pointer || m_inventory.IsVisible();
  return capture;
}

void GameUiHost::PrepareRender() {
  ZoneScopedN("GameUiHost::PrepareRender");
  // R4 (remediation, design §3.1/§3.4): paint phase only. The draw list is
  // cleared, the migrated surfaces paint their commands (host-owned buffers,
  // reserved at Initialize time; overflow is telemetry, never hot-path
  // reallocation), and Finalize() sorts the commands into the total draw
  // order for the single-pass backend submit (Draw). The viewport fit and
  // runtime input/layout steps run in Update; world-hit intents are enqueued
  // from this render phase and executed by the handler on the next Update.
  if (!m_initialized) {
    return;
  }
  m_drawList.Clear();
  // R5: the migrated HUD-family panels paint through the draw list in the
  // render phase (design §3.4: Hud layer below the Panels/Modal layers).
  // Order between panels is stable (same layer, distinct node ids, final
  // appendSequence). Monster bars first (world overhead), then HUD (player
  // hud + hotbar), then the minimap top-right.
  m_monsterHealthBars.Paint(m_drawList, m_viewport);
  m_playerHud.Paint(m_drawList, m_viewport);
  m_skillHotbar.Paint(m_drawList, m_viewport);
  m_minimap.Paint(m_drawList, m_viewport);
  // R6: the migrated inventory + character panels and the overlay surfaces
  // (context menu, quantity popup, message box) paint through the draw list.
  // Panels first (Panels layer), overlay last = topmost (Modal layer for the
  // quantity popup). Their interaction phases ran in Update; Paint is
  // registry-free and input-free.
  m_inventory.Paint(m_drawList, m_viewport, m_snapshot);
  m_character.Paint(m_drawList, m_viewport, m_snapshot);
  // R7: the migrated stash + crafting panels paint through the draw list in
  // the render phase (design §3.4: Panels layer, registry-free and
  // input-free). Their interaction phases ran in Update (intent-driven); the
  // paint order is stable (same layer, distinct node ids, final
  // appendSequence).
  m_stash.Paint(m_drawList, m_viewport, m_snapshot);
  m_crafting.Paint(m_drawList, m_viewport, m_snapshot);
  // R8: the skill tree (hub + talent canvas) and the astrolabe are
  // snapshot-only surfaces now: they paint their custom commands (Panels
  // layer) in the render phase and their interactions ran in Update
  // (intent-driven). Astrolabe first (full-screen background), then the skill
  // tree (opens on top of the hub).
  m_astrolabe.Paint(m_drawList, m_viewport);
  m_skillTree.Paint(m_drawList, m_viewport, m_snapshot);
  m_overlay.Paint(m_drawList, m_viewport);
  m_drawList.Finalize();
}

void GameUiHost::Draw(const LevelManager &levelManager,
                      const Camera2D &camera,
                      NoMoreDay::systems::SpatialHashGrid *spatialGrid) {
  if (!m_initialized) {
    return;
  }
  // R8: the registry parameter is gone — every panel is a
  // snapshot/intent/draw-list surface now (design §3.1/§3.4). Retained for
  // the public API contract: minimap/player-hud draw through their own routes
  // with their own signatures.
  (void)levelManager;
  (void)spatialGrid;
  // U8 final: the legacy UISystem::Draw orchestration moved into the host
  // (UISystem::Draw is gone). Frame order and behaviour are preserved: scale
  // fit, per-frame hover/mouse resets, the modal pointer gate, the tooltip
  // state machine (which resolves this frame's hover writes and mirrors the
  // result onto the frame object). R8: ground-hover detection reads the world
  // frame (domain ids) — no registry.
  m_tooltip.ResetFrame();
  m_tooltip.DetectGroundHover(camera);

  // --- Scale Calculation ---
  const float scaleX = static_cast<float>(GetScreenWidth()) / UI_REF_WIDTH;
  const float scaleY = static_cast<float>(GetScreenHeight()) / UI_REF_HEIGHT;
  const float scale = std::min(scaleX, scaleY);
  UIRenderer::SetScale(scale);

  m_mouseOverUI = false;
  SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  // Per-frame hover reset on the frame-scoped bridge (render read-side
  // highlight; the tooltip state machine writes the resolved hover back).
  if (m_worldFrame != nullptr) {
    m_worldFrame->ClearHovered();
  }

  if (IsModalInputCaptured()) {
    m_mouseOverUI = true;
  }

  // R8: the astrolabe and skill-tree panels are snapshot-only paints now
  // (PrepareRender, via their registered custom painters); their interactions
  // ran in Update (intent-driven), so nothing draws here. Same for the
  // inventory/stash/crafting panels and the HUD/overlay surfaces.

  // R8: resolve the domain-id hover reported by the snapshot-driven inventory
  // controller (Update phase) into the tooltip's domain-id contract, at its
  // original frame position (before UpdateState).
  if (m_hoveredItemDomainId != 0) {
    m_tooltip.SetHoveredItemDomain(m_hoveredItemDomainId);
    m_hoveredItemDomainId = 0;
  }

  m_tooltip.UpdateState(m_snapshot, GetFrameTime());

  // U6b: pickup click detection moved out of the legacy draw path. Read-only
  // hit test against the visible item cache; the resulting intent is executed
  // by GameUiCommandHandler on the next Update (accepted one-frame delay).
  DetectPickupClick(camera);

  // R8: the tooltip paints its custom command (Tooltip layer) after the
  // UpdateState phase resolved this frame's active item/skill/buff; the drag
  // phantom paints its preview (DragPreview layer) from the drag session +
  // snapshot right before it (both registry-free, design §3.4).
  PaintDragPhantom(m_drawList);
  m_tooltip.Paint(m_drawList, m_viewport, m_snapshot);
  // The tooltip command appends after PrepareRender's Finalize; re-sort so the
  // single-pass submit below respects the layer order.
  m_drawList.Finalize();

  // R4: single-pass submit of the finalized draw list (prepared + sorted in
  // PrepareRender) onto the native framebuffer.
  m_backend.Render(m_viewport, m_drawList);
}

void GameUiHost::DrawMinimap(const LevelManager &levelManager,
                             NoMoreDay::systems::SpatialHashGrid *grid) {
  // R5: minimap is a snapshot-only paint (design §3.4 Hud layer). Update keeps
  // the fog-texture maintenance (retained GPU resource driven by the world's
  // LevelManager — not the ECS registry) and resolves the overlay markers from
  // the frame snapshot; Paint emits the draw-list commands in PrepareRender.
  m_minimap.Update(m_snapshot, levelManager, grid, GetFrameTime());
  // R5: keep the backend's registered handle for the controller-owned minimap
  // texture in sync (the texture is recreated when the fog grid resizes).
  const Texture2D &texture = m_minimap.Texture();
  if (texture.id != m_registeredMinimapTextureId) {
    m_backend.RegisterTexture(kMinimapTextureResourceId, texture);
    m_registeredMinimapTextureId = texture.id;
  }
}

void GameUiHost::DrawHud(entt::registry &registry) {
  // R5: the player HUD is a snapshot-only paint. Its per-frame snapshot
  // resolution runs in Update (m_playerHud.Update) and the draw-list commands
  // are emitted in PrepareRender (Paint); the legacy registry-based Draw route
  // is gone, so this legacy call position is a no-op.
  (void)registry;
}

void GameUiHost::DrawDraggingPhantom() {
  // U8 final: the drag phantom + top-most tooltip pass. R8: the phantom and
  // the tooltip are painted from the host-owned drag session and the frame
  // snapshot inside Draw (draw list, DragPreview/Tooltip layers), so this
  // legacy call position (after the player HUD) is a no-op kept for the
  // public API contract and the call order.
  (void)m_dragSession;
}

void GameUiHost::RenderMonsterHealthBars(entt::registry &registry,
                                         const Camera2D &camera) {
  // R5: monster health bars are a snapshot-only paint. This legacy world-pass
  // call position (inside Mode2D, before the screen-space UI pass) forwards
  // the camera transform and the raw mouse position as plain data — the
  // controller culls / picks / batches from the frame snapshot (design §3.2:
  // no raylib types on the controller paint path). Paint emits the draw-list
  // commands in PrepareRender.
  (void)registry;
  const Vector2 mouse = GetMousePosition();
  m_monsterHealthBars.Update(
      m_snapshot, camera.target.x, camera.target.y, camera.offset.x,
      camera.offset.y, camera.zoom, mouse.x, mouse.y, GetScreenWidth(),
      GetScreenHeight());
}

void GameUiHost::RenderMonsterHealthBarsUI(entt::registry &registry) {
  // R5: the hovered-target widget is painted through the draw list in
  // PrepareRender (MonsterHealthBarController::Paint), so this legacy
  // screen-pass call position is a no-op.
  (void)registry;
}

void GameUiHost::DrawCharacter(entt::registry &registry) {
  // R6: the character panel is a snapshot-only paint (PrepareRender) and its
  // interaction runs in Update (UpdateInput). This legacy call position is a
  // no-op (kept for the public API contract).
  (void)registry;
}

void GameUiHost::CraftingOpenMergePanel() {
  m_crafting.OpenMergePanel();
}

void GameUiHost::CraftingSetTargetItem(entt::entity item) {
  m_crafting.SetTargetItem(item);
}

void GameUiHost::CloseAstrolabe() {
  m_astrolabe.Close();
}

void GameUiHost::RegisterSnapshotIconTextures(
    const GameUiSnapshot &snapshot) {
  // R5 (design §3.7): the snapshot-driven panels reference texture asset ids
  // directly as UiResourceIds (identity mapping: SkillData::icon_id /
  // BuffVisualData::icon_asset->id / SummonComponent::icon_id are asset ids).
  // Registering is idempotent and short-circuited by IsRegistered, so the
  // steady state of this per-frame pass is a bounded set of unordered_map
  // finds (no allocation); first sight registers the texture once.
  for (const GameUiSkillBarSlotView &slot : snapshot.skillBar.slots) {
    if (slot.iconId != 0 && !m_backend.IsRegistered(slot.iconId)) {
      m_backend.RegisterTexture(slot.iconId,
                                AssetLoadingSystem::GetTexture(slot.iconId));
    }
  }
  for (const GameUiBuffView &buff : snapshot.buffs) {
    if (buff.iconAssetId != 0 && !m_backend.IsRegistered(buff.iconAssetId)) {
      m_backend.RegisterTexture(
          buff.iconAssetId, AssetLoadingSystem::GetTexture(buff.iconAssetId));
    }
  }
  for (const GameUiSummonGroupView &group : snapshot.player.summonGroups) {
    if (group.iconId != 0 && !m_backend.IsRegistered(group.iconId)) {
      m_backend.RegisterTexture(group.iconId,
                                AssetLoadingSystem::GetTexture(group.iconId));
    }
  }
  // R6: the migrated inventory/character/overlay panels paint item icons and
  // the avatar through the draw list. Item texture asset ids travel in the
  // snapshot view models (identity mapping: asset id == UiResourceId);
  // register each referenced id once (IsRegistered short-circuits the steady
  // state, so this pass is a bounded find-only loop after the first frame).
  for (const GameUiItemView &item : snapshot.inventory.items) {
    if (item.textureId != 0 && !m_backend.IsRegistered(item.textureId)) {
      m_backend.RegisterTexture(item.textureId,
                                AssetLoadingSystem::GetTexture(item.textureId));
    }
  }
  for (const GameUiEquippedSlotView &slot : snapshot.equipment) {
    if (slot.textureId != 0 && !m_backend.IsRegistered(slot.textureId)) {
      m_backend.RegisterTexture(slot.textureId,
                                AssetLoadingSystem::GetTexture(slot.textureId));
    }
  }
  for (const GameUiBagSlotView &bagSlot : snapshot.inventory.bagSlots) {
    if (bagSlot.textureId != 0 && !m_backend.IsRegistered(bagSlot.textureId)) {
      m_backend.RegisterTexture(
          bagSlot.textureId, AssetLoadingSystem::GetTexture(bagSlot.textureId));
    }
  }
  for (const GameUiItemView &item : snapshot.displayedItems) {
    if (item.textureId != 0 && !m_backend.IsRegistered(item.textureId)) {
      m_backend.RegisterTexture(item.textureId,
                                AssetLoadingSystem::GetTexture(item.textureId));
    }
  }
  // R7: the migrated stash panel paints item icons from the stash view model
  // (same identity mapping). Register each referenced slot texture id once.
  for (const GameUiStashTabView &tab : snapshot.stash.tabs) {
    for (const GameUiStashSlotView &slot : tab.slots) {
      if (slot.textureId != 0 && !m_backend.IsRegistered(slot.textureId)) {
        m_backend.RegisterTexture(
            slot.textureId, AssetLoadingSystem::GetTexture(slot.textureId));
      }
    }
  }
  // R6: the overlay skill context menu paints every skill icon (the legacy
  // path only rendered hotbar slots); register the referenced asset ids once.
  for (const auto &[skillId, skill] : SkillRegistry::Get().GetAllSkills()) {
    (void)skillId;
    if (skill.icon_id != 0 && !m_backend.IsRegistered(skill.icon_id)) {
      m_backend.RegisterTexture(skill.icon_id,
                                AssetLoadingSystem::GetTexture(skill.icon_id));
    }
  }
}

GameUiSnapshotOptions GameUiHost::SnapshotOptions() const {
  // R1: the builder receives the UI session display requests as stable
  // integer domain ids (design §3.2). All reads are UI-local state; no
  // gameplay write happens here.
  GameUiSnapshotOptions options;
  const std::uint64_t hovered = m_tooltip.ActiveTooltipItemDomain();
  options.hoveredItem = (hovered == kInvalidDomainId) ? kInvalidDomainId
                                                       : hovered;
  options.draggedItem = m_dragSession.draggedItemDomainId;
  // R6: the context-menu target (open via the inventory right-click or the
  // skill hotbar) is included in the displayed-items cache so the snapshot
  // carries the menu item's full view model.
  options.contextMenuItem = m_overlay.ContextMenuItemDomainId();
  options.forgeTarget = m_crafting.GetForgeTargetDomainId();
  options.mergeBase = m_crafting.GetMergeBaseDomainId();
  options.mergeFodder = m_crafting.GetMergeFodderDomainId();
  options.mergeCatalyst = m_crafting.GetMergeCatalystDomainId();
  options.salvageItem = m_crafting.GetSalvageItemDomainId();
  options.stashActiveTab = m_stash.GetActiveTabIndex();
  // R7: the stash search query + active stash type travel in the options so
  // the builder can compute the per-slot matchesSearch flags and pick the
  // right stash authority (Personal vs Shared) without touching the panel.
  options.stashType = static_cast<std::uint32_t>(m_stash.GetActiveType());
  options.stashSearchQuery = m_stash.SearchQuery();
  return options;
}

void GameUiHost::SetInventoryVisible(bool visible) {
  m_inventory.SetVisible(visible);
}

bool GameUiHost::IsInventoryVisible() const noexcept {
  return m_inventory.IsVisible();
}

void GameUiHost::OpenContextMenu(entt::entity item, bool fromInventory,
                                 int inventoryIndex,
                                 NoMoreDay::EquipmentSlot slot) {
  // U8 inventory takeover: right-click interaction of the inventory panel
  // routes through the hosted overlay controller (instance API; it mirrors
  // the legacy UISystem::State fields, so the overlay re-adopts and draws at
  // the original UISystem::Draw overlay pass).
  m_overlay.OpenContextMenu(item, fromInventory, inventoryIndex, slot);
}

void GameUiHost::OpenContextMenuDomain(std::uint64_t domainId,
                                       bool fromInventory, int inventoryIndex,
                                       NoMoreDay::EquipmentSlot slot) {
  // R6: domain-id channel for the snapshot-driven inventory controller. The
  // overlay re-validates the entity (registry.valid + ItemComponent) when it
  // builds its menu, so an unresolvable id degrades to a closed menu (no
  // crash); no registry access happens here.
  if (domainId == kInvalidDomainId) {
    return;
  }
  const entt::entity item =
      static_cast<entt::entity>(static_cast<entt::id_type>(domainId));
  m_overlay.OpenContextMenu(item, fromInventory, inventoryIndex, slot);
}

bool GameUiHost::IsAnyPanelOpen() const {
  // U8 host read-side migration: aggregates the hosted panel controllers'
  // instance visibility (replaces the legacy State.showSkillTree ||
  // State.showCharacterPanel anyPanelOpen check in GameplayState). The
  // character panel also counts the fading-out alpha (legacy render gate
  // semantics). R8: the astrolabe is registry-free.
  return m_inventory.IsVisible() || m_skillTree.IsVisible() ||
         IsCharacterPanelVisible() || m_stash.IsVisible() ||
         m_crafting.IsVisible() || m_astrolabe.IsVisible();
}

void GameUiHost::SetCharacterPanelVisible(bool visible) {
  // U8 final: the character panel visibility routes through the hosted
  // controller only (the legacy State.showCharacterPanel mirror is gone).
  m_character.SetVisible(visible);
}

bool GameUiHost::IsCharacterPanelVisible() const noexcept {
  // Instance visibility + fading-out alpha, matching the legacy render gate
  // State.showCharacterPanel || State.characterPanelAlpha > 0.
  return m_character.IsVisible() || m_character.Alpha() > 0.0f;
}

void GameUiHost::CloseInventory() { m_inventory.SetVisible(false); }

void GameUiHost::CloseCharacterPanel() { SetCharacterPanelVisible(false); }

void GameUiHost::CloseSkillTree() { m_skillTree.Close(); }

void GameUiHost::CloseContextMenu() { m_overlay.CloseContextMenu(); }

void GameUiHost::ShowMessageBox(const char *text) {
  m_overlay.ShowMessageBox(text);
}

void GameUiHost::SetHoveredSkillId(uint32_t skillId) {
  // U8 skill-hover channel: forwards the hovered skill id to the tooltip
  // controller's hover source (the skill hub/tree hover writes route through
  // this host channel instead of the static State.hoveredSkillId slot).
  m_tooltip.SetHoveredSkill(skillId);
}

bool GameUiHost::IsTyping() const noexcept {
  // U8 typing-gate aggregation: any focused text input (inventory/stash
  // search, quantity popup) captures text input. Replaces the legacy
  // State.isTyping read in InputCapture.
  return m_stash.IsSearchFocused() || m_inventory.IsSearchFocused() ||
         m_overlay.IsTyping();
}

void GameUiHost::BindWorldFrame(NoMoreDay::ui::WorldUiFrame *frame) {
  m_worldFrame = frame;
  m_tooltip.BindWorldFrame(frame);
}

void GameUiHost::DetectPickupClick(const Camera2D &camera) {
  // Replicates the legacy ground-item pickup hit test (previously inside
  // UISystem::Draw) without touching the ECS: read-only collision against the
  // frame-scoped visible item cache, then a distance check against the
  // player (R8: the player position comes from the frame snapshot, so the
  // render phase never reads the registry). The intent is enqueued here and
  // re-validated (entity validity, distance, capacity) by
  // GameUiCommandHandler during the Update phase (design §6.2).
  // U8 final: the modal-input gate moved from UISystem::IsModalInputCaptured
  // to this host instance (overlay quantity popup or skill tree open).
  if (IsModalInputCaptured()) {
    return; // Modal UI blocks world interaction, matching the legacy gate.
  }
  // U8 inventory takeover: the open inventory overlay also blocks ground-item
  // pickup clicks (legacy InventoryState pushed a blocking state; the panel
  // must not be able to fire pickups from under itself).
  if (m_inventory.IsVisible()) {
    return;
  }
  // R3 (remediation, design §3.5.4): world intents are enqueued only while a
  // valid current-pass view exists. A bound-but-never-opened frame or a stale
  // frame (rotated token) yields an invalid view => no world target; the
  // click is dropped instead of acting on a previous frame's proxies.
  if (m_worldFrame == nullptr) {
    return; // Frame not bound: no visible item cache to test against.
  }
  const WorldUiFrame::View worldView = m_worldFrame->AcquireView();
  if (!worldView.IsValid()) {
    return;
  }
  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    return;
  }
  if (!m_snapshot.player.hasPlayer) {
    return; // No positioned player: nothing to pick up for.
  }
  const float playerPosX = m_snapshot.player.worldX;
  const float playerPosY = m_snapshot.player.worldY;

  const Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

  // Iterate ONLY visible items (already culled by RenderSystem).
  for (const auto &itemData : worldView.VisibleItems()) {
    // Simple AABB check in world space; first hit is the top-most item.
    if (!CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
      continue;
    }
    // R8: no registry on the interaction path — the pickup distance test uses
    // the world frame's already-culled world-space AABB (the same data the
    // hover highlight uses), and the domain id is derived from the entity
    // handle the render adapter published. The registry is never re-queried.
    const float itemCenterX = itemData.worldRect.x + itemData.worldRect.width * 0.5f;
    const float itemCenterY = itemData.worldRect.y + itemData.worldRect.height * 0.5f;
    const float dx = itemCenterX - playerPosX;
    const float dy = itemCenterY - playerPosY;
    const float distSq = dx * dx + dy * dy;
    if (distSq > 180.0f * 180.0f) {
      break; // Out of pickup range: keep the hover highlight, no intent.
    }

    GameUiIntent intent;
    intent.sourceNode = kInvalidUiId;
    intent.kind = GameUiIntentKind::PickupItem;
    intent.payload.sourceDomainId = entt::to_integral(itemData.entity);
    EnqueueIntent(intent);
    break;
  }
}

void GameUiHost::PaintDragPhantom(UiDrawList& drawList) {
  // R8 (design §3.4 DragPreview layer): the drag preview paints from the
  // host-owned drag session + the frame snapshot. The item preview resolves
  // the dragged domain id against snapshot.displayedItems (the same cache the
  // tooltip paints from), so no entity handle is ever re-resolved on the
  // paint path; the skill preview uses the skill icon asset id (registered
  // identity mapping). The logical mouse is the preview anchor.
  const Vector2 mPos = UISystem::GetMousePositionLogic();
  const UiVec2 logicalMouse{mPos.x, mPos.y};

  // 1. Item Phantom
  if (m_dragSession.draggedItemDomainId != 0) {
    const float size = 64.0f;
    const UiRect rect{{logicalMouse.x - size * 0.5f,
                       logicalMouse.y - size * 0.5f},
                      {size, size}};
    const GameUiItemView* view = nullptr;
    for (const GameUiItemView& candidate : m_snapshot.displayedItems) {
      if (candidate.domainId == m_dragSession.draggedItemDomainId) {
        view = &candidate;
        break;
      }
    }
    if (view != nullptr && view->textureId != 0) {
      drawList.Image(UiDrawLayer::DragPreview, 0, rect, view->textureId,
                     {255, 255, 255, 178});
    } else {
      // Unresolvable id / no texture: fall back to a dim placeholder so the
      // drag feedback stays visible (matches the legacy blue-slot fallback).
      drawList.FillRect(UiDrawLayer::DragPreview, 0, rect, {70, 70, 90, 128});
    }
  }

  // 2. Skill Phantom
  if (m_dragSession.isDraggingSkill &&
      m_dragSession.draggedSkillId != NoMoreDay::INVALID_SKILL_ID) {
    const float size = 48.0f;
    const UiRect rect{{logicalMouse.x - size * 0.5f,
                       logicalMouse.y - size * 0.5f},
                      {size, size}};
    const auto* skill = SkillRegistry::Get().GetSkill(m_dragSession.draggedSkillId);
    if (skill != nullptr && skill->icon_id != 0) {
      drawList.Image(UiDrawLayer::DragPreview, 0, rect, skill->icon_id,
                     {255, 255, 255, 178});
    } else {
      drawList.FillRect(UiDrawLayer::DragPreview, 0, rect, {60, 60, 180, 128});
    }
  }
}

void GameUiHost::ResetSessionState() {
  // Gameplay-scoped session data: the panel controllers reset themselves on
  // Enter/LeaveGameplay; the host-owned drag session is cleared here so no
  // drag state leaks into the next run.
  m_dragSession.Clear();
}

} // namespace NoMoreDay::ui
