#pragma once

#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include <cstdint>
#include <vector>


// 地图瓦片类型
struct Tile {
  enum class Type : uint8_t { WALL, FLOOR, DOOR, STAIRS_UP, STAIRS_DOWN };

  Type type = Type::WALL;
  bool isAirWall = false; // Visual-only wall; still blocked by Tile::Type::WALL.
  uint8_t visibility = 0; // 0=未探索, 1=已探索, 2=可见 (合并了 bool isExplored)

  [[nodiscard]] constexpr bool isWalkable() const noexcept {
    return type != Type::WALL;
  }
};

// 地图瓦片组件 - 用于标记实体为地图瓦片
struct MapTileComponent {
  int gridX, gridY;
  Tile::Type tileType;

  MapTileComponent(int x = 0, int y = 0, Tile::Type type = Tile::Type::FLOOR)
      : gridX(x), gridY(y), tileType(type) {}
};

// 可见性组件 - 用于标记实体的可见性状态
struct VisibilityComponent {
  uint8_t visibilityLevel; // 0=未探索, 1=已探索, 2=可见

  VisibilityComponent(uint8_t level = 0) : visibilityLevel(level) {}
};

// 地图边界组件 - 用于标记地图边界实体
struct MapBoundaryComponent {
  bool isSolid;

  MapBoundaryComponent(bool solid = true) : isSolid(solid) {}
};

// 地图生成参数组件 - 用于存储地图生成参数
struct MapGenerationParams {
  int width;
  int height;
  float wallProbability;
  int smoothIterations;
  float connectivityThreshold;

  MapGenerationParams(int w = 128, int h = 128, float prob = 0.45f,
                      int iterations = 5, float threshold = 0.1f)
      : width(w), height(h), wallProbability(prob),
        smoothIterations(iterations), connectivityThreshold(threshold) {}
};

// 传送门类型枚举
enum class PortalType : uint8_t {
  Dungeon,  // 地下城入口/出口 (直接传送)
  Town,     // 回城门
  Boss,     // BOSS房入口
  Return,   // 返回门（双向）
  NextLevel, // 下一层入口 (触发维度拼接编辑器)
  DimensionalGate // 维度传送门 (在城镇中选择难度并开启)
};

// 待处理的维度传送门 UI 请求
struct PendingDimensionalGateTag {};

// 传送门组件
struct PortalComponent {
  PortalType type = PortalType::Dungeon;
  NoMoreDay::BiomeID targetBiome = NoMoreDay::BiomeID::None;
  int targetLevel = 1;
  std::string targetEntranceId = "start";
  bool isOneWay = false;
  bool isActive = true;

  // For Town Portal (return functionality)
  NoMoreDay::BiomeID originBiome = NoMoreDay::BiomeID::None; // 原始生物群系
  int originLevel = 0;                                       // 原始层数
  float originX = 0.0f;                                      // 原始X位置
  float originY = 0.0f;                                      // 原始Y位置

  // Visual animation
  float animationTimer = 0.0f; // 用于渲染动画
  float radius = 30.0f;        // 视觉半径
};

// 回城门施法状态组件
struct TownPortalCastingComponent {
  float castTime = 1.0f;    // 吟唱时间（用户要求改为1秒）
  float elapsedTime = 0.0f; // 已过时间
  bool isCasting = false;
  float castX = 0.0f; // 开始施法的位置X
  float castY = 0.0f; // 开始施法的位置Y
};

// 待处理的维度拼接编辑器请求 (由 PortalSystem 设置，GameplayState 处理)
// 待处理的维度拼接编辑器请求 (由 PortalSystem 设置，GameplayState 处理)
struct PendingMosaicEditorTag {};

// JSON Serialization
NLOHMANN_JSON_SERIALIZE_ENUM(PortalType, {
    {PortalType::Dungeon, "Dungeon"},
    {PortalType::Town, "Town"},
    {PortalType::Boss, "Boss"},
    {PortalType::Return, "Return"},
    {PortalType::NextLevel, "NextLevel"},
    {PortalType::DimensionalGate, "DimensionalGate"}
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortalComponent, 
    type, targetBiome, targetLevel, targetEntranceId, isOneWay, isActive,
    originBiome, originLevel, originX, originY,
    radius)
