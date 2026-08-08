#include "game/systems/ui/UICrafting.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/systems/ui/UIRenderer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/ui_shared/UiShared.hpp"
#include <algorithm>
#include <cmath>


namespace NoMoreDay {

// Define static members here since they are inline in HPP but some compilers
// might complain if not used carefully actually inline static in C++17 is fine.

void UICrafting::Toggle() {
  m_visible = !m_visible;
  if (!m_visible) {
    // Return item to inventory if panel is closed?
    // For now, keep it referenced but maybe clear if entity destroyed.
  }
}

bool UICrafting::IsVisible() { return m_visible; }

void UICrafting::OpenMergePanel() {
    m_visible = true;
    m_currentTab = CraftingTab::Merging;
}

void UICrafting::SetTargetItem(entt::entity item) {
  m_forgeItem = item;
  m_visible = true; // Auto-open when setting target via context menu
}

entt::entity UICrafting::GetTargetItem() { return m_forgeItem; }

void UICrafting::ClearTargetItem() { m_forgeItem = entt::null; }

void UICrafting::Update(entt::registry &registry) {
  float dt = GetFrameTime();
  float alphaSpeed = 6.0f;
  if (m_visible)
    m_craftingAlpha = std::min(1.0f, m_craftingAlpha + dt * alphaSpeed);
  else
    m_craftingAlpha = std::max(0.0f, m_craftingAlpha - dt * alphaSpeed);

  if (m_forgeItem != entt::null && !registry.valid(m_forgeItem))
    m_forgeItem = entt::null;
  if (m_mergeBase != entt::null && !registry.valid(m_mergeBase))
    m_mergeBase = entt::null;
  if (m_mergeFodder != entt::null && !registry.valid(m_mergeFodder))
    m_mergeFodder = entt::null;
  if (m_mergeCatalyst != entt::null && !registry.valid(m_mergeCatalyst))
    m_mergeCatalyst = entt::null;
  if (m_salvageItem != entt::null && !registry.valid(m_salvageItem))
    m_salvageItem = entt::null;
}

void UICrafting::Draw(entt::registry &registry) {
  if (m_craftingAlpha <= 0.0f)
    return;

  DrawCraftingPanel(registry);
}

void UICrafting::DrawCraftingPanel(entt::registry &registry) {
  auto &state = UISystem::State;
  auto &s_theme = UIRenderer::GetTheme();
  float alpha = m_craftingAlpha;

  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();

  // Logic Dimensions
  float panelW_Logic = 600.0f;
  float panelH_Logic = 700.0f;

  // Initial Logic Position (Centered)
  float startX_Logic = (UI_REF_WIDTH - panelW_Logic) / 2.0f;
  float startY_Logic = (UI_REF_HEIGHT - panelH_Logic) / 2.0f;

  // Handle Drag in Logic Space
  UISystem::UpdatePanelDrag(UIPanelID::Crafting, startX_Logic, startY_Logic, panelW_Logic, panelH_Logic, 60.0f);

  // Convert to Screen Space for Drawing (legacy behavior of this file)
  float panelW = panelW_Logic * state.scaleFactor;
  float panelH = panelH_Logic * state.scaleFactor;
  float startX = startX_Logic * state.scaleFactor;
  float startY = startY_Logic * state.scaleFactor;

  // Background
  DrawRectangleRec({startX, startY, panelW, panelH},
                   Fade(Color{30, 30, 40, 255}, 0.95f * alpha));
  DrawRectangleLinesEx({startX, startY, panelW, panelH}, 2.0f,
                       Fade(GOLD, alpha));

  // Title & Tabs
  float titleY = startY + 20;

  // Tab Buttons
  float tabW_Logic = 120.0f;
  float tabH_Logic = 32.0f;
  float tabX_Logic = startX_Logic + 20.0f;
  float titleY_Logic = startY_Logic + 20.0f;

  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

  auto DrawTab = [&](const char *label, CraftingTab tab) {
    bool active = (m_currentTab == tab);
    Rectangle tabRect_Logic = {tabX_Logic, titleY_Logic, tabW_Logic, tabH_Logic};
    bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), tabRect_Logic);

