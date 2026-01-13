#include "game/data/ResonanceCalculator.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <unordered_map>

namespace NoMoreDay {

// 静态常量定义
constexpr int ResonanceCalculator::DX[4];
constexpr int ResonanceCalculator::DY[4];

ResonanceResult ResonanceCalculator::Calculate(MosaicGrid &grid,
                                               entt::registry &registry) {
  // 清理无效实体 (Side effect: cleaning the input grid)
  grid.ValidateEntities(registry);

  ResonanceResult result;

  // 统计元素分布
  std::unordered_map<FragmentElement, int> elementCounts;
  int totalFragments = 0;

  // 第一遍：收集所有碎片属性
  for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
    entt::entity entity = grid.cells[i];
    if (entity == entt::null)
      continue;

    auto *fragment = registry.try_get<MapFragmentComponent>(entity);
    if (!fragment)
      continue;

    ++totalFragments;
    elementCounts[fragment->element]++;

    // 累加基础属性 (乘法应用)
    result.totalEnemyDensity *= fragment->enemyDensityMod;
    result.totalDropRate *= fragment->dropRateMod;
    result.totalLevelMod += fragment->monsterLevelMod;

    // 特殊房间
    if (fragment->hasBoss)
      result.hasBoss = true;
    if (fragment->hasMerchant)
      result.hasMerchant = true;
    if (fragment->hasTreasure)
      result.hasTreasure = true;

    // 生物群系 (使用第一个非空的覆盖)
    if (!fragment->biomeOverride.empty() && result.primaryBiome == "cave") {
      result.primaryBiome = fragment->biomeOverride;
    }
  }

  if (totalFragments == 0) {
    return result; // 空网格，返回默认值
  }

  // 找出主导元素
  int maxCount = 0;
  for (const auto &[elem, count] : elementCounts) {
    if (elem != FragmentElement::None && count > maxCount) {
      maxCount = count;
      result.dominantElement = elem;
    }
  }

  // 第二遍：计算相邻共鸣加成 (加法加成)
  float additiveResonanceBonus = 0.0f;
  for (int y = 0; y < MosaicGrid::SIZE; ++y) {
    for (int x = 0; x < MosaicGrid::SIZE; ++x) {
      int index = MosaicGrid::ToIndex(x, y);
      entt::entity entity = grid.cells[index];
      if (entity == entt::null)
        continue;

      auto *fragment = registry.try_get<MapFragmentComponent>(entity);
      if (!fragment || fragment->element == FragmentElement::None)
        continue;

      int adjacentCount =
          CountAdjacentSameElement(grid, x, y, fragment->element, registry);
      if (adjacentCount > 0) {
        additiveResonanceBonus += (adjacentCount * ADJACENT_BONUS);
        ++result.resonanceChainCount;
      }
    }
  }

  // 检查横线/竖线共鸣
  if (result.dominantElement != FragmentElement::None &&
      CheckLineResonance(grid, result.dominantElement, registry)) {
    additiveResonanceBonus += LINE_BONUS;
    LOG_DEBUG("Line resonance bonus applied: +50%");
  }

  // 第三遍：应用加成顺序
  // 1. 先应用共鸣的百分比加成 (加法)
  result.totalDropRate *= (1.0f + additiveResonanceBonus);

  // 2. 最后检查完美共鸣 (所有9格同元素)
  if (totalFragments == MosaicGrid::TOTAL_CELLS &&
      maxCount == MosaicGrid::TOTAL_CELLS &&
      result.dominantElement != FragmentElement::None) {
    result.isPerfectResonance = true;
    
    // 完美共鸣是最终翻倍
    result.totalEnemyDensity *= PERFECT_MULTIPLIER;
    result.totalDropRate *= PERFECT_MULTIPLIER;
    
    LOG_INFO("Perfect Resonance achieved! Element: {}, final multiplier applied",
             static_cast<int>(result.dominantElement));
  }

  LOG_DEBUG("Resonance calculated: density={:.2f}, drop={:.2f}, chains={}",
            result.totalEnemyDensity, result.totalDropRate,
            result.resonanceChainCount);

  return result;
}

float ResonanceCalculator::CalculateCellResonance(const MosaicGrid &grid, int x,
                                                  int y,
                                                  entt::registry &registry) {
  if (!MosaicGrid::IsValidPos(x, y))
    return 1.0f;

  int index = MosaicGrid::ToIndex(x, y);
  entt::entity entity = grid.cells[index];
  if (entity == entt::null || !registry.valid(entity))
    return 1.0f;

  auto *fragment = registry.try_get<MapFragmentComponent>(entity);
  if (!fragment || fragment->element == FragmentElement::None)
    return 1.0f;

  int adjacentCount =
      CountAdjacentSameElement(grid, x, y, fragment->element, registry);
  return 1.0f + adjacentCount * ADJACENT_BONUS;
}

bool ResonanceCalculator::CanResonate(const MapFragmentComponent &a,
                                      const MapFragmentComponent &b) {
  if (a.element == FragmentElement::None ||
      b.element == FragmentElement::None) {
    return false;
  }
  return a.element == b.element;
}

int ResonanceCalculator::CountAdjacentSameElement(const MosaicGrid &grid, int x,
                                                  int y,
                                                  FragmentElement element,
                                                  entt::registry &registry) {
  int count = 0;

  for (int d = 0; d < 4; ++d) {
    int nx = x + DX[d];
    int ny = y + DY[d];

    if (!MosaicGrid::IsValidPos(nx, ny))
      continue;

    entt::entity neighbor = grid.GetCell(nx, ny);
    if (neighbor == entt::null || !registry.valid(neighbor))
      continue;

    auto *neighborFragment = registry.try_get<MapFragmentComponent>(neighbor);
    if (neighborFragment && neighborFragment->element == element) {
      ++count;
    }
  }

  return count;
}

bool ResonanceCalculator::CheckLineResonance(const MosaicGrid &grid,
                                             FragmentElement element,
                                             entt::registry &registry) {
  // 检查每一行
  for (int y = 0; y < MosaicGrid::SIZE; ++y) {
    bool fullRow = true;
    for (int x = 0; x < MosaicGrid::SIZE; ++x) {
      entt::entity entity = grid.GetCell(x, y);
      if (entity == entt::null || !registry.valid(entity)) {
        fullRow = false;
        break;
      }
      auto *fragment = registry.try_get<MapFragmentComponent>(entity);
      if (!fragment || fragment->element != element) {
        fullRow = false;
        break;
      }
    }
    if (fullRow)
      return true;
  }

  // 检查每一列
  for (int x = 0; x < MosaicGrid::SIZE; ++x) {
    bool fullCol = true;
    for (int y = 0; y < MosaicGrid::SIZE; ++y) {
      entt::entity entity = grid.GetCell(x, y);
      if (entity == entt::null || !registry.valid(entity)) {
        fullCol = false;
        break;
      }
      auto *fragment = registry.try_get<MapFragmentComponent>(entity);
      if (!fragment || fragment->element != element) {
        fullCol = false;
        break;
      }
    }
    if (fullCol)
      return true;
  }

  return false;
}

} // namespace NoMoreDay
