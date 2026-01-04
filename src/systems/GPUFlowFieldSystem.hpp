#pragma once
#include "../core/ResourceManager.hpp"
#include "raylib.h"
#include <vector>

namespace NoMoreDay::systems {

class GPUFlowFieldSystem {
public:
    static GPUFlowFieldSystem& Get() {
        static GPUFlowFieldSystem instance;
        return instance;
    }

    void Init(ResourceManager& rm);
    
    // Upload cost map and compute flow field towards targetPos
    // costMap: 0 for walkable, 255 for wall
    void Update(const std::vector<unsigned char>& costMap, int width, int height, Vector2 targetPos);
    
    unsigned int GetFlowTexture() const { return m_flowTexture; }

    void Shutdown();

private:
    GPUFlowFieldSystem() = default;

    Shader m_integrationShader;
    Shader m_flowShader;
    Shader m_resetShader;

    unsigned int m_costTexture = 0;
    unsigned int m_integrationTexture = 0;
    unsigned int m_flowTexture = 0;
    
    int m_width = 0;
    int m_height = 0;
};

} // namespace NoMoreDay::systems
