#include "engine/render/MDIRenderer.hpp"
#include "engine/render/GPUUtils.hpp"
#include "raymath.h"
#include <iostream>

// OpenGL function pointers for Indirect Draw
#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(unsigned int mode, const void *indirect);
static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = nullptr;

typedef void (*PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);
static PFNGLBINDBUFFERPROC glBindBuffer = nullptr;

namespace NoMoreDay::render {

MDIRenderer::~MDIRenderer() {
    Shutdown();
}

void MDIRenderer::Init(ResourceManager& rm, uint32_t maxEntities) {
    if (m_quadVAO != 0) return; // Already initialized

    m_maxEntities = maxEntities;

    // Load OpenGL extensions
    if (!glDrawArraysIndirect) {
        glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)glfwGetProcAddress("glDrawArraysIndirect");
        if (!glDrawArraysIndirect) {
            std::cerr << "[MDIRenderer] Failed to load glDrawArraysIndirect" << std::endl;
        }
    }
    if (!glBindBuffer) {
        glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    }

    // Initialize Buffers
    // Binding 0: Instance Data
    m_instanceBuffer.Create(maxEntities * sizeof(GPUInstanceData), nullptr, RL_DYNAMIC_DRAW);
    
    // Binding 1: Visible Indices (Worst case all visible)
    m_visibleBuffer.Create(maxEntities * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW); 

    // Binding 2: Indirect Command
    // We only need 1 command for now (Entity Type batching is a future improvement if needed)
    DrawArraysIndirectCommand cmd = {};
    cmd.count = 4; // Quad
    cmd.instanceCount = 0; // Starts at 0, filled by GPU
    cmd.first = 0;
    cmd.baseInstance = 0;
    m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand), &cmd, RL_DYNAMIC_DRAW);

    // Load Shaders
    // Using hashed string for IDs as per ResourceManager usage in GPUEntitySystem
    m_cullShader = rm.loadComputeShader(entt::hashed_string{"mdi_cull"}, "assets/shaders/cull.compute");
    m_renderShader = rm.loadShader(entt::hashed_string{"mdi_render"}, "assets/shaders/entity_mdi.vert", "assets/shaders/entity_mdi.frag");

    // Create Quad VAO
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };
    
    m_quadVAO = rlLoadVertexArray();
    rlEnableVertexArray(m_quadVAO);
    
    m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlSetVertexAttribute(0, 2, RL_FLOAT, 0, 0, 0); // Location 0: position (vec2)
    rlEnableVertexAttribute(0);

    rlDisableVertexArray();
}

void MDIRenderer::UpdateInstances(const std::vector<GPUInstanceData>& data) {
    if (data.empty()) return;
    size_t uploadSize = data.size() * sizeof(GPUInstanceData);
    if (uploadSize > m_instanceBuffer.GetSize()) {
        // Resize or warn
        std::cerr << "[MDIRenderer] Instance data exceeds buffer size" << std::endl;
        uploadSize = m_instanceBuffer.GetSize();
    }
    m_instanceBuffer.Update(data.data(), uploadSize);
}

void MDIRenderer::ResetCommand() {
    // Reset instanceCount to 0 in command buffer
    // Method 1: CPU Update (easiest for now, though Plan said Compute Shader)
    // To stick to Plan Task 1.2 which says "Use Compute Shader to clear":
    // For now I'll use CPU update as placeholder or if Shader loading isn't clear yet.
    // Actually, `atomicCounter` or `atomicAdd` requires the buffer to be reset.
    // The command buffer `instanceCount` is at offset 4.
    uint32_t zero = 0;
    m_commandBuffer.Update(&zero, sizeof(uint32_t), offsetof(DrawArraysIndirectCommand, instanceCount));
    
    // Memory Barrier to ensure update is visible
    NoMoreDay::utils::GPUUtils::MemoryBarrier();
}

void MDIRenderer::Cull(Vector4 viewBounds) {
    if (!m_cullShader.id) return;

    // reset command buffer instance count
    ResetCommand();

    rlEnableShader(m_cullShader.id);

    // Bind Buffers
    m_instanceBuffer.BindBase(0);
    m_visibleBuffer.BindBase(1);
    m_commandBuffer.BindBase(2);

    // Set Uniforms
    int locBounds = rlGetLocationUniform(m_cullShader.id, "viewBounds");
    int locMax = rlGetLocationUniform(m_cullShader.id, "maxEntities");
    
    if (locBounds != -1) rlSetUniform(locBounds, &viewBounds, RL_SHADER_UNIFORM_VEC4, 1);
    if (locMax != -1) rlSetUniform(locMax, &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);

    // Dispatch
    // Group size 256
    int groups = (m_maxEntities + 255) / 256;
    rlComputeShaderDispatch(groups, 1, 1);

    rlDisableShader();
    
    NoMoreDay::utils::GPUUtils::MemoryBarrier();
}

void MDIRenderer::Render(const Matrix& viewProj) {
    if (!m_renderShader.id || !glDrawArraysIndirect) return;

    rlEnableShader(m_renderShader.id);

    // Set ViewProj
    int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
    if (locVP != -1) rlSetUniformMatrix(locVP, viewProj);

    // Bind Buffers for Vertex Shader
    m_instanceBuffer.BindBase(0);
    m_visibleBuffer.BindBase(1);

    rlEnableVertexArray(m_quadVAO);
    
    // Bind Indirect Buffer
    m_commandBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);

    // Draw
    glDrawArraysIndirect(GL_TRIANGLES, 0); // 0 offset in indirect buffer

    // Unbind
    // explicit unbind if needed, or rlgl wrapper handles generic state?
    // Indirect buffer binding is global state in GL
    if (glBindBuffer) glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0); 

    rlDisableVertexArray();
    rlDisableShader();
}

void MDIRenderer::Shutdown() {
    if (m_quadVAO != 0) {
        rlUnloadVertexArray(m_quadVAO);
        rlUnloadVertexBuffer(m_quadVBO);
        m_quadVAO = 0;
    }
    m_instanceBuffer.Release();
    m_visibleBuffer.Release();
    m_commandBuffer.Release();
    
    // Shaders are managed by ResourceManager, simply reset IDs
    if (m_cullShader.id != 0) {
        m_cullShader.id = 0;
    }
    if (m_renderShader.id != 0) {
        m_renderShader.id = 0;
    }
}

} // namespace NoMoreDay::render
