#include "GPUParticleSystem.hpp"
#include "../utils/GPUUtils.hpp"
#include "glad.h"
#include "rlgl.h"
#include "raymath.h"
#include "../tools/Logger.hpp"

namespace NoMoreDay::systems {

void GPUParticleSystem::Init(ResourceManager& rm, int maxParticles) {
    m_maxParticles = maxParticles;
    
    LOG_INFO("Initializing GPUParticleSystem with {} particles", maxParticles);

    // 1. Setup SSBO
    // Initializing with zero/inactive particles
    std::vector<components::GPUParticle> initial(maxParticles);
    for (auto& p : initial) {
        p.lifetime = 0.0f;
        p.scale = 0.0f;
    }
    m_particleBuffer.Create(maxParticles * sizeof(components::GPUParticle), initial.data(), core::BufferUsage::Dynamic);
    
    // 2. Load Shaders
    m_computeShader = rm.loadComputeShader(entt::hashed_string{"particle_update"}, "assets/shaders/particle.compute");
    m_renderShader = LoadShader("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    
    if (m_renderShader.id == 0) {
        LOG_ERROR("GPUParticleSystem: Failed to load render shader!");
    }
    
    m_renderShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_renderShader, "mvp");

    // 3. Setup Quad for instancing
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f
    };

    m_quadVAO = rlLoadVertexArray();
    rlEnableVertexArray(m_quadVAO);
    {
        m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
        rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(0);
    }
    rlDisableVertexArray();
    
    LOG_INFO("GPUParticleSystem initialized successfully.");
}

void GPUParticleSystem::Update(float dt) {
    if (m_computeShader.id == 0) return;

    // Dispatch Compute Shader
    glUseProgram(m_computeShader.id);
    
    int dtLoc = glGetUniformLocation(m_computeShader.id, "dt");
    int maxLoc = glGetUniformLocation(m_computeShader.id, "maxParticles");
    glUniform1f(dtLoc, dt);
    glUniform1i(maxLoc, m_maxParticles);
    
    m_particleBuffer.BindBase(0);
    
    // Dispatch (Workgroup size 256 as defined in shader)
    int numGroups = (m_maxParticles + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Barrier to ensure render can see the results
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(0);
}

void GPUParticleSystem::Render() {
    if (m_renderShader.id == 0) return;

    // Use standard raylib matrix
    Matrix mvp = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    Matrix finalMvp = MatrixMultiply(mvp, projection);

    BeginShaderMode(m_renderShader);
        SetShaderValueMatrix(m_renderShader, m_renderShader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);
        
        m_particleBuffer.BindBase(0);
        
        rlEnableVertexArray(m_quadVAO);
        // Draw instanced quads
        rlDrawVertexArrayInstanced(0, 4, m_maxParticles);
        rlDisableVertexArray();
    EndShaderMode();
}

void GPUParticleSystem::Emit(const components::GPUParticle& p) {
    if (m_maxParticles <= 0) return;
    // Basic circular buffer emission
    m_particleBuffer.Update(&p, sizeof(components::GPUParticle), m_poolIndex * sizeof(components::GPUParticle));
    
    m_poolIndex = (m_poolIndex + 1) % m_maxParticles;
}

void GPUParticleSystem::EmitBatch(const std::vector<components::GPUParticle>& particles) {
    if (particles.empty() || m_maxParticles <= 0) return;

    int count = (int)particles.size();
    if (count > m_maxParticles) count = m_maxParticles; // Cap at max

    int start = m_poolIndex;
    int end = (start + count) % m_maxParticles;

    if (end > start) {
        // One contiguous block
        m_particleBuffer.Update(particles.data(), count * sizeof(components::GPUParticle), start * sizeof(components::GPUParticle));
    } else {
        // Wrap around
        int firstChunk = m_maxParticles - start;
        int secondChunk = count - firstChunk;

        m_particleBuffer.Update(particles.data(), firstChunk * sizeof(components::GPUParticle), start * sizeof(components::GPUParticle));
        m_particleBuffer.Update(particles.data() + firstChunk, secondChunk * sizeof(components::GPUParticle), 0);
    }

    m_poolIndex = end;
}

void GPUParticleSystem::Shutdown() {
    LOG_INFO("Shutting down GPUParticleSystem...");
    if (m_quadVAO > 0) rlUnloadVertexArray(m_quadVAO);
    if (m_quadVBO > 0) rlUnloadVertexBuffer(m_quadVBO);
    if (m_renderShader.id > 0) UnloadShader(m_renderShader);
    // Compute shader is owned by ResourceManager
}

} // namespace NoMoreDay::systems
