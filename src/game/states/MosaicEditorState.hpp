#pragma once

#include "engine/scene/State.hpp"
#include "game/data/MosaicData.hpp"
#include "game/data/MapAffix.hpp"
#include "game/systems/world/MapAffixCalculator.hpp"
#include "raylib.h"
#include <vector>

namespace NoMoreDay {

/**
 * @brief 维度拼接编辑器状态
 *
 * 玩家在每层结束时进入此界面，将收集的碎片放入 3x3 网格来构建下一层地图。
 */
class MosaicEditorState : public IState {
public:
  using IState::IState;

  void OnEnter() override;
  void OnExit() override;
  bool OnUpdate(float dt) override;
  void OnRender() override;

  // 编辑器是透明的，可以看到下面的游戏画面
  bool IsTransparent() const override { return true; }

private:
  // UI 布局常量
  static constexpr float CELL_SIZE = 100.0f;
  static constexpr float CELL_PADDING = 8.0f;
  static constexpr float GRID_OFFSET_X = 100.0f;
  static constexpr float GRID_OFFSET_Y = 100.0f;
  static constexpr float INVENTORY_X = 450.0f;
  static constexpr float FRAGMENT_ICON_SIZE = 64.0f;

  // 拼图网格
  MosaicGrid m_grid;

  // UI 状态
  entt::entity m_draggedFragment = entt::null;
  int m_hoveredCellIndex = -1;
  int m_hoveredInventoryIndex = -1;
  Vector2 m_dragOffset = {0, 0};

  // 缓存的共鸣结果
  ResonanceResult m_cachedResonance;
  bool m_resonanceDirty = true;

  // Affix & Difficulty Preview
  std::vector<MapAffix> m_previewAffixes;
  int m_previewDS = 0;
  float m_previewRarity = 0.0f;
  float m_previewQuantity = 0.0f;

  // 玩家碎片库存 (从 registry 中筛选)
  std::vector<entt::entity> m_fragmentInventory;
  int m_inventoryScrollOffset = 0;

  // Cached Aggregated Affixes (Optimization)
  std::vector<AggregatedAffix> m_cachedAggregatedAffixes;

  // 纭瀵硅瘽妗
  bool m_showConfirmDialog = false;
  bool m_showActiveRiftBlockedDialog = false;

  // 渲染方法
  void RenderBackground();
  void RenderGrid();
  void RenderGridCell(int x, int y, float screenX, float screenY);
  void RenderInventory();
  void RenderResonancePreview();
  void RenderConfirmButton();
  void RenderDraggedFragment();
  void RenderTooltip();
  void RenderConfirmDialog();
  void RenderActiveRiftBlockedDialog();

  // 交互方法
  void HandleInput();
  void HandleGridClick(int cellIndex);
  void HandleInventoryClick(int invIndex);
  void StartDrag(entt::entity fragment);
  void EndDrag();
  void TryPlaceFragment(int cellIndex);

  // 数据方法
  void RefreshFragmentInventory();
  void RecalculateResonance();
  void ConfirmAndGenerate();

  // 辅助方法
  Rectangle GetCellRect(int x, int y) const;
  Rectangle GetInventorySlotRect(int index) const;
  int GetCellIndexAtPos(Vector2 pos) const;
  int GetInventoryIndexAtPos(Vector2 pos) const;
  Color GetRarityColor(Rarity rarity) const;
  Color GetElementColor(FragmentElement element) const;
};

} // namespace NoMoreDay
