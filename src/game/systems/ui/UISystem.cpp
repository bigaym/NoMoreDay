#include "game/systems/ui/UISystem.hpp"
#include "app/Game.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/RenderSystem.hpp" // ADDED
#include "engine/physics/SpatialGrid.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/components/UIAnimationComponent.hpp"
#include "game/data/BuffRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UIAnimationSystem.hpp"
#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/systems/ui/UICharacter.hpp"
#include "game/systems/ui/UICrafting.hpp" // ADDED
#include "game/systems/ui/UIInventory.hpp"
#include "game/systems/ui/UIMinimap.hpp"
#include "game/systems/ui/UISkillHub.hpp"
#include "game/systems/ui/UIPanelDragService.hpp"
#include "game/systems/ui/UISkillTalentTree.hpp"
#include "game/systems/ui/UIStash.hpp"
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
  LoadEmojiFallbackFont();

  UIAstrolabe::Initialize();
}

void UISystem::Shutdown() {
  State.globalFont = {0};
  State.emojiFont = {0};
  UIMinimap::Cleanup();
  AssetLoadingSystem::Shutdown();
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

bool UISystem::IsSkillTreeVisible(entt::registry &registry,
                                  entt::entity entity) {
  return State.showSkillTree; // We need to add showSkillTree to UIContext
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
                      const LevelManager &levelManager) {
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

  if (State.showSkillTree)
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
      if (!UIAstrolabe::IsVisible(registry, view.front())) {
        UIAstrolabe::Toggle(registry, view.front());
      } else {
        UIAstrolabe::ResetView();
      }

      if (UIAstrolabe::IsVisible(registry, view.front())) {
        State.showInventory = false;
        State.showCharacterPanel = false;
        State.showContextMenu = false;
        State.showSkillTree = false;
      }
    }
  }

  // Skill Tree (S)
  if (IsKeyPressed(KEY_S)) {
    State.showSkillTree = !State.showSkillTree;
    if (State.showSkillTree) {
      State.showInventory = false;
      State.showCharacterPanel = false;
      State.showContextMenu = false;
      // Also close Astrolabe if open
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        if (UIAstrolabe::IsVisible(registry, view.front())) {
          UIAstrolabe::Toggle(registry, view.front());
        }
      }
    } else {
      State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID; // Reset view
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
          UIStash::Open(interact.type);
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
      State.showQuantityPopup = false;
      State.isTyping = false;
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
      State.showContextMenu = false;
    } else if (State.showSkillTree) {
      State.showSkillTree = false;
    } else {
      // Check if Astrolabe is open
      auto view = registry.view<PlayerTag>();
      if (view.begin() != view.end()) {
        if (UIAstrolabe::IsVisible(registry, view.front())) {
          UIAstrolabe::Toggle(registry, view.front());
        }
      }
    }
  }

  // Debug
  if (IsKeyPressed(KEY_F1))
    UIMinimap::ToggleDebugReveal();

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

  UIInventory::Update(registry);
  UIStash::Update(registry);
  UIAstrolabe::Update(registry);
  UICrafting::Update(registry); // ADDED

  if (IsKeyPressed(KEY_K)) {
    UICrafting::Toggle();
    if (UICrafting::IsVisible()) {
      State.showInventory = true; // Open inventory to drag items
    }
  }

  if (State.showMessageBox) {
    State.messageBoxTimer -= GetFrameTime();
    if (State.messageBoxTimer <= 0.0f)
      State.showMessageBox = false;
  }
}

