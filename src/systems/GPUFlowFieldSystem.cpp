#include "GPUFlowFieldSystem.hpp"
#include "../utils/GPUUtils.hpp"
#include "rlgl.h"
#include "../tools/Logger.hpp"

namespace NoMoreDay::systems {

void GPUFlowFieldSystem::Init(ResourceManager& resources, int width, int height) {
    LOG_INFO("Initializing GPUFlowFieldSystem ({}x{})...", width, height);
    
    m_width = width;
    m_height = height;

    m_resetShader = resources.loadComputeShader(entt::hashed_string{"flow_reset"}, "assets/shaders/flow_reset.compute");
    m_flowShader = resources.loadComputeShader(entt::hashed_string{"flow_vector"}, "assets/shaders/flow_vector.compute");
    m_integrationShader = resources.loadComputeShader(entt::hashed_string{"flow_integration"}, "assets/shaders/flow_integration.compute");

    size_t cellCount = (size_t)width * height;

    // 1. Cost Buffer (uint32_t for alignment)
    // Initialize with 255 (Wall)
    std::vector<uint32_t> initialCost(cellCount, 255);
    m_costBuffer.Create(cellCount * sizeof(uint32_t), initialCost.data(), RL_DYNAMIC_DRAW);

    // 2. Integration Buffers (uint32_t)
    // Initialize with max int
    std::vector<uint32_t> initialInt(cellCount, 0xFFFFFFFF);
    m_integrationBuffer.Create(cellCount * sizeof(uint32_t), initialInt.data(), RL_DYNAMIC_DRAW);
    m_integrationBuffer2.Create(cellCount * sizeof(uint32_t), initialInt.data(), RL_DYNAMIC_DRAW);

    // 3. Flow Buffer (Vector2)
    // Initialize with zero
    std::vector<Vector2> initialFlow(cellCount, {0.0f, 0.0f});
    m_flowBuffer.Create(cellCount * sizeof(Vector2), initialFlow.data(), RL_DYNAMIC_DRAW);
    
    LOG_INFO("GPUFlowFieldSystem buffers allocated.");
}

void GPUFlowFieldSystem::Update(const std::vector<unsigned char>& costMap, Vector2 targetPos, Vector2 gridOrigin) {
    if (m_width == 0 || m_height == 0) return;
    
    m_gridOrigin = gridOrigin;

    // 0. Upload Cost Map
    // Convert 8-bit cost to 32-bit buffer
    if (costMap.size() == (size_t)m_width * m_height) {
        std::vector<uint32_t> costInt(costMap.size());
        for (size_t i = 0; i < costMap.size(); ++i) {
            costInt[i] = (uint32_t)costMap[i];
        }
        m_costBuffer.Update(costInt.data(), costInt.size() * sizeof(uint32_t));
    } else {
        LOG_WARN("GPUFlowFieldSystem: Cost map size mismatch! Expected {}, Got {}", m_width * m_height, costMap.size());
    }

    // Bind Buffers to Bindings (Must match shader layout binding = X)
    // Binding 0: Cost (readonly)
    // Binding 1: Integration (readwrite)
    // Binding 2: Flow (writeonly)
    m_costBuffer.BindBase(0);
    m_integrationBuffer.BindBase(1);
    m_flowBuffer.BindBase(2);

    // 1. Reset Integration Field
    rlEnableShader(m_resetShader.id);
    
    Vector2 targetGrid = { targetPos.x - gridOrigin.x, targetPos.y - gridOrigin.y };
    int locW = rlGetLocationUniform(m_resetShader.id, "width");
    int locH = rlGetLocationUniform(m_resetShader.id, "height");
    int locTarget = rlGetLocationUniform(m_resetShader.id, "targetPos");
    
    int targetX = (int)targetGrid.x;
    int targetY = (int)targetGrid.y;
    int targetIVec[2] = { targetX, targetY };

    rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(locTarget, targetIVec, RL_SHADER_UNIFORM_IVEC2, 1);

    m_integrationBuffer.BindBase(1);
    rlComputeShaderDispatch((m_width + 15)/16, (m_height + 15)/16, 1);
    utils::GPUUtils::MemoryBarrier();

    // 2. Integration (Iterative Relaxation)
    rlEnableShader(m_integrationShader.id);
    locW = rlGetLocationUniform(m_integrationShader.id, "width");
    locH = rlGetLocationUniform(m_integrationShader.id, "height");
    rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);

    int passes = 256; 
    m_integrationBuffer.BindBase(1);
    
    for (int i = 0; i < passes; ++i) {
        rlComputeShaderDispatch((m_width + 15)/16, (m_height + 15)/16, 1);
        utils::GPUUtils::MemoryBarrier();
    }

    // 3. Vector Field Generation
    rlEnableShader(m_flowShader.id);
    locW = rlGetLocationUniform(m_flowShader.id, "width");
    locH = rlGetLocationUniform(m_flowShader.id, "height");
    rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
    
    rlComputeShaderDispatch((m_width + 15)/16, (m_height + 15)/16, 1);
    utils::GPUUtils::MemoryBarrier();
    
    rlDisableShader();
}

std::vector<Vector2> GPUFlowFieldSystem::DownloadFlowField() const {
    size_t cellCount = (size_t)m_width * m_height;
    std::vector<Vector2> flowData(cellCount);
    m_flowBuffer.Read(flowData.data(), cellCount * sizeof(Vector2));
    return flowData;
}

void GPUFlowFieldSystem::Shutdown() {
    LOG_INFO("Shutting down GPUFlowFieldSystem...");
    m_costBuffer.Release();
    m_integrationBuffer.Release();
    m_flowBuffer.Release();
}

} // namespace NoMoreDay::systems
