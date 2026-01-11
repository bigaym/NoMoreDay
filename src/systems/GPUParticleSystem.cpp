#include "GPUParticleSystem.hpp"
#include "../tools/Logger.hpp"
#include "../utils/GPUUtils.hpp"
#include <rlgl.h>
#include <fstream>
#include <sstream>
#include <algorithm>

// For glfwGetProcAddress
#include <GLFW/glfw3.h>

// OpenGL function pointers for indirect drawing
typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(unsigned int mode, const void *indirect);
typedef void (*PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);

static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirectPtr = nullptr;
static PFNGLBINDBUFFERPROC glBindBufferPtr = nullptr;

// OpenGL constants
#define GL_TRIANGLES 0x0004
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F

namespace NoMoreDay::systems {

GPUParticleSystem& GPUParticleSystem::Get() {
    static GPUParticleSystem instance;
    return instance;
}

void GPUParticleSystem::Init(int maxParticles) {
    if (m_initialized) {
        LOG_WARN("GPUParticleSystem: Already initialized!");
        return;
    }
    
    m_maxParticles = maxParticles;
    LOG_INFO("Initializing GPUParticleSystem with {} max particles...", maxParticles);
    
    // Get OpenGL extension for indirect drawing using GLFW
    glDrawArraysIndirectPtr = (PFNGLDRAWARRAYSINDIRECTPROC)glfwGetProcAddress("glDrawArraysIndirect");
    glBindBufferPtr = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    
    if (!glDrawArraysIndirectPtr) {
        LOG_ERROR("GPUParticleSystem: glDrawArraysIndirect not available!");
        return;
    }
    
    if (!glBindBufferPtr) {
        LOG_ERROR("GPUParticleSystem: glBindBuffer not available!");
        return;
    }
    
    // Create resources
    LoadShaders();
    CreateBuffers();
    CreateQuadVAO();
    
    m_stagedParticles.reserve(10000);
    m_initialized = true;
    
    LOG_INFO("GPUParticleSystem: Initialized successfully with Indirect Drawing support.");
}

void GPUParticleSystem::Shutdown() {
    if (!m_initialized) return;
    
    LOG_INFO("GPUParticleSystem: Shutting down...");
    
    // Clean up shaders
    if (m_computeShader.id != 0) {
        rlUnloadShaderProgram(m_computeShader.id);
        m_computeShader.id = 0;
    }
    if (m_renderShader.id != 0) {
        UnloadShader(m_renderShader);
    }
    
    // VAO/VBO cleanup
    if (m_quadVBO != 0) {
        rlUnloadVertexBuffer(m_quadVBO);
        m_quadVBO = 0;
    }
    if (m_quadVAO != 0) {
        rlUnloadVertexArray(m_quadVAO);
        m_quadVAO = 0;
    }
    
    // Buffers are cleaned up by ComputeBuffer destructor
    
    m_stagedParticles.clear();
    m_initialized = false;
}

void GPUParticleSystem::LoadShaders() {
    // 1. Load Compute Shader
    // Using existing V2 shader files as we are porting V2 code
    std::ifstream compFile("assets/shaders/particle.compute");
    if (compFile.is_open()) {
        std::stringstream ss;
        ss << compFile.rdbuf();
        std::string source = ss.str();
        
        unsigned int compId = rlCompileShader(source.c_str(), RL_COMPUTE_SHADER);
        if (compId != 0) {
            m_computeShader.id = rlLoadComputeShaderProgram(compId);
            if (m_computeShader.id != 0) {
                LOG_INFO("GPUParticleSystem: Compute shader loaded (ID: {})", m_computeShader.id);
                m_computeDtLoc = rlGetLocationUniform(m_computeShader.id, "dt");
                m_computeTotalLoc = rlGetLocationUniform(m_computeShader.id, "totalParticles");
            } else {
                LOG_ERROR("GPUParticleSystem: Compute shader linking failed!");
            }
        } else {
            LOG_ERROR("GPUParticleSystem: Compute shader compilation failed!");
        }
    } else {
        LOG_ERROR("GPUParticleSystem: Could not open assets/shaders/particle.compute");
    }
    
    // 2. Load Render Shaders
    m_renderShader = LoadShader("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    if (m_renderShader.id != 0) {
        LOG_INFO("GPUParticleSystem: Render shader loaded (ID: {})", m_renderShader.id);
        m_renderMvpLoc = GetShaderLocation(m_renderShader, "mvp");
    } else {
        LOG_ERROR("GPUParticleSystem: Render shader loading failed!");
    }
}

void GPUParticleSystem::CreateBuffers() {
    size_t particleSize = sizeof(components::GPUParticle);
    size_t totalSize = m_maxParticles * particleSize;
    
    // Particle buffer (all particles)
    m_particleBuffer.Create(totalSize);
    LOG_DEBUG("GPUParticleSystem: Created particle buffer ({} bytes)", totalSize);
    
    // Compact buffer (alive particles only)
    m_compactBuffer.Create(totalSize);
    LOG_DEBUG("GPUParticleSystem: Created compact buffer ({} bytes)", totalSize);
    
    // DrawIndirect buffer (16 bytes)
    DrawArraysIndirectCommand cmd = { 6, 0, 0, 0 };  // 6 vertices, 0 instances initially
    m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand));
    m_indirectBuffer.Update(&cmd, sizeof(cmd));
    LOG_DEBUG("GPUParticleSystem: Created indirect buffer");
    
    // Atomic counter buffer (4 bytes)
    uint32_t zero = 0;
    m_atomicBuffer.Create(sizeof(uint32_t));
    m_atomicBuffer.Update(&zero, sizeof(zero));
    LOG_DEBUG("GPUParticleSystem: Created atomic buffer");
}

