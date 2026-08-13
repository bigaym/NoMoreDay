#pragma once

#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include <array>
#include <cstdint>
#include <string_view>
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

// 场景入口标识（传送门/过渡的出生点身份）。
// 序列化以字符串形式保存以兼容旧存档（见下方 to_json/from_json）。
enum class EntranceId : uint8_t {
  Start,               // 默认出生点
  RiftResume,          // 裂缝续战：恢复到上次离开城镇时的位置
  RiftCompleteReturn,  // 裂缝完成后返回城镇
  Count
};

[[nodiscard]] constexpr std::string_view ToString(EntranceId id) noexcept {
  constexpr std::array<std::string_view, static_cast<size_t>(EntranceId::Count)>
      kEntranceIdNames = {"start", "rift_resume", "rift_complete_return"};
  const auto idx = static_cast<size_t>(id);
  return idx < kEntranceIdNames.size() ? kEntranceIdNames[idx] : "start";
}

[[nodiscard]] constexpr EntranceId FromString(std::string_view s) noexcept {
  if (s == "start")
    return EntranceId::Start;
  if (s == "rift_resume")
    return EntranceId::RiftResume;
  if (s == "rift_complete_return")
    return EntranceId::RiftCompleteReturn;
  // Unknown historical values behave like the default entrance.
  return EntranceId::Start;
}

// JSON boundary: keep the string form used by old saves.
inline void to_json(nlohmann::json &j, const EntranceId &id) {
  j = ToString(id);
}

inline void from_json(const nlohmann::json &j, EntranceId &id) {
  id = FromString(j.get<std::string>());
}

// 待处理的维度传送门 UI 请求
struct PendingDimensionalGateTag {};

// 传送门组件
struct PortalComponent {
  PortalType type = PortalType::Dungeon;
  NoMoreDay::BiomeID targetBiome = NoMoreDay::BiomeID::None;
  int targetLevel = 1;
  EntranceId targetEntranceId = EntranceId::Start;
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
