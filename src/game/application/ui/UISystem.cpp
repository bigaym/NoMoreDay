#include "game/application/ui/UISystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/UIAnimationComponent.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/application/ui/UIAnimationSystem.hpp"
#include "game/application/ui/AstrolabeController.hpp"
#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/UICharacter.hpp"
#include "game/application/ui/UICrafting.hpp" // ADDED
#include "game/application/ui/UICraftingController.hpp"
#include "game/application/ui/UIInventory.hpp"
#include "game/application/ui/UIMinimap.hpp"
#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/application/ui/UIStash.hpp"
#include "game/application/ui/UIStashController.hpp"
#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/SkillHotbarController.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/systems/world/LevelManager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath> // For std::min
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>


using namespace NoMoreDay;

// --- Static Member Initialization ---
NoMoreDay::UIContext UISystem::State;

// --- Lifecycle ---

void UISystem::Initialize(ResourceManager &resourceManager) {
  AssetLoadingSystem::Initialize(resourceManager);
  AssetLoadingSystem::LoadAllUI(); // Load core UI textures (buttons, panels, etc.)
  AssetLoadingSystem::LoadAllEquipment(); // Ensure all equipment textures are
                                          // registered

  // Initialize Animations
  State.inventorySlotAnims.assign(100,
                                  {0.0f, 1.0f}); // Assume max 100 slots for now
  State.equipmentSlotAnims.assign(15, {0.0f, 1.0f});
  State.bagSlotAnims.assign(4, {0.0f, 1.0f});

  State.hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  State.activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
  State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  State.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;

#ifdef TEST_HEADLESS
  LOG_INFO("UISystem: Headless mode, skipping font loading.");
  State.globalFont = GetFontDefault();
  UiShared::SetGlobalFont(State.globalFont);
  return;
#endif

  const auto &mainFont = assets::ui::fonts::Main_Chinese;
  State.emojiFont = {0};

  auto LoadEmojiFallbackFont = [&]() {
    // Explicitly load the emoji set currently used by UI labels.
    std::vector<int> emojiCodepoints = {
        0x2694, // ⚔
        0x1F392, // 🎒
        0x1F451, // 👑
        0x1F455, // 👕
        0x1F48D, // 💍
        0x1F4FF, // 📿
        0x1F6E1, // 🛡
        0x1F97E, // 🥾
        0x1F9BF, // 🦿
        0x1F9E4, // 🧤
        0x1F9E9, // 🧩
        0x1F9EA, // 🧪
        0x1F9F9, // 🧹
        0x1FA96, // 🪖
        0x1FA99  // 🪙
    };

    std::vector<std::string> emojiFontCandidates = {
        "C:/Windows/Fonts/seguiemj.ttf", // Segoe UI Emoji
        "C:/Windows/Fonts/seguisym.ttf"  // Segoe UI Symbol
    };

    const entt::id_type emojiFontId = entt::hashed_string("ui_font_emoji");
    for (const auto &emojiPath : emojiFontCandidates) {
      if (!FileExists(emojiPath.c_str())) {
        continue;
      }
      State.emojiFont = resourceManager.loadFont(
          emojiFontId, emojiPath, mainFont.defaultSize, emojiCodepoints.data(),
          (int)emojiCodepoints.size());
      if (State.emojiFont.texture.id != 0) {
        SetTextureFilter(State.emojiFont.texture, TEXTURE_FILTER_BILINEAR);
        LOG_INFO("UISystem: Loaded emoji fallback font from '{}'", emojiPath);
        return;
      }
    }

    LOG_WARN("UISystem: Emoji fallback font unavailable, emoji will degrade to '?'");
    State.emojiFont = {0};
  };

  std::vector<int> codepoints;
  for (int i = 32; i <= 126; ++i)
    codepoints.push_back(i);
  codepoints.push_back(0x2022); // •
  codepoints.push_back(0x00B7); // ·
  codepoints.push_back(0x2605); // ★
  codepoints.push_back(0x26A0); // ⚠️
  for (int i = 0x3000; i <= 0x303F; ++i)
    codepoints.push_back(i);
  for (int i = 0x4E00; i <= 0x9FFF; ++i)
    codepoints.push_back(i);
  for (int i = 0xFF00; i <= 0xFFEF; ++i)
    codepoints.push_back(i);

  std::vector<std::string> fontCandidates;
  fontCandidates.push_back("C:/Windows/Fonts/simhei.ttf");
  fontCandidates.push_back("C:/Windows/Fonts/msyh.ttc");
  fontCandidates.push_back("C:/Windows/Fonts/simsun.ttc");

  for (const auto &path : fontCandidates) {
    if (FileExists(path.c_str())) {
      LOG_INFO("UISystem: Attempting to load font from '{}'...", path);
      State.globalFont =
          resourceManager.loadFont(mainFont.id, path, mainFont.defaultSize,
                                   codepoints.data(), (int)codepoints.size());

      if (State.globalFont.texture.id != 0) {
        SetTextureFilter(State.globalFont.texture, TEXTURE_FILTER_BILINEAR);
        LoadEmojiFallbackFont();
        UiShared::SetGlobalFont(State.globalFont);
        LOG_INFO("UISystem: Successfully loaded Chinese font from '{}'", path);
        return;
      } else {
        LOG_WARN(
            "UISystem: Failed to load font from '{}', trying next candidate...",
            path);
      }
    }
  }

  LOG_ERROR("UISystem: All Chinese font candidates failed. Falling back to "
            "default font (??? for Chinese).");
  if (State.globalFont.texture.id == 0)
    State.globalFont = GetFontDefault();
  UiShared::SetGlobalFont(State.globalFont);
  LoadEmojiFallbackFont();

  // U7 group 5: astrolabe initialization moved to GameUiHost::Initialize
  // (the hosted AstrolabeController loads the shaders/renderer right after
  // this call, keeping the original load order).
}

