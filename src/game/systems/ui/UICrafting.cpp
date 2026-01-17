#include "game/systems/ui/UICrafting.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
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
  float tabW = 120.0f * state.scaleFactor;
  float tabH = 30.0f * state.scaleFactor;
  float tabX = startX + 20;

  auto DrawTab = [&](const char *label, CraftingTab tab) {
    bool active = (m_currentTab == tab);
    Rectangle tabRect = {tabX, titleY, tabW, tabH};
    bool hover = CheckCollisionPointRec(GetMousePosition(), tabRect);

    Color bg = active ? s_theme.buttonPress
                      : (hover ? s_theme.buttonHover : s_theme.buttonNormal);
    DrawRectangleRec(tabRect, Fade(bg, alpha));
    DrawRectangleLinesEx(tabRect, 1.0f, Fade(active ? GOLD : GRAY, alpha));
    UISystem::DrawTextUI(label, tabRect.x + 10, tabRect.y + 5, 20,
                         active ? GOLD : WHITE, alpha);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      m_currentTab = tab;
    tabX += tabW + 10;
  };

  DrawTab("词缀锻造", CraftingTab::Forging);
  DrawTab("传奇融合", CraftingTab::Merging);
  DrawTab("装备分解", CraftingTab::Salvaging);

  // Close Button
  if (CheckCollisionPointRec(GetMousePosition(),
                             {startX + panelW - 40, startY + 10, 30, 30})) {
    UISystem::DrawTextUI("X", startX + panelW - 35, startY + 15, 20, RED,
                         alpha);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      Toggle();
  } else {
    UISystem::DrawTextUI("X", startX + panelW - 35, startY + 15, 20, WHITE,
                         alpha);
  }

  if (m_currentTab == CraftingTab::Merging) {
    DrawMergePanel(registry, startX, startY, panelW, panelH, alpha);
    return;
  }
  if (m_currentTab == CraftingTab::Salvaging) {
    DrawSalvagePanel(registry, startX, startY, panelW, panelH, alpha);
    return;
  }

  // Target Item Slot
  float slotSize = 80.0f * state.scaleFactor;
  float slotX = startX + (panelW - slotSize) / 2.0f;
  float slotY = startY + 80.0f * state.scaleFactor;

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
      state.hoveredItem = entt::null;
      UIRenderer::DrawTooltip(state.globalFont, registry, m_forgeItem, alpha);
    }
  }

  if (m_forgeItem != entt::null) {
    auto &item = registry.get<ItemComponent>(m_forgeItem);
    char potBuf[64];
    snprintf(potBuf, 64, "锻造潜力: %d", item.forgingPotential);
    float potW = MeasureTextEx(state.globalFont, potBuf, 20, 1.0f).x;
    UISystem::DrawTextUI(potBuf, startX + (panelW - potW) / 2.0f,
                         slotY + slotSize + 10, 20, SKYBLUE, alpha);
    
    // Guidance Text
    const char* guide = "提示：锻造会消耗装备潜力。潜力耗尽后将无法再修改。";
    float guideW = MeasureTextEx(state.globalFont, guide, 16 * state.scaleFactor, 1.0f).x;
    UISystem::DrawTextUI(guide, (panelW / state.scaleFactor - guideW / state.scaleFactor) / 2.0f + startX / state.scaleFactor, (panelH / state.scaleFactor) + startY / state.scaleFactor - 40, 16, GRAY, alpha);

    DrawAffixList(registry, m_forgeItem, startX, startY);
  } else {
    const char* guide = "将装备拖入上方槽位开始锻造（升级、粉碎、重置词缀）";
    float guideW = MeasureTextEx(state.globalFont, guide, 16 * state.scaleFactor, 1.0f).x;
    UISystem::DrawTextUI(guide, (panelW / state.scaleFactor - guideW / state.scaleFactor) / 2.0f + startX / state.scaleFactor, slotY / state.scaleFactor + 100, 16, GRAY, alpha);
  }
}

