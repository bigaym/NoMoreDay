#pragma once
#include "game/components/Common.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include <array>
#include <entt/entt.hpp>
#include <string>

namespace NoMoreDay {

/**
 * @brief 用于持久化的碎片快照
 */
struct FragmentSnapshot {
  bool hasFragment = false;
  FragmentElement element = FragmentElement::None;
  FragmentType type = FragmentType::Terrain;
  Rarity rarity = Rarity::Common;
  float enemyDensityMod = 1.0f;
  int monsterLevelMod = 0;
  int remainingLayers = 3;
  std::string name = "";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FragmentSnapshot, 
    hasFragment, element, type, rarity, 
    enemyDensityMod, monsterLevelMod, remainingLayers, name)

/**
 * @brief 3x3 拼图网格数据结构
 */
struct MosaicGrid {
  static constexpr int SIZE = 3;
  static constexpr int TOTAL_CELLS = SIZE * SIZE;

  // 9 个格子，每个可以放置一个碎片实体 (null = 空)
  std::array<entt::entity, TOTAL_CELLS> cells = {
      entt::null, entt::null, entt::null, entt::null, entt::null,
      entt::null, entt::null, entt::null, entt::null};

  // 共鸣计算结果缓存 (每个格子的乘数)
  std::array<float, TOTAL_CELLS> resonanceMultipliers = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

  // 二维坐标转一维索引
  static constexpr int ToIndex(int x, int y) { return y * SIZE + x; }

  // 一维索引转二维坐标
  static constexpr void ToCoord(int index, int &x, int &y) {
    x = index % SIZE;
    y = index / SIZE;
  }

  // 检查坐标是否有效
  static constexpr bool IsValidPos(int x, int y) {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
  }

  // 获取指定坐标的碎片实体
  entt::entity GetCell(int x, int y) const {
    return IsValidPos(x, y) ? cells[ToIndex(x, y)] : entt::null;
  }

  // 设置指定坐标的碎片实体
  void SetCell(int x, int y, entt::entity fragment) {
    if (IsValidPos(x, y)) {
      cells[ToIndex(x, y)] = fragment;
    }
  }

  // 移除指定坐标的碎片
  entt::entity RemoveCell(int x, int y) {
    if (!IsValidPos(x, y))
      return entt::null;
    entt::entity removed = cells[ToIndex(x, y)];
    cells[ToIndex(x, y)] = entt::null;
    return removed;
  }

  // 清空网格
  void Clear() {
    cells.fill(entt::null);
    resonanceMultipliers.fill(1.0f);
  }

  // 计算已填充的格子数量
  int GetFilledCount() const {
    int count = 0;
    for (auto e : cells) {
      if (e != entt::null)
        ++count;
    }
    return count;
  }

  // 检查网格是否为空
  bool IsEmpty() const { return GetFilledCount() == 0; }

  // 检查网格是否已满
  bool IsFull() const { return GetFilledCount() == TOTAL_CELLS; }

  /**
   * @brief 清理无效实体 (防止悬空实体导致的崩溃)
   * @param registry 实体注册表
   */
  void ValidateEntities(const entt::registry &registry) {
    for (auto &entity : cells) {
      if (entity != entt::null && !registry.valid(entity)) {
        entity = entt::null;
      }
    }
  }
};

/**
 * @brief 共鸣计算结果
 */
struct ResonanceResult {
  // 4字节成员
  float totalEnemyDensity = 1.0f;
  float totalDropRate = 1.0f;
  int totalLevelMod = 0;
  int resonanceChainCount = 0;

  // 1字节成员
  FragmentElement dominantElement = FragmentElement::None;
  NoMoreDay::BiomeID primaryBiome = NoMoreDay::BiomeID::Cave;
  bool hasBoss = false;
  bool hasMerchant = false;
  bool hasTreasure = false;
  bool isPerfectResonance = false;

  // 获取共鸣描述文字
  std::string GetSummary() const {
    std::string summary;
    summary += "Density: ";
    summary += std::to_string(static_cast<int>(totalEnemyDensity * 100));
    summary += "%\n";
    summary += "Drop Rate: ";
    summary += std::to_string(static_cast<int>(totalDropRate * 100));
    summary += "%\n";
    if (totalLevelMod != 0) {
      summary += "Monster Level: ";
      if (totalLevelMod > 0)
        summary += "+";
      summary += std::to_string(totalLevelMod);
      summary += "\n";
    }
    if (resonanceChainCount > 0) {
      summary += "Resonance Chains: ";
      summary += std::to_string(resonanceChainCount);
      summary += "\n";
    }
    if (isPerfectResonance) {
      summary += "[Perfect Resonance] x2.0 Bonus!\n";
    }
    return summary;
  }
};

/**
 * @brief 维度拼接状态组件
 */
struct MosaicProgressComponent {
  MosaicGrid currentGrid;
  int completedLayers = 0;
  bool isEditorOpen = false;
};

} // namespace NoMoreDay