void UISystem::Draw(entt::registry &registry, const LevelManager &levelManager,
                    const Camera2D &camera,
                    NoMoreDay::systems::SpatialHashGrid *spatialGrid) {
  // --- Scale Calculation ---
  float scaleX = (float)GetScreenWidth() / UI_REF_WIDTH;
  float scaleY = (float)GetScreenHeight() / UI_REF_HEIGHT;
  float scale = std::min(scaleX, scaleY);
  State.scaleFactor = scale;
  UIRenderer::SetScale(scale);

  State.isMouseOverUI = false;
  SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  State.hoveredItem = entt::null;
  State.hoveredSkillSlot = -1;
  State.hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  State.hoveredBuffIdx = -1;

  if (IsModalInputCaptured()) {
    State.isMouseOverUI = true;
    assert(State.isMouseOverUI && "Modal UI must capture pointer input");
  }

  // 1. Draw Subsystems (Passed logic coordinates will be scaled by UIRenderer)
  UIMinimap::Draw(registry, levelManager);
  UIAstrolabe::Draw(registry);
  UICrafting::Draw(registry);
  UIStash::Draw(registry);

  // Skill Tree Hub
  auto playerView = registry.view<PlayerTag>();
  if (playerView.begin() != playerView.end()) {
    entt::entity player = playerView.front();
    if (State.showSkillTree) {
      if (State.selectedSkillId == NoMoreDay::INVALID_SKILL_ID) {
        UISkillHub::Draw(registry, player);
      } else {
        NoMoreDay::SkillTreeUI::Draw(&registry, (int)player, State.selectedSkillId);
      }
    }
  }

  // 2. HUD (Always on top of panels if requested, or keep HUD accessible)
  DrawSkillHotbar(registry);
  DrawBuffs(registry);

  // 3. Ground Interaction highlights (drawn below overlays)
  if (!IsModalInputCaptured() && State.hoveredItem == entt::null) {

    // Phase 1 Optimization: Use shared cache from RenderSystem
    // Use Mouse World Position to check against Item World Rects directly
    Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

    // Prepare player info for pickup check
    Vector2 playerPos2D = {0, 0};
    entt::entity playerEntity = entt::null;
    auto pView = registry.view<PlayerTag, Position>();
    if (pView.begin() != pView.end()) {
      playerEntity = pView.front();
      auto &p = pView.get<Position>(playerEntity);
      playerPos2D = {p.x, p.y};
    }

    // Iterate ONLY visible items (Already culled by RenderSystem)
    for (const auto& itemData : RenderSystem::VisibleItemCache::visibleItems) {
        // Simple AABB Check in World Space
        if (CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
            State.hoveredItem = itemData.entity;

            // Interaction: Pickup
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && playerEntity != entt::null) {
                // Since we don't have Item Position in cache, we assume Rect Center or query registry
                // Optimization: Just query registry for this one hit
                if (registry.valid(itemData.entity)) {
                     const auto& p = registry.get<Position>(itemData.entity);
                     float dx = p.x - playerPos2D.x;
                     float dy = p.y - playerPos2D.y;
                     float distSq = dx * dx + dy * dy;

                     if (distSq <= 180.0f * 180.0f) {
                        if (InventorySystem::pickUpItem(registry, playerEntity, itemData.entity)) {
                            State.hoveredItem = entt::null;
                        } else {
                            State.showMessageBox = true;
                            utils::FormatToBuffer(State.messageBoxText,
                                                  "背包已满");
                            State.messageBoxTimer = 2.0f;
                        }
                     }
                }
            }
            break; // Found top-most item (or first hit)
        }
    }
  } // End of hoverTimer scope

  if (State.showContextMenu)
    DrawContextMenu(registry);
  if (State.showQuantityPopup)
    DrawQuantityPopup(registry);
  if (State.showMessageBox)
    DrawMessageBox();

  // 4. Overlays (Drawn LAST - Absolute Topmost)

  // Update Hover Targets
  uint32_t currentHoverSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity currentHoverItem = entt::null;
  int currentHoverBuffIdx = -1;

  if (State.hoveredItem != entt::null && registry.valid(State.hoveredItem)) {
      currentHoverItem = State.hoveredItem;
  } else if (State.hoveredSkillId != NoMoreDay::INVALID_SKILL_ID) {
      currentHoverSkillId = State.hoveredSkillId;
  } else if (State.hoveredSkillSlot != -1) {
      auto view = registry.view<PlayerTag, ActiveSkillsComponent>();
      if (view.begin() != view.end()) {
          const auto &active = view.get<ActiveSkillsComponent>(view.front());
          currentHoverSkillId = active.slots[State.hoveredSkillSlot].id;
      }
  } else if (State.hoveredBuffIdx != -1) {
      currentHoverBuffIdx = State.hoveredBuffIdx;
  }

  // State Machine Logic
  const float dt = GetFrameTime();
  const bool isAnythingHovered = (currentHoverSkillId != NoMoreDay::INVALID_SKILL_ID ||
                                  currentHoverItem != entt::null ||
                                  currentHoverBuffIdx != -1);
  const bool targetChanged = isAnythingHovered &&
                             (currentHoverSkillId != State.activeTooltipSkillId ||
                              currentHoverItem != State.activeTooltipItem ||
                              currentHoverBuffIdx != State.activeTooltipBuffIdx);

  if (targetChanged) {
      State.activeTooltipSkillId = currentHoverSkillId;
      State.activeTooltipItem = currentHoverItem;
      State.activeTooltipBuffIdx = currentHoverBuffIdx;
      State.tooltipDelayTimer = (State.tooltipAlpha > 0.01f) ? 0.05f : 0.12f;
      State.tooltipInitialized = false;
  }

  if (isAnythingHovered) {
      if (State.tooltipDelayTimer > 0.0f) {
          State.tooltipDelayTimer = std::max(0.0f, State.tooltipDelayTimer - dt);
      } else {
          State.tooltipAlpha = std::min(1.0f, State.tooltipAlpha + dt * 10.0f);
      }
  } else {
      if (State.tooltipHoveredLastFrame) {
          State.tooltipDelayTimer = 0.08f;
      }

      if (State.tooltipDelayTimer > 0.0f) {
          State.tooltipDelayTimer = std::max(0.0f, State.tooltipDelayTimer - dt);
      } else if (State.tooltipAlpha > 0.0f) {
          State.tooltipAlpha = std::max(0.0f, State.tooltipAlpha - dt * 8.0f);
      }

      if (State.tooltipAlpha <= 0.0f && State.tooltipDelayTimer <= 0.0f) {
          State.activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
          State.activeTooltipItem = entt::null;
          State.activeTooltipBuffIdx = -1;
          State.tooltipInitialized = false;
      }
  }

  State.tooltipHoveredLastFrame = isAnythingHovered;
}

