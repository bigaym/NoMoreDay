#include "FogOfWarSystem.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>
#include <cmath>

FogOfWarSystem::FogOfWarSystem() : m_width(0), m_height(0), m_textureValid(false) {
    m_fogTexture.id = 0;
}

FogOfWarSystem::~FogOfWarSystem() {
    if (m_textureValid && m_fogTexture.id != 0) {
        UnloadTexture(m_fogTexture);
    }
}

void FogOfWarSystem::initialize(int width, int height) {
    initData(width, height);
    initTexture();
}

void FogOfWarSystem::initData(int width, int height) {
    m_width = width;
    m_height = height;
    m_visibilityGrid.assign(width * height, VISIBILITY_UNEXPLORED);
    // Texture logic moved to initTexture
}

void FogOfWarSystem::initTexture() {
    if (m_textureValid && m_fogTexture.id != 0) {
        UnloadTexture(m_fogTexture);
    }
    
    Image fogImage = GenImageColor(m_width, m_height, BLACK);
    m_fogTexture = LoadTextureFromImage(fogImage);
    UnloadImage(fogImage);
    m_textureValid = true;
    
    LOG_INFO("Initialized fog of war texture for {}x{} map", m_width, m_height);
}

void FogOfWarSystem::updateVisibility(const Position& playerPos, float viewRadius) {
    if (m_width == 0 || m_height == 0) return;
    
    // 转换像素坐标到网格坐标 (假设瓦片大小为 10.0f)
    
    // 计算玩家周围的可见区域
    int playerGridX = static_cast<int>(playerPos.x / TILE_SIZE);
    int playerGridY = static_cast<int>(playerPos.y / TILE_SIZE);
    // 使用 ceil 确保覆盖完整半径，并增加缓冲以匹配玩家实际视觉范围
    int gridRadius = static_cast<int>(std::ceil(viewRadius / TILE_SIZE)) + 2;
    
    int minX = std::max(0, playerGridX - gridRadius);
    int maxX = std::min(m_width - 1, playerGridX + gridRadius);
    int minY = std::max(0, playerGridY - gridRadius);
    int maxY = std::min(m_height - 1, playerGridY + gridRadius);
    
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float dist = std::sqrt(static_cast<float>((x - playerGridX) * (x - playerGridX) + 
                                 (y - playerGridY) * (y - playerGridY)));
            
            if (dist <= gridRadius) {
                // 设置为可见
                size_t index = y * m_width + x;
                if (index < m_visibilityGrid.size()) {
                    m_visibilityGrid[index] = VISIBILITY_VISIBLE;
                }
            }
        }
    }
    
    // 更新雾纹理
    updateFogTexture();
}

void FogOfWarSystem::renderFog() const {
    if (m_textureValid && m_fogTexture.id != 0) {
        float scale = TILE_SIZE;
        float mapPixelW = m_width * scale;
        float mapPixelH = m_height * scale;

        // 1. 绘制地图外的黑色背景 (处理"只有左上角有迷雾"的问题)
        // 我们绘制四个巨大的矩形来覆盖地图边界以外的区域
        DrawRectangle(-5000, -5000, mapPixelW + 10000, 5000, BLACK);          // Top
        DrawRectangle(-5000, mapPixelH, mapPixelW + 10000, 5000, BLACK);      // Bottom
        DrawRectangle(-5000, 0, 5000, mapPixelH, BLACK);                      // Left
        DrawRectangle(mapPixelW, 0, 5000, mapPixelH, BLACK);                  // Right

        // 缩放 10 倍以匹配瓦片大小 (10x10)
        DrawTextureEx(m_fogTexture, {0.0f, 0.0f}, 0.0f, scale, Fade(BLACK, FOG_ALPHA));
    }
}

bool FogOfWarSystem::isVisible(int x, int y) const {
    return getVisibility(x, y) >= VISIBILITY_VISIBLE;
}

bool FogOfWarSystem::isExplored(int x, int y) const {
    return getVisibility(x, y) >= VISIBILITY_EXPLORED;
}

uint8_t FogOfWarSystem::getVisibility(int x, int y) const {
    if (isValidCoordinate(x, y)) {
        size_t index = y * m_width + x;
        if (index < m_visibilityGrid.size()) {
            return m_visibilityGrid[index];
        }
    }
    return VISIBILITY_UNEXPLORED;
}

void FogOfWarSystem::setVisibility(int x, int y, uint8_t visibility) {
    if (isValidCoordinate(x, y)) {
        size_t index = y * m_width + x;
        if (index < m_visibilityGrid.size()) {
            m_visibilityGrid[index] = std::max(m_visibilityGrid[index], visibility);
        }
    }
}

bool FogOfWarSystem::isValidCoordinate(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

void FogOfWarSystem::updateFogTexture() {
    if (!m_textureValid || m_fogTexture.id == 0) {
        return;
    }
    
    // 创建图像数据用于更新纹理
    Image fogImage = GenImageColor(m_width, m_height, BLACK);
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            uint8_t visibility = getVisibility(x, y);
            Color color = BLACK;
            
            switch (visibility) {
                case VISIBILITY_UNEXPLORED:
                    // 未探索 - 完全黑色
                    color = BLACK;
                    break;
                case VISIBILITY_EXPLORED:
                    // 已探索 - 深灰色
                    color = Fade(GRAY, 0.2f);
                    break;
                case VISIBILITY_VISIBLE:
                    // 可见 - 透明（通过渲染时的混合实现）
                    color = BLANK; // Raylib中的透明色
                    break;
            }
            
            ImageDrawPixel(&fogImage, x, y, color);
        }
    }
    
    // 更新纹理
    UpdateTexture(m_fogTexture, fogImage.data);
    UnloadImage(fogImage);
}

void FogOfWarSystem::floodFillExplore(int startX, int startY) {
    if (!isValidCoordinate(startX, startY) || isVisible(startX, startY)) {
        return;
    }
    
    std::vector<std::pair<int, int>> stack;
    stack.emplace_back(startX, startY);
    
    while (!stack.empty()) {
        auto [x, y] = stack.back();
        stack.pop_back();
        
        if (!isValidCoordinate(x, y) || isVisible(x, y)) {
            continue;
        }
        
        // 设置为已探索
        setVisibility(x, y, VISIBILITY_EXPLORED);
        
        // 添加相邻格子
        stack.emplace_back(x + 1, y);
        stack.emplace_back(x - 1, y);
        stack.emplace_back(x, y + 1);
        stack.emplace_back(x, y - 1);
    }
}