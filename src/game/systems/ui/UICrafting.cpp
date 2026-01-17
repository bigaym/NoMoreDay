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
  m_targetItem = item;
  m_visible = true; // Auto-open when setting target via context menu
}

entt::entity UICrafting::GetTargetItem() { return m_targetItem; }

void UICrafting::ClearTargetItem() { m_targetItem = entt::null; }

void UICrafting::Update(entt::registry &registry) {
  float dt = GetFrameTime();
  float alphaSpeed = 6.0f;
  if (m_visible)
    m_craftingAlpha = std::min(1.0f, m_craftingAlpha + dt * alphaSpeed);
  else
    m_craftingAlpha = std::max(0.0f, m_craftingAlpha - dt * alphaSpeed);

  if (m_targetItem != entt::null && !registry.valid(m_targetItem)) {
    m_targetItem = entt::null;
  }
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

  DrawTab("锻造 (Forge)", CraftingTab::Forging);
  DrawTab("融合 (Merge)", CraftingTab::Merging);
  DrawTab("分解 (Salvage)", CraftingTab::Salvaging);

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

  // --- FORGING PANEL (Original Logic) ---

  // Target Item Slot
  float slotSize = 80.0f * state.scaleFactor;
  float slotX = startX + (panelW - slotSize) / 2.0f;
  float slotY = startY + 80.0f * state.scaleFactor;

  UIRenderer::DrawSlot(state.globalFont, registry, slotX, slotY, slotSize,
                       m_targetItem, "放入装备", false, false, alpha);

  // Handle Item Drop for Forging
  Rectangle slotRect = {slotX, slotY, slotSize, slotSize};
  if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
    if (state.draggedItem != entt::null &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (registry.any_of<ItemComponent>(state.draggedItem)) {
        auto &item = registry.get<ItemComponent>(state.draggedItem);
        // Allow equipment
        if (item.type == ItemType::Weapon || item.type == ItemType::Armor ||
            item.type == ItemType::Jewelry) {
          m_targetItem = state.draggedItem;
          state.draggedItem = entt::null;
        }
      }
    }
    if (m_targetItem != entt::null) {
      state.hoveredItem = entt::null;
      UIRenderer::DrawTooltip(state.globalFont, registry, m_targetItem, alpha);
    }
  }

  if (m_targetItem != entt::null) {
    auto &item = registry.get<ItemComponent>(m_targetItem);
    char potBuf[64];
    snprintf(potBuf, 64, "锻造潜力: %d", item.forgingPotential);
    float potW = MeasureTextEx(state.globalFont, potBuf, 20, 1.0f).x;
    UISystem::DrawTextUI(potBuf, startX + (panelW - potW) / 2.0f,
                         slotY + slotSize + 10, 20, SKYBLUE, alpha);
    DrawAffixList(registry, m_targetItem);
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
                       m_targetItem, "基底(Unique)", false, false, alpha);

  // Fodder Slot
  float fodderX = midX + spacing;
  UIRenderer::DrawSlot(state.globalFont, registry, fodderX, topY, slotSize,
                       m_fodderItem, "耗材(Exalted)", false, false, alpha);

  // Catalyst Slot
  float catX = midX - slotSize / 2.0f;
  float catY = topY + slotSize + spacing * 2;
  UIRenderer::DrawSlot(state.globalFont, registry, catX, catY, slotSize,
                       m_catalystItem, "核心", false, false, alpha);

  // Handle Drops
  auto HandleDrop = [&](entt::entity &target, float x, float y,
                        const char *filterType) {
    Rectangle r = {x, y, slotSize, slotSize};
    if (CheckCollisionPointRec(GetMousePosition(), r)) {
      if (state.draggedItem != entt::null &&
          IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // Should validate type further here potentially
        target = state.draggedItem;
        state.draggedItem = entt::null;
      }
      if (target != entt::null) {
        state.hoveredItem = entt::null;
        UIRenderer::DrawTooltip(state.globalFont, registry, target, alpha);
      }
    }
  };

  HandleDrop(m_targetItem, baseX, topY, "Unique");
  HandleDrop(m_fodderItem, fodderX, topY, "Exalted");
  HandleDrop(m_catalystItem, catX, catY, "Material");

  // Affix Selection Interface
  if (m_fodderItem != entt::null && registry.valid(m_fodderItem)) {
    auto &fodder = registry.get<ItemComponent>(m_fodderItem);
    float affixY = catY + slotSize + 20.0f;
    UISystem::DrawTextUI("选择词缀 (Select Affix to Keep):", startX + 40,
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
  bool canFuse = m_targetItem != entt::null && m_fodderItem != entt::null &&
                 m_catalystItem != entt::null && m_selectedAffixIndex != -1;

  Color btnColor = canFuse ? RED : DARKGRAY;
  if (canFuse && CheckCollisionPointRec(GetMousePosition(), btnRect)) {
    btnColor = ORANGE;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      CraftingResult res =
          CraftingSystem::fuseLegendary(registry, m_targetItem, m_fodderItem,
                                        m_catalystItem, m_selectedAffixIndex);
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
        if (!registry.valid(m_fodderItem))
          m_fodderItem = entt::null;
        if (!registry.valid(m_catalystItem))
          m_catalystItem = entt::null;
        m_selectedAffixIndex = -1;
      } else {
        // Show error message?
      }
    }
  }

  DrawRectangleRec(btnRect, Fade(btnColor, alpha));
  DrawRectangleLinesEx(btnRect, 2.0f, Fade(WHITE, 0.5f * alpha));
  UISystem::DrawTextUI("融合 (FUSE)", btnX + 35, btnY + 15, 24,
                       canFuse ? WHITE : GRAY, alpha);
}