void UISystem::DrawDraggingPhantom(entt::registry &registry) {
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

  // Draw tooltip at top-most overlay layer.
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

void UISystem::DrawSkillHotbar(entt::registry &registry) {
  auto view = registry.view<PlayerTag, ActiveSkillsComponent, CombatStats>();
  if (view.begin() == view.end())
    return;

  entt::entity player = view.front();
  const auto &active = view.get<ActiveSkillsComponent>(player);
  const auto &stats = view.get<CombatStats>(player);

  float slotSize = 54.0f;
  float padding = 8.0f;
  float totalW = (slotSize * 5) + (padding * 4);

  // Logic Position: Bottom Center
  float startX = (UI_REF_WIDTH - totalW) / 2.0f;
  float startY = UI_REF_HEIGHT - slotSize - 20.0f;

  const char *labels[] = {"Q", "W", "E", "R", "RMB"};

  for (int i = 0; i < 5; ++i) {
    const auto &slot = active.slots[i];
    float x = startX + i * (slotSize + padding);
    float y = startY;

    Texture2D icon = {0};
    float cooldownRatio = 0.0f;
    float manaCost = 0.0f;
    int maxCharges = 1;
    bool hasEnoughMana = true;

    if (slot.id != 0) {
      const auto *skillData = SkillRegistry::Get().GetSkill(slot.id);
      if (skillData) {
        if (skillData->icon_id != 0) {
          icon = AssetLoadingSystem::GetTexture(skillData->icon_id);
        }

        manaCost = skillData->mana_cost;
        maxCharges = skillData->max_charges;
        hasEnoughMana = stats.mana >= manaCost;

        if (skillData->cooldown > 0) {
          cooldownRatio =
              std::clamp(slot.cooldown / skillData->cooldown, 0.0f, 1.0f);
        }
      }
    }

    bool isHovered = CheckCollisionPointRec(GetMousePositionLogic(),
                                            {x, y, slotSize, slotSize});
    bool isPressed = false;
    if (i == 0)
      isPressed = IsKeyDown(KEY_Q);
    else if (i == 1)
      isPressed = IsKeyDown(KEY_W);
    else if (i == 2)
      isPressed = IsKeyDown(KEY_E);
    else if (i == 3)
      isPressed = IsKeyDown(KEY_R);
    else if (i == 4)
      isPressed = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);

    if (isHovered) {
      State.hoveredSkillSlot = i;
      State.isMouseOverUI = true;

      // Debug: Trace cooldown values for first few slots
      if (i < 2) {
        // LOG_TRACE("Skill Slot {}: ID={}, Cooldown={:.2f}, Charges={}/{}", i,
        // slot.id, slot.cooldown, slot.current_charges, maxCharges);
      }

      // Drop logic
      if (State.isDraggingSkill && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // ... (existing drop logic)
        auto *activePtr = registry.try_get<ActiveSkillsComponent>(player);
        if (activePtr) {
          activePtr->slots[i].id = State.draggedSkillId;
          LOG_INFO("Assigned skill {} to hotbar slot {}", State.draggedSkillId,
                   i);
        }
        State.isDraggingSkill = false;
        State.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
      }

      // Right-click context menu
      if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        State.showContextMenu = true;
        State.contextMenuPos = GetMousePosition();
        State.isSkillContext = true;
        State.contextSourceSkillSlot = i;
        State.isContextFromInventory = false;
        State.contextMenuItem = entt::null;
      }
    }

    NoMoreDay::UIRenderer::DrawSkillSlot(
        State.globalFont, x, y, slotSize, icon, labels[i], cooldownRatio,
        slot.cooldown, manaCost, slot.current_charges, maxCharges,
        hasEnoughMana, isHovered, isPressed, 0.8f);
  }
}

void UISystem::DrawBuffs(entt::registry &registry) {
  auto view = registry.view<PlayerTag, ActiveEffectsComponent>();
  if (view.begin() == view.end())
    return;

  entt::entity player = view.front();
  const auto &effects = view.get<ActiveEffectsComponent>(player);
  if (effects.effects.empty())
    return;

  // --- Metrics (Sync with PlayerHUD.cpp) ---
  float slotSize = 54.0f;
  float hotbarPadding = 8.0f;
  float hotbarW = (slotSize * 5) + (hotbarPadding * 4);
  float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f;
  float hotbarRight = hotbarLeft + hotbarW;

  float barWidth = 450.0f;
  float barMargin = 50.0f;
  float barTopY = UI_REF_HEIGHT - 30.0f - 28.0f;

  float hpLeftX = hotbarLeft - barMargin - barWidth;
  float manaLeftX = hotbarRight + barMargin;

  // Buff Metrics
  float iconSize = 40.0f; // Slightly larger for better visibility
  float padding = 4.0f;
  float yOffset = 10.0f; // Space between bar and icons

  int maxPerRow = (int)std::floor((barWidth + padding) / (iconSize + padding));
  if (maxPerRow < 1)
    maxPerRow = 1;

  int currentBuffs = 0;
  int currentDebuffs = 0;

  for (int i = 0; i < (int)effects.effects.size(); ++i) {
    const auto &effect = effects.effects[i];
    bool isDebuff = effect.is_debuff;

    int count = isDebuff ? currentDebuffs++ : currentBuffs++;
    float startX = isDebuff ? manaLeftX : hpLeftX;

    int row = count / maxPerRow;
    int col = count % maxPerRow;

    float x = startX + col * (iconSize + padding);
    float y = barTopY - yOffset - (row + 1) * (iconSize + padding);

    const auto &visual = BuffRegistry::GetVisualData(effect.type);
    Texture2D icon = {0};
    if (visual.icon_asset) {
      icon = AssetLoadingSystem::GetTexture(visual.icon_asset->id);
    }
    const char *iconText = visual.icon_text.c_str();

    float ratio = 0.0f;
    if (effect.duration > 0) {
      ratio = std::clamp(effect.remaining / effect.duration, 0.0f, 1.0f);
    }

    NoMoreDay::UIRenderer::DrawBuffIcon(State.globalFont, x, y, iconSize, icon,
                                        iconText, ratio, effect.stacks,
                                        isDebuff, 0.9f);

    if (CheckCollisionPointRec(GetMousePositionLogic(),
                               {x, y, iconSize, iconSize})) {
      State.hoveredBuffIdx = i;
      State.isMouseOverUI = true;
    }
  }
}

void UISystem::Benchmark(entt::registry &registry,
                         const LevelManager &levelManager, int frames) {}