    Color tabTint = active ? GOLD : WHITE;
    Color textColor = active ? BLACK : WHITE;

    UIRenderer::DrawButton(state.globalFont, rectTex, tabRect_Logic, label, 20.0f, textColor, tabTint, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      m_currentTab = tab;
    tabX_Logic += tabW_Logic + 10.0f;
  };

  DrawTab("词缀锻造", CraftingTab::Forging);
  DrawTab("传奇融合", CraftingTab::Merging);
  DrawTab("装备分解", CraftingTab::Salvaging);

  // Close Button
  Rectangle closeRect_Logic = {startX_Logic + panelW_Logic - 40, startY_Logic + 15, 28, 28};
  bool closeHover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), closeRect_Logic);
  Texture2D squareTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Square.id);
  
  UIRenderer::DrawButton(state.globalFont, squareTex, closeRect_Logic, "X", 20, closeHover ? RED : WHITE, WHITE, closeHover, closeHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
  if (closeHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) Toggle();

  if (m_currentTab == CraftingTab::Merging) {
    DrawMergePanel(registry, startX_Logic, startY_Logic, panelW_Logic, panelH_Logic, alpha);
    return;
  }
  if (m_currentTab == CraftingTab::Salvaging) {
    DrawSalvagePanel(registry, startX_Logic, startY_Logic, panelW_Logic, panelH_Logic, alpha);
    return;
  }

  // Target Item Slot
  float slotSize = 80.0f * state.scaleFactor;
  float slotX = startX_Logic * state.scaleFactor + (panelW - slotSize) / 2.0f;
  float slotY = startY_Logic * state.scaleFactor + 80.0f * state.scaleFactor;

  UIRenderer::DrawSlot(state.globalFont, registry, slotX, slotY, slotSize,
                       m_forgeItem, "放入装备", false, false, alpha);

  // Handle Item Drop for Forging
  Rectangle slotRect = {slotX, slotY, slotSize, slotSize};
  if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
    if (state.draggedItem != entt::null &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (registry.any_of<ItemComponent>(state.draggedItem)) {
        auto &item = registry.get<ItemComponent>(state.draggedItem);
        // Allow equipment
        if (item.type == ItemType::Weapon || item.type == ItemType::Armor ||
            item.type == ItemType::Jewelry || item.type == ItemType::Shield) {
          m_forgeItem = state.draggedItem;
          state.draggedItem = entt::null;
        }
      }
    }
    if (m_forgeItem != entt::null) {
      UiShared::HoveredItem() = entt::null;
      UIRenderer::DrawTooltip(state.globalFont, registry, m_forgeItem, alpha);
    }
  }

  if (m_forgeItem != entt::null) {
    auto &item = registry.get<ItemComponent>(m_forgeItem);
    char potBuf[64];
    utils::FormatToBuffer(potBuf, "锻造潜力: {}", item.forgingPotential);
    float potW = MeasureTextEx(state.globalFont, potBuf, 20, 1.0f).x;
    UISystem::DrawTextUI(potBuf, startX_Logic + (panelW_Logic - potW / state.scaleFactor) / 2.0f,
                         80.0f + slotSize / state.scaleFactor + 10, 20, SKYBLUE, alpha);
    
    // Guidance Text
    const char* guide = "提示：锻造会消耗装备潜力。潜力耗尽后将无法再修改。";
    float guideW = MeasureTextEx(state.globalFont, guide, 16 * state.scaleFactor, 1.0f).x;
    UISystem::DrawTextUI(guide, (panelW_Logic - guideW / state.scaleFactor) / 2.0f + startX_Logic, panelH_Logic + startY_Logic - 40, 16, GRAY, alpha);

    DrawAffixList(registry, m_forgeItem, startX_Logic, startY_Logic);
  } else {
    const char* guide = "将装备拖入上方槽位开始锻造（升级、粉碎、重置词缀）";
    float guideW = MeasureTextEx(state.globalFont, guide, 16 * state.scaleFactor, 1.0f).x;
    UISystem::DrawTextUI(guide, (panelW_Logic - guideW / state.scaleFactor) / 2.0f + startX_Logic, 80.0f + 100, 16, GRAY, alpha);
  }
}

