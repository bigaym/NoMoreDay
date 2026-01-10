#include "GPUParticleSystem.hpp"
#include "../utils/GPUUtils.hpp"
#include "rlgl.h"
#include "raymath.h"
#include "../tools/Logger.hpp"
#include <fstream>
#include <sstream>

namespace NoMoreDay::systems {

void GPUParticleSystem::Init(ResourceManager& rm, int maxParticles) {
    m_maxParticles = maxParticles;
    m_poolIndex = 0;
    m_stagedParticles.reserve(2048);
    m_useBufferA = true;

    // 1. Load Shaders
    std::ifstream f("assets/shaders/particle.compute");
    if (f.is_open()) {
        std::stringstream ss; ss << f.rdbuf();
        std::string shaderSource = ss.str();
        LOG_INFO("GPUParticleSystem: Loaded compute shader source ({} bytes)", shaderSource.size());
        unsigned int compId = rlCompileShader(shaderSource.c_str(), RL_COMPUTE_SHADER);
        if (compId == 0) {
            LOG_ERROR("GPUParticleSystem: Compute shader compilation FAILED!");
        } else {
            m_computeShader.id = rlLoadComputeShaderProgram(compId);
            if (m_computeShader.id == 0) {
                LOG_ERROR("GPUParticleSystem: Compute shader program linking FAILED!");
            } else {
                LOG_INFO("GPUParticleSystem: Compute shader loaded successfully (ID: {})", m_computeShader.id);
            }
        }
    } else {
        LOG_WARN("GPUParticleSystem: Failed to load assets/shaders/particle.compute - check working directory!");
    }

    m_renderShader = LoadShader("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    if (m_renderShader.id == 0) {
        LOG_ERROR("GPUParticleSystem: Render shader loading FAILED!");
    } else {
        LOG_INFO("GPUParticleSystem: Render shader loaded successfully (ID: {})", m_renderShader.id);
    }
    m_renderShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_renderShader, "mvp");

    // 2. SSBOs - Double Buffering
    size_t structSize = sizeof(components::GPUParticle);
    size_t bufferSize = m_maxParticles * structSize;
    std::vector<unsigned char> zeroData(bufferSize, 0);
    
    m_particleBufferA.Create(bufferSize, zeroData.data(), RL_DYNAMIC_DRAW);
    m_particleBufferB.Create(bufferSize, zeroData.data(), RL_DYNAMIC_DRAW);
    
    // 3. Atomic Counter Buffer
    uint32_t zeroCounter = 0;
    m_atomicCounter.Create(sizeof(uint32_t), &zeroCounter, RL_DYNAMIC_DRAW);

    // 4. VAO/VBO for Instancing
    float vertices[] = { 
        -0.5f, -0.5f, 
         0.5f, -0.5f, 
        -0.5f,  0.5f, 
        -0.5f,  0.5f, 
         0.5f, -0.5f, 
         0.5f,  0.5f 
    };
    m_quadVAO = rlLoadVertexArray();
    rlEnableVertexArray(m_quadVAO);
    m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlEnableVertexBuffer(m_quadVBO);
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();

    LOG_INFO("GPUParticleSystem: Refactored with Double-Buffering (Slot 10/11) initialized.");
}

void GPUParticleSystem::Update(float dt) {
    static int s_debugCounter = 0;
    bool doDebug = (++s_debugCounter % 120 == 1); // Log every ~2 seconds at 60fps

    if (m_computeShader.id == 0) {
        if (doDebug) {
            LOG_WARN("[GPUParticle] Compute shader not loaded! Particles won't be processed.");
        }
        return;
    }

    // 1. Reset atomic counter
    uint32_t zero = 0;
    m_atomicCounter.Update(&zero, sizeof(uint32_t));

    // 2. Dispatch Compute (process existing particles)
    rlEnableShader(m_computeShader.id);
    float clampedDt = (dt > 0.1f) ? 0.016f : dt;
    rlSetUniform(rlGetLocationUniform(m_computeShader.id, "dt"), &clampedDt, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(m_computeShader.id, "maxParticles"), &m_maxParticles, RL_SHADER_UNIFORM_INT, 1);
    
    core::ComputeBuffer& inputBuf = m_useBufferA ? m_particleBufferA : m_particleBufferB;
    core::ComputeBuffer& outputBuf = m_useBufferA ? m_particleBufferB : m_particleBufferA;
    
    inputBuf.BindBase(10);
    outputBuf.BindBase(11);
    m_atomicCounter.BindBase(12);
    
    rlComputeShaderDispatch((m_maxParticles + 255) / 256, 1, 1);
    utils::GPUUtils::MemoryBarrier();
    rlDisableShader();

    // 3. Read back alive count (after compute)
    int prevAlive = m_aliveCount;
    m_atomicCounter.Read(&m_aliveCount, sizeof(int));
    
    // 4. Upload NEW particles to the OUTPUT buffer (after the compacted alive particles)
    // This is critical: new particles go into the output buffer so they survive the swap!
    if (!m_stagedParticles.empty()) {
        int count = (int)m_stagedParticles.size();
        if (count > m_maxParticles - m_aliveCount) {
            count = m_maxParticles - m_aliveCount; // Don't overflow
        }
        
        if (count > 0) {
            size_t structSize = sizeof(components::GPUParticle);
            // Append new particles after the alive ones in the OUTPUT buffer
            outputBuf.Update(m_stagedParticles.data(), count * structSize, m_aliveCount * structSize);
            m_aliveCount += count; // Include new particles in alive count
            
            if (doDebug || count > 10) {
                LOG_INFO("[GPUParticle] Uploaded {} NEW particles after {} alive (total={})", 
                    count, prevAlive, m_aliveCount);
            }
        }
        m_stagedParticles.clear();
    }
    
    if (doDebug && m_aliveCount > 0) {
        LOG_INFO("[GPUParticle] aliveCount={} (prev={}), bufferA={}", m_aliveCount, prevAlive, m_useBufferA);
    }

    // 5. Swap Buffers - output becomes next frame's input
    m_useBufferA = !m_useBufferA;
}

