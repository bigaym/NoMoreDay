#pragma once

#include "game/data/MosaicData.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay {

/**
 * @brief 拼图地图生成器
 *
 * 基于玩家放置的碎片生成地图。
 * 将 3x3 网格划分为 9 个区域，每个区域应用对应碎片的属性。
 */
class MosaicMapGenerator : public MapGenerator {
public:
  // 设置拼图网格和共鸣结果
  void SetMosaicData(const MosaicGrid &grid, const ResonanceResult &resonance,
                     entt::registry *registry);

  // 实现基类的生成方法
  MapData Generate(int width, int height, uint32_t seed, float wallProb,
                   int iterations) override;

private:
  MosaicGrid m_grid;
  ResonanceResult m_resonance;
  entt::registry *m_registry = nullptr;

  // 区域信息
  struct ZoneInfo {
    int startX, startY;
    int endX, endY;
    MapFragmentComponent *fragment = nullptr;
    float resonanceMultiplier = 1.0f;
  };

    // 计算区域划分
    std::array<ZoneInfo, MosaicGrid::TOTAL_CELLS> CalculateZones(int width, int height);

  // 生成单个区域
  void GenerateZone(std::vector<Tile> &grid, int mapWidth, int mapHeight,
                    const ZoneInfo &zone, uint32_t seed);

  // 应用碎片效果到区域
  void ApplyFragmentEffects(std::vector<Tile> &grid, int mapWidth,
                            const ZoneInfo &zone);

  // 平滑区域边界
  void SmoothZoneBorders(std::vector<Tile> &grid, int width, int height);

  // 放置特殊房间
  void PlaceSpecialRooms(std::vector<Tile> &grid, int width, int height,
                         uint32_t seed);

  // 确保连通性
  void EnsureConnectivity(std::vector<Tile> &grid, int width, int height);

  // 放置出口
  void PlaceExits(std::vector<Tile> &grid, int width, int height,
                  uint32_t seed);

  // 辅助方法
  void SmoothIteration(const std::vector<Tile> &src, std::vector<Tile> &dst,
                       int w, int h);
};

} // namespace NoMoreDay