void UICrafting::DrawMergePanel(entt::registry &registry, float startX,
                                float startY, float panelW, float panelH,
                                float alpha) {
  auto &state = UISystem::State;

  float slotSize = 64.0f * state.scaleFactor;
  float spacing = 20.0f * state.scaleFactor;

  // Layout: Base (Left), Fodder (Right), Result/Arrow (Center?), Catalyst
  // (Bottom Center) Actually typically: Base + Fodder -> Result.

  float midX = startX + panelW / 2.0f;
  float topY = startY + 100.0f * state.scaleFactor;

  // Base Slot
  float baseX = midX - slotSize - spacing;
  UIRenderer::DrawSlot(state.globalFont, registry, baseX, topY, slotSize,
                       m_mergeBase, m_mergeBase == entt::null ? "放入暗金(LP > 0)" : "", false, false, alpha);

  // Fodder Slot
  float fodderX = midX + spacing;
  UIRenderer::DrawSlot(state.globalFont, registry, fodderX, topY, slotSize,
                       m_mergeFodder, m_mergeFodder == entt::null ? "放入崇高(T6+)" : "", false, false, alpha);

  // Catalyst Slot
  float catX = midX - slotSize / 2.0f;
  float catY = topY + slotSize + spacing * 2;
  UIRenderer::DrawSlot(state.globalFont, registry, catX, catY, slotSize,
                       m_mergeCatalyst, "放入时空核心", false, false, alpha);

  // Guidance Labels
  UISystem::DrawTextUI("暗金基底", (baseX / state.scaleFactor), (topY / state.scaleFactor) - 25, 18, GOLD, alpha);
  UISystem::DrawTextUI("崇高物品", (fodderX / state.scaleFactor), (topY / state.scaleFactor) - 25, 18, PURPLE, alpha);
  UISystem::DrawTextUI("传奇核心", (catX / state.scaleFactor), (catY / state.scaleFactor) - 25, 18, SKYBLUE, alpha);

  // Handle Drops
  auto HandleMergeDrop = [&](entt::entity &target, float x, float y, int type) {
    Rectangle r = {x, y, slotSize, slotSize};
    if (CheckCollisionPointRec(GetMousePosition(), r)) {
      if (state.draggedItem != entt::null &&
          IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (registry.any_of<ItemComponent>(state.draggedItem)) {
            bool valid = false;
            auto& item = registry.get<ItemComponent>(state.draggedItem);
            
            if (type == 0) { // Base: Legendary + LP > 0
                if (item.rarity == Rarity::Legendary && item.legendaryPotential > 0) valid = true;
            } else if (type == 1) { // Fodder: Exalted (Rare with T6+)
                bool hasT6 = false;
                for(const auto& aff : item.affixes) if(aff.tier >= 6) { hasT6 = true; break; }
                if (hasT6) valid = true;
            } else if (type == 2) { // Catalyst
                if (item.type == ItemType::Material) valid = true; // Simplified check
            }

            if (valid) {
                target = state.draggedItem;
                state.draggedItem = entt::null;
            }
        }
      }
      if (target != entt::null) {
        state.hoveredItem = entt::null;
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
    float affixY = catY + slotSize + 20.0f;
    UISystem::DrawTextUI("选择要转移并保留的词缀:", startX + 40,
                         affixY, 18, LIGHTGRAY, alpha);

    affixY += 30.0f;
    for (int i = 0; i < (int)fodder.affixes.size(); ++i) {
      float x = startX + 40;
      float w = panelW - 80;
      float h = 40;
      Rectangle rowRect = {x, affixY, w, h};

      bool selected = (m_selectedAffixIndex == i);
      bool hover = CheckCollisionPointRec(GetMousePosition(), rowRect);

      Color bg = selected ? Fade(RED, 0.3f) : Fade(DARKGRAY, 0.5f);
      if (hover && !selected)
        bg = Fade(GRAY, 0.4f);

      DrawRectangleRec(rowRect, Fade(bg, alpha));
      DrawRectangleLinesEx(rowRect, 1.0f, Fade(selected ? RED : GRAY, alpha));

      Color textColor = GetAffixTierColor(fodder.affixes[i].tier);
      char buf[128];
      snprintf(buf, 128, "%s",
               GetAffixDescription(fodder.affixes[i], true).c_str());
      UISystem::DrawTextUI(buf, x + 10, affixY + 10, 18, textColor, alpha);

      if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_selectedAffixIndex = i;
      }

      affixY += h + 5;
    }
  }

  // Fuse Button
  float btnW = 160.0f * state.scaleFactor;
  float btnH = 50.0f * state.scaleFactor;
  float btnX = midX - btnW / 2.0f;
  float btnY = startY + panelH - 80.0f;

  Rectangle btnRect = {btnX, btnY, btnW, btnH};
  bool canFuse = m_mergeBase != entt::null && m_mergeFodder != entt::null &&
                 m_mergeCatalyst != entt::null && m_selectedAffixIndex != -1;

  Color btnColor = canFuse ? RED : DARKGRAY;
  if (canFuse && CheckCollisionPointRec(GetMousePosition(), btnRect)) {
    btnColor = ORANGE;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      CraftingResult res =
          CraftingSystem::fuseLegendary(registry, m_mergeBase, m_mergeFodder,
                                        m_mergeCatalyst, m_selectedAffixIndex);
      if (res == CraftingResult::Success) {
        // VFX: Burst of Gold and Red particles
        auto &ps = systems::GPUParticleSystem::Get();
        Vector2 center = {btnX + btnW / 2.0f, btnY + btnH / 2.0f};

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
      } else {
        // Show error message?
      }
    }
  }

  DrawRectangleRec(btnRect, Fade(btnColor, alpha));
  DrawRectangleLinesEx(btnRect, 2.0f, Fade(WHITE, 0.5f * alpha));
  UISystem::DrawTextUI("开始融合", btnX + 45, btnY + 15, 24,
                       canFuse ? WHITE : GRAY, alpha);

  const char* bottomGuide = "融合会将崇高物品的随机词缀转移到暗金基底上。";
  float bW = MeasureTextEx(state.globalFont, bottomGuide, 14 * state.scaleFactor, 1.0f).x;
  UISystem::DrawTextUI(bottomGuide, (midX / state.scaleFactor - bW / state.scaleFactor / 2.0f), (panelH / state.scaleFactor) + (startY / state.scaleFactor) - 30, 14, GRAY, alpha);
}