void UICrafting::DrawAffixList(entt::registry &registry, entt::entity entity) {
  auto &state = UISystem::State;
  auto &item = registry.get<ItemComponent>(entity);
  float alpha = m_craftingAlpha;

  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();
  float panelW = 600.0f * state.scaleFactor;
  float panelH = 700.0f * state.scaleFactor;
  float startX = (screenW - panelW) / 2.0f;
  float startY = (screenH - panelH) / 2.0f;

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
      snprintf(nameBuf, 128, "[T%d] %s", affix->tier,
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

  UISystem::DrawTextUI("前缀 (Prefixes)", startX + 20, currentY, 20, LIGHTGRAY,
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
  UISystem::DrawTextUI("后缀 (Suffixes)", startX + 20, currentY, 20, LIGHTGRAY,
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
  float slotSize = 80.0f * state.scaleFactor;
  float midX = startX + panelW / 2.0f;
  float slotY = startY + 100.0f * state.scaleFactor;

  // Single Item Salvage Slot
  UIRenderer::DrawSlot(state.globalFont, registry, midX - slotSize / 2.0f, slotY,
                       slotSize, m_targetItem, "放入分解物品", false, false,
                       alpha);

  // Handle Drop
  Rectangle slotRect = {midX - slotSize / 2.0f, slotY, slotSize, slotSize};
  if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
    if (state.draggedItem != entt::null &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (registry.any_of<ItemComponent>(state.draggedItem)) {
        const auto &item = registry.get<ItemComponent>(state.draggedItem);
        if (SalvageSystem::CanSalvage(item)) {
          m_targetItem = state.draggedItem;
          state.draggedItem = entt::null;
        }
      }
    }
    if (m_targetItem != entt::null) {
      state.hoveredItem = entt::null;
      UIRenderer::DrawTooltip(state.globalFont, registry, m_targetItem, alpha);
    }
  }

  // Yield Preview
  if (m_targetItem != entt::null && registry.valid(m_targetItem)) {
    const auto &item = registry.get<ItemComponent>(m_targetItem);
    auto yield = SalvageSystem::CalculateYield(item);

    float yieldY = slotY + slotSize + 40.0f * state.scaleFactor;
    UISystem::DrawTextUI("预估产出 (Estimated Yield):", startX + 40, yieldY, 20,
                         LIGHTGRAY, alpha);
    yieldY += 30.0f;

    if (yield.empty()) {
      UISystem::DrawTextUI("该物品无产出 (No yield)", startX + 60, yieldY, 18,
                           GRAY, alpha);
    } else {
      for (const auto &res : yield) {
        const auto *def = MaterialRegistry::Get().GetMaterial(res.materialId);
        char buf[128];
        snprintf(buf, 128, "%s x %d",
                 def ? def->name.c_str() : "Unknown Shard", res.count);
        UISystem::DrawTextUI(buf, startX + 60, yieldY, 18, WHITE, alpha);
        yieldY += 25.0f;
      }
    }

    // Salvage Button
    float btnW = 160.0f * state.scaleFactor;
    float btnH = 50.0f * state.scaleFactor;
    float btnX = midX - btnW / 2.0f;
    float btnY = startY + panelH - 100.0f * state.scaleFactor;

    Rectangle btnRect = {btnX, btnY, btnW, btnH};
    bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
    DrawRectangleRec(btnRect, Fade(hover ? RED : Color{180, 0, 0, 255}, alpha));
    DrawRectangleLinesEx(btnRect, 2.0f, Fade(WHITE, 0.5f * alpha));
    UISystem::DrawTextUI("分解 (SALVAGE)", btnX + 25, btnY + 15, 20, WHITE,
                         alpha);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      // Execute Salvage
      auto playerEnt = UISystem::GetPlayerEntity(registry);
      SalvageSystem::Execute(registry, m_targetItem, playerEnt);
      m_targetItem = entt::null;
      
      // Simple Sound/VFX Placeholder
      // ...
    }
  }

  // Batch Salvage Options
  float batchY = startY + panelH - 250.0f * state.scaleFactor;
  UISystem::DrawTextUI("快速分解 (Quick Salvage):", startX + 40, batchY, 20,
                       GOLD, alpha);
  batchY += 40.0f;

  auto DrawBatchButton = [&](const char *label, Rarity maxRarity, float x,
                             float y) {
    float bW = 200.0f * state.scaleFactor;
    float bH = 40.0f * state.scaleFactor;
    Rectangle r = {x, y, bW, bH};
    bool h = CheckCollisionPointRec(GetMousePosition(), r);

    DrawRectangleRec(r, Fade(h ? DARKGRAY : Color{40, 40, 50, 255}, alpha));
    DrawRectangleLinesEx(r, 1.0f, Fade(GRAY, alpha));
    UISystem::DrawTextUI(label, x + 10, y + 10, 18, WHITE, alpha);

    if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      auto playerEnt = UISystem::GetPlayerEntity(registry);
      auto* inv = registry.try_get<InventoryComponent>(playerEnt);
      if (!inv) return;

      std::vector<entt::entity> toSalvage;
      for (auto entity : inv->items) {
        if (!registry.valid(entity)) continue;
        const auto &item = registry.get<ItemComponent>(entity);
        
        // Only salvage if within rarity filter and satisfies CanSalvage (checks isLocked, Type, etc.)
        if (item.rarity <= maxRarity && SalvageSystem::CanSalvage(item)) {
           toSalvage.push_back(entity);
        }
      }
      
      if (!toSalvage.empty()) {
          SalvageSystem::BatchExecute(registry, toSalvage, playerEnt);
      }
    }
  };

  DrawBatchButton("分解所有 稀有/魔法", Rarity::Rare, startX + 40, batchY);
}

} // namespace NoMoreDay