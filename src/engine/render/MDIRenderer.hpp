#pragma once

#include "raylib.h"
#include "rlgl.h"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include <vector>
#include <cstdint>

namespace NoMoreDay::render {

// Matches generic InstanceData structure for MDI
struct alignas(16) GPUInstanceData {
    Vector2 position;      // 8 bytes
    Vector2 scale;         // 8 bytes (using Vector2 for independent xy scaling if needed, or x=scale, y=unused)
    float rotation;        // 4 bytes
    uint32_t textureIndex; // 4 bytes
    uint32_t flags;        // 4 bytes
    float _padding;        // 4 bytes to reach 32 bytes
};
static_assert(sizeof(GPUInstanceData) == 32, "GPUInstanceData must be 32 bytes");

struct DrawArraysIndirectCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t first;
    uint32_t baseInstance;
};

class MDIRenderer {
public:
    static MDIRenderer& Get() {
        static MDIRenderer instance;
        return instance;
    }

    // Initialize MDI renderer with resource manager and max entity capacity
    void Init(ResourceManager& rm, uint32_t maxEntities);

    // Upload instance data to GPU
    void UpdateInstances(const std::vector<GPUInstanceData>& data);

    // Perform GPU culling and command generation
    // viewBounds: x=minX, y=minY, z=maxX, w=maxY (Axis Aligned)
    void Cull(Vector4 viewBounds);

    // Execute indirect draw
    void Render(const Matrix& viewProj);

    // Shutdown and release resources
    void Shutdown();

    // Reset command buffer (instanceCount = 0)
    void ResetCommand();

    bool IsInitialized() const { return m_quadVAO != 0; }

private:
    MDIRenderer() = default;
    ~MDIRenderer();

    // No copy/move
    MDIRenderer(const MDIRenderer&) = delete;
    MDIRenderer& operator=(const MDIRenderer&) = delete;

    core::ComputeBuffer m_instanceBuffer;     // SSBO Binding 0
    core::ComputeBuffer m_visibleBuffer;      // SSBO Binding 1
    core::ComputeBuffer m_commandBuffer;      // SSBO Binding 2 (and Indirect Buffer)

    Shader m_cullShader;
    Shader m_renderShader;
    Shader m_resetShader; // Simple compute to reset counter if needed, or use separate kernel in cull

    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;
    uint32_t m_maxEntities = 0;
};

} // namespace NoMoreDay::render