void GPUParticleSystem::Render() {
    static int s_renderDebugCounter = 0;
    bool doDebug = (++s_renderDebugCounter % 120 == 1);
    
    if (m_aliveCount <= 0) {
        if (doDebug) LOG_DEBUG("[GPUParticle Render] No alive particles to render (aliveCount={})", m_aliveCount);
        return;
    }
    
    if (doDebug) {
        LOG_INFO("[GPUParticle Render] Rendering {} particles, shader={}, bufferA={}", 
            m_aliveCount, m_renderShader.id, m_useBufferA);
    }

    // TEMPORARY: CPU fallback rendering to diagnose shader issues
    // Read back particle data and render with simple circles
    core::ComputeBuffer& renderBuffer = m_useBufferA ? m_particleBufferA : m_particleBufferB;
    
    // Only read back a subset to avoid performance issues
    int readCount = std::min(m_aliveCount, 500);
    std::vector<components::GPUParticle> particles(readCount);
    renderBuffer.Read(particles.data(), readCount * sizeof(components::GPUParticle));
    
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < readCount; ++i) {
        const auto& p = particles[i];
        if (p.lifetime > 0.0f && p.scale > 0.1f) {
            float alpha = p.lifetime / p.maxLifetime;
            if (alpha > 1.0f) alpha = 1.0f;
            if (alpha < 0.0f) alpha = 0.0f;
            
            Color col = p.color;
            col.a = (unsigned char)(col.a * alpha);
            
            DrawCircleV({p.position.x, p.position.y}, p.scale, col);
        }
    }
    EndBlendMode();
    
    // Also try GPU rendering (may or may not work)
    if (m_renderShader.id != 0) {
        Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
        BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(m_renderShader);
            rlSetUniformMatrix(m_renderShader.locs[SHADER_LOC_MATRIX_MVP], mvp);
            renderBuffer.BindBase(11);
            rlEnableVertexArray(m_quadVAO);
            rlDrawVertexArrayInstanced(0, 6, m_aliveCount);
            rlDisableVertexArray();
        EndShaderMode();
        EndBlendMode();
    }
}

void GPUParticleSystem::Emit(const components::GPUParticle& p) {
    if (m_stagedParticles.size() < (size_t)m_maxParticles) {
        m_stagedParticles.push_back(p);
    }
}

void GPUParticleSystem::EmitBatch(const std::vector<components::GPUParticle>& particles) {
    for (const auto& p : particles) Emit(p);
}

void GPUParticleSystem::Shutdown() {
    m_particleBufferA.Release();
    m_particleBufferB.Release();
    m_atomicCounter.Release();
    rlUnloadShaderProgram(m_computeShader.id);
    UnloadShader(m_renderShader);
}

components::GPUParticle InkEffectHelper::CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life) {
    components::GPUParticle p;
    p.position = pos; p.velocity = vel; p.color = COLOR_INK_LIGHT;
    p.lifetime = life; p.maxLifetime = life; p.scale = scale; p.flags = 0; p.growthRate = 0.2f; return p;
}

std::vector<components::GPUParticle> InkEffectHelper::CreateInkSplash(Vector2 pos, int count, float radius, float force) {
    std::vector<components::GPUParticle> res;
    for (int i = 0; i < count; ++i) {
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float r = (float)GetRandomValue(0, (int)radius);
        components::GPUParticle p;
        p.position = { pos.x + cosf(angle) * r, pos.y + sinf(angle) * r };
        p.velocity = { cosf(angle) * force, sinf(angle) * force };
        p.color = COLOR_GOLD_CORE; p.scale = 10.0f; p.lifetime = 1.0f; p.maxLifetime = 1.0f;
        p.flags = 5; p.growthRate = 0.5f; res.push_back(p);
    }
    return res;
}

components::GPUParticle InkEffectHelper::CreateGoldParticle(Vector2 pos, Vector2 vel, float scale) {
    components::GPUParticle p;
    p.position = pos; p.velocity = vel; p.color = COLOR_GOLD_CORE;
    p.lifetime = 1.0f; p.maxLifetime = 1.0f; p.scale = scale; p.flags = 2; p.growthRate = -scale; return p;
}

} // namespace NoMoreDay::systems