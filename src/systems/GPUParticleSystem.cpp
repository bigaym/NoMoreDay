#include "GPUParticleSystem.hpp"
#include "../utils/GPUUtils.hpp"
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
    m_particleBuffer.Create(maxParticles * sizeof(components::GPUParticle), initial.data(), RL_DYNAMIC_DRAW);
    
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

    // Use unified rlgl functions
    rlEnableShader(m_computeShader.id);
    
    int dtLoc = rlGetLocationUniform(m_computeShader.id, "dt");
    int maxLoc = rlGetLocationUniform(m_computeShader.id, "maxParticles");
    rlSetUniform(dtLoc, &dt, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(maxLoc, &m_maxParticles, RL_SHADER_UNIFORM_INT, 1);
    
    m_particleBuffer.BindBase(0);
    
    // Dispatch (Workgroup size 256 as defined in shader)
    int numGroups = (m_maxParticles + 255) / 256;
    rlComputeShaderDispatch(numGroups, 1, 1);
    
    // Barrier to ensure render can see the results
    utils::GPUUtils::MemoryBarrier();
    
    rlDisableShader();
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
}

// --- InkEffectHelper Implementation ---

components::GPUParticle InkEffectHelper::CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life) {
    components::GPUParticle p;
    p.position = pos;
    p.velocity = vel;
    p.acceleration = { 0.0f, 0.0f };
    p.color = COLOR_INK_LIGHT;
    p.lifetime = life;
    p.maxLifetime = life;
    p.scale = scale;
    // Flags: Bit 0-2 (Shape 5: Ink Splat), Bit 3 (Ink Fade: 8) -> 5 | 8 = 13
    p.flags = 13; 
    p.growthRate = 0.2f; // Slight spread
    return p;
}

std::vector<components::GPUParticle> InkEffectHelper::CreateInkSplash(Vector2 pos, int count, float radius, float force) {
    std::vector<components::GPUParticle> particles;
    particles.reserve(count);

    for (int i = 0; i < count; ++i) {
        components::GPUParticle p;
        
        // Random offset
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float dist = (float)GetRandomValue(0, 100) / 100.0f * radius;
        p.position = { pos.x + cosf(angle) * dist, pos.y + sinf(angle) * dist };
        
        // Outward velocity
        float speed = (float)GetRandomValue(50, 100) / 100.0f * force;
        p.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        
        p.acceleration = { 0.0f, 0.0f }; // Maybe gravity?
        
        // Mix of dark and light ink
        p.color = (GetRandomValue(0, 1) == 0) ? COLOR_INK_DARK : COLOR_INK_LIGHT;
        
        float life = 0.5f + (float)GetRandomValue(0, 50) / 100.0f; // 0.5 - 1.0s
        p.lifetime = life;
        p.maxLifetime = life;
        
        p.scale = 0.5f + (float)GetRandomValue(0, 50) / 100.0f;
        p.flags = 13; // Ink Splat + Fade
        p.growthRate = 0.8f; // Rapid spread
        
        particles.push_back(p);
    }
    return particles;
}

components::GPUParticle InkEffectHelper::CreateGoldParticle(Vector2 pos, Vector2 vel, float scale) {
    components::GPUParticle p;
    p.position = pos;
    p.velocity = vel;
    p.acceleration = { 0.0f, 0.0f };
    p.color = COLOR_GOLD_CORE;
    p.lifetime = 0.5f;
    p.maxLifetime = 0.5f;
    p.scale = scale;
    // Flags: Shape 2 (Spark) -> 2
    // Optional: Add Bit 3 (Ink Fade) -> 10? Or just linear fade. 
    // Let's use Spark shape with linear fade (default).
    p.flags = 2; 
    p.growthRate = -scale; // Shrink to zero over 1 sec (approx)
    return p;
}

} // namespace NoMoreDay::systems