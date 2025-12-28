#pragma once

#include <vector>
#include <cstdint>
#include "../components/Common.hpp"
#include "raylib.h"

class FogOfWarSystem {
private:
    std::vector<uint8_t> m_visibilityGrid;  // 0=未探索, 1=已探索, 2=可见
    Texture2D m_fogTexture;                 // 雾层纹理
    int m_width, m_height;
    bool m_textureValid;
    
    // 编译期常量
    static constexpr float FOG_ALPHA = 0.7f;
    static constexpr int VISIBILITY_UNEXPLORED = 0;
    static constexpr int VISIBILITY_EXPLORED = 1;
    static constexpr int VISIBILITY_VISIBLE = 2;

public:
    static constexpr float TILE_SIZE = 10.0f;
    FogOfWarSystem();
    ~FogOfWarSystem();
    
    // 初始化系统
    void initialize(int width, int height);
    
    // 更新可见性（基于玩家位置和视野半径）
    void updateVisibility(const Position& playerPos, float viewRadius);
    
    // 渲染战争迷雾
    void renderFog() const;
    
    // 检查位置可见性
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    uint8_t getVisibility(int x, int y) const;
    
    // 设置特定位置的可见性
    void setVisibility(int x, int y, uint8_t visibility);
    
    // 获取地图尺寸
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // 获取可见性网格引用（用于小地图）
    const std::vector<uint8_t>& getVisibilityGrid() const { return m_visibilityGrid; }

private:
    // 更新雾纹理
    void updateFogTexture();
    
    // 泛洪填充算法（用于探索新区域）
    void floodFillExplore(int startX, int startY);
    
    // 检查坐标是否在范围内
    bool isValidCoordinate(int x, int y) const;
};