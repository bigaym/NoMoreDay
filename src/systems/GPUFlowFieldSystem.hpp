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
    
    // Upload cost map and compute flow field towards targetPos
    // costMap: 0 for walkable, 255 for wall (size must match width*height)
    void Update(const std::vector<unsigned char>& costMap, Vector2 targetPos, Vector2 gridOrigin);
    
    void Shutdown();

    // Accessors for Debugging
    const core::ComputeBuffer& GetFlowBuffer() const { return m_flowBuffer; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    Vector2 GetGridOrigin() const { return m_gridOrigin; }

    // Debugging
    bool m_debugDraw = false;
    std::vector<Vector2> DownloadFlowField() const;

private:
    GPUFlowFieldSystem() = default;

    Shader m_integrationShader;
    Shader m_flowShader;
    Shader m_resetShader;

    // Buffers (SSBOs)
    core::ComputeBuffer m_costBuffer;        // uint8_t[] (Layout: std430 requires alignment, careful!)
                                             // Actually, std430 array of scalars has stride = size.
                                             // uint8 is not valid GLSL type for buffer directly? uint is min 32-bit?
                                             // GLSL `uint` is 32-bit. We can use `uint` and pack 4 costs, or just use `uint` per cell.
                                             // Using `uint` per cell (0-255 value) is safe and aligned.
    
    core::ComputeBuffer m_integrationBuffer;  // Ping
    core::ComputeBuffer m_integrationBuffer2; // Pong
    core::ComputeBuffer m_flowBuffer;        // Vector2[] (Flow Direction)
    
    int m_width = 0;
    int m_height = 0;
    Vector2 m_gridOrigin = {0, 0};
};

} // namespace NoMoreDay::systems
