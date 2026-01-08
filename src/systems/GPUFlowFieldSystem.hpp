#pragma once
#include "../core/ResourceManager.hpp"
#include "../core/ComputeBuffer.hpp"
#include "raylib.h"
#include <vector>

namespace NoMoreDay::systems {

class GPUFlowFieldSystem {
public:
    static GPUFlowFieldSystem& Get() {
        static GPUFlowFieldSystem instance;
        return instance;
    }

    void Init(ResourceManager& rm, int width, int height);
    
    // Update with full cost map and target position
    // Will extract a window of m_width * m_height based on gridOrigin
    void Update(const std::vector<unsigned char>& fullCostMap, int mapW, int mapH, Vector2 targetPos, Vector2 gridOrigin);
    
    // Update crowd density from GPU entity buffer
    void UpdateCrowdDensity(unsigned int entityBufferId, int entityCount, float cellSize);

    void Shutdown();

    // Accessors for Debugging
    const core::ComputeBuffer& GetFlowBuffer() const { return m_flowBuffer; }
    const core::ComputeBuffer& GetDensityBuffer() const { return m_densityBuffer; }
    const core::ComputeBuffer& GetIntegrationBuffer() const { return m_integrationBuffer; }
    const core::ComputeBuffer& GetCostBuffer() const { return m_costBuffer; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    Vector2 GetGridOrigin() const { return m_gridOrigin; }

    // Density Weight for Flanking
    float m_densityWeight = 10.0f;

    // Debugging
    bool m_debugDraw = false;
    std::vector<Vector2> DownloadFlowField() const;

private:
    GPUFlowFieldSystem() = default;

    Shader m_integrationShader;
    Shader m_flowShader;
    Shader m_resetShader;
    Shader m_gridCountShader;
    Shader m_gridClearShader;

    // Buffers (SSBOs)
    core::ComputeBuffer m_costBuffer;        // uint32_t[] (Static obstacles)
    core::ComputeBuffer m_densityBuffer;     // uint32_t[] (Dynamic crowd density)
    
    core::ComputeBuffer m_integrationBuffer;  // Ping
    core::ComputeBuffer m_integrationBuffer2; // Pong
    core::ComputeBuffer m_flowBuffer;        // Vector2[] (Flow Direction)
    
    int m_width = 0;
    int m_height = 0;
    float m_cellSize = 10.0f;
    Vector2 m_gridOrigin = {0, 0};
};

} // namespace NoMoreDay::systems