void GPUParticleSystem::CreateQuadVAO() {
    // Simple quad vertices: two triangles forming a square
    // Positions are in [-0.5, 0.5] range, centered at origin
    float vertices[] = {
        // Triangle 1
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        // Triangle 2
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
    };
    
    m_quadVAO = rlLoadVertexArray();
    rlEnableVertexArray(m_quadVAO);
    
    m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, 2 * sizeof(float), 0);
    rlEnableVertexAttribute(0);
    
    rlDisableVertexArray();
    
    LOG_DEBUG("GPUParticleSystem: Created quad VAO (ID: {})", m_quadVAO);
}

void GPUParticleSystem::Emit(const components::GPUParticle& particle) {
    if (m_stagedParticles.size() < (size_t)m_maxParticles) {
        m_stagedParticles.push_back(particle);
    }
}

void GPUParticleSystem::EmitBatch(const std::vector<components::GPUParticle>& particles) {
    if (particles.empty()) return;
    for (const auto& p : particles) {
        Emit(p);
    }
}

void GPUParticleSystem::Update(float dt) {
    if (!m_initialized || m_computeShader.id == 0) return;
    
    // 0. Determine Buffers for Ping-Pong
    // m_pingPong = false: Input=ParticleBuffer, Output=CompactBuffer
    // m_pingPong = true:  Input=CompactBuffer,  Output=ParticleBuffer
    core::ComputeBuffer& bufIn = m_pingPong ? m_compactBuffer : m_particleBuffer;
    core::ComputeBuffer& bufOut = m_pingPong ? m_particleBuffer : m_compactBuffer;
    
    // 1. Reset atomic counter to 0 (This counts ALIVE particles compacted)
    uint32_t zero = 0;
    m_atomicBuffer.Update(&zero, sizeof(zero));
    
    // 2. Dispatch compute shader
    rlEnableShader(m_computeShader.id);
    
    float clampedDt = (dt > 0.1f) ? 0.016f : dt;
    rlSetUniform(m_computeDtLoc, &clampedDt, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(m_computeTotalLoc, &m_currentParticleCount, RL_SHADER_UNIFORM_INT, 1);
    
    // Bind SSBOs
    bufIn.BindBase(0);   // Input: Read from last frame's valid state
    bufOut.BindBase(1);  // Output: Write alive particles here
    m_indirectBuffer.BindBase(2);
    m_atomicBuffer.BindBase(3);
    
    // Dispatch
    int workGroups = (m_currentParticleCount + 255) / 256;
    if (workGroups > 0) {
        rlComputeShaderDispatch(workGroups, 1, 1);
    }
    
    // Memory barrier to ensure compute results are visible
    utils::GPUUtils::MemoryBarrier();
    
    rlDisableShader();
    
    // 3. Read back alive count to know where to append new particles
    uint32_t aliveCount = 0;
    m_atomicBuffer.Read(&aliveCount, sizeof(uint32_t));
    
    // 4. Append New Particles to Output Buffer
    // They are appended AFTER the compacted alive particles
    if (!m_stagedParticles.empty()) {
        int newCount = (int)m_stagedParticles.size();
        size_t structSize = sizeof(components::GPUParticle);
        
        // Ensure we don't overflow
        if (aliveCount + newCount > (uint32_t)m_maxParticles) {
            newCount = m_maxParticles - aliveCount;
        }
        
        if (newCount > 0) {
            bufOut.Update(m_stagedParticles.data(), newCount * structSize, aliveCount * structSize);
            aliveCount += newCount;
        }
        m_stagedParticles.clear();
    }
    
    // 5. Update state for next frame
    m_currentParticleCount = aliveCount;
    
    // 6. Update Indirect Buffer for Rendering
    // We want to draw ALL valid particles (compacted alive + newly appended)
    DrawArraysIndirectCommand cmd = { 6, aliveCount, 0, 0 };
    m_indirectBuffer.Update(&cmd, sizeof(cmd));
    
    // 7. Swap Buffers for next frame
    m_pingPong = !m_pingPong;
}

Matrix GPUParticleSystem::BuildMVP(const Camera2D& camera) const {
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    
    // View matrix: camera transform
    Matrix view = MatrixIdentity();
    
    // 1. Translate to camera target (negate for view)
    view = MatrixMultiply(view, MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f));
    
    // 2. Apply rotation
    if (camera.rotation != 0.0f) {
        view = MatrixMultiply(view, MatrixRotateZ(camera.rotation * DEG2RAD));
    }
    
    // 3. Apply zoom
    view = MatrixMultiply(view, MatrixScale(camera.zoom, camera.zoom, 1.0f));
    
    // 4. Translate by offset (screen center)
    view = MatrixMultiply(view, MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f));
    
    // Projection matrix: orthographic, Y-down (Raylib convention)
    Matrix proj = MatrixOrtho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    
    // MVP = View * Projection
    return MatrixMultiply(view, proj);
}