void UISystem::Shutdown() {
  State.globalFont = {0};
  UiShared::SetGlobalFont(State.globalFont);
  State.emojiFont = {0};
  AssetLoadingSystem::Shutdown();
}

void UISystem::ResetSessionState() {
  // Gameplay-session scoped state only (design §4.1): anything a player can
  // leave open mid-run is cleared so no session state leaks into the next
  // run. Panel internals (tabs, search) keep their own defaults and are not
  // touched here.
  State.showCharacterPanel = false;
  State.characterPanelAlpha = 0.0f;
  State.showInventory = false;
  State.inventoryAlpha = 0.0f;
  State.showStash = false;
  State.stashAlpha = 0.0f;
  State.showSkillTree = false;
  State.skillTreeAlpha = 0.0f;
  State.selectedSkillId = INVALID_SKILL_ID;

  State.activeDragPanel = UIPanelID::None;
  for (auto &panel : State.panelStates) {
    panel = PanelState{};
  }

  State.showContextMenu = false;
  State.contextMenuItem = entt::null;
  State.contextMenuPos = {0.0f, 0.0f};
  State.isContextFromInventory = false;
  State.contextSourceInventoryIndex = -1;
  State.contextSourceEquipmentSlot = EquipmentSlot::None;
  State.isSkillContext = false;
  State.contextSourceSkillSlot = -1;

  State.draggedItem = entt::null;
  State.isDraggingFromInventory = false;
  State.dragSourceInventoryIndex = -1;
  State.dragSourceEquipmentSlot = EquipmentSlot::None;
  State.dragSourceBagSlotIndex = -1;
  State.isDraggingFromStash = false;
  State.dragSourceStashTab = -1;
  State.dragSourceStashSlot = -1;
  State.dragSourceStashType = StashType::Personal;
  State.draggedSkillId = INVALID_SKILL_ID;
  State.isDraggingSkill = false;

  State.hoveredSkillSlot = -1;
  State.hoveredSkillId = INVALID_SKILL_ID;
  State.hoveredBuffIdx = -1;

  State.activeTooltipSkillId = INVALID_SKILL_ID;
  State.activeTooltipItem = entt::null;
  State.activeTooltipBuffIdx = -1;
  State.tooltipDelayTimer = 0.0f;
  State.tooltipAlpha = 0.0f;
  State.tooltipPos = {0.0f, 0.0f};
  State.tooltipInitialized = false;
  State.tooltipHoveredLastFrame = false;

  State.showMessageBox = false;
  State.messageBoxText[0] = '\0';
  State.messageBoxTimer = 0.0f;

  State.showQuantityPopup = false;
  State.quantityTargetItem = entt::null;
  State.quantityActionType = 0;
  State.quantityVal = 1;
  State.quantityMax = 1;
  State.quantityInputBuf[0] = '\0';

  State.isTyping = false;
  State.isMouseOverUI = false;
  State.playerEntity = entt::null;

  State.inventorySlotAnims.assign(100, {0.0f, 1.0f});
  State.equipmentSlotAnims.assign(15, {0.0f, 1.0f});
  State.bagSlotAnims.assign(4, {0.0f, 1.0f});

  UiShared::HoveredItem() = entt::null;
}

