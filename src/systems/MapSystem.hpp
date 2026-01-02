#pragma once

#include <vector>
#include <random>
#include <memory>
#include <span>
#include "../components/Common.hpp"
#include "../components/MapComponent.hpp"
#include "SpatialGrid.hpp"
#include "raylib.h"

// 抽象地图生成器基类 (Strategy Pattern / Template Method)
class MapGenerator {
public:
    struct MapData {
        int width;
        int height;
        std::vector<Tile> grid; // 扁平化存储
    };
    
    virtual ~MapGenerator() = default;
    
    // 纯虚函数：生成地图逻辑
    virtual MapData Generate(int width, int height, uint32_t seed, float wallProb, int iterations) = 0;
};

// 具体实现：洞穴生成器
class CaveMapGenerator : public MapGenerator {
public:
    MapData Generate(int width, int height, uint32_t seed, float wallProb, int iterations) override;
private:
    void SmoothIteration(const std::vector<Tile>& src, std::vector<Tile>& dst, int w, int h);
    void ApplyBoundaries(std::vector<Tile>& grid, int w, int h);
    void GenerateObstacles(std::vector<Tile>& grid, int w, int h, uint32_t seed);
};

class MapSystem {
public:
    struct MapData {
        std::vector<Tile> grid; // 扁平化存储优化缓存
        int width = 0;
        int height = 0;
        
        Tile::Type getTile(int x, int y) const {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                return grid[y * width + x].type;
            }
            return Tile::Type::WALL; // 边界默认为墙
        }
        
        bool isWalkable(int x, int y) const {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                return grid[y * width + x].isWalkable();
            }
            return false;
        }
    };

private:
    MapData m_mapData;
    Texture2D m_fogTexture;
    bool m_fogTextureValid = false;
    std::string m_currentBiomeId = "cave";
    
    // 编译期常量
    static constexpr int SMOOTH_ITERATIONS = 5;
    
    // 随机数生成器（编译期种子）
    std::mt19937 m_gen;

    // --- 寻路相关 (Pathfinding & Flow Field) ---
    std::vector<Vector2> m_flowField;      // 指向目标的向量场 (用于群聚寻路)
    std::vector<int> m_distanceField;      // 距离场 (用于生成流场)
    Position m_lastFlowTarget = {-1, -1};  // 上次计算流场的目标

public:
    MapSystem();
    ~MapSystem();
    
    // 生成洞穴地图
    void generateCaveMap(int width, int height);
    
    // 生成指定生物群系的地图
    void generateMap(int width, int height, const std::string& biome = "cave");
    
    // 获取地图数据
    const MapData& getMapData() const { return m_mapData; }
    
    // 检查是否可行走
    bool isWalkable(int x, int y) const;
    
    // 获取瓦片类型
    Tile::Type getTileType(int x, int y) const;
    
    // 获取地图尺寸
    int getWidth() const { return m_mapData.width; }
    int getHeight() const { return m_mapData.height; }
    
    // 可见性管理
    void setVisibility(int x, int y, uint8_t visibility) {
        if (x >= 0 && x < m_mapData.width && y >= 0 && y < m_mapData.height) {
            m_mapData.grid[y * m_mapData.width + x].visibility = visibility;
        }
    }
    uint8_t getVisibility(int x, int y) const {
        if (x >= 0 && x < m_mapData.width && y >= 0 && y < m_mapData.height) {
            return m_mapData.grid[y * m_mapData.width + x].visibility;
        }
        return 0;
    }
    bool isVisible(int x, int y) const { return getVisibility(x, y) == 2; }
    bool isExplored(int x, int y) const { return getVisibility(x, y) >= 1; }
    
    // 更新可见性（基于玩家位置）
    void updateVisibility(const Position& playerPos, float viewRadius);
    
    // 渲染地图
    void render(const Camera2D& camera) const;
    
    // 渲染战争迷雾
    void renderFog(const Camera2D& camera) const;

    // --- 寻路方法 ---
    // 更新流场 (当目标移动超过一定距离时调用)
    void updateFlowField(const Position& targetPos);
    
    // 获取流场方向 (用于 CHASE 状态)
    Vector2 getFlowDirection(const Position& pos) const;
    
    // 获取 A* 路径的下一步 (用于 PATROL/RETURN 状态)
    Position getPathNextStep(const Position& start, const Position& end) const;

private:
    void ensureConnectivity(std::vector<Tile>& grid, int width, int height);
    
    // 连通性检查
    void floodFill(int startX, int startY, 
                   std::vector<bool>& visited,
                   std::vector<int>& regionMap,
                   int regionId);
    
    // 更新雾纹理
    void updateFogTexture();
    
    // 初始化雾纹理
    void initializeFogTexture(int width, int height);
};