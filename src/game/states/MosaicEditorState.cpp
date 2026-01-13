#include "game/states/MosaicEditorState.hpp"
#include "core/logging/Logger.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include "game/data/ResonanceCalculator.hpp"
#include "game/systems/world/LevelManager.hpp"
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
  m_inventoryScrollOffset = 0;
}

void MosaicEditorState::OnExit() {
  LOG_INFO("MosaicEditorState: Exited");
  m_fragmentInventory.clear();
}

bool MosaicEditorState::OnUpdate(float dt) {
  HandleInput();
  if (m_resonanceDirty) {
    RecalculateResonance();
    m_resonanceDirty = false;
  }
  return false;
}

void MosaicEditorState::OnRender() {
  RenderBackground();
  RenderGrid();
  RenderFragmentInventory();
  RenderResonancePreview();
  RenderConfirmButton();
  RenderDraggedFragment();
  if (m_showConfirmDialog) {
    RenderConfirmDialog();
  }
}

void MosaicEditorState::RenderBackground() {
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 180});
  const char *title = "Dimensional Mosaic";
  int titleWidth = MeasureText(title, 40);
  DrawText(title, (GetScreenWidth() - titleWidth) / 2, 30, 40, WHITE);
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
        snprintf(buf, sizeof(buf), "x%.1f", resonance);
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

void MosaicEditorState::RenderFragmentInventory() {
  float invWidth = 250.0f;
  float invHeight = 400.0f;
  DrawRectangleRounded(
      Rectangle{INVENTORY_X, GRID_OFFSET_Y, invWidth, invHeight}, 0.05f, 8,
      Color{40, 40, 60, 200});
  DrawText("Fragments", static_cast<int>(INVENTORY_X + 10),
           static_cast<int>(GRID_OFFSET_Y + 10), 20, WHITE);

  int visibleCount = 5;
  for (int i = 0;
       i < visibleCount && (i + m_inventoryScrollOffset) <
                               static_cast<int>(m_fragmentInventory.size());
       ++i) {
    int actualIndex = i + m_inventoryScrollOffset;
    entt::entity frag = m_fragmentInventory[actualIndex];
    if (!m_context->registry->valid(frag))
      continue;

    auto *fragComp = m_context->registry->try_get<MapFragmentComponent>(frag);
    auto *itemComp = m_context->registry->try_get<ItemComponent>(frag);
    if (!fragComp || !itemComp)
      continue;

    Rectangle slotRect = GetInventorySlotRect(i);
    Color bgColor = (actualIndex == m_hoveredInventoryIndex)
                        ? Color{70, 70, 100, 255}
                        : Color{50, 50, 70, 255};
    DrawRectangleRounded(slotRect, 0.1f, 4, bgColor);

    Color elemColor = GetElementColor(fragComp->element);
    DrawRectangle(static_cast<int>(slotRect.x), static_cast<int>(slotRect.y), 5,
                  static_cast<int>(slotRect.height), elemColor);
    DrawText(itemComp->name.c_str(), static_cast<int>(slotRect.x + 12),
             static_cast<int>(slotRect.y + 8), 16,
             GetRarityColor(itemComp->rarity));

    char attrBuf[64];
    snprintf(attrBuf, sizeof(attrBuf), "D:%.0f%% R:%.0f%%",
             fragComp->enemyDensityMod * 100, fragComp->dropRateMod * 100);
    DrawText(attrBuf, static_cast<int>(slotRect.x + 12),
             static_cast<int>(slotRect.y + 28), 12, LIGHTGRAY);
    DrawRectangleRoundedLinesEx(
        slotRect, 0.1f, 4, 1.5f,
        actualIndex == m_hoveredInventoryIndex ? SKYBLUE : DARKGRAY);
  }
}

void MosaicEditorState::RenderResonancePreview() {
  float previewX = INVENTORY_X + 270;
  float previewY = GRID_OFFSET_Y;
  float previewWidth = 200.0f;
  float previewHeight = 180.0f;

  DrawRectangleRounded(
      Rectangle{previewX, previewY, previewWidth, previewHeight}, 0.05f, 8,
      Color{40, 40, 60, 200});
  DrawText("Resonance Preview", static_cast<int>(previewX + 10),
           static_cast<int>(previewY + 10), 16, GOLD);

  float y = previewY + 35;
  char buf[64];

  snprintf(buf, sizeof(buf), "Density: %.0f%%",
           m_cachedResonance.totalEnemyDensity * 100);
  DrawText(buf, static_cast<int>(previewX + 10), static_cast<int>(y), 14,
           WHITE);
  y += 22;

  snprintf(buf, sizeof(buf), "Drop Rate: %.0f%%",
           m_cachedResonance.totalDropRate * 100);
  DrawText(buf, static_cast<int>(previewX + 10), static_cast<int>(y), 14,
           m_cachedResonance.totalDropRate > 1.5f ? GREEN : WHITE);
  y += 22;

  if (m_cachedResonance.totalLevelMod != 0) {
    snprintf(buf, sizeof(buf), "Level: %s%d",
             m_cachedResonance.totalLevelMod > 0 ? "+" : "",
             m_cachedResonance.totalLevelMod);
    DrawText(buf, static_cast<int>(previewX + 10), static_cast<int>(y), 14,
             WHITE);
    y += 22;
  }
  if (m_cachedResonance.resonanceChainCount > 0) {
    snprintf(buf, sizeof(buf), "Chains: %d",
             m_cachedResonance.resonanceChainCount);
    DrawText(buf, static_cast<int>(previewX + 10), static_cast<int>(y), 14,
             SKYBLUE);
    y += 22;
  }
  if (m_cachedResonance.isPerfectResonance) {
    DrawText("PERFECT x2!", static_cast<int>(previewX + 10),
             static_cast<int>(y), 16, GOLD);
  }
}

void MosaicEditorState::RenderConfirmButton() {
  float btnX = GRID_OFFSET_X + 50;
  float btnY = GRID_OFFSET_Y + 350;
  float btnW = 200;
  float btnH = 50;
  Rectangle btnRect = {btnX, btnY, btnW, btnH};
  Vector2 mouse = GetMousePosition();
  bool hovered = CheckCollisionPointRec(mouse, btnRect);

  Color btnColor =
      m_grid.GetFilledCount() == 0
          ? Color{60, 60, 60, 255}
          : (hovered ? Color{60, 140, 60, 255} : Color{40, 100, 40, 255});
  DrawRectangleRounded(btnRect, 0.2f, 8, btnColor);
  DrawRectangleRoundedLinesEx(btnRect, 0.2f, 8, 2.0f,
                              hovered ? WHITE : DARKGREEN);

  const char *text = "Generate Map";
  int textW = MeasureText(text, 24);
  DrawText(text, static_cast<int>(btnX + btnW / 2 - textW / 2),
           static_cast<int>(btnY + 12), 24, WHITE);
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
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 150});
  float dlgW = 400, dlgH = 200;
  float dlgX = (GetScreenWidth() - dlgW) / 2;
  float dlgY = (GetScreenHeight() - dlgH) / 2;

  DrawRectangleRounded(Rectangle{dlgX, dlgY, dlgW, dlgH}, 0.1f, 8,
                       Color{50, 50, 70, 255});
  DrawRectangleRoundedLinesEx(Rectangle{dlgX, dlgY, dlgW, dlgH}, 0.1f, 8, 2.0f,
                              SKYBLUE);
  DrawText("Generate Map?", static_cast<int>(dlgX + 120),
           static_cast<int>(dlgY + 30), 24, WHITE);

  Rectangle yesBtn = {dlgX + 50, dlgY + 120, 120, 40};
  Rectangle noBtn = {dlgX + 230, dlgY + 120, 120, 40};
  Vector2 mouse = GetMousePosition();

  DrawRectangleRounded(yesBtn, 0.2f, 4,
                       CheckCollisionPointRec(mouse, yesBtn) ? GREEN
                                                             : DARKGREEN);
  DrawText("Yes", static_cast<int>(yesBtn.x + 40),
           static_cast<int>(yesBtn.y + 10), 20, WHITE);
  DrawRectangleRounded(noBtn, 0.2f, 4,
                       CheckCollisionPointRec(mouse, noBtn) ? RED : MAROON);
  DrawText("No", static_cast<int>(noBtn.x + 45), static_cast<int>(noBtn.y + 10),
           20, WHITE);
}

void MosaicEditorState::HandleInput() {
  Vector2 mouse = GetMousePosition();
  m_hoveredCellIndex = GetCellIndexAtPos(mouse);
  m_hoveredInventoryIndex = GetInventoryIndexAtPos(mouse);

  if (IsKeyPressed(KEY_ESCAPE)) {
    if (m_showConfirmDialog)
      m_showConfirmDialog = false;
    else
      m_stateManager->PopState();
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
}

void MosaicEditorState::ConfirmAndGenerate() {
  LOG_INFO("MosaicEditorState: Generating map with {} fragments",
           m_grid.GetFilledCount());

  RecalculateResonance(); // Ensure resonance data is fresh

  if (m_context && m_context->levelManager) {
    m_context->levelManager->loadMosaicLevel(m_grid, m_cachedResonance,
                                             m_context->registry);
  } else {
    LOG_ERROR(
        "MosaicEditorState: LevelManager not available in SharedContext!");
  }

  m_showConfirmDialog = false;
  m_stateManager->PopState();
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
