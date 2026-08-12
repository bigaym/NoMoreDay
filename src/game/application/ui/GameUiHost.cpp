#include "game/application/ui/GameUiHost.hpp"

#include "game/application/ui/UIAnimationSystem.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/WorldUiFrame.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace NoMoreDay::ui {

namespace {
// Handle under which the legacy global font is registered with the new
// backend. Panels migrate to draw-list Text commands against this handle.
constexpr UiResourceId kGlobalFontResourceId = 1;
} // namespace

GameUiHost::GameUiHost()
    : m_playerHud(m_runtime), m_minimap(m_runtime),
      // U8: host back-pointer so the hotbar routes its skill-drag reads
      // (drop target) through the host-owned drag session.
      m_skillHotbar(m_runtime, &m_tooltip, this), m_monsterHealthBars(m_runtime),
      // U8: back-pointer to the host so the inventory controller routes its
      // panel hover writes and context-menu opens through the host channels
      // (same pattern as m_stash/m_crafting below).
      m_inventory(m_runtime, this), m_character(m_runtime),
      // U8: back-pointer to the host so the panel controllers route their
      // hover writes through the host channel (SetHoveredItem) instead of the
      // static UiShared::HoveredItem() slot.
      m_stash(m_runtime, this), m_crafting(m_runtime, this),
      // U8: back-pointers so the skill tree routes its sibling closes and
      // the tooltip bind happens through the host (the tree also injects the
      // tooltip/host into its hub + talent tree instances).
      m_skillTree(m_runtime, &m_tooltip, this), m_astrolabe(m_runtime),
      m_overlay(m_runtime) {
  // U8: the tooltip controller's modal gate (DetectGroundHover) consults the
  // host instance instead of the legacy UISystem::IsModalInputCaptured query.
  m_tooltip.BindHost(this);
}

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
  // intents surface their user-facing message through the hosted message box
  // (was the legacy State.showMessageBox bridge; U8 routes it through the
  // overlay controller).
  for (const GameUiResult &result : m_pendingNotifications) {
    if (!result.success && !result.notification.empty()) {
      m_overlay.ShowMessageBox(result.notification.c_str());
    }
  }
  m_pendingNotifications.clear();

  // U8 inventory takeover: the KEY_I toggle moved out of GameplayState
  // (PushState<InventoryState> is gone) into the host update, at the original
  // frame position (before the legacy update) so the toggle is applied before
  // UISystem::Update reads the mirrored State.showInventory this frame.
  if (IsKeyPressed(KEY_I)) {
    m_inventory.Toggle();
  }

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
  // U8 final: the legacy UISystem::Update orchestration moved into the host
  // (UISystem::Update is gone). Frame order and behaviour are preserved:
  // animations, global hotkeys, the one-shot test-item grant and the
  // per-frame panel updates all run at their original positions.
  const float dt = GetFrameTime();

  // 0. Update Animation System
  UIAnimationSystem::Update(registry, dt);

  // Skill tree alpha animation (was the legacy UISystem::Update alpha block;
  // inventory/character alphas animate in their controllers below).
  m_skillTree.UpdateAlpha(dt);

  // 1. Global Hotkeys

  // Character Panel (C): toggles the hosted controller; closing also resets
  // the attribute-confirm temp values and the context menu (legacy
  // behaviour).
  if (IsKeyPressed(KEY_C)) {
    m_character.SetVisible(!m_character.IsVisible());
    if (!m_character.IsVisible()) {
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        auto &ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
        ui.tempStr = ui.tempDex = ui.tempInt = ui.tempVit = 0;
        ui.showConfirmPopup = false;
      }
    }
    m_overlay.CloseContextMenu();
  }

  // Quick Sort (Z)
  if (IsKeyPressed(KEY_Z)) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() != playerView.end()) {
      InventorySystem::organize(registry, playerView.front());
    }
  }

  // Astrolabe (N): first press opens (resetting the view), repeat presses
  // reset the camera instead of closing (legacy semantics). Opening closes
  // the sibling panels through the hosted controllers.
  if (IsKeyPressed(KEY_N)) {
    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
      entt::entity player = view.front();
      if (!m_astrolabe.IsVisible(registry, player)) {
        m_astrolabe.Toggle(registry, player);
      } else {
        m_astrolabe.ResetView();
      }
      if (m_astrolabe.IsVisible(registry, player)) {
        m_inventory.SetVisible(false);
        m_character.SetVisible(false);
        m_overlay.CloseContextMenu();
        m_skillTree.Close();
      }
    }
  }

  // Skill Tree (S)
  if (IsKeyPressed(KEY_S)) {
    m_skillTree.Toggle(registry);
  }

  // Stash Interaction (E)
  if (IsKeyPressed(KEY_E)) {
    auto playerView = registry.view<PlayerTag, Position>();
    if (playerView.begin() != playerView.end()) {
      const entt::entity playerEntity = playerView.front();
      const auto &pPos = playerView.get<Position>(playerEntity);

      auto stashView = registry.view<StashInteractableComponent, Position>();
      for (auto entity : stashView) {
        const auto &iPos = stashView.get<Position>(entity);
        float dx = iPos.x - pPos.x;
        float dy = iPos.y - pPos.y;
        if (dx * dx + dy * dy < 100.0f * 100.0f) {
          const auto &interact =
              stashView.get<StashInteractableComponent>(entity);
          m_stash.Open(interact.type);
          break;
        }
      }
    }
  }

  // Quick Pickup (F)
  if (IsKeyPressed(KEY_F)) {
    auto playerView = registry.view<PlayerTag, Position>();
    if (playerView.begin() != playerView.end()) {
      const entt::entity playerEntity = playerView.front();
      const auto &pPos = playerView.get<Position>(playerEntity);

      auto groundItemView = registry.view<ItemComponent, Position>();
      float pickupRangeSq = 200.0f * 200.0f; // Max pickup range (Increased)
      std::vector<entt::entity> itemsToPick;

      for (auto entity : groundItemView) {
        const auto &iPos = groundItemView.get<Position>(entity);
        float dx = iPos.x - pPos.x;
        float dy = iPos.y - pPos.y;
        float distSq = dx * dx + dy * dy;

        if (distSq < pickupRangeSq) {
          itemsToPick.push_back(entity);
        }
      }

      bool anyPicked = false;
      bool anyFailed = false;
      int attemptCount = 0;
      int successCount = 0;

      for (auto item : itemsToPick) {
        if (registry.valid(item)) {
          attemptCount++;
          if (InventorySystem::pickUpItem(registry, playerEntity, item)) {
            anyPicked = true;
            successCount++;
          } else {
            anyFailed = true;
          }
        }
      }

      if (attemptCount > 0) {
        LOG_LIMITED_INFO(1.0f, "批量拾取: 范围内的物品 {}, 成功拾取 {}",
                         attemptCount, successCount);
      }

      if (anyFailed && !anyPicked) {
        m_overlay.ShowMessageBox("背包已满");
      }
    }
  }

  // ESC Handling (quantity popup -> character panel -> context menu -> skill
  // tree -> astrolabe; original legacy priority order).
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (m_overlay.IsQuantityPopupVisible()) {
      m_overlay.CloseQuantityPopup();
    } else if (m_character.IsVisible()) {
      bool popupHandled = false;
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        auto &ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
        if (ui.showConfirmPopup) {
          ui.showConfirmPopup = false;
          popupHandled = true;
        }
      }
      if (!popupHandled) {
        m_character.SetVisible(false);
      }
    } else if (m_overlay.IsContextMenuVisible()) {
      m_overlay.CloseContextMenu();
    } else if (m_skillTree.IsVisible()) {
      m_skillTree.Close();
    } else {
      // Check if Astrolabe is open; ESC closes the panel.
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        if (m_astrolabe.IsVisible(registry, view.front())) {
          m_astrolabe.Toggle(registry, view.front());
        }
      }
    }
  }

  // One-shot debug/test item grant per gameplay session (was the legacy
  // UISystem::s_hasGivenTestItems static).
  if (!m_hasGivenTestItems) {
    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
      auto bag = ItemFactory::createBag(registry, 1, Rarity::Common);
      registry.get<ItemComponent>(bag).name = "破烂的背包";
      registry.get<ItemComponent>(bag).bagCapacity = 40;
      InventorySystem::pickUpItem(registry, view.front(), bag);

      // Test Skills
      auto &active =
          registry.get_or_emplace<ActiveSkillsComponent>(view.front());
      active.slots[0] = {1, 0.0f, 1}; // Skill 1: 1 charge
      active.slots[1] = {2, 0.0f, 3}; // Skill 2: 3 charges
      active.slots[4] = {2, 0.0f, 3}; // Skill 2: Also on RMB for testing

      // Test Buff
      auto &effects =
          registry.get_or_emplace<ActiveEffectsComponent>(view.front());
      effects.effects.clear();

      // Power Boost: +10% Phys Damage per stack
      BuffEffect pb;
      pb.id = "test_power";
      pb.name = "力量爆发";
      pb.description = "提升攻击力";
      pb.type = BuffType::PowerBoost;
      pb.duration = 30.0f;
      pb.remaining = 30.0f;
      pb.stacks = 3;
      pb.max_stacks = 10;
      pb.is_debuff = false;
      pb.modifiers.push_back({.value = 10.0f,
                              .type = StatType::PhysicalDamage,
                              .mode = ModifierMode::PercentAdd});
      effects.AddOrRefresh(pb);

      // Speed Up: +20% Move Speed
      BuffEffect spd;
      spd.id = "test_speed";
      spd.name = "疾风步";
      spd.description = "提升移动速度";
      spd.type = BuffType::SpeedUp;
      spd.duration = 15.0f;
      spd.remaining = 15.0f;
      spd.stacks = 1;
      spd.max_stacks = 1;
      spd.is_debuff = false;
      spd.modifiers.push_back({.value = 20.0f,
                               .type = StatType::MoveSpeed,
                               .mode = ModifierMode::PercentAdd});
      effects.AddOrRefresh(spd);

      // Stun: -100% Move Speed
      BuffEffect stn;
      stn.id = "test_stun";
      stn.name = "眩晕";
      stn.description = "无法行动";
      stn.type = BuffType::Stun;
      stn.duration = 2.0f;
      stn.remaining = 2.0f;
      stn.stacks = 1;
      stn.max_stacks = 1;
      stn.is_debuff = true;
      stn.modifiers.push_back({.value = -100.0f,
                               .type = StatType::MoveSpeed,
                               .mode = ModifierMode::PercentAdd});
      effects.AddOrRefresh(stn);

      // Poison: Just a visual debuff for now
      effects.effects.push_back({"test_poison", "剧毒", "持续受到伤害",
                                 BuffType::Poison, 5.0f, 5.0f, 10, 10, true});

      registry.emplace_or_replace<StatsDirty>(view.front());
      m_hasGivenTestItems = true;
    }
  }

  // Per-frame panel updates (was the legacy UISystem::Update tail).
  m_stash.Update(registry);
  m_astrolabe.Update(registry);
  m_crafting.Update(registry);

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

  // U8 inventory takeover: Esc/TAB close the inventory overlay. Esc runs
  // AFTER the legacy ESC chain (quantity popup -> character panel -> context
  // menu -> skill tree -> astrolabe) so those modal surfaces keep their
  // original priority over the panel; the inventory is closed only when no
  // other surface consumed the key. This replaces the legacy
  // InventoryState::OnUpdate Esc handling (pop the state). TAB matches the
  // legacy InventoryState close-on-TAB behaviour.
  if (IsKeyPressed(KEY_ESCAPE) && m_inventory.IsVisible() &&
      !m_overlay.IsQuantityPopupVisible() && !m_overlay.IsContextMenuVisible() &&
      !m_character.IsVisible()) {
    m_inventory.SetVisible(false);
  }
  if (IsKeyPressed(KEY_TAB) && m_inventory.IsVisible()) {
    m_inventory.SetVisible(false);
  }

  // U7 group 2: inventory alpha animation moved out of UISystem::Update (the
  // legacy call site was removed); it runs right after the legacy update so
  // the per-frame animation timing is equivalent. GetFrameTime() is
  // frame-scoped, so this ordering is behaviour-preserving.
  m_inventory.Update(registry, levelManager);

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
  // Retained for the public API contract; the in-place UI pass below only
  // needs the registry + camera (minimap/player-hud draw through their own
  // routes with their own signatures).
  (void)levelManager;
  (void)spatialGrid;
  // U8 final: the legacy UISystem::Draw orchestration moved into the host
  // (UISystem::Draw is gone). Frame order and behaviour are preserved: scale
  // fit, per-frame hover/mouse resets, the modal pointer gate, the panel
  // draw passes, then the tooltip state machine (which resolves this frame's
  // hover writes and mirrors the result onto the frame object).
  m_tooltip.ResetFrame();
  m_tooltip.DetectGroundHover(registry, camera);

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

  // 1. Draw panels (logic coordinates are scaled by UIRenderer). Pass order
  // matches the legacy UISystem::Draw sections.
  m_astrolabe.Draw(registry);
  m_crafting.Draw(registry);
  m_stash.Draw(registry);
  m_inventory.Draw(registry);

  // Skill Tree Hub + talent tree (needs the player entity).
  auto playerView = registry.view<PlayerTag>();
  if (playerView.begin() != playerView.end()) {
    m_skillTree.Draw(registry, playerView.front());
  }

  // 2. HUD (skill hotbar + buff strip) drawn after the panels, matching the
  // legacy frame order (hover/tooltip frame coupling preserved).
  m_skillHotbar.Draw(registry);

  // 3. Global overlays (context menu, quantity popup, message box) drawn at
  // the original frame position (after the panels, before the tooltip state
  // machine).
  m_overlay.DrawOverlays(registry);

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
  // U8 final: the drag phantom + top-most tooltip pass. The hosted drag
  // session is the single authoritative drag state; the phantom draws
  // directly from it (the legacy UISystem::DrawDraggingPhantom mirror bridge
  // is gone).
  // 1. Item Phantom
  if (m_dragSession.draggedItem != entt::null) {
    const Vector2 mPos = UISystem::GetMousePositionLogic();
    const float size = 64.0f;
    UIRenderer::DrawSlot(UISystem::GetFont(), registry, mPos.x - size * 0.5f,
                         mPos.y - size * 0.5f, size, m_dragSession.draggedItem,
                         nullptr, true);
  }

  // 2. Skill Phantom
  if (m_dragSession.isDraggingSkill &&
      m_dragSession.draggedSkillId != NoMoreDay::INVALID_SKILL_ID) {
    const Vector2 mPos = UISystem::GetMousePositionLogic();
    const float size = 48.0f;
    const auto *skill = SkillRegistry::Get().GetSkill(m_dragSession.draggedSkillId);
    if (skill != nullptr && skill->icon_id != 0) {
      const Texture2D icon = AssetLoadingSystem::GetTexture(skill->icon_id);
      DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height},
                     {mPos.x - size * 0.5f, mPos.y - size * 0.5f, size, size},
                     {0, 0}, 0.0f, Fade(WHITE, 0.7f));
    } else {
      DrawRectangleRec({mPos.x - size * 0.5f, mPos.y - size * 0.5f, size, size},
                       Fade(BLUE, 0.5f));
    }
  }

  // 3. Top-most tooltip pass (the hosted controller draws the active tooltip
  // from its own members).
  m_tooltip.DrawTooltip(registry);
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

void GameUiHost::SetHoveredItem(entt::entity entity) {
  // U8: converge panel hover write points (stash/crafting/inventory
  // controllers and states) onto the tooltip controller's hover source. The
  // static UiShared::HoveredItem() slot must stay write-free outside the U8
  // fallback branches in UISystem.
  m_tooltip.SetHoveredItem(entity);
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

bool GameUiHost::IsAnyPanelOpen(entt::registry &registry) const {
  // U8 host read-side migration: aggregates the hosted panel controllers'
  // instance visibility (replaces the legacy State.showSkillTree ||
  // State.showCharacterPanel anyPanelOpen check in GameplayState). The
  // character panel also counts the fading-out alpha (legacy render gate
  // semantics); the astrolabe needs the registry to resolve the player.
  const entt::entity player = UISystem::GetPlayerEntity(registry);
  return m_inventory.IsVisible() || m_skillTree.IsVisible() ||
         IsCharacterPanelVisible() || m_stash.IsVisible() ||
         m_crafting.IsVisible() || m_astrolabe.IsVisible(registry, player);
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

void GameUiHost::DetectPickupClick(entt::registry &registry,
                                   const Camera2D &camera) {
  // Replicates the legacy ground-item pickup hit test (previously inside
  // UISystem::Draw) without touching the ECS: read-only collision against the
  // frame-scoped visible item cache, then a distance check against the
  // player. The intent is enqueued here and re-validated (entity validity,
  // distance, capacity) by GameUiCommandHandler during the Update phase
  // (design §6.2).
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

  if (m_worldFrame == nullptr) {
    return; // Frame not bound: no visible item cache to test against.
  }

  // Iterate ONLY visible items (already culled by RenderSystem).
  for (const auto &itemData : m_worldFrame->VisibleItems()) {
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
  // Gameplay-scoped session data: the panel controllers reset themselves on
  // Enter/LeaveGameplay; the host-owned drag session is cleared here so no
  // drag state leaks into the next run.
  m_dragSession.Clear();
}

} // namespace NoMoreDay::ui
