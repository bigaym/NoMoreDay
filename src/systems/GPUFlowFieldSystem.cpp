#include "GPUFlowFieldSystem.hpp"
#include "glad.h"
#include "../tools/Logger.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::systems {

void GPUFlowFieldSystem::Init(ResourceManager& rm) {
    // Assume max size or match map size
    m_width = 5000 / 32 + 1; // ~157
    m_height = 5000 / 32 + 1;

    LOG_INFO("Initializing GPUFlowFieldSystem ({}x{})", m_width, m_height);

    m_integrationShader = rm.loadComputeShader(entt::hashed_string{"flow_integration"}, "assets/shaders/flow_integration.compute");
    m_flowShader = rm.loadComputeShader(entt::hashed_string{"flow_vector"}, "assets/shaders/flow_vector.compute");
    m_resetShader = rm.loadComputeShader(entt::hashed_string{"flow_reset"}, "assets/shaders/flow_reset.compute");

    // 1. Cost Texture (R8UI)
    glGenTextures(1, &m_costTexture);
    glBindTexture(GL_TEXTURE_2D, m_costTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, m_width, m_height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 2. Integration Texture (R32UI)
    glGenTextures(1, &m_integrationTexture);
    glBindTexture(GL_TEXTURE_2D, m_integrationTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, m_width, m_height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 3. Flow Texture (RG16F)
    glGenTextures(1, &m_flowTexture);
    glBindTexture(GL_TEXTURE_2D, m_flowTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16F, m_width, m_height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GPUFlowFieldSystem::Update(const std::vector<unsigned char>& costMap, int width, int height, Vector2 targetPos) {
    if (m_integrationShader.id == 0) return;

    // Ensure size matches or handle resize (omitted for simplicity, assuming fixed max size)
    
    // 1. Upload Cost Map
    // Only upload if changed? For now upload every frame or when dirty.
    glBindTexture(GL_TEXTURE_2D, m_costTexture);
    // Note: glPixelStorei(GL_UNPACK_ALIGNMENT, 1) might be needed if width is not multiple of 4
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, costMap.data());

    // 2. Reset Integration Field
    int tx = (int)(targetPos.x / 32.0f);
    int ty = (int)(targetPos.y / 32.0f);
    
    glUseProgram(m_resetShader.id);
    glUniform2i(glGetUniformLocation(m_resetShader.id, "targetCoord"), tx, ty);
    glBindImageTexture(0, m_integrationTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    glDispatchCompute((width + 15)/16, (height + 15)/16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 3. Iterative Integration (Wavefront)
    glUseProgram(m_integrationShader.id);
    glBindImageTexture(0, m_integrationTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(1, m_costTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
    
    // Iterate enough times to propagate across the screen
    // 40 iterations covers ~40 tiles radius (1280 pixels).
    for(int i=0; i<40; i++) { 
        glDispatchCompute((width + 15)/16, (height + 15)/16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // 4. Generate Flow Vectors
    glUseProgram(m_flowShader.id);
    glBindImageTexture(0, m_integrationTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    glBindImageTexture(2, m_flowTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    glDispatchCompute((width + 15)/16, (height + 15)/16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    
    glUseProgram(0);
}

void GPUFlowFieldSystem::Shutdown() {
    if (m_costTexture) glDeleteTextures(1, &m_costTexture);
    if (m_integrationTexture) glDeleteTextures(1, &m_integrationTexture);
    if (m_flowTexture) glDeleteTextures(1, &m_flowTexture);
}

} // namespace NoMoreDay::systems
