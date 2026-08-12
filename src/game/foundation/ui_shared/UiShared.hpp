#pragma once

#include <raylib.h>

#include "game/foundation/components/ItemComponent.hpp"

namespace NoMoreDay {

/**
 * @brief 无状态 UI/渲染辅助（设计 §5.3 环2 破环 sink）。
 *
 * U8 收尾处置（计划 §11「UiShared 剩余成员处置决议」）：
 *  - s_itemGrid / s_itemGridDirty 迁入 item 域 LootGridSystem（MarkDirty/GetGrid）；
 *  - VisibleItemCache / HoveredItem 由帧作用域 WorldUiFrame 接管后删除；
 *  - s_globalFont / GlobalFont / SetGlobalFont 删除（UISystem 私有 static 规范持有，
 *    render 侧改由 GameplayRenderAdapter::SetFont 注入）；
 *  - Init/Shutdown 随网格迁移一并删除。
 * 本目标现仅保留无状态纯函数 GetRarityColor。
 */
class UiShared {
public:
  // 稀有度 -> 颜色查表（原 UIRenderer::GetRarityColor 实现迁入）。
  static Color GetRarityColor(Rarity rarity);
};

} // namespace NoMoreDay