// --- Helper ---

entt::entity UISystem::GetPlayerEntity(entt::registry &registry) {
  auto view = registry.view<PlayerTag>();
  if (view.begin() == view.end())
    return entt::null;
  return view.front();
}

Vector2 UISystem::GetMousePositionLogic() {
  Vector2 m = GetMousePosition();
  float s = State.scaleFactor;
  if (s <= 0.0001f)
    s = 1.0f;
  return {m.x / s, m.y / s};
}

void UISystem::UpdatePanelDrag(NoMoreDay::UIPanelID id, float &x, float &y,
                               float w, float h, float headerHeight) {
  auto &pState = State.panelStates[(int)id];
  UIPanelDragInputs input{};
  input.mousePosition = GetMousePositionLogic();
  input.isMousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  input.isMouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

  UIPanelDragBounds bounds{};
  bounds.panelWidth = w;
  bounds.panelHeight = h;
  bounds.headerHeight = headerHeight;
  bounds.minVisiblePixels = 50.0f;
  bounds.uiRefWidth = UI_REF_WIDTH;
  bounds.uiRefHeight = UI_REF_HEIGHT;

  UIPanelDragService::UpdatePanelDrag(pState, id, State.activeDragPanel, x, y,
                                      input, bounds);
}

// --- Main Loop ---