void UICrafting::DrawMergePanel(entt::registry &registry, float startX,
                                float startY, float panelW, float panelH,
                                float alpha) {
  auto &state = UISystem::State;
  float scale = state.scaleFactor;

  float slotSize_Logic = 64.0f;
  float spacing_Logic = 20.0f;

  float midX = startX + panelW / 2.0f;
  float topY = startY + 100.0f;

  // Base Slot
  float baseX = midX - slotSize_Logic - spacing_Logic;
  UIRenderer::DrawSlot(state.globalFont, registry, baseX * scale, topY * scale, slotSize_Logic * scale,
                       m_mergeBase, m_mergeBase == entt::null ? "放入暗金(LP > 0)" : "", false, false, alpha);

  // Fodder Slot
  float fodderX = midX + spacing_Logic;
  UIRenderer::DrawSlot(state.globalFont, registry, fodderX * scale, topY * scale, slotSize_Logic * scale,
                       m_mergeFodder, m_mergeFodder == entt::null ? "放入崇高(T6+)" : "", false, false, alpha);

  // Catalyst Slot
  float catX = midX - slotSize_Logic / 2.0f;
  float catY = topY + slotSize_Logic + spacing_Logic * 2;
  UIRenderer::DrawSlot(state.globalFont, registry, catX * scale, catY * scale, slotSize_Logic * scale,
                       m_mergeCatalyst, "放入时空核心", false, false, alpha);

  // Guidance Labels
  UISystem::DrawTextUI("暗金基底", baseX, topY - 25, 18, GOLD, alpha);
  UISystem::DrawTextUI("崇高物品", fodderX, topY - 25, 18, PURPLE, alpha);
  UISystem::DrawTextUI("传奇核心", catX, catY - 25, 18, SKYBLUE, alpha);

  // Handle Drops
  auto HandleMergeDrop = [&](entt::entity &target, float x_logic, float y_logic, int type) {
    Rectangle r_logic = {x_logic, y_logic, slotSize_Logic, slotSize_Logic};
    if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), r_logic)) {
      if (state.draggedItem != entt::null &&
          IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (registry.any_of<ItemComponent>(state.draggedItem)) {
            bool valid = false;
            auto& item = registry.get<ItemComponent>(state.draggedItem);
            
            if (type == 0) { // Base: LP > 0
                if (item.legendaryPotential > 0) valid = true;
            } else if (type == 1) { // Fodder: Exalted (Rare with T6+)
                bool hasT6 = false;
                for(const auto& aff : item.affixes) if(aff.tier >= 6) { hasT6 = true; break; }
                if (hasT6) valid = true;
            } else if (type == 2) { // Catalyst
                // Allow Material OR Consumable (Legendary Core)
                if (item.type == ItemType::Material || item.type == ItemType::Consumable) valid = true; 
            }

            if (valid) {
                target = state.draggedItem;
                state.draggedItem = entt::null;
            }
        }
      }
      if (target != entt::null) {
        UiShared::HoveredItem() = entt::null;
        UIRenderer::DrawTooltip(state.globalFont, registry, target, alpha);
      }
    }
  };

  HandleMergeDrop(m_mergeBase, baseX, topY, 0);
  HandleMergeDrop(m_mergeFodder, fodderX, topY, 1);
  HandleMergeDrop(m_mergeCatalyst, catX, catY, 2);

  // Affix Selection Interface
  if (m_mergeFodder != entt::null && registry.valid(m_mergeFodder)) {
    auto &fodder = registry.get<ItemComponent>(m_mergeFodder);
    float affixY = catY + slotSize_Logic + 20.0f;
    UISystem::DrawTextUI("选择要转移并保留的词缀:", startX + 40,
                         affixY, 18, LIGHTGRAY, alpha);

    affixY += 30.0f;
    for (int i = 0; i < (int)fodder.affixes.size(); ++i) {
      float x = startX + 40;
      float w = panelW - 80;
      float h = 40;
      Rectangle rowRect_Logic = {x, affixY, w, h};

      bool selected = (m_selectedAffixIndex == i);
      bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), rowRect_Logic);

      Color bg = selected ? Fade(RED, 0.3f) : Fade(DARKGRAY, 0.5f);
      if (hover && !selected)
        bg = Fade(GRAY, 0.4f);

      DrawRectangleRec({rowRect_Logic.x * scale, rowRect_Logic.y * scale, rowRect_Logic.width * scale, rowRect_Logic.height * scale}, Fade(bg, alpha));
      DrawRectangleLinesEx({rowRect_Logic.x * scale, rowRect_Logic.y * scale, rowRect_Logic.width * scale, rowRect_Logic.height * scale}, 1.0f * scale, Fade(selected ? RED : GRAY, alpha));

      Color textColor = GetAffixTierColor(fodder.affixes[i].tier);
      char buf[128];
      utils::FormatToBuffer(buf, "{}",
                            GetAffixDescription(fodder.affixes[i], true));
      UISystem::DrawTextUI(buf, x + 10, affixY + 10, 18, textColor, alpha);

      if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_selectedAffixIndex = i;
      }

      affixY += h + 5;
    }
  }

  // Fuse Button
  float btnW = 160.0f;
  float btnH = 50.0f;
  float btnX = midX - btnW / 2.0f;
  float btnY = startY + panelH - 80.0f;

  Rectangle btnRect_Logic = {btnX, btnY, btnW, btnH};
  bool canFuse = m_mergeBase != entt::null && m_mergeFodder != entt::null &&
                 m_mergeCatalyst != entt::null && m_selectedAffixIndex != -1;

  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
  bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), btnRect_Logic);

  UIRenderer::DrawButton(state.globalFont, rectTex, btnRect_Logic, "开始融合", 24, canFuse ? WHITE : GRAY, canFuse ? RED : DARKGRAY, canFuse && hover, canFuse && hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

  if (canFuse && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      CraftingResult res =
          CraftingSystem::fuseLegendary(registry, m_mergeBase, m_mergeFodder,
                                        m_mergeCatalyst, m_selectedAffixIndex);
      if (res == CraftingResult::Success) {
        // VFX: Burst of Gold and Red particles
        auto &ps = systems::GPUParticleSystem::Get();
        Vector2 center = {(btnX + btnW / 2.0f) * scale, (btnY + btnH / 2.0f) * scale};

        for (int i = 0; i < 40; ++i) {
          float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
          float speed = (float)GetRandomValue(100, 300);
          Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};

          if (i < 20) {
            // Gold Sparks
            auto p =
                systems::InkEffectHelper::CreateSpark(center, vel, GOLD, 2.5f);
            ps.Emit(p);
          } else {
            // Red/Ancient Ink
            auto p = systems::InkEffectHelper::CreateInkTrail(center, vel, 2.0f,
                                                              0.8f);
            p.color = {230, 0, 0, 200}; // Ancient Red
            ps.Emit(p);
          }
        }

        // Clear consumed slots
        if (!registry.valid(m_mergeFodder))
          m_mergeFodder = entt::null;
        if (!registry.valid(m_mergeCatalyst))
          m_mergeCatalyst = entt::null;
        m_selectedAffixIndex = -1;
      }
  }

  const char* bottomGuide = "融合会将崇高物品的随机词缀转移到暗金基底上。";
  float bW = MeasureTextEx(state.globalFont, bottomGuide, 14 * scale, 1.0f).x;
  UISystem::DrawTextUI(bottomGuide, (midX - bW / scale / 2.0f), (panelH + startY - 30), 14, GRAY, alpha);
}

