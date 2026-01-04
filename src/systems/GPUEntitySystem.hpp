#pragma once
#include <entt/entt.hpp>
#include <vector>
#include "../core/ComputeBuffer.hpp"
#include "../components/GPUData.hpp"
#include "../core/ResourceManager.hpp"

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
    static GPUEntitySystem& Get() {
        static GPUEntitySystem instance;
        return instance;
    }

    void Init(ResourceManager& rm, int maxEntities = 20000);
    
    void Update(entt::registry& registry, float dt);
    void SyncBack(entt::registry& registry);
    void Render(); // Render instanced entities

    void Shutdown();

private:
    GPUEntitySystem() = default;

    int m_maxEntities = 0;
    core::ComputeBuffer m_entityBuffer;
    
    // Grid Buffers
    core::ComputeBuffer m_cellCountBuffer;
    core::ComputeBuffer m_cellOffsetBuffer;
    core::ComputeBuffer m_entityIndicesBuffer; // Sorted entity IDs
    core::ComputeBuffer m_tempCountBuffer;
    
    std::vector<components::GPUEntity> m_localData;
    std::vector<uint32_t> m_gridCounts;
    std::vector<uint32_t> m_gridOffsets;

    // Compute Shaders
    Shader m_physicsShader;
    Shader m_gridClearShader;
    Shader m_gridCountShader;
    Shader m_gridSortShader;

    // Rendering
    Shader m_renderShader;
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    void InitRender(ResourceManager& rm);
};

} // namespace NoMoreDay::systems