void UISystem::Update(entt::registry &registry,
                      const LevelManager &levelManager,
                      NoMoreDay::ui::UIStashController *stashController,
                      NoMoreDay::ui::UICraftingController *craftingController,
                      NoMoreDay::ui::SkillTreeController *skillTreeController,
                      NoMoreDay::ui::AstrolabeController *astrolabeController,
                      NoMoreDay::ui::OverlayController *overlayController) {
  float dt = GetFrameTime();

  // 0. Cache Player Entity for efficient UI access
  if (State.playerEntity == entt::null || !registry.valid(State.playerEntity)) {
      State.playerEntity = GetPlayerEntity(registry);
  }

  // 0. Update Animation System
  UIAnimationSystem::Update(registry, dt);

  // Transition Panel Alphas
  float alphaSpeed = 6.0f;
  if (State.showInventory)
    State.inventoryAlpha =
        std::min(1.0f, State.inventoryAlpha + dt * alphaSpeed);
  else
    State.inventoryAlpha =
        std::max(0.0f, State.inventoryAlpha - dt * alphaSpeed);

  if (State.showCharacterPanel)
    State.characterPanelAlpha =
        std::min(1.0f, State.characterPanelAlpha + dt * alphaSpeed);
  else
    State.characterPanelAlpha =
        std::max(0.0f, State.characterPanelAlpha - dt * alphaSpeed);

  if (skillTreeController) {
    skillTreeController->UpdateAlpha(dt);
  } else if (State.showSkillTree)
    State.skillTreeAlpha =
        std::min(1.0f, State.skillTreeAlpha + dt * alphaSpeed);
  else
    State.skillTreeAlpha =
        std::max(0.0f, State.skillTreeAlpha - dt * alphaSpeed);

  // 1. Global Hotkeys

  // Character Panel (C)
  if (IsKeyPressed(KEY_C)) {
    State.showCharacterPanel = !State.showCharacterPanel;
    if (!State.showCharacterPanel) {
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        auto &ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
        ui.tempStr = ui.tempDex = ui.tempInt = ui.tempVit = 0;
        ui.showConfirmPopup = false;
      }
    }
    State.showContextMenu = false;
  }

  // Quick Sort (Z)
  if (IsKeyPressed(KEY_Z)) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() != playerView.end()) {
      InventorySystem::organize(registry, playerView.front());
    }
  }

  // Astrolabe (N)
  if (IsKeyPressed(KEY_N)) {
    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
      // U7 group 5: KEY_N routes through the host controller. Legacy
      // semantics preserved: first press opens (resetting the view),
      // repeat presses reset the camera instead of closing.
      entt::entity player = view.front();
      if (astrolabeController) {
        if (!astrolabeController->IsVisible(registry, player)) {
          astrolabeController->Toggle(registry, player);
        } else {
          astrolabeController->ResetView();
        }

        if (astrolabeController->IsVisible(registry, player)) {
          State.showInventory = false;
          State.showCharacterPanel = false;
          State.showContextMenu = false;
          State.showSkillTree = false;
        }
      }
    }
  }

  // Skill Tree (S)
  if (IsKeyPressed(KEY_S)) {
    if (skillTreeController) {
      skillTreeController->Toggle(registry);
    } else {
      State.showSkillTree = !State.showSkillTree;
      if (State.showSkillTree) {
        State.showInventory = false;
        State.showCharacterPanel = false;
        State.showContextMenu = false;
        // Also close Astrolabe if open (U7 group 5: host controller route;
        // the hosted SkillTreeController handles this via SharedContext).
        if (astrolabeController) {
          auto view = registry.view<PlayerTag>();
          if (view.begin() != view.end()) {
            if (astrolabeController->IsVisible(registry, view.front())) {
              astrolabeController->Toggle(registry, view.front());
            }
          }
        }
      } else {
        State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID; // Reset view
      }
    }
  }

  // Stash Interaction (E)
  if (IsKeyPressed(KEY_E)) {
    auto playerView = registry.view<PlayerTag, Position>();
    if (playerView.begin() != playerView.end()) {
      auto playerEntity = playerView.front();
      const auto &pPos = playerView.get<Position>(playerEntity);

      auto stashView = registry.view<StashInteractableComponent, Position>();
      for (auto entity : stashView) {
        const auto &iPos = stashView.get<Position>(entity);
        float dx = iPos.x - pPos.x;
        float dy = iPos.y - pPos.y;
        if (dx * dx + dy * dy < 100.0f * 100.0f) {
          const auto &interact =
              stashView.get<StashInteractableComponent>(entity);
          // U7 group 3: stash open routes through the host controller; the
          // legacy static panel remains as the null-controller fallback.
          if (stashController) {
            stashController->Open(interact.type);
          } else {
            UIStash::Open(interact.type);
          }
          break;
        }
      }
    }
  }

  // Quick Pickup (F)
  if (IsKeyPressed(KEY_F)) {
    auto playerView = registry.view<PlayerTag, Position>();
    if (playerView.begin() != playerView.end()) {
      auto playerEntity = playerView.front();
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
        LOG_LIMITED_INFO(1.0f, "批量拾取: 范围内的物品 {}, 成功拾取 {}", attemptCount,
                 successCount);
      }

      if (anyFailed && !anyPicked) {
        State.showMessageBox = true;
        utils::FormatToBuffer(State.messageBoxText, "背包已满");
        State.messageBoxTimer = 1.5f;
      }
    }
  }

  // ESC Handling
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (State.showQuantityPopup) {
      // U7 group 6: the quantity popup closes through the hosted overlay
      // controller (legacy static close kept as the null-controller
      // fallback).
      if (overlayController) {
        overlayController->CloseQuantityPopup();
      } else {
        State.showQuantityPopup = false;
        State.isTyping = false;
      }
    } else if (State.showCharacterPanel) {
      bool popupHandled = false;
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        auto &ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
        if (ui.showConfirmPopup) {
          ui.showConfirmPopup = false;
          popupHandled = true;
        }
      }
      if (!popupHandled)
        State.showCharacterPanel = false;
    } else if (State.showContextMenu) {
      // U7 group 6: the context menu closes through the hosted overlay
      // controller (legacy static close kept as the null-controller
      // fallback).
      if (overlayController) {
        overlayController->CloseContextMenu();
      } else {
        State.showContextMenu = false;
      }
    } else if (skillTreeController ? skillTreeController->IsVisible()
                                  : State.showSkillTree) {
      if (skillTreeController) {
        skillTreeController->Close();
      } else {
        State.showSkillTree = false;
      }
    } else {
      // Check if Astrolabe is open (U7 group 5: routes through the host
      // controller; ESC closes the panel).
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        if (astrolabeController &&
            astrolabeController->IsVisible(registry, view.front())) {
          astrolabeController->Toggle(registry, view.front());
        }
      }
    }
  }

  // Debug (minimap F1 toggle moved to GameUiHost::Update, U7 group 1).

  if (!s_hasGivenTestItems) {
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

      // Skill 2 in JSON has 2s cooldown. Let's make it have 3 charges for
      // testing Actually I should update the JSON or the component after
      // loading. For now, I'll just set it to 1/1.

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
      s_hasGivenTestItems = true;
    }
  }

  // U7 group 2: UIInventory::Update (inventory alpha animation) moved to
  // GameUiHost::Update (m_inventory.Update) right after this call.
  // U7 group 3: stash/crafting update routes through the host controllers;
  // the legacy static panels remain as the null-controller fallbacks.
  if (stashController) {
    stashController->Update(registry);
  } else {
    UIStash::Update(registry);
  }
  // U7 group 5: astrolabe update routes through the host controller.
  if (astrolabeController) {
    astrolabeController->Update(registry);
  }
  if (craftingController) {
    craftingController->Update(registry);
  } else {
    UICrafting::Update(registry); // ADDED
  }

  if (IsKeyPressed(KEY_K)) {
    if (craftingController) {
      craftingController->Toggle();
      if (craftingController->IsVisible()) {
        State.showInventory = true; // Open inventory to drag items
      }
    } else {
      UICrafting::Toggle();
      if (UICrafting::IsVisible()) {
        State.showInventory = true; // Open inventory to drag items
      }
    }
  }

  // U7 group 6: the message box timer moved to
  // OverlayController::UpdateMessageBox (called by GameUiHost::Update right
  // after this function, keeping the original frame position). The
  // null-controller fallback keeps the legacy decay so the null-host path
  // still auto-dismisses.
  if (overlayController == nullptr && State.showMessageBox) {
    State.messageBoxTimer -= GetFrameTime();
    if (State.messageBoxTimer <= 0.0f)
      State.showMessageBox = false;
  }
}

void UISystem::Draw(entt::registry &registry, const LevelManager &levelManager,
                    const Camera2D &camera,
                    NoMoreDay::systems::SpatialHashGrid *spatialGrid,
                    NoMoreDay::ui::SkillHotbarController *hotbarController,
                    NoMoreDay::ui::UIStashController *stashController,
                    NoMoreDay::ui::UICraftingController *craftingController,
                    NoMoreDay::ui::SkillTreeController *skillTreeController,
                    NoMoreDay::ui::AstrolabeController *astrolabeController,
                    NoMoreDay::ui::OverlayController *overlayController) {
  // --- Scale Calculation ---
  float scaleX = (float)GetScreenWidth() / UI_REF_WIDTH;
  float scaleY = (float)GetScreenHeight() / UI_REF_HEIGHT;
  float scale = std::min(scaleX, scaleY);
  State.scaleFactor = scale;
  UIRenderer::SetScale(scale);

  State.isMouseOverUI = false;
  SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  UiShared::HoveredItem() = entt::null;
  State.hoveredSkillSlot = -1;
  State.hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  State.hoveredBuffIdx = -1;

  if (IsModalInputCaptured()) {
    State.isMouseOverUI = true;
    assert(State.isMouseOverUI && "Modal UI must capture pointer input");
  }

  // 1. Draw Subsystems (Passed logic coordinates will be scaled by UIRenderer)
  // UIMinimap moved to GameUiHost::DrawMinimap (U7 group 1); the legacy
  // null-host fallback in GameplayState still draws it.
  // U7 group 3: crafting/stash draw routes through the host controllers; the
  // legacy static panels remain as the null-controller fallbacks.
  // U7 group 5: astrolabe draw routes through the host controller; the
  // legacy static panel is removed (no null-host fallback).
  if (astrolabeController) {
    astrolabeController->Draw(registry);
  }
  if (craftingController) {
    craftingController->Draw(registry);
  } else {
    UICrafting::Draw(registry);
  }
  if (stashController) {
    stashController->Draw(registry);
  } else {
    UIStash::Draw(registry);
  }

  // Skill Tree Hub (migrated to SkillTreeController, U7 group 4: the panel
  // classes are instance types now, so the legacy fallback branch is gone).
  auto playerView = registry.view<PlayerTag>();
  if (playerView.begin() != playerView.end()) {
    entt::entity player = playerView.front();
    if (skillTreeController) {
      skillTreeController->Draw(registry, player);
    }
  }

  // 2. HUD (Always on top of panels if requested, or keep HUD accessible)
  // Skill hotbar + buff strip migrated to SkillHotbarController (U7 group 1).
  // The controller draws here, in-place, because the hover/tooltip state
  // machine below (State.hoveredSkillSlot/hoveredBuffIdx) and the context
  // menu pass must see this frame's hotbar hover updates (frame-order
  // coupling preserved). GameUiHost passes its controller instance.
  if (hotbarController != nullptr) {
    hotbarController->Draw(registry);
  }

  // 3. Ground Interaction highlights (drawn below overlays).
  // Pickup click execution moved to GameUiHost::Draw (U6b); this block only
  // performs the hover hit test and writes the highlighted item. No gameplay
  // mutation happens here anymore.
  if (!IsModalInputCaptured() && UiShared::HoveredItem() == entt::null) {

    // Phase 1 Optimization: Use shared cache from RenderSystem
    // Use Mouse World Position to check against Item World Rects directly
    Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

    // Iterate ONLY visible items (Already culled by RenderSystem)
    for (const auto& itemData : UiShared::VisibleItemCache::visibleItems) {
        // Simple AABB Check in World Space
        if (CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
            UiShared::HoveredItem() = itemData.entity;
            break; // Found top-most item (or first hit)
        }
    }
  } // End of hoverTimer scope

  // U7 group 6: the three global overlays (context menu, quantity popup,
  // message box) route through the hosted OverlayController, drawn here at
  // the original frame position (after the ground hover pass, before the
  // tooltip state machine) so the stacking order is unchanged. The legacy
  // static draws remain as the null-controller fallback.
  if (overlayController) {
    overlayController->DrawOverlays(registry);
  } else {
    if (State.showContextMenu)
      DrawContextMenu(registry);
    if (State.showQuantityPopup)
      DrawQuantityPopup(registry);
    if (State.showMessageBox)
      DrawMessageBox();
  }

  // 4. Overlays (Drawn LAST - Absolute Topmost)

  // U7 group 6-B: the tooltip hover resolution and delay/fade state machine
  // moved out of UISystem::Draw into the hosted TooltipController (see
  // GameUiHost::Draw: ResetFrame before this pass, UpdateState right after
  // it). The frame-order contract is unchanged: every hover producer of this
  // frame (skill hub / talent tree via State.hoveredSkillId, hotbar and buff
  // strip via the controller cache, ground items via UiShared::HoveredItem)
  // writes before the state machine runs.
}

void UISystem::DrawDraggingPhantom(
    entt::registry &registry,
    NoMoreDay::ui::TooltipController *tooltipController) {
  // 1. Item Phantom
  if (State.draggedItem != entt::null) {
    Vector2 mPos = GetMousePositionLogic();
    float size = 64.0f;
    UIRenderer::DrawSlot(State.globalFont, registry, mPos.x - size * 0.5f,
                         mPos.y - size * 0.5f, size, State.draggedItem, nullptr,
                         true);
  }

  // 2. Skill Phantom
  if (State.isDraggingSkill && State.draggedSkillId != NoMoreDay::INVALID_SKILL_ID) {
    Vector2 mPos = GetMousePositionLogic();
    float size = 48.0f;
    const auto *skill = SkillRegistry::Get().GetSkill(State.draggedSkillId);
    if (skill && skill->icon_id != 0) {
      Texture2D icon = AssetLoadingSystem::GetTexture(skill->icon_id);
      DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height},
                     {mPos.x - size * 0.5f, mPos.y - size * 0.5f, size, size},
                     {0, 0}, 0.0f, Fade(WHITE, 0.7f));
    } else {
      DrawRectangleRec({mPos.x - size * 0.5f, mPos.y - size * 0.5f, size, size},
                       Fade(BLUE, 0.5f));
    }
  }

  // Draw tooltip at the top-most overlay layer. U7 group 6-B: the hosted
  // controller draws the active tooltip from its own members when present;
  // the legacy State-based block below remains as the null-controller
  // fallback (null-host render path only).
  if (tooltipController) {
    tooltipController->DrawTooltip(registry);
    return;
  }
  if (State.tooltipAlpha > 0.01f) {
    if (State.activeTooltipItem != entt::null && registry.valid(State.activeTooltipItem)) {
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
      UIRenderer::DrawTooltip(State.globalFont, registry, State.activeTooltipItem,
                              State.tooltipAlpha);
    } else if (State.activeTooltipSkillId != NoMoreDay::INVALID_SKILL_ID) {
      UIRenderer::DrawSkillTooltip(State.globalFont, registry,
                                   State.activeTooltipSkillId,
                                   State.tooltipAlpha);
    } else if (State.activeTooltipBuffIdx != -1) {
      auto view = registry.view<PlayerTag, ActiveEffectsComponent>();
      if (view.begin() != view.end()) {
        const auto &effects = view.get<ActiveEffectsComponent>(view.front());
        if (State.activeTooltipBuffIdx < (int)effects.effects.size()) {
          UIRenderer::DrawBuffTooltip(State.globalFont,
                                      effects.effects[State.activeTooltipBuffIdx],
                                      State.tooltipAlpha);
        }
      }
    }
  }
}

// --- Delegate to UIRenderer ---

void UISystem::DrawSlot(entt::registry &registry, float x, float y, float size,
                        entt::entity item, const char *defaultLabel,
                        bool highlighted, bool isLocked, float alpha) {
  UIRenderer::DrawSlot(State.globalFont, registry, x, y, size, item,
                       defaultLabel, highlighted, isLocked, alpha);
}

void UISystem::DrawTextUI(const char *text, float x, float y, float fontSize,
                          Color color, float alpha) {
  UIRenderer::DrawTextUI(State.globalFont, text, x, y, fontSize, color, alpha);
}

void UISystem::DrawTextScaled(const char *text, float x, float y,
                              float fontSize, float maxWidth, Color color,
                              float alpha) {
  UIRenderer::DrawTextScaled(State.globalFont, text, x, y, fontSize, maxWidth,
                             color, alpha);
}

void UISystem::OpenContextMenu(entt::entity item, bool fromInv, int invIdx,
                               NoMoreDay::EquipmentSlot slot) {
  State.showContextMenu = true;
  State.contextMenuItem = item;
  State.contextMenuPos =
      GetMousePosition(); // Store Screen Pos for Context Menu (handled by
                          // UIRenderer specially)
  State.isContextFromInventory = fromInv;
  State.contextSourceInventoryIndex = invIdx;
  State.contextSourceEquipmentSlot = slot;
}

void UISystem::DrawContextMenu(entt::registry &registry) {
  UIRenderer::DrawContextMenu(
      State.globalFont, State, registry,
      1.0f); // Context menu usually immediate? Or use global alpha if needed.
}

void UISystem::DrawTooltip(entt::registry &registry, entt::entity item) {
  UIRenderer::DrawTooltip(State.globalFont, registry, item, 1.0f);
}

void UISystem::DrawMessageBox() {
  UIRenderer::DrawMessageBox(State.globalFont, State, 1.0f);
}

void UISystem::DrawQuantityPopup(entt::registry &registry) {
  if (!State.showQuantityPopup) {
    return;
  }

  auto closeQuantityPopup = []() {
    State.showQuantityPopup = false;
    State.quantityTargetItem = entt::null;
    State.quantityInputBuf[0] = '\0';
    State.isTyping = false;
  };

  State.isTyping = true;

  if (!registry.valid(State.quantityTargetItem) ||
      !registry.all_of<ItemComponent>(State.quantityTargetItem)) {
    closeQuantityPopup();
    return;
  }

  auto view = registry.view<PlayerTag>();
  if (view.begin() == view.end()) {
    closeQuantityPopup();
    return;
  }
  const entt::entity player = view.front();

  const auto &item = registry.get<ItemComponent>(State.quantityTargetItem);
  State.quantityMax = std::max(1, std::min(State.quantityMax, item.quantity));
  State.quantityVal = std::clamp(State.quantityVal, 1, State.quantityMax);

  while (int key = GetCharPressed()) {
    if (key >= '0' && key <= '9') {
      const size_t len = std::strlen(State.quantityInputBuf);
      if (len + 1 < sizeof(State.quantityInputBuf)) {
        State.quantityInputBuf[len] = (char)key;
        State.quantityInputBuf[len + 1] = '\0';
      }
    }
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    const size_t len = std::strlen(State.quantityInputBuf);
    if (len > 0) {
      State.quantityInputBuf[len - 1] = '\0';
    }
  }

  if (State.quantityInputBuf[0] != '\0') {
    const int parsed = std::atoi(State.quantityInputBuf);
    State.quantityVal = std::clamp(parsed, 1, State.quantityMax);
  }

  const int wheelDelta = (int)GetMouseWheelMove();
  if (wheelDelta != 0) {
    State.quantityVal = std::clamp(State.quantityVal + wheelDelta, 1, State.quantityMax);
    utils::FormatToBuffer(State.quantityInputBuf, "{}", State.quantityVal);
  }

  if (IsKeyPressed(KEY_UP)) {
    State.quantityVal = std::min(State.quantityVal + 1, State.quantityMax);
    utils::FormatToBuffer(State.quantityInputBuf, "{}", State.quantityVal);
  }
  if (IsKeyPressed(KEY_DOWN)) {
    State.quantityVal = std::max(State.quantityVal - 1, 1);
    utils::FormatToBuffer(State.quantityInputBuf, "{}", State.quantityVal);
  }

  const float popupW = 320.0f;
  const float popupH = 190.0f;
  const float x = (float)GetScreenWidth() * 0.5f - popupW * 0.5f;
  const float y = (float)GetScreenHeight() * 0.5f - popupH * 0.5f;

  DrawRectangle((int)x, (int)y, (int)popupW, (int)popupH, Fade(BLACK, 0.88f));
  DrawRectangleLinesEx({x, y, popupW, popupH}, 1.5f, Fade(WHITE, 0.75f));

  const char *actionLabel = State.quantityActionType == 1 ? "销毁数量" : "丢弃数量";
  DrawText(actionLabel, (int)(x + 14), (int)(y + 12), 24, WHITE);
  DrawText(item.name.c_str(), (int)(x + 14), (int)(y + 48), 20, LIGHTGRAY);

  char rangeText[64] = {0};
  utils::FormatToBuffer(rangeText, "范围: 1 - {}", State.quantityMax);
  DrawText(rangeText, (int)(x + 14), (int)(y + 78), 18, GRAY);

  char valueText[64] = {0};
  utils::FormatToBuffer(valueText, "数量: {}", State.quantityVal);
  DrawText(valueText, (int)(x + 14), (int)(y + 104), 24, GOLD);

  const Rectangle confirmRect = {x + 14.0f, y + popupH - 52.0f, 136.0f, 36.0f};
  const Rectangle cancelRect = {x + popupW - 150.0f, y + popupH - 52.0f, 136.0f, 36.0f};
  const Vector2 mouse = GetMousePosition();

  const bool confirmHovered = CheckCollisionPointRec(mouse, confirmRect);
  const bool cancelHovered = CheckCollisionPointRec(mouse, cancelRect);

  DrawRectangleRec(confirmRect, confirmHovered ? DARKGREEN : Fade(DARKGREEN, 0.8f));
  DrawRectangleRec(cancelRect, cancelHovered ? MAROON : Fade(MAROON, 0.8f));
  DrawText("确认", (int)(confirmRect.x + 48), (int)(confirmRect.y + 8), 20, WHITE);
  DrawText("取消", (int)(cancelRect.x + 48), (int)(cancelRect.y + 8), 20, WHITE);

  bool confirmAction = false;
  bool cancelAction = false;
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    confirmAction = true;
  }
  if (IsKeyPressed(KEY_ESCAPE)) {
    cancelAction = true;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    if (confirmHovered) {
      confirmAction = true;
    } else if (cancelHovered || !CheckCollisionPointRec(mouse, {x, y, popupW, popupH})) {
      cancelAction = true;
    }
  }

  if (confirmAction) {
    const int quantity = std::clamp(State.quantityVal, 1, State.quantityMax);
    if (State.quantityActionType == 1) {
      InventorySystem::destroyItem(registry, player, State.quantityTargetItem, quantity);
    } else {
      InventorySystem::dropItem(registry, player, State.quantityTargetItem, quantity);
    }

    closeQuantityPopup();
  } else if (cancelAction) {
    closeQuantityPopup();
  }
}

bool UISystem::IsModalInputCaptured() {
  return State.showQuantityPopup || State.showSkillTree;
}

void UISystem::Benchmark(entt::registry &registry,
                         const LevelManager &levelManager, int frames) {}
