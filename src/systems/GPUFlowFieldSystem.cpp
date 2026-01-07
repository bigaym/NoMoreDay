#include "GPUFlowFieldSystem.hpp"
#include "../utils/GPUUtils.hpp"
#include "rlgl.h"
#include "../tools/Logger.hpp"

namespace NoMoreDay::systems {

void GPUFlowFieldSystem::Init(ResourceManager& resources) {
    LOG_INFO("Initializing GPUFlowFieldSystem...");
    
    m_resetShader = resources.loadComputeShader(entt::hashed_string{"flow_reset"}, "assets/shaders/flow_reset.compute");
    m_flowShader = resources.loadComputeShader(entt::hashed_string{"flow_vector"}, "assets/shaders/flow_vector.compute");
    m_integrationShader = resources.loadComputeShader(entt::hashed_string{"flow_integration"}, "assets/shaders/flow_integration.compute");

    // In Raylib, to get a float texture, we might need a different approach or raw gl
    // For now, let's use a standard RGBA texture and encode vectors
    Image img = GenImageColor(m_width > 0 ? m_width : 128, m_height > 0 ? m_height : 128, BLANK);
    m_flowTexture = LoadTextureFromImage(img).id;
    UnloadImage(img);
    
    LOG_INFO("GPUFlowFieldSystem initialized.");
}

void GPUFlowFieldSystem::Update(const std::vector<unsigned char>& costMap, int width, int height, Vector2 targetPos) {
    // 1. Reset
    rlEnableShader(m_resetShader.id);
    rlComputeShaderDispatch((width + 15)/16, (height + 15)/16, 1);
    utils::GPUUtils::MemoryBarrier();

    // 2. Vector Field (e.g. towards player)
    rlEnableShader(m_flowShader.id);
    int locTarget = rlGetLocationUniform(m_flowShader.id, "targetPos");
    rlSetUniform(locTarget, &targetPos, RL_SHADER_UNIFORM_VEC2, 1);
    rlComputeShaderDispatch((width + 15)/16, (height + 15)/16, 1);
    utils::GPUUtils::MemoryBarrier();

    // 3. Integration/Smoothing
    rlEnableShader(m_integrationShader.id);
    rlComputeShaderDispatch((width + 15)/16, (height + 15)/16, 1);
    utils::GPUUtils::MemoryBarrier();
    
    rlDisableShader();
}

void GPUFlowFieldSystem::Shutdown() {
    LOG_INFO("Shutting down GPUFlowFieldSystem...");
    if (m_flowTexture > 0) {
        rlUnloadTexture(m_flowTexture);
    }
}

} // namespace NoMoreDay::systems
