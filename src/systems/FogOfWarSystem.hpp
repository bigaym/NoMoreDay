#pragma once

#include <vector>
#include <cstdint>
#include "../components/Common.hpp"
#include "../core/ComputeBuffer.hpp"
#include "raylib.h"

class ResourceManager;

/**
 * @brief GPU-accelerated Fog of War System
 * 
 * Uses OpenGL 4.3 Compute Shaders for high-performance visibility updates.
 * Visibility grid and texture generation happen entirely on GPU.
 */
class FogOfWarSystem {
public:
    // 可见性状态 (必须与 fog_update.compute 中的常量匹配)
    static constexpr uint32_t VISIBILITY_UNEXPLORED = 0u;
    static constexpr uint32_t VISIBILITY_EXPLORED = 1u;
    static constexpr uint32_t VISIBILITY_VISIBLE = 2u;
    
    static constexpr float TILE_SIZE = 10.0f;
    static constexpr float FOG_ALPHA = 0.7f;

    FogOfWarSystem();
    ~FogOfWarSystem();
    
    // 禁止拷贝
    FogOfWarSystem(const FogOfWarSystem&) = delete;
    FogOfWarSystem& operator=(const FogOfWarSystem&) = delete;
    
    // 允许移动
    FogOfWarSystem(FogOfWarSystem&&) noexcept = default;
    FogOfWarSystem& operator=(FogOfWarSystem&&) noexcept = default;

    /**
     * @brief 初始化系统 (必须在主线程调用)
     * @param resources 资源管理器用于加载 Compute Shader
     * @param width 地图宽度 (格子数)
     * @param height 地图高度 (格子数)
     */
    void initialize(ResourceManager& resources, int width, int height);
    
    /**
     * @brief 基于玩家位置更新可见性 (GPU 计算)
     * @param playerPos 玩家世界坐标
     * @param viewRadius 视野半径 (像素)
     */
    void updateVisibility(const Position& playerPos, float viewRadius);
    
    /**
     * @brief 渲染战争迷雾
     */
    void renderFog() const;
    
    /**
     * @brief 检查位置是否可见 (需要从 GPU 读回数据, 较慢)
     */
    [[nodiscard]] bool isVisible(int x, int y) const;
    [[nodiscard]] bool isExplored(int x, int y) const;
    [[nodiscard]] uint32_t getVisibility(int x, int y) const;
    
    /**
     * @brief 强制设置特定位置的可见性
     */
    void setVisibility(int x, int y, uint32_t visibility);
    
    // 获取地图尺寸
    [[nodiscard]] int getWidth() const { return m_width; }
    [[nodiscard]] int getHeight() const { return m_height; }
    
    // 获取可见性网格引用 (用于小地图, 需要先同步)
    [[nodiscard]] const std::vector<uint32_t>& getVisibilityGrid() const { return m_cpuVisibilityCache; }
    
    /**
     * @brief 从 GPU 同步可见性数据到 CPU (用于小地图等需要 CPU 访问的场景)
     */
    void syncToCPU();
    
    /**
     * @brief 释放 GPU 资源
     */
    void shutdown();

private:
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    
    // GPU 资源
    NoMoreDay::core::ComputeBuffer m_visibilityBuffer;  // uint32_t 数组
    Shader m_fogShader;                                  // Compute Shader
    Texture2D m_fogTexture;                              // 输出纹理 (GPU 生成)
    
    // CPU 缓存 (用于 isVisible 等查询)
    mutable std::vector<uint32_t> m_cpuVisibilityCache;
    mutable bool m_cpuCacheDirty = true;
};