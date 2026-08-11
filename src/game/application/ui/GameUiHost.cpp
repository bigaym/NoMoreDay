#include "game/application/ui/GameUiHost.hpp"

#include "game/application/ui/UISystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

#include <cstdint>
#include <utility>

namespace NoMoreDay::ui {

namespace {
// Handle under which the legacy global font is registered with the new
// backend. Panels migrate to draw-list Text commands against this handle.
constexpr UiResourceId kGlobalFontResourceId = 1;
} // namespace

GameUiHost::GameUiHost()
    : m_playerHud(m_runtime), m_minimap(m_runtime),
      m_skillHotbar(m_runtime, &m_tooltip), m_monsterHealthBars(m_runtime),
      m_inventory(m_runtime), m_character(m_runtime), m_stash(m_runtime),
      m_crafting(m_runtime), m_skillTree(m_runtime), m_astrolabe(m_runtime),
      m_overlay(m_runtime) {}

void GameUiHost::Initialize(ResourceManager &resourceManager) {
  if (m_initialized) {
    return;
  }

  // Transitional (U4): the legacy facade (UISystem) still owns font/texture
  // resources. Register its font handle with the new backend so draw-list
  // commands can resolve it once panels migrate (U5+).
  UISystem::Initialize(resourceManager);
  m_backend.RegisterFont(kGlobalFontResourceId, UISystem::GetFont());
  // U7 group 5: astrolabe initialization moved out of UISystem::Initialize
  // into the hosted controller (same load order: assets are up by now).
  m_astrolabe.Initialize();

  m_runtime.Reserve(64);
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
  if (!m_initialized) {
    return;
  }
  m_snapshot = snapshot;

  // U6b: consume results published by the gameplay Update phase. Failed
  // intents surface their user-facing message through the legacy message box
  // so the pickup failure UX is unchanged. U7 removes this compatibility
  // bridge once panels own their notifications.
  for (const GameUiResult &result : m_pendingNotifications) {
    if (!result.success && !result.notification.empty()) {
      UISystem::State.showMessageBox = true;
      utils::FormatToBuffer(UISystem::State.messageBoxText, "{}",
                            result.notification);
      UISystem::State.messageBoxTimer = 2.0f;
    }
  }
  m_pendingNotifications.clear();

  // Transitional: keep the legacy panel renderer as the single update path so
  // behaviour is unchanged; panels migrate to the runtime in later stages.
  // U7 group 3: stash/crafting route in-place through the parameters (KEY_E
  // interaction and KEY_K toggle keep their original frame position).
  // U7 group 4: skill tree KEY_S/ESC/alpha animation route in-place through
  // the skillTreeController parameter.
  // U7 group 5: astrolabe KEY_N/ESC/Update route through the controller.
  // U7 group 6: the global overlays (context menu, quantity popup, message
  // box) route through the overlay controller; its ESC handling and the
  // message box timer run in-place.
  UISystem::Update(registry, levelManager, &m_stash, &m_crafting,
                   &m_skillTree, &m_astrolabe, &m_overlay);
  // U7 group 6: message box timer decay runs right after the legacy update
  // (the legacy UISystem::Update timer block moved here; same frame
  // position).
  m_overlay.UpdateMessageBox();
  // U7 group 2: inventory alpha animation moved out of UISystem::Update (the
  // legacy call site was removed); it runs right after the legacy update so
  // the per-frame animation timing is equivalent. GetFrameTime() is
  // frame-scoped, so this ordering is behaviour-preserving.
  m_inventory.Update(registry, levelManager);

  // U7 group 1: minimap debug toggle (F1) moved out of the legacy Update
  // into the host route. Same frame-position semantics as before.
  if (IsKeyPressed(KEY_F1)) {
    m_minimap.ToggleDebugReveal();
  }
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

UiInputCapture GameUiHost::InputCapture(entt::registry &registry) const {
  // U5: gameplay input gating consumes this aggregate instead of touching
  // UISystem static state. The retained runtime capture is authoritative;
  // the legacy queries below are transitional and removed by the U7 panel
  // migration.
  UiInputCapture capture = m_runtime.InputCapture();

  const entt::entity player = UISystem::GetPlayerEntity(registry);
  // A visible astrolabe is a full-screen surface: treat it as modal so
  // gameplay input is fully blocked, matching the pre-U5 gate.
  if (m_astrolabe.IsVisible(registry, player)) {
    capture.modal = true;
  }
  // U7 group 4: skill tree visibility is owned by the controller now.
  capture.keyboard = capture.keyboard || m_skillTree.IsVisible();
  capture.text = capture.text || UISystem::State.isTyping;
  capture.modal = capture.modal || UISystem::IsModalInputCaptured();
  capture.pointer = capture.pointer || UISystem::State.isMouseOverUI;
  return capture;
}

void GameUiHost::PrepareRender() {
  // Re-fit to the current framebuffer every frame; the game window is
  // resizable and the UI must track the native resolution.
  m_viewport = UiViewport::Fit(
      {static_cast<float>(GetScreenWidth()),
       static_cast<float>(GetScreenHeight())});
  m_drawList.Clear();
  m_drawList.Reserve(64);
}

void GameUiHost::Draw(entt::registry &registry,
                      const LevelManager &levelManager,
                      const Camera2D &camera,
                      NoMoreDay::systems::SpatialHashGrid *spatialGrid) {
  if (!m_initialized) {
    return;
  }
  // Transitional: legacy panel renderer first, then submit the new draw list
  // through the raylib backend. Same frame position as the U3 integration
  // point: after the scene composite, before EndDrawing.
  // The migrated skill hotbar + buff strip draws in-place inside
  // UISystem::Draw (frame-order coupling with the tooltip state machine and
  // the context menu pass; see UISystem::Draw section 2).
  // U7 group 5: astrolabe draws in-place (KEY_N/ESC frame coupling).
  // U7 group 6: the global overlays draw in-place through UISystem::Draw
  // (frame-order coupling with the tooltip state machine and the ground
  // hover pass; see UISystem::Draw section 3).
  // U7 group 6-B: the tooltip state machine moved out of UISystem::Draw into
  // the hosted controller. ResetFrame clears the hover cache before the legacy
  // pass writes it (skill hub / hotbar / buff strip / ground items);
  // UpdateState runs the state machine after the pass so every hover producer
  // of this frame has written, matching the original inline block position at
  // the end of UISystem::Draw.
  m_tooltip.ResetFrame();
  UISystem::Draw(registry, levelManager, camera, spatialGrid, &m_skillHotbar,
                 &m_stash, &m_crafting, &m_skillTree, &m_astrolabe,
                 &m_overlay);
  m_tooltip.UpdateState(registry);

  // U6b: pickup click detection moved out of the legacy draw path. Read-only
  // hit test against the visible item cache; the resulting intent is executed
  // by GameUiCommandHandler on the next Update (accepted one-frame delay).
  DetectPickupClick(registry, camera);

  m_backend.Render(m_viewport, m_drawList);
}

void GameUiHost::DrawMinimap(entt::registry &registry,
                             const LevelManager &levelManager,
                             NoMoreDay::systems::SpatialHashGrid *grid) {
  m_minimap.Draw(registry, levelManager, grid);
}

void GameUiHost::DrawHud(entt::registry &registry) {
  m_playerHud.Draw(registry);
}

void GameUiHost::DrawDraggingPhantom(entt::registry &registry) {
  // U7 group 6-B: the drag phantom + top-most tooltip pass. The hosted
  // TooltipController draws the active tooltip from its own members; the
  // legacy State-based path stays inside UISystem::DrawDraggingPhantom as the
  // null-controller (null-host) fallback only.
  UISystem::DrawDraggingPhantom(registry, &m_tooltip);
}

void GameUiHost::RenderMonsterHealthBars(entt::registry &registry,
                                         const Camera2D &camera) {
  m_monsterHealthBars.Render(registry, camera);
}

void GameUiHost::RenderMonsterHealthBarsUI(entt::registry &registry) {
  m_monsterHealthBars.RenderUI(registry);
}

void GameUiHost::DrawCharacter(entt::registry &registry) {
  // Legacy call sites pass no player; the controller falls back to a
  // PlayerTag lookup inside Draw.
  m_character.Draw(registry, entt::null);
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

void GameUiHost::DetectPickupClick(entt::registry &registry,
                                   const Camera2D &camera) {
  // Replicates the legacy ground-item pickup hit test (previously inside
  // UISystem::Draw) without touching the ECS: read-only collision against the
  // visible item cache, then a distance check against the player. The intent
  // is enqueued here and re-validated (entity validity, distance, capacity)
  // by GameUiCommandHandler during the Update phase (design §6.2).
  if (UISystem::IsModalInputCaptured()) {
    return; // Modal UI blocks world interaction, matching the legacy gate.
  }
  if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    return;
  }

  const Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

  const auto playerView = registry.view<PlayerTag, Position>();
  if (playerView.begin() == playerView.end()) {
    return; // No positioned player: nothing to pick up for.
  }
  const entt::entity player = playerView.front();
  const auto &playerPos = playerView.get<Position>(player);

  // Iterate ONLY visible items (already culled by RenderSystem).
  for (const auto &itemData : NoMoreDay::UiShared::VisibleItemCache::visibleItems) {
    // Simple AABB check in world space; first hit is the top-most item.
    if (!CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
      continue;
    }
    if (!registry.valid(itemData.entity)) {
      break;
    }
    const auto *itemPos = registry.try_get<const Position>(itemData.entity);
    if (itemPos == nullptr) {
      break;
    }
    const float dx = itemPos->x - playerPos.x;
    const float dy = itemPos->y - playerPos.y;
    const float distSq = dx * dx + dy * dy;
    if (distSq > 180.0f * 180.0f) {
      break; // Out of pickup range: keep the hover highlight, no intent.
    }

    GameUiIntent intent;
    intent.sourceNode = kInvalidUiId;
    intent.kind = GameUiIntentKind::PickupItem;
    intent.domainId = entt::to_integral(itemData.entity);
    EnqueueIntent(intent);
    break;
  }
}

void GameUiHost::ResetSessionState() {
  // Gameplay-scoped session data only: panel visibility, drag state, tooltip,
  // message box and quantity popup. Delegated to the legacy facade so the
  // reset lives next to the state it owns (design §4.1 state ownership).
  UISystem::ResetSessionState();
}

} // namespace NoMoreDay::ui
