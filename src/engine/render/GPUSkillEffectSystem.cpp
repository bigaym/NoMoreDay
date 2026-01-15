#include "engine/render/GPUSkillEffectSystem.hpp"
#include "core/logging/Logger.hpp"
#include "rlgl.h"

namespace NoMoreDay::systems {

void GPUSkillEffectSystem::Init(ResourceManager& rm, int maxEffects) {
    if (m_shader.id != 0) return; // Prevent double init
    
    m_maxEffects = maxEffects;
    LOG_INFO("Initializing GPUSkillEffectSystem with max {} effects...", maxEffects);

    // Pre-allocate host buffer
    m_hostBuffer.resize(m_maxEffects);
    m_currentCount = 0;

    // Create GPU Buffer (SSBO)
    // Dynamic Draw because we update it every frame
    m_gpuBuffer.Create(m_maxEffects * sizeof(components::GPUSkillEffect), nullptr, RL_DYNAMIC_DRAW);

    // Load Shader via ResourceManager? Or helper? 
    // GPUEntitySystem uses direct LoadShader or ResourceManager.
    // Let's use direct LoadShader for now or ResourceManager if available.
    // The plan said: src/assets/shaders/sh_skill_effect.fs
    // We'll trust the path.
    m_shader = LoadShader("assets/shaders/sh_skill_effect.vs", "assets/shaders/sh_skill_effect.fs");
    
    // Get Locations
    m_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_shader, "mvp");

    InitRender();
}

void GPUSkillEffectSystem::InitRender() {
    // Setup Quad (2 Triangles, 6 vertices)
    float vertices[] = {
        // Triangle 1
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
        // Triangle 2
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    m_quadVAO = rlLoadVertexArray();
    rlEnableVertexArray(m_quadVAO);
    {
        m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
        rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(0);
    }
    rlDisableVertexArray();
}

void GPUSkillEffectSystem::Submit(const components::GPUSkillEffect& effect) {
    if (m_currentCount < m_maxEffects) {
        m_hostBuffer[m_currentCount] = effect;
        m_currentCount++;
    }
}

void GPUSkillEffectSystem::Render(const Camera2D& camera) {
    if (m_currentCount == 0) return;
    if (m_shader.id == 0) return;

    // 1. Upload Data
    // We only update the part we used
    m_gpuBuffer.Update(m_hostBuffer.data(), m_currentCount * sizeof(components::GPUSkillEffect));

    // 2. Setup MVP
    // We need to construct MVP manually or use rlgl
    rlDrawRenderBatchActive(); // Flush previous batch to ensure state is clean
    
    Matrix mvp = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    Matrix finalMvp = MatrixMultiply(mvp, projection);

    // 3. Render Instanced
    // 3. Render Instanced with State Handling
    BeginBlendMode(BLEND_ALPHA); // Enable alpha blending
    rlDisableDepthTest();        // 2D Overlay
    rlDisableBackfaceCulling();  // Ensure visibility
    
    rlEnableShader(m_shader.id);
    rlSetUniformMatrix(m_shader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);
    
    // Bind SSBO to Binding 5 (matches shader layout(binding=5))
    m_gpuBuffer.BindBase(5);
    
    rlEnableVertexArray(m_quadVAO);
    // Draw 6 vertices (2 triangles) * instance count
    rlDrawVertexArrayInstanced(0, 6, m_currentCount);
    rlDisableVertexArray();
    
    rlDisableShader();
    // Do NOT enable BackfaceCulling or DepthTest here. 
    // We are in a 2D render pass (BeginMode2D), so these should remain disabled.
    // Enabling them corrupts the state for subsequent 2D rendering (e.g. FogOfWar).
    EndBlendMode();

    // Reset for next frame
    m_currentCount = 0;
}

void GPUSkillEffectSystem::Shutdown() {
    LOG_INFO("Shutting down GPUSkillEffectSystem...");
    m_gpuBuffer.Release();
    UnloadShader(m_shader);
    rlUnloadVertexArray(m_quadVAO);
    rlUnloadVertexBuffer(m_quadVBO);
    
    m_shader.id = 0;
    m_quadVAO = 0;
    m_quadVBO = 0;
    m_maxEffects = 0;
    m_currentCount = 0;
    m_hostBuffer.clear();
}

} // namespace NoMoreDay::systems