void GPUParticleSystem::Render(const Camera2D& camera) {
    if (!m_initialized || m_renderShader.id == 0) return;
    
    // Build MVP matrix
    Matrix mvp = BuildMVP(camera);
    
    // Begin rendering
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(m_renderShader);
    
    // Set MVP uniform
    SetShaderValueMatrix(m_renderShader, m_renderMvpLoc, mvp);
    
    // Bind the buffer containing the valid particles for this frame
    // Since we flipped m_pingPong at the end of Update, the valid data is in the buffer 
    // that was the Output (which matches the NEW value of m_pingPong logic)
    core::ComputeBuffer& bufferToRender = m_pingPong ? m_compactBuffer : m_particleBuffer;
    bufferToRender.BindBase(0);
    
    // Enable VAO
    rlEnableVertexArray(m_quadVAO);
    rlDisableDepthTest(); // Ensure depth test is off
    rlDisableBackfaceCulling(); // Ensure we see both sides
    
    // Direct Instanced Draw using CPU count
    // This is safer than Indirect Draw and we already have the count on CPU
    if (m_currentParticleCount > 0) {
        rlDrawVertexArrayInstanced(0, 6, m_currentParticleCount);
    }
    
    // Cleanup
    rlDisableVertexArray();
    EndShaderMode();
    EndBlendMode();
}

// Implement InkEffectHelper
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