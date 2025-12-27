#pragma once

#include "Common.hpp"
#include <vector>
#include <cstdint>

// 地图瓦片类型
struct Tile {
    enum class Type : uint8_t { WALL, FLOOR, DOOR, STAIRS_UP, STAIRS_DOWN };
    
    Type type = Type::WALL;
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
    uint8_t visibilityLevel;  // 0=未探索, 1=已探索, 2=可见
    
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