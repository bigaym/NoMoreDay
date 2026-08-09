#pragma once
#include "game/foundation/data/MosaicData.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

/**
 * @brief 共鸣计算器
 *
 * 计算拼图网格中相邻碎片的共鸣效果。
 *
 * 共鸣规则：
 * 1. 相邻同元素碎片 → 各自获得 +25% 奖励
 * 2. 3个以上同元素连成一线 → 额外 +50% 奖励
 * 3. 所有9格同元素 → 触发"完美共鸣"，总奖励 ×2
 */
class ResonanceCalculator {
public:
  // 计算整个拼图的共鸣结果 (同时清理 grid 中的无效实体)
  static ResonanceResult Calculate(MosaicGrid &grid,
                                   entt::registry &registry);

  // 计算单个格子的共鸣乘数 (用于 UI 预览)
  static float CalculateCellResonance(const MosaicGrid &grid, int x, int y,
                                      entt::registry &registry);

private:
  // 检查两个碎片是否共鸣 (相同元素且都不为 None)
  static bool CanResonate(const MapFragmentComponent &a,
                          const MapFragmentComponent &b);

  // 获取相邻的碎片数量 (同元素)
  static int CountAdjacentSameElement(const MosaicGrid &grid, int x, int y,
                                      FragmentElement element,
                                      entt::registry &registry);

  // 检查是否形成完整的横线/竖线
  static bool CheckLineResonance(const MosaicGrid &grid,
                                 FragmentElement element,
                                 entt::registry &registry);

  // 4方向邻居偏移
  static constexpr int DX[4] = {0, 1, 0, -1};
  static constexpr int DY[4] = {-1, 0, 1, 0};

  // 共鸣常量
  static constexpr float ADJACENT_BONUS = 0.25f;    // 每个相邻同元素 +25%
  static constexpr float LINE_BONUS = 0.50f;        // 连成一线额外 +50%
  static constexpr float PERFECT_MULTIPLIER = 2.0f; // 完美共鸣 ×2
};

} // namespace NoMoreDay