void UICrafting::DrawAffixList(entt::registry &registry, entt::entity entity, float panelStartX, float panelStartY) {
  auto &state = UISystem::State;
  auto &item = registry.get<ItemComponent>(entity);
  auto playerEnt = UISystem::GetPlayerEntity(registry);
  float alpha = m_craftingAlpha;
  float scale = state.scaleFactor;

  float panelW = 600.0f;
  float startX = panelStartX;
  float startY = panelStartY;

  float currentY = startY + 200.0f;
  float rowH = 50.0f;
  float padding = 10.0f;

  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
  Texture2D squareTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Square.id);

  // Helper to draw an affix row
  auto DrawAffixRow = [&](Affix *affix, int index, bool isPrefix, int slotIdx) {
    float x = startX + 20.0f;
    float w = panelW - 40.0f;
    Rectangle rowRect_Logic = {x, currentY, w, rowH};

    DrawRectangleRec({rowRect_Logic.x * scale, rowRect_Logic.y * scale, rowRect_Logic.width * scale, rowRect_Logic.height * scale}, Fade(DARKGRAY, 0.5f * alpha));
    DrawRectangleLinesEx({rowRect_Logic.x * scale, rowRect_Logic.y * scale, rowRect_Logic.width * scale, rowRect_Logic.height * scale}, 1.0f * scale, Fade(GRAY, alpha));

    if (affix) {
      // Existing Affix
      char nameBuf[128];
      utils::FormatToBuffer(nameBuf, "T{} - {}", affix->tier,
                            GetAffixDescription(*affix, false));
      UISystem::DrawTextUI(nameBuf, x + 10, currentY + 15, 18, WHITE, alpha);

      // Upgrade Button
      float btnW = 60.0f;
      float btnH = 30.0f;
      float btnX = x + w - btnW - 10;

      bool canAfford = item.forgingPotential > 0;

      if (affix->tier < 5) {
        Rectangle btnRect_Logic = {btnX, currentY + 10, btnW, btnH};
        bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), btnRect_Logic);
        
        UIRenderer::DrawButton(state.globalFont, rectTex, btnRect_Logic, "升级", 16, WHITE, canAfford ? (hover ? GREEN : DARKGREEN) : GRAY, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

        if (canAfford && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::upgradeAffix(item, index);
            if (playerEnt != entt::null) registry.get_or_emplace<StatsDirty>(playerEnt);
        }
      } else {
        UISystem::DrawTextUI("MAX", btnX + 10, currentY + 15, 16, GOLD, alpha);
      }

      // Chaos (C)
      if (affix->tier < 5) {
        Rectangle cRect_Logic = {btnX - 35.0f, currentY + 10, 30.0f, btnH};
        bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), cRect_Logic);
        UIRenderer::DrawButton(state.globalFont, squareTex, cRect_Logic, "C", 16, WHITE, canAfford ? (hover ? PURPLE : VIOLET) : GRAY, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
        
        if (canAfford && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::chaosAffix(item, index);
            if (playerEnt != entt::null) registry.get_or_emplace<StatsDirty>(playerEnt);
        }
      }

      // Refine (R) - Values
      {
        Rectangle rRect_Logic = {btnX - 70.0f, currentY + 10, 30.0f, btnH};
        bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), rRect_Logic);
        UIRenderer::DrawButton(state.globalFont, squareTex, rRect_Logic, "R", 16, WHITE, canAfford ? (hover ? SKYBLUE : BLUE) : GRAY, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
        
        if (canAfford && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::refineAffixValues(item, index);
            if (playerEnt != entt::null) registry.get_or_emplace<StatsDirty>(playerEnt);
        }
      }

    } else {
      // Empty Slot
      UISystem::DrawTextUI(isPrefix ? "空前缀槽位" : "空后缀槽位", x + 10,
                           currentY + 15, 18, GRAY, alpha);

      // Add Button
      float btnW = 80.0f;
      float btnH = 30.0f;
      Rectangle btnRect_Logic = {x + w - btnW - 10, currentY + 10, btnW, btnH};
      bool canAfford = item.forgingPotential > 0;
      bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), btnRect_Logic);
      
      UIRenderer::DrawButton(state.globalFont, rectTex, btnRect_Logic, "添加", 16, WHITE, canAfford ? (hover ? BLUE : DARKBLUE) : GRAY, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

      if (canAfford && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            AffixType types[] = {
                AffixType::Strength,           AffixType::Dexterity,
                AffixType::Intelligence,       AffixType::Vitality,
                AffixType::FlatPhysicalDamage, AffixType::AttackSpeed};
            AffixType t = types[GetRandomValue(0, 5)];
            CraftingSystem::addAffix(item, t, isPrefix);
            if (playerEnt != entt::null) registry.get_or_emplace<StatsDirty>(playerEnt);
      }
    }

    currentY += rowH + padding;
  };

  // Sort affixes into prefixes and suffixes
  std::vector<int> prefixIndices;
  std::vector<int> suffixIndices;
  for (size_t i = 0; i < item.affixes.size(); ++i) {
    if (item.affixes[i].isPrefix)
      prefixIndices.push_back((int)i);
    else
      suffixIndices.push_back((int)i);
  }

  UISystem::DrawTextUI("前缀属性", startX + 20, currentY, 20, LIGHTGRAY,
                       alpha);
  currentY += 30;

  for (int i = 0; i < 2; ++i) {
    if (i < (int)prefixIndices.size()) {
      DrawAffixRow(&item.affixes[prefixIndices[i]], prefixIndices[i], true, i);
    } else {
      DrawAffixRow(nullptr, -1, true, i);
    }
  }

  currentY += 10;
  UISystem::DrawTextUI("后缀属性", startX + 20, currentY, 20, LIGHTGRAY,
                       alpha);
  currentY += 30;

  for (int i = 0; i < 2; ++i) {
    if (i < (int)suffixIndices.size()) {
      DrawAffixRow(&item.affixes[suffixIndices[i]], suffixIndices[i], false, i);
    } else {
      DrawAffixRow(nullptr, -1, false, i);
    }
  }
}

void UICrafting::DrawSalvagePanel(entt::registry &registry, float startX,
                                 float startY, float panelW, float panelH,
                                 float alpha) {
  auto &state = UISystem::State;
  auto &s_theme = UIRenderer::GetTheme();
  float scale = state.scaleFactor;

  float slotSize_Logic = 80.0f;
  float midX = startX + panelW / 2.0f;
  float topMargin = 150.0f;
  float slotY = startY + topMargin;

  // --- Altar VFX ---
  float time = (float)GetTime();
  Color ringColor1 = Fade(SKYBLUE, 0.2f * alpha);
  Color ringColor2 = Fade(BLUE, 0.15f * alpha);
  Vector2 center = {midX * scale, (slotY + slotSize_Logic / 2.0f) * scale}; 
  float radius = slotSize_Logic * 0.9f * scale;

  DrawRing(center, radius, radius + 2.0f * scale, time * 20.0f, time * 20.0f + 240.0f, 32, ringColor1);
  DrawPolyLines(center, 6, radius + 20 * scale, time * 30.0f, ringColor2);
  DrawPolyLines(center, 3, radius + 35 * scale, -time * 20.0f, ringColor1);

  // Single Item Salvage Slot
  UIRenderer::DrawSlot(state.globalFont, registry, (midX - slotSize_Logic / 2.0f) * scale, slotY * scale,
                       slotSize_Logic * scale, m_salvageItem, "放入分解物品", false, false,
                       alpha);

  // Handle Drop
  Rectangle slotRect_Logic = {midX - slotSize_Logic / 2.0f, slotY, slotSize_Logic, slotSize_Logic};
  if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), slotRect_Logic)) {
    if (state.draggedItem != entt::null &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (registry.any_of<ItemComponent>(state.draggedItem)) {
        const auto &item = registry.get<ItemComponent>(state.draggedItem);
        if (SalvageSystem::CanSalvage(item)) {
          m_salvageItem = state.draggedItem;
          state.draggedItem = entt::null;
        }
      }
    }
    if (m_salvageItem != entt::null) {
      UiShared::HoveredItem() = entt::null;
      UIRenderer::DrawTooltip(state.globalFont, registry, m_salvageItem, alpha);
    }
  }

  // Yield Preview
  if (m_salvageItem != entt::null && registry.valid(m_salvageItem)) {
    const auto &item = registry.get<ItemComponent>(m_salvageItem);
    // Deterministic Range Calculation
    struct YieldRange { uint32_t matId; int min; int max; };
    std::vector<YieldRange> ranges;
    for (const auto& aff : item.affixes) {
        if (aff.type == AffixType::Count) continue;
        uint32_t materialId = (aff.isLegendary || IsLegendaryAffix(aff.type)) ? 4999 : 4000 + static_cast<uint32_t>(aff.type);
        int t = aff.tier;
        int min = (t < 4) ? 0 : (t - 3);
        int max = t;
        
        bool found = false;
        for (auto& r : ranges) { if (r.matId == materialId) { r.min += min; r.max += max; found = true; break; } }
        if (!found) ranges.push_back({materialId, min, max});
    }

    float yieldY = slotY + slotSize_Logic + 60.0f;
    
    // Header
    const char* headerText = "分解产出预估:";
    float headerW = MeasureTextEx(state.globalFont, headerText, 20, 1.0f).x;
    
    UISystem::DrawTextUI(headerText, midX - headerW/2.0f, yieldY, 20, SKYBLUE, alpha);
    
    yieldY += 40.0f;

    if (ranges.empty()) {
      UISystem::DrawTextUI("该物品无任何可分解产出", midX - 90, yieldY, 18, GRAY, alpha);
    } else {
      float matSize_Logic = 48.0f;
      float gap_Logic = 15.0f;
      int count = (int)ranges.size();
      float totalW = count * matSize_Logic + (count - 1) * gap_Logic;
      float curX = midX - totalW / 2.0f;
      float curY = yieldY;
      
      for (int i = 0; i < count; ++i) {
          Rectangle mRect_Logic = {curX, curY, matSize_Logic, matSize_Logic};
          DrawRectangleRec({mRect_Logic.x * scale, mRect_Logic.y * scale, mRect_Logic.width * scale, mRect_Logic.height * scale}, Fade(s_theme.slotBackground, alpha));
          DrawRectangleLinesEx({mRect_Logic.x * scale, mRect_Logic.y * scale, mRect_Logic.width * scale, mRect_Logic.height * scale}, 1.0f * scale, Fade(s_theme.panelBorder, alpha));
          
          const auto *def = MaterialRegistry::Get().GetMaterial(ranges[i].matId);
          if (def) {
              Color matColor = UIRenderer::GetRarityColor(def->rarity);
              DrawRectangleRec({(curX+4) * scale, (curY+4) * scale, (matSize_Logic-8) * scale, (matSize_Logic-8) * scale}, Fade(matColor, 0.3f * alpha));
              
              char rangeBuf[32];
              utils::FormatToBuffer(rangeBuf, "{}~{}", ranges[i].min,
                                    ranges[i].max);
              UISystem::DrawTextUI(rangeBuf, curX + 2, curY + 48 - 14, 12, SKYBLUE, alpha);
              
              if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), mRect_Logic)) {
                    UISystem::DrawTextUI(def->name.c_str(), curX, curY - 20, 16, matColor, alpha);
              }
          }
          curX += matSize_Logic + gap_Logic;
      }
    }

    // Salvage Button
    float btnW = 200.0f;
    float btnH = 60.0f;
    float btnX = midX - btnW / 2.0f;
    float btnY = startY + panelH - 120.0f;

    Rectangle btnRect_Logic = {btnX, btnY, btnW, btnH};
    bool hover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), btnRect_Logic);
    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

    UIRenderer::DrawButton(state.globalFont, rectTex, btnRect_Logic, "开始分解装备", 24, WHITE, hover ? RED : Color{120, 20, 20, 255}, hover, hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      auto playerEnt = UISystem::GetPlayerEntity(registry);
      SalvageSystem::Execute(registry, m_salvageItem, playerEnt);
      m_salvageItem = entt::null;
      
      // VFX
      auto &ps = systems::GPUParticleSystem::Get();
      for (int i = 0; i < 30; ++i) {
           float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
           float speed = (float)GetRandomValue(150, 400);
           Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
           auto p = systems::InkEffectHelper::CreateSpark(center, vel, RED, 2.0f);
           ps.Emit(p);
      }
    }
  }

  // Batch Salvage Options
  float batchY = startY + panelH - 40.0f;
  
  // Filter Toggle
  Rectangle filterBtn_Logic = {startX + 20.0f, startY + panelH - 80.0f, 100.0f, 32.0f};
  bool filterHover = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), filterBtn_Logic);
  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
  
  UIRenderer::DrawButton(state.globalFont, rectTex, filterBtn_Logic, "筛选设置", 16, WHITE, m_showSalvageFilter ? RED : DARKGRAY, filterHover, filterHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
  if (filterHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_showSalvageFilter = !m_showSalvageFilter;

  if (m_showSalvageFilter) {
      float fx = startX - 220.0f;
      float fy = startY + 100.0f;
      float fw = 200.0f;
      float fh = 300.0f;
      DrawRectangleRec({fx * scale, fy * scale, fw * scale, fh * scale}, Fade({40, 40, 50, 255}, 0.9f * alpha));
      DrawRectangleLinesEx({fx * scale, fy * scale, fw * scale, fh * scale}, 1.0f * scale, Fade(GOLD, alpha));
      UISystem::DrawTextUI("分解过滤器", fx + 10, fy + 10, 18, GOLD, alpha);

      auto DrawOption = [&](const char* label, bool& val, float y_off) {
          Rectangle r_logic = {fx + 10, fy + y_off, 180.0f, 24.0f};
          bool h = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), r_logic);
          if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val = !val;
          
          UIRenderer::DrawButton(state.globalFont, rectTex, r_logic, label, 14, WHITE, val ? RED : DARKGRAY, h, h && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
      };
      
      DrawOption("排除已锁定", m_salvageFilter.excludeLocked, 40.0f);
      DrawOption("保留 T6+ 装备", m_salvageFilter.keepIfTier6Plus, 70.0f);
      
      UISystem::DrawTextUI("稀有度限制:", fx + 10, fy + 110, 14, GRAY, alpha);
      auto DrawRarity = [&](const char* label, Rarity rar, float y_off) {
          bool active = (m_salvageFilter.rarityMask & (1 << (uint32_t)rar));
          Rectangle r_logic = {fx + 10, fy + y_off, 180.0f, 24.0f};
          bool h = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), r_logic);
          if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
              m_salvageFilter.rarityMask ^= (1 << (uint32_t)rar);
          }
          
          UIRenderer::DrawButton(state.globalFont, rectTex, r_logic, label, 14, WHITE, active ? UIRenderer::GetRarityColor(rar) : DARKGRAY, h, h && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
      };
      DrawRarity("Magic (蓝色)", Rarity::Magic, 140.0f);
      DrawRarity("Rare (黄色)", Rarity::Rare, 170.0f);
      DrawRarity("Exalted (紫色)", Rarity::Epic, 200.0f);
  }

  auto DrawBatchButton = [&](const char *label, float x_logic, float y_logic) {
    float bW = 200.0f;
    float bH = 32.0f;
    Rectangle r_logic = {x_logic, y_logic, bW, bH};
    bool h = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), r_logic);
    
    UIRenderer::DrawButton(state.globalFont, rectTex, r_logic, label, 16, WHITE, DARKGRAY, h, h && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);
    
    if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      auto playerEnt = UISystem::GetPlayerEntity(registry);
      auto* inv = registry.try_get<InventoryComponent>(playerEnt);
      if (!inv) return;

      std::vector<entt::entity> toSalvage;
      for (auto entity : inv->items) {
        if (!registry.valid(entity)) continue;
        const auto &item = registry.get<ItemComponent>(entity);
        
        // Apply Filters
        if (m_salvageFilter.excludeLocked && item.isLocked) continue;
        if (!(m_salvageFilter.rarityMask & (1 << (uint32_t)item.rarity))) continue;
        if (m_salvageFilter.keepIfTier6Plus) {
            bool hasT6 = false;
            for(const auto& aff : item.affixes) if(aff.tier >= 6) { hasT6 = true; break; }
            if(hasT6) continue;
        }

        if (SalvageSystem::CanSalvage(item)) {
           toSalvage.push_back(entity);
        }
      }
      if (!toSalvage.empty()) {
          SalvageSystem::BatchExecute(registry, toSalvage, playerEnt);
      }
    }
  };

  DrawBatchButton("按过滤器批量分解", midX - 100.0f, batchY - 40.0f);
}

} // namespace NoMoreDay