void UICrafting::DrawAffixList(entt::registry &registry, entt::entity entity, float panelStartX, float panelStartY) {
  auto &state = UISystem::State;
  auto &item = registry.get<ItemComponent>(entity);
  float alpha = m_craftingAlpha;

  float panelW = 600.0f * state.scaleFactor;
  float startX = panelStartX;
  float startY = panelStartY;

  float currentY = startY + 200.0f * state.scaleFactor;
  float rowH = 50.0f * state.scaleFactor;
  float padding = 10.0f * state.scaleFactor;

  // Helper to draw an affix row
  auto DrawAffixRow = [&](Affix *affix, int index, bool isPrefix, int slotIdx) {
    float x = startX + 20 * state.scaleFactor;
    float w = panelW - 40 * state.scaleFactor;
    Rectangle rowRect = {x, currentY, w, rowH};

    DrawRectangleRec(rowRect, Fade(DARKGRAY, 0.5f * alpha));
    DrawRectangleLinesEx(rowRect, 1.0f, Fade(GRAY, alpha));

    if (affix) {
      // Existing Affix
      Color textColor = WHITE; // Determine by tier?
      char nameBuf[128];
      snprintf(nameBuf, 128, "T%d - %s", affix->tier,
               GetAffixDescription(*affix, false).c_str());
      UISystem::DrawTextUI(nameBuf, x + 10, currentY + 15, 18, textColor,
                           alpha);

      // Upgrade Button
      float btnW = 60.0f * state.scaleFactor;
      float btnH = 30.0f * state.scaleFactor;
      float btnX = x + w - btnW - 10;

      bool canAfford = item.forgingPotential > 0;

      if (affix->tier < 5) {
        Rectangle btnRect = {btnX, currentY + 10, btnW, btnH};
        if (canAfford) {
          bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
          DrawRectangleRec(btnRect, Fade(hover ? GREEN : DARKGREEN, alpha));
          UISystem::DrawTextUI("升级", btnRect.x + 10, btnRect.y + 5, 16, WHITE,
                               alpha);
          if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::upgradeAffix(item, index);
          }
        } else {
          DrawRectangleRec(btnRect, Fade(RED, 0.3f * alpha));
          UISystem::DrawTextUI("潜力", btnRect.x + 10, btnRect.y + 5, 14, GRAY,
                               alpha);
        }
      } else {
        UISystem::DrawTextUI("MAX", btnX + 10, currentY + 15, 16, GOLD, alpha);
      }

      // Chaos (C)
      if (affix->tier < 5) {
        Rectangle cRect = {btnX - 35 * state.scaleFactor, currentY + 10,
                           30.0f * state.scaleFactor, btnH};
        if (canAfford) {
          bool hover = CheckCollisionPointRec(GetMousePosition(), cRect);
          DrawRectangleRec(cRect, Fade(hover ? PURPLE : VIOLET, alpha));
          UISystem::DrawTextUI("C", cRect.x + 8, cRect.y + 5, 16, WHITE, alpha);
          if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::chaosAffix(item, index);
          }
          if (hover) {
            // Tooltip for Chaos
            // UIRenderer::DrawSimpleTooltip(...) // TODO
          }
        }
      }

      // Refine (R) - Values
      {
        Rectangle rRect = {btnX - 70 * state.scaleFactor, currentY + 10,
                           30.0f * state.scaleFactor, btnH};
        if (canAfford) {
          bool hover = CheckCollisionPointRec(GetMousePosition(), rRect);
          DrawRectangleRec(rRect, Fade(hover ? SKYBLUE : BLUE, alpha));
          UISystem::DrawTextUI("R", rRect.x + 8, rRect.y + 5, 16, WHITE, alpha);
          if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CraftingSystem::refineAffixValues(item, index);
          }
        }
      }

    } else {
      // Empty Slot
      UISystem::DrawTextUI(isPrefix ? "空前缀槽位" : "空后缀槽位", x + 10,
                           currentY + 15, 18, GRAY, alpha);

      // Add Button
      float btnW = 80.0f * state.scaleFactor;
      Rectangle btnRect = {x + w - btnW - 10, currentY + 10, btnW,
                           30.0f * state.scaleFactor};
      bool canAfford = item.forgingPotential > 0;
      if (canAfford) {
        bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
        DrawRectangleRec(btnRect, Fade(hover ? BLUE : DARKBLUE, alpha));
        UISystem::DrawTextUI("添加", btnRect.x + 20, btnRect.y + 5, 16, WHITE,
                             alpha);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          // Logic to show shard selection popup...
          // For prototype, just add a random relevant affix
          // We need to know type. This is hard without a UI popup.
          // Let's create a "Add Random" for now.
          AffixType types[] = {
              AffixType::Strength,           AffixType::Dexterity,
              AffixType::Intelligence,       AffixType::Vitality,
              AffixType::FlatPhysicalDamage, AffixType::AttackSpeed};
          AffixType t = types[GetRandomValue(0, 5)];
          CraftingSystem::addAffix(item, t, isPrefix);
        }
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
  float slotSize = 80.0f * state.scaleFactor;
  float midX = startX + panelW / 2.0f;
  float topMargin = 150.0f * state.scaleFactor;
  float slotY = startY + topMargin;

  // --- Altar VFX ---
  float time = (float)GetTime();
  Color ringColor1 = Fade(SKYBLUE, 0.2f * alpha);
  Color ringColor2 = Fade(BLUE, 0.15f * alpha);
  Vector2 center = {midX, slotY + slotSize / 2.0f}; 
  float radius = slotSize * 0.9f;

  if (IsWindowReady()) {
      DrawRing(center, radius, radius + 2.0f * state.scaleFactor, time * 20.0f, time * 20.0f + 240.0f, 32, ringColor1);
      DrawPolyLines(center, 6, radius + 20 * state.scaleFactor, time * 30.0f, ringColor2);
      DrawPolyLines(center, 3, radius + 35 * state.scaleFactor, -time * 20.0f, ringColor1);
  }

  // Single Item Salvage Slot
  UIRenderer::DrawSlot(state.globalFont, registry, midX - slotSize / 2.0f, slotY,
                       slotSize, m_salvageItem, "放入分解物品", false, false,
                       alpha);

  // Handle Drop
  Rectangle slotRect = {midX - slotSize / 2.0f, slotY, slotSize, slotSize};
  if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
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
      state.hoveredItem = entt::null;
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

    float yieldY = slotY + slotSize + 60.0f * state.scaleFactor;
    
    // Header
    const char* headerText = "分解产出预估:";
    float headerW = MeasureTextEx(state.globalFont, headerText, 20 * state.scaleFactor, 1.0f).x;
    
    // Note: Passing logic coords to DrawTextUI as it scales them internally
    float logicMidX = midX / state.scaleFactor;
    float logicYieldY = yieldY / state.scaleFactor;
    float logicHeaderW = headerW / state.scaleFactor;
    
    UISystem::DrawTextUI(headerText, logicMidX - logicHeaderW/2.0f, logicYieldY, 20, SKYBLUE, alpha);
    
    yieldY += 40.0f * state.scaleFactor;

    if (ranges.empty()) {
      UISystem::DrawTextUI("该物品无任何可分解产出", logicMidX - 90, yieldY / state.scaleFactor, 18, GRAY, alpha);
    } else {
      float matSize = 48.0f * state.scaleFactor;
      float gap = 15.0f * state.scaleFactor;
      int count = (int)ranges.size();
      float totalW = count * matSize + (count - 1) * gap;
      float curX = midX - totalW / 2.0f;
      float curY = yieldY;
      
      for (int i = 0; i < count; ++i) {
          Rectangle mRect = {curX, curY, matSize, matSize};
          DrawRectangleRec(mRect, Fade(s_theme.slotBackground, alpha));
          DrawRectangleLinesEx(mRect, 1.0f, Fade(s_theme.panelBorder, alpha));
          
          const auto *def = MaterialRegistry::Get().GetMaterial(ranges[i].matId);
          if (def) {
              Color matColor = UIRenderer::GetRarityColor(def->rarity);
              DrawRectangleRec({curX+4, curY+4, matSize-8, matSize-8}, Fade(matColor, 0.3f * alpha));
              
              char rangeBuf[32];
              snprintf(rangeBuf, 32, "%d~%d", ranges[i].min, ranges[i].max);
              UISystem::DrawTextUI(rangeBuf, curX/state.scaleFactor + 2, curY/state.scaleFactor + 48 - 14, 12, SKYBLUE, alpha);
              
              if (CheckCollisionPointRec(GetMousePosition(), mRect)) {
                    UISystem::DrawTextUI(def->name.c_str(), curX/state.scaleFactor, curY/state.scaleFactor - 20, 16, matColor, alpha);
              }
          }
          curX += matSize + gap;
      }
    }

    // Salvage Button
    float btnW = 200.0f * state.scaleFactor;
    float btnH = 60.0f * state.scaleFactor;
    float btnX = midX - btnW / 2.0f;
    float btnY = startY + panelH - 120.0f * state.scaleFactor;

    Rectangle btnRect = {btnX, btnY, btnW, btnH};
    bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
    Color btnColor = hover ? RED : Color{120, 20, 20, 255};
    
    DrawRectangleRec(btnRect, Fade(btnColor, alpha));
    DrawRectangleLinesEx(btnRect, 2.0f, Fade(WHITE, (hover ? 0.8f : 0.4f) * alpha));
    
    const char* btnLabel = "开始分解装备";
    float txtW = MeasureTextEx(state.globalFont, btnLabel, 24 * state.scaleFactor, 1.0f).x;
    UISystem::DrawTextUI(btnLabel, (btnX + (btnW - txtW)/2)/state.scaleFactor, (btnY + (btnH - 24*state.scaleFactor)/2)/state.scaleFactor, 24, WHITE, alpha);

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
  float batchY = startY + panelH - 40.0f * state.scaleFactor;
  
  // Filter Toggle
  Rectangle filterBtn = {startX + 20*state.scaleFactor, startY + panelH - 80*state.scaleFactor, 100*state.scaleFactor, 30*state.scaleFactor};
  bool filterHover = CheckCollisionPointRec(GetMousePosition(), filterBtn);
  DrawRectangleRec(filterBtn, Fade(m_showSalvageFilter ? RED : DARKGRAY, alpha));
  UISystem::DrawTextUI("筛选设置", filterBtn.x/state.scaleFactor + 10, filterBtn.y/state.scaleFactor + 5, 16, WHITE, alpha);
  if (filterHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_showSalvageFilter = !m_showSalvageFilter;

  if (m_showSalvageFilter) {
      float fx = startX - 220 * state.scaleFactor;
      float fy = startY + 100 * state.scaleFactor;
      float fw = 200 * state.scaleFactor;
      float fh = 300 * state.scaleFactor;
      DrawRectangleRec({fx, fy, fw, fh}, Fade({40, 40, 50, 255}, 0.9f * alpha));
      DrawRectangleLinesEx({fx, fy, fw, fh}, 1.0f, Fade(GOLD, alpha));
      UISystem::DrawTextUI("分解过滤器", fx/state.scaleFactor + 10, fy/state.scaleFactor + 10, 18, GOLD, alpha);

      auto DrawOption = [&](const char* label, bool& val, float y) {
          Rectangle r = {fx + 10, fy + y, 180*state.scaleFactor, 24*state.scaleFactor};
          bool h = CheckCollisionPointRec(GetMousePosition(), r);
          if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) val = !val;
          DrawRectangleRec(r, Fade(val ? RED : DARKGRAY, alpha));
          UISystem::DrawTextUI(label, r.x/state.scaleFactor + 5, r.y/state.scaleFactor + 4, 14, WHITE, alpha);
      };
      
      DrawOption("排除已锁定", m_salvageFilter.excludeLocked, 40*state.scaleFactor);
      DrawOption("保留 T6+ 装备", m_salvageFilter.keepIfTier6Plus, 70*state.scaleFactor);
      
      UISystem::DrawTextUI("稀有度限制:", fx/state.scaleFactor + 10, fy/state.scaleFactor + 110, 14, GRAY, alpha);
      auto DrawRarity = [&](const char* label, Rarity rar, float y) {
          bool active = (m_salvageFilter.rarityMask & (1 << (uint32_t)rar));
          Rectangle r = {fx + 10, fy + y, 180*state.scaleFactor, 24*state.scaleFactor};
          if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
              m_salvageFilter.rarityMask ^= (1 << (uint32_t)rar);
          }
          DrawRectangleRec(r, Fade(active ? UIRenderer::GetRarityColor(rar) : DARKGRAY, 0.5f * alpha));
          UISystem::DrawTextUI(label, r.x/state.scaleFactor + 5, r.y/state.scaleFactor + 4, 14, WHITE, alpha);
      };
      DrawRarity("Magic (蓝色)", Rarity::Magic, 130*state.scaleFactor);
      DrawRarity("Rare (黄色)", Rarity::Rare, 160*state.scaleFactor);
      DrawRarity("Exalted (紫色)", Rarity::Epic, 190*state.scaleFactor);
  }

  auto DrawBatchButton = [&](const char *label, float x, float y) {
    float bW = 200.0f * state.scaleFactor;
    float bH = 30.0f * state.scaleFactor;
    Rectangle r = {x, y, bW, bH};
    bool h = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, Fade(h ? RED : DARKGRAY, alpha));
    DrawRectangleLinesEx(r, 1.0f, Fade(GRAY, alpha));
    UISystem::DrawTextUI(label, x/state.scaleFactor + 10, y/state.scaleFactor + 5, 16, WHITE, alpha);
    
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

  DrawBatchButton("按过滤器批量分解", midX - 100 * state.scaleFactor, batchY - 40 * state.scaleFactor);
}

} // namespace NoMoreDay