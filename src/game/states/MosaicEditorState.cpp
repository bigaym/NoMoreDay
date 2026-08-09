#include "game/states/MosaicEditorState.hpp"
#include "game/data/BiomeTypes.hpp"
#include "core/logging/Logger.hpp"
#include "game/scene/StateManager.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include "game/data/ResonanceCalculator.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/scene/SceneManager.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UIRenderer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/data/AffixMapping.hpp"
#include "game/components/WorldState.hpp"
#include "game/systems/world/MapAffixCalculator.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"
#include <algorithm>


namespace NoMoreDay {

void MosaicEditorState::OnEnter() {
  LOG_INFO("MosaicEditorState: Entered");
  RefreshFragmentInventory();
  m_grid.Clear();
  m_resonanceDirty = true;
  m_draggedFragment = entt::null;
  m_hoveredCellIndex = -1;
  m_hoveredInventoryIndex = -1;
  m_showConfirmDialog = false;
  m_showActiveRiftBlockedDialog = false;
  m_inventoryScrollOffset = 0;
}

void MosaicEditorState::OnExit() {
  LOG_INFO("MosaicEditorState: Exited");
  m_fragmentInventory.clear();
}

bool MosaicEditorState::OnUpdate(float dt) {
  // 1. 处理场景过渡 (Loading)
  if (m_context->sceneManager->IsTransitioning()) {
      // 必须手动更新 SceneManager，否则过渡动画会因为 GameplayState 被阻塞而停止
      m_context->sceneManager->Update(dt);
      return false; // Block input and updates during transition
  }

  HandleInput();
  if (m_resonanceDirty) {
    RecalculateResonance();
    m_resonanceDirty = false;
    // 重新排序库存，确保刚刚取下的碎片也能正确排序
    RefreshFragmentInventory();
  }
  return false;
}

void MosaicEditorState::OnRender() {
  RenderBackground();
  RenderGrid();
  RenderInventory();
  RenderResonancePreview();
  RenderConfirmButton();
  RenderDraggedFragment();
  RenderTooltip(); // Draw tooltip on top
  if (m_showConfirmDialog) {
    RenderConfirmDialog();
  }
  if (m_showActiveRiftBlockedDialog) {
    RenderActiveRiftBlockedDialog();
  }
}

void MosaicEditorState::RenderBackground() {
  DrawRectangle(0, 0, (float)GetScreenWidth(), (float)GetScreenHeight(), Color{0, 0, 0, 180});
  const char *title = "维度拼接 (Dimensional Mosaic)";
  Font font = UISystem::GetFont();
  float titleWidth = MeasureTextEx(font, title, 32, 1.0f).x;
  UIRenderer::DrawTextUI(font, title, ((float)GetScreenWidth() - titleWidth) / 2.0f, 30.0f, 32, WHITE, 1.0f);
}

void MosaicEditorState::RenderGrid() {
  float gridWidth =
      MosaicGrid::SIZE * (CELL_SIZE + CELL_PADDING) - CELL_PADDING;
  float gridHeight = gridWidth;
  DrawRectangleRounded(Rectangle{GRID_OFFSET_X - 10, GRID_OFFSET_Y - 10,
                                 gridWidth + 20, gridHeight + 20},
                       0.1f, 8, Color{40, 40, 60, 200});
  for (int y = 0; y < MosaicGrid::SIZE; ++y) {
    for (int x = 0; x < MosaicGrid::SIZE; ++x) {
      float screenX = GRID_OFFSET_X + x * (CELL_SIZE + CELL_PADDING);
      float screenY = GRID_OFFSET_Y + y * (CELL_SIZE + CELL_PADDING);
      RenderGridCell(x, y, screenX, screenY);
    }
  }
}

void MosaicEditorState::RenderGridCell(int x, int y, float screenX,
                                       float screenY) {
  int index = MosaicGrid::ToIndex(x, y);
  entt::entity fragment = m_grid.cells[index];
  Rectangle rect = {screenX, screenY, CELL_SIZE, CELL_SIZE};

  Color bgColor = (index == m_hoveredCellIndex) ? Color{80, 80, 120, 255}
                                                : Color{60, 60, 80, 255};
  DrawRectangleRounded(rect, 0.15f, 4, bgColor);

  if (fragment != entt::null && m_context->registry->valid(fragment)) {
    auto *fragComp =
        m_context->registry->try_get<MapFragmentComponent>(fragment);
    if (fragComp) {
      Color elemColor = GetElementColor(fragComp->element);
      elemColor.a = 150;
      DrawRectangleRounded(
          Rectangle{screenX + 5, screenY + 5, CELL_SIZE - 10, CELL_SIZE - 10},
          0.1f, 4, elemColor);

      const char *typeIcon = "?";
      switch (fragComp->type) {
      case FragmentType::Terrain:
        typeIcon = "T";
        break;
      case FragmentType::Affix:
        typeIcon = "A";
        break;
      case FragmentType::Unique:
        typeIcon = "U";
        break;
      }
      DrawText(typeIcon, static_cast<int>(screenX + CELL_SIZE / 2 - 10),
               static_cast<int>(screenY + CELL_SIZE / 2 - 15), 30, WHITE);

      float resonance = m_grid.resonanceMultipliers[index];
      if (resonance > 1.0f) {
        char buf[16];
        utils::FormatToBuffer(buf, "x{:.1f}", resonance);
        DrawText(buf, static_cast<int>(screenX + 5),
                 static_cast<int>(screenY + CELL_SIZE - 20), 12, GOLD);
      }
    }
  } else {
    DrawText("+", static_cast<int>(screenX + CELL_SIZE / 2 - 8),
             static_cast<int>(screenY + CELL_SIZE / 2 - 15), 30, GRAY);
  }
  DrawRectangleRoundedLinesEx(rect, 0.15f, 4, 2.0f,
                              index == m_hoveredCellIndex ? SKYBLUE : DARKGRAY);
}

void MosaicEditorState::RenderInventory() {
  Rectangle rect = {INVENTORY_X, GRID_OFFSET_Y, 250, 400};
  DrawRectangleRounded(rect, 0.05f, 8, Color{30, 30, 50, 200});
  DrawRectangleRoundedLinesEx(rect, 0.05f, 8, 2.0f, DARKGRAY);

  Font font = UISystem::GetFont();
  UIRenderer::DrawTextUI(font, "Fragments", INVENTORY_X + 10, GRID_OFFSET_Y + 10,
                   20, GOLD, 1.0f);

  for (int i = 0; i < 5; ++i) {
    int actualIndex = i + m_inventoryScrollOffset;
    if (actualIndex >= static_cast<int>(m_fragmentInventory.size()))
      break;

    entt::entity entity = m_fragmentInventory[actualIndex];
    auto *itemComp = m_context->registry->try_get<ItemComponent>(entity);
    auto *fragComp =
        m_context->registry->try_get<MapFragmentComponent>(entity);
    if (!itemComp || !fragComp)
      continue;

    Rectangle slotRect = GetInventorySlotRect(i);
    Color bgColor = (actualIndex == m_hoveredInventoryIndex)
                        ? Color{70, 70, 100, 255}
                        : Color{50, 50, 70, 255};
    DrawRectangleRounded(slotRect, 0.1f, 4, bgColor);

    Color elemColor = GetElementColor(fragComp->element);
    DrawRectangle(static_cast<int>(slotRect.x), static_cast<int>(slotRect.y), 5,
                  static_cast<int>(slotRect.height), elemColor);
    
    // Use dynamic Chinese name from mapping to avoid ???? issue
    std::string displayName = std::string(FragmentElementzh[static_cast<size_t>(fragComp->element)]) + 
                              std::string(FragmentTypezh[static_cast<size_t>(fragComp->type)]);
    
    UIRenderer::DrawTextUI(font, displayName.c_str(), slotRect.x + 12,
             slotRect.y + 8, 16, GetRarityColor(itemComp->rarity), 1.0f);

    char attrBuf[64];
    utils::FormatToBuffer(attrBuf, "密度:{:.0f}%",
                          fragComp->enemyDensityMod * 100);
    UIRenderer::DrawTextUI(font, attrBuf, slotRect.x + 12,
             slotRect.y + 28, 12, LIGHTGRAY, 1.0f);
    DrawRectangleRoundedLinesEx(
        slotRect, 0.1f, 4, 1.5f,
        actualIndex == m_hoveredInventoryIndex ? SKYBLUE : DARKGRAY);
  }
}

void MosaicEditorState::RenderResonancePreview() {
  float previewX = INVENTORY_X + 270;
  float previewY = GRID_OFFSET_Y;
  float previewWidth = 280.0f;
  float previewHeight = 400.0f; // Taller for affix list

  DrawRectangleRounded(
      Rectangle{previewX, previewY, previewWidth, previewHeight}, 0.05f, 8,
      Color{30, 30, 40, 230});
  DrawRectangleRoundedLinesEx(
      Rectangle{previewX, previewY, previewWidth, previewHeight}, 0.05f, 8, 2.0f, DARKGRAY);

  Font font = UISystem::GetFont();
  UIRenderer::DrawTextUI(font, "维度概览 (Mosaic Summary)", previewX + 10, previewY + 10, 18, GOLD, 1.0f);

  float y = previewY + 40;
  char buf[128];

  // 1. Difficulty Section
  DrawRectangleGradientH(static_cast<int>(previewX), static_cast<int>(y), static_cast<int>(previewWidth), 30, Color{60,0,0,100}, Color{20,0,0,0});
  utils::FormatToBuffer(buf, "难度系数 (DS): {}", m_previewDS);
  UIRenderer::DrawTextUI(font, buf, previewX + 10, y + 5, 20, RED, 1.0f);
  y += 35;

  // 2. Rewards Section (Calculated)
  utils::FormatToBuffer(buf, "物品掉宝: +{:.0f}%",
                        m_previewRarity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, previewX + 10, y, 16, components::Colors::RARITY_LEGENDARY, 1.0f); // Gold/Orange
  y += 20;

  utils::FormatToBuffer(buf, "物品数量: +{:.0f}%",
                        m_previewQuantity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, previewX + 10, y, 16, components::Colors::RARITY_EPIC, 1.0f); // Purple
  y += 25;
  
  // High LP Chance Note
  if (m_previewRarity > 1.0f) {
       float lpMult = MapAffixCalculator::CalculateLPProbabilityMultiplier(m_previewRarity);
       utils::FormatToBuffer(buf, "Legendary Potential: {:.1f}x", lpMult);
       UIRenderer::DrawTextUI(font, buf, previewX + 10, y, 12, GRAY, 1.0f);
       y += 20;
  }
  
  DrawRectangle(static_cast<int>(previewX) + 10, static_cast<int>(y), static_cast<int>(previewWidth) - 20, 1, GRAY);
  y += 5;

  // 3. Base Stats (Resonance)
  if (m_cachedResonance.totalEnemyDensity != 1.0f) {
      utils::FormatToBuffer(buf, "基础密度: {:.0f}%",
                            m_cachedResonance.totalEnemyDensity * 100);
      UIRenderer::DrawTextUI(font, buf, previewX + 10, y, 14, LIGHTGRAY, 1.0f);
      y += 18;
  }
  if (m_cachedResonance.totalLevelMod != 0) {
      utils::FormatToBuffer(buf, "怪物等级: {:+}",
                            m_cachedResonance.totalLevelMod);
      UIRenderer::DrawTextUI(font, buf, previewX + 10, y, 14, WHITE, 1.0f);
      y += 18;
  }
  
  if (m_cachedResonance.isPerfectResonance) {
    UIRenderer::DrawTextUI(font, "★ 完美共鸣 (Perfect) ★", previewX + 10, y, 14, GOLD, 1.0f);
    y += 20;
  }
  
  y += 10;
  // Layer Count Info
  UIRenderer::DrawTextUI(font, "有效层数 (Duration): 3 层", previewX + 10, y, 14, SKYBLUE, 1.0f);
  y += 20;
  
  UIRenderer::DrawTextUI(font, "激活词缀 (Active Affixes):", previewX + 10, y, 16, WHITE, 1.0f);
  y += 20;

  // 4. Affix List
  for (const auto& agg : m_cachedAggregatedAffixes) {
      // Show Name + Tier
      Color color = WHITE;
      if (agg.category == MapAffixCategory::Debuff) color = RED;
      else if (agg.category == MapAffixCategory::Buff) color = GREEN;
      else if (agg.category == MapAffixCategory::Environment) color = SKYBLUE;
      
      std::string desc = MapAffixRegistry::FormatDescription(agg.type, agg.totalValue);
      
      UIRenderer::DrawTextUI(font, desc.c_str(), previewX + 15, y, 14, color, 1.0f);
      y += 16;
      
      if (y > previewY + previewHeight - 20) break; // Overflow protection
  }
}

void MosaicEditorState::RenderConfirmButton() {
  float btnX_Logic = (GRID_OFFSET_X + 50);
  float btnY_Logic = (GRID_OFFSET_Y + 350);
  float btnW_Logic = 220;
  float btnH_Logic = 55;
  Rectangle btnRect_Logic = {btnX_Logic, btnY_Logic, btnW_Logic, btnH_Logic};
  Vector2 mouseLogic = UISystem::GetMousePositionLogic();
  bool hovered = CheckCollisionPointRec(mouseLogic, btnRect_Logic);

  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
  bool canGenerate = m_grid.GetFilledCount() > 0;

  Color tint = canGenerate ? (hovered ? GREEN : DARKGREEN) : GRAY;
  
  UIRenderer::DrawButton(UISystem::GetFont(), rectTex, btnRect_Logic, "Generate Map", 24, WHITE, tint, canGenerate && hovered, canGenerate && hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON), 1.0f);
}

void MosaicEditorState::RenderDraggedFragment() {
  if (m_draggedFragment == entt::null)
    return;
  auto *fragComp =
      m_context->registry->try_get<MapFragmentComponent>(m_draggedFragment);
  if (!fragComp)
    return;

  Vector2 mouse = GetMousePosition();
  float x = mouse.x - FRAGMENT_ICON_SIZE / 2 + m_dragOffset.x;
  float y = mouse.y - FRAGMENT_ICON_SIZE / 2 + m_dragOffset.y;

  Color elemColor = GetElementColor(fragComp->element);
  elemColor.a = 200;
  DrawRectangleRounded(Rectangle{x, y, FRAGMENT_ICON_SIZE, FRAGMENT_ICON_SIZE},
                       0.15f, 4, elemColor);
  DrawRectangleRoundedLinesEx(
      Rectangle{x, y, FRAGMENT_ICON_SIZE, FRAGMENT_ICON_SIZE}, 0.15f, 4, 2.0f,
      WHITE);
}

void MosaicEditorState::RenderConfirmDialog() {
  DrawRectangle(0, 0, (float)GetScreenWidth(), (float)GetScreenHeight(), Color{0, 0, 0, 150});
  float dlgW = 450, dlgH = 220;
  float dlgX_Logic = (UI_REF_WIDTH - dlgW) / 2;
  float dlgY_Logic = (UI_REF_HEIGHT - dlgH) / 2;

  float scale = UIRenderer::GetScale();
  DrawRectangleRounded(Rectangle{dlgX_Logic * scale, dlgY_Logic * scale, dlgW * scale, dlgH * scale}, 0.1f, 8,
                       Color{30, 30, 45, 255});
  DrawRectangleRoundedLinesEx(Rectangle{dlgX_Logic * scale, dlgY_Logic * scale, dlgW * scale, dlgH * scale}, 0.1f, 8, 2.0f * scale,
                              SKYBLUE);
  
  Font font = UISystem::GetFont();
  UIRenderer::DrawTextUI(font, "Generate Map?", dlgX_Logic + 140, dlgY_Logic + 40, 28, WHITE, 1.0f);

  Rectangle yesBtn_Logic = {dlgX_Logic + 50, dlgY_Logic + 130, 140, 45};
  Rectangle noBtn_Logic = {dlgX_Logic + 260, dlgY_Logic + 130, 140, 45};
  
  Vector2 mouseLogic = UISystem::GetMousePositionLogic();
  bool hoveredYes = CheckCollisionPointRec(mouseLogic, yesBtn_Logic);
  bool hoveredNo = CheckCollisionPointRec(mouseLogic, noBtn_Logic);
  
  Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

  UIRenderer::DrawButton(font, rectTex, yesBtn_Logic, "确定", 22, WHITE, hoveredYes ? GREEN : DARKGREEN, hoveredYes, hoveredYes && IsMouseButtonDown(MOUSE_LEFT_BUTTON), 1.0f);
  UIRenderer::DrawButton(font, rectTex, noBtn_Logic, "取消", 22, WHITE, WHITE, hoveredNo, hoveredNo && IsMouseButtonDown(MOUSE_LEFT_BUTTON), 1.0f);
}
void MosaicEditorState::RenderTooltip() {
  entt::entity hoveredEntity = entt::null;

  if (m_hoveredInventoryIndex >= 0) {
    int index = m_hoveredInventoryIndex + m_inventoryScrollOffset;
    if (index >= 0 && index < static_cast<int>(m_fragmentInventory.size())) {
      hoveredEntity = m_fragmentInventory[index];
    }
  } else if (m_hoveredCellIndex >= 0) {
    hoveredEntity = m_grid.cells[m_hoveredCellIndex];
  }

  if (hoveredEntity == entt::null || !m_context->registry->valid(hoveredEntity))
    return;

  auto *frag = m_context->registry->try_get<MapFragmentComponent>(hoveredEntity);
  auto *item = m_context->registry->try_get<ItemComponent>(hoveredEntity);
  if (!frag) return;

  Vector2 mouse = GetMousePosition();
  float x = mouse.x + 15;
  float y = mouse.y + 15;
  float w = 240;
  
  // Calculate dynamic height
  float h = 80; // Base height (Header + Density)
  if (frag->monsterLevelMod != 0) h += 20;
  h += 25; // Implicit Header
  if (frag->element != FragmentElement::None) h += 18;
  if (item->rarity >= Rarity::Magic) h += 18;
  h += 20; // Duration line

  // Background
  DrawRectangleRounded(Rectangle{x, y, w, h}, 0.1f, 4, Color{20, 20, 30, 240});
  DrawRectangleRoundedLinesEx(Rectangle{x, y, w, h}, 0.1f, 4, 1.5f, LIGHTGRAY);

  Font font = UISystem::GetFont();
  std::string name = item ? item->name : "Unknown Fragment";
  // Attempt dynamic name if missing
  if (name.empty() || name == "Unknown Fragment") {
       name = std::string(FragmentElementzh[static_cast<size_t>(frag->element)]) + 
              std::string(FragmentTypezh[static_cast<size_t>(frag->type)]);
  }

  UIRenderer::DrawTextUI(font, name.c_str(), x + 10, y + 10, 16, GOLD, 1.0f);
  
  float ty = y + 35;
  char buf[64];
  
  utils::FormatToBuffer(buf, "怪物密度: {:+.0f}%",
                        (frag->enemyDensityMod - 1.0f) * 100.0f);
  UIRenderer::DrawTextUI(font, buf, x + 10, ty, 14, WHITE, 1.0f);
  ty += 20;

  if (frag->monsterLevelMod != 0) {
      utils::FormatToBuffer(buf, "怪物等级: {:+}", frag->monsterLevelMod);
      UIRenderer::DrawTextUI(font, buf, x + 10, ty, 14, WHITE, 1.0f);
      ty += 20;
  }
  
  // Duration
  utils::FormatToBuffer(buf, "有效层数: {} 层", frag->remainingLayers);
  UIRenderer::DrawTextUI(font, buf, x + 10, ty, 14, SKYBLUE, 1.0f);
  ty += 20;

  // Show Implicit Affix Sources
  ty += 5;
  UIRenderer::DrawTextUI(font, "隐含词缀 (Implicit):", x + 10, ty, 14, GRAY, 1.0f);
  ty += 18;
  
  // 1. Element Source
  if (frag->element != FragmentElement::None) {
      std::string elName = std::string(FragmentElementzh[static_cast<size_t>(frag->element)]);
      utils::FormatToBuffer(buf, "• {} (元素)", elName);
      UIRenderer::DrawTextUI(font, buf, x + 15, ty, 12, GetElementColor(frag->element), 1.0f);
      ty += 16;
  }
  
  // 2. Rarity Source
  if (item->rarity >= Rarity::Magic) {
       utils::FormatToBuffer(buf, "• {} (稀有度)",
                             (item->rarity == Rarity::Legendary ? "传说"
                                                                : "魔法"));
       UIRenderer::DrawTextUI(font, buf, x + 15, ty, 12, GetRarityColor(item->rarity), 1.0f);
       ty += 16;
  }
}

void MosaicEditorState::HandleInput() {
  Vector2 mouse = GetMousePosition();
  m_hoveredCellIndex = GetCellIndexAtPos(mouse);
  m_hoveredInventoryIndex = GetInventoryIndexAtPos(mouse);

  if (IsKeyPressed(KEY_ESCAPE)) {
    if (m_showActiveRiftBlockedDialog) {
      m_showActiveRiftBlockedDialog = false;
    } else if (m_showConfirmDialog) {
      m_showConfirmDialog = false;
    } else {
      m_stateManager->PopState();
    }
    return;
  }

  if (m_showActiveRiftBlockedDialog) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      Rectangle okBtn{(GetScreenWidth() - 160.0f) * 0.5f,
                      (GetScreenHeight() - 220.0f) * 0.5f + 150.0f,
                      160.0f, 44.0f};
      if (CheckCollisionPointRec(mouse, okBtn)) {
        m_showActiveRiftBlockedDialog = false;
      }
    }
    return;
  }

  if (m_showConfirmDialog) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      float dlgX = (GetScreenWidth() - 400) / 2;
      float dlgY = (GetScreenHeight() - 200) / 2;
      Rectangle yesBtn = {dlgX + 50, dlgY + 120, 120, 40};
      Rectangle noBtn = {dlgX + 230, dlgY + 120, 120, 40};
      if (CheckCollisionPointRec(mouse, yesBtn))
        ConfirmAndGenerate();
      else if (CheckCollisionPointRec(mouse, noBtn))
        m_showConfirmDialog = false;
    }
    return;
  }

  float wheel = GetMouseWheelMove();
  if (wheel != 0 && m_hoveredInventoryIndex >= 0) {
    m_inventoryScrollOffset -= static_cast<int>(wheel);
    m_inventoryScrollOffset =
        std::max(0, std::min(m_inventoryScrollOffset,
                             static_cast<int>(m_fragmentInventory.size()) - 5));
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (m_draggedFragment == entt::null) {
      if (m_hoveredInventoryIndex >= 0 &&
          m_hoveredInventoryIndex <
              static_cast<int>(m_fragmentInventory.size())) {
        int idx = m_hoveredInventoryIndex;
        if (idx < static_cast<int>(m_fragmentInventory.size())) {
          StartDrag(m_fragmentInventory[idx]);
        }
      } else if (m_hoveredCellIndex >= 0) {
        entt::entity existing = m_grid.cells[m_hoveredCellIndex];
        if (existing != entt::null) {
          m_grid.cells[m_hoveredCellIndex] = entt::null;
          StartDrag(existing);
          m_resonanceDirty = true;
        }
      }
    }
  }

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (m_draggedFragment != entt::null) {
      if (m_hoveredCellIndex >= 0)
        TryPlaceFragment(m_hoveredCellIndex);
      EndDrag();
    }
    Rectangle btnRect = {GRID_OFFSET_X + 50, GRID_OFFSET_Y + 350, 200, 50};
    if (CheckCollisionPointRec(mouse, btnRect) && m_grid.GetFilledCount() > 0) {
      m_showConfirmDialog = true;
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && m_hoveredCellIndex >= 0) {
    entt::entity removed = m_grid.cells[m_hoveredCellIndex];
    if (removed != entt::null) {
      m_grid.cells[m_hoveredCellIndex] = entt::null;
      m_fragmentInventory.push_back(removed);
      m_resonanceDirty = true;
    }
  }
}

void MosaicEditorState::StartDrag(entt::entity fragment) {
  m_draggedFragment = fragment;
  m_dragOffset = {0, 0};
  auto it = std::find(m_fragmentInventory.begin(), m_fragmentInventory.end(),
                      fragment);
  if (it != m_fragmentInventory.end())
    m_fragmentInventory.erase(it);
}

void MosaicEditorState::EndDrag() {
  if (m_draggedFragment != entt::null) {
    bool isInGrid = false;
    for (size_t i = 0; i < m_grid.cells.size(); ++i) {
      if (m_grid.cells[i] == m_draggedFragment) {
        isInGrid = true;
        break;
      }
    }
    if (!isInGrid)
      m_fragmentInventory.push_back(m_draggedFragment);
  }
  m_draggedFragment = entt::null;
}

void MosaicEditorState::TryPlaceFragment(int cellIndex) {
  if (cellIndex < 0 || cellIndex >= MosaicGrid::TOTAL_CELLS)
    return;
  entt::entity existing = m_grid.cells[cellIndex];
  if (existing != entt::null)
    m_fragmentInventory.push_back(existing);
  m_grid.cells[cellIndex] = m_draggedFragment;
  m_draggedFragment = entt::null;
  m_resonanceDirty = true;
}

void MosaicEditorState::RefreshFragmentInventory() {
  m_fragmentInventory.clear();
  auto view = m_context->registry->view<MapFragmentTag, ItemComponent>();
  for (auto entity : view) {
    bool inGrid = false;
    for (size_t i = 0; i < m_grid.cells.size(); ++i) {
      if (m_grid.cells[i] == entity) {
        inGrid = true;
        break;
      }
    }
    if (!inGrid)
      m_fragmentInventory.push_back(entity);
  }
  
  // Sort by Rarity (Primary) -> Element (Secondary) -> Type (Tertiary)
  std::sort(m_fragmentInventory.begin(), m_fragmentInventory.end(), [this](entt::entity a, entt::entity b) {
      auto& itemA = m_context->registry->get<ItemComponent>(a);
      auto& itemB = m_context->registry->get<ItemComponent>(b);
      
      if (itemA.rarity != itemB.rarity) {
          return static_cast<int>(itemA.rarity) > static_cast<int>(itemB.rarity);
      }
      
      auto* fragA = m_context->registry->try_get<MapFragmentComponent>(a);
      auto* fragB = m_context->registry->try_get<MapFragmentComponent>(b);
      
      if (fragA && fragB) {
          if (fragA->element != fragB->element) {
               return static_cast<int>(fragA->element) < static_cast<int>(fragB->element);
          }
          return static_cast<int>(fragA->type) < static_cast<int>(fragB->type);
      }
      return false;
  });

  LOG_DEBUG("MosaicEditorState: Found {} fragments in inventory",
            m_fragmentInventory.size());
}

void MosaicEditorState::RecalculateResonance() {
  m_cachedResonance =
      ResonanceCalculator::Calculate(m_grid, *m_context->registry);
  for (int y = 0; y < MosaicGrid::SIZE; ++y) {
    for (int x = 0; x < MosaicGrid::SIZE; ++x) {
      m_grid.resonanceMultipliers[MosaicGrid::ToIndex(x, y)] =
          ResonanceCalculator::CalculateCellResonance(m_grid, x, y,
                                                      *m_context->registry);
    }
  }

  // Preview Affixes & Difficulty
  m_previewAffixes = MapAffixCalculator::GenerateAffixesFromGrid(m_grid, *m_context->registry);
  m_cachedAggregatedAffixes = MapAffixCalculator::AggregateAffixes(m_previewAffixes);
  m_previewDS = MapAffixCalculator::CalculateDifficultyScore(m_previewAffixes);
  auto rewards = MapAffixCalculator::CalculateRewards(m_previewDS);
  m_previewRarity = rewards.rarityBonus;
  m_previewQuantity = rewards.quantityBonus;
}

void MosaicEditorState::ConfirmAndGenerate() {
  LOG_INFO("MosaicEditorState: Generating map with {} fragments",
           m_grid.GetFilledCount());

  if (m_context->registry->ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      const auto& activeState = m_context->registry->ctx().get<NoMoreDay::ActiveDimensionalState>();
      if (NoMoreDay::HasInProgressRift(activeState)) {
          LOG_WARN("MosaicEditorState blocked: active in-progress rift cannot be overwritten.");
          m_showConfirmDialog = false;
          m_showActiveRiftBlockedDialog = true;
          return;
      }
  }

  RecalculateResonance(); // Ensure resonance data is fresh

  // --- Map Affix & Persistence Logic ---
  if (m_context->registry->ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      auto& worldState = m_context->registry->ctx().get<NoMoreDay::ActiveDimensionalState>();
      
      // 1. Generate Affixes from Grid
      worldState.explicitAffixes = MapAffixCalculator::GenerateAffixesFromGrid(m_grid, *m_context->registry);
      
      // 2. Calculate Stats
      worldState.difficultyScore = MapAffixCalculator::CalculateDifficultyScore(worldState.explicitAffixes);
      auto rewards = MapAffixCalculator::CalculateRewards(worldState.difficultyScore);
      worldState.calculatedRarity = rewards.rarityBonus;
      worldState.calculatedQuantity = rewards.quantityBonus;
      
      // 3. Initialize State
      worldState.isActive = true;
      // Simple seed generation (time + grid hash)
      worldState.seed = static_cast<uint32_t>(GetTime() * 1000) ^ m_grid.GetFilledCount(); 
      const auto effectiveBiome = (m_cachedResonance.primaryBiome == NoMoreDay::BiomeID::None)
                                      ? NoMoreDay::BiomeID::Cave
                                      : m_cachedResonance.primaryBiome;
      worldState.biome = effectiveBiome;
      m_cachedResonance.primaryBiome = effectiveBiome;
      worldState.currentDepth = 1;
      worldState.maxDepth = 3; // Default
      worldState.resonance = m_cachedResonance;
      worldState.explicitAffixes = m_previewAffixes;
      worldState.aggregatedAffixes = m_cachedAggregatedAffixes;
      worldState.isBossKilled = false;
      worldState.isCompleted = false;
      worldState.killCounter = 0;
      worldState.lastExitPosition = {0.0f, 0.0f};
      
      // 4. Snapshot the grid for persistence
      for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
          entt::entity fragEntity = m_grid.cells[i];
          auto& snap = worldState.gridSnapshots[i];
          if (m_context->registry->valid(fragEntity)) {
              auto* frag = m_context->registry->try_get<MapFragmentComponent>(fragEntity);
              auto* item = m_context->registry->try_get<ItemComponent>(fragEntity);
              if (frag && item) {
                  snap.hasFragment = true;
                  snap.element = frag->element;
                  snap.type = frag->type;
                  snap.rarity = item->rarity;
                  snap.enemyDensityMod = frag->enemyDensityMod;
                  snap.monsterLevelMod = frag->monsterLevelMod;
                  snap.remainingLayers = frag->remainingLayers;
                  snap.name = item->name;
              } else {
                  snap.hasFragment = false;
              }
          } else {
              snap.hasFragment = false;
          }
      }
      
      LOG_INFO("Dimensional State Activated: BaseLv={}, DS={}, Rarity={:.1f}%, Quant={:.1f}%", 
               worldState.selectedBaseLevel, worldState.difficultyScore, worldState.calculatedRarity*100.0f, worldState.calculatedQuantity*100.0f);
  }

  if (!m_context->registry->ctx().contains<NoMoreDay::RiftCompletionPromptState>()) {
    m_context->registry->ctx().emplace<NoMoreDay::RiftCompletionPromptState>();
  }
  m_context->registry->ctx().get<NoMoreDay::RiftCompletionPromptState>().isPending = false;

  if (m_context && m_context->sceneManager) {
    m_context->sceneManager->RequestMosaicTransition(m_grid, m_cachedResonance);
  } else if (m_context && m_context->levelManager) {
    // Fallback for systems without SceneManager (though unlikely in current architecture)
    m_context->levelManager->loadMosaicLevel(m_grid, m_cachedResonance,
                                             m_context->registry);
  } else {
    LOG_ERROR(
        "MosaicEditorState: SceneManager/LevelManager not available!");
  }

  m_showConfirmDialog = false;
  m_stateManager->PopState();
}

void MosaicEditorState::RenderActiveRiftBlockedDialog() {
  DrawRectangle(0, 0, (float)GetScreenWidth(), (float)GetScreenHeight(), Color{0, 0, 0, 150});
  const float dlgW = 560.0f;
  const float dlgH = 220.0f;
  const float dlgX = ((float)GetScreenWidth() - dlgW) * 0.5f;
  const float dlgY = ((float)GetScreenHeight() - dlgH) * 0.5f;

  DrawRectangleRounded(Rectangle{dlgX, dlgY, dlgW, dlgH}, 0.1f, 8, Color{45, 32, 32, 255});
  DrawRectangleRoundedLinesEx(Rectangle{dlgX, dlgY, dlgW, dlgH}, 0.1f, 8, 2.0f, ORANGE);

  DrawText("无法创建新裂隙", (int)(dlgX + 170.0f), (int)(dlgY + 30.0f), 34, WHITE);
  DrawText("已有进行中的维度裂隙，请先继续或放弃后再新建。",
           (int)(dlgX + 45.0f), (int)(dlgY + 90.0f), 24, LIGHTGRAY);

  Rectangle okBtn{dlgX + (dlgW - 160.0f) * 0.5f, dlgY + 150.0f, 160.0f, 44.0f};
  const Vector2 mouse = GetMousePosition();
  const bool hovered = CheckCollisionPointRec(mouse, okBtn);
  DrawRectangleRounded(okBtn, 0.2f, 4, hovered ? GRAY : DARKGRAY);
  DrawRectangleRoundedLinesEx(okBtn, 0.2f, 4, 2.0f, RAYWHITE);
  DrawText("知道了", (int)(okBtn.x + 44.0f), (int)(okBtn.y + 11.0f), 22, WHITE);
}

Rectangle MosaicEditorState::GetCellRect(int x, int y) const {
  float screenX = GRID_OFFSET_X + x * (CELL_SIZE + CELL_PADDING);
  float screenY = GRID_OFFSET_Y + y * (CELL_SIZE + CELL_PADDING);
  return Rectangle{screenX, screenY, CELL_SIZE, CELL_SIZE};
}

Rectangle MosaicEditorState::GetInventorySlotRect(int index) const {
  float slotH = 55.0f;
  float startY = GRID_OFFSET_Y + 40;
  return Rectangle{INVENTORY_X + 5, startY + index * (slotH + 5), 240, slotH};
}

int MosaicEditorState::GetCellIndexAtPos(Vector2 pos) const {
  for (int y = 0; y < MosaicGrid::SIZE; ++y) {
    for (int x = 0; x < MosaicGrid::SIZE; ++x) {
      if (CheckCollisionPointRec(pos, GetCellRect(x, y)))
        return MosaicGrid::ToIndex(x, y);
    }
  }
  return -1;
}

int MosaicEditorState::GetInventoryIndexAtPos(Vector2 pos) const {
  for (int i = 0; i < 5; ++i) {
    if (CheckCollisionPointRec(pos, GetInventorySlotRect(i)))
      return i + m_inventoryScrollOffset;
  }
  return -1;
}

Color MosaicEditorState::GetRarityColor(Rarity rarity) const {
  switch (rarity) {
  case Rarity::Common:
    return WHITE;
  case Rarity::Magic:
    return BLUE;
  case Rarity::Rare:
    return YELLOW;
  case Rarity::Epic:
    return PURPLE;
  case Rarity::Legendary:
    return ORANGE;
  default:
    return GRAY;
  }
}

Color MosaicEditorState::GetElementColor(FragmentElement element) const {
  switch (element) {
  case FragmentElement::Fire:
    return Color{255, 80, 80, 255};
  case FragmentElement::Cold:
    return Color{80, 160, 255, 255};
  case FragmentElement::Lightning:
    return Color{255, 255, 80, 255};
  case FragmentElement::Shadow:
    return Color{160, 80, 200, 255};
  case FragmentElement::Chaos:
    return Color{150, 150, 150, 255};
  default:
    return Color{100, 100, 120, 255};
  }
}

} // namespace NoMoreDay
