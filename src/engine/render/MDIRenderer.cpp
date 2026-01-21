#include "engine/render/MDIRenderer.hpp"
#include "engine/render/GPUUtils.hpp"
#include "raymath.h"
#include <iostream>

// OpenGL function pointers for Indirect Draw
#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif

#ifndef GL_COMMAND_BARRIER_BIT
#define GL_COMMAND_BARRIER_BIT 0x00000040
#endif

#ifndef GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
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

    // Binding 1: Visible Indices (Double Buffered)
    m_visibleBuffer.Create(maxEntities * sizeof(uint32_t)); 

    // Binding 2: Indirect Command (Double Buffered)
    m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand));

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
    // Redundant - Logic moved to GPUEntitySystem zero-copy path
}

void MDIRenderer::ResetCommand() {
    auto* cmd = (DrawArraysIndirectCommand*)m_commandBuffer.BeginWrite();
    cmd->count = 4;
    cmd->instanceCount = 0;
    cmd->first = 0;
    cmd->baseInstance = 0;
    m_commandBuffer.Flush();
}

void MDIRenderer::Cull(Vector4 viewBounds) {
    if (!m_cullShader.id) return;

    // reset command buffer instance count
    ResetCommand();

    rlEnableShader(m_cullShader.id);

    // Bind Buffers
    // Skip instanceBuffer binding as it is provided by the caller (GPUEntitySystem) via BindPrevious(0)
    
    // Bind CURRENT write slots for culling output
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
    
    // Lock the buffers after writing
    m_visibleBuffer.Lock();
    m_commandBuffer.Lock();
    
    // Memory Barrier to ensure instanceCount and visibleIndices are visible for Indirect Draw and Shaders
    NoMoreDay::utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void MDIRenderer::Render(const Matrix& viewProj, float renderAlpha) {
    if (!m_renderShader.id || !glDrawArraysIndirect) return;

    // Culling happens just before Render in the same frame, 
    // we already have a barrier at the end of Cull().

    rlEnableShader(m_renderShader.id);

    // Binding 0: Instance Data (Triple Buffered in GPUEntitySystem)
    // Note: binding 0 is usually already bound by GPUEntitySystem::Render, 
    // but re-binding here is safer against intermediate state changes.
    // The actual binding is done by the caller using m_persistentEntityBuffer.BindPrevious(0).

    // Set ViewProj
    int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
    if (locVP != -1) rlSetUniformMatrix(locVP, viewProj);

    // Set Interpolation Factor
    int locInterp = rlGetLocationUniform(m_renderShader.id, "interpolationFactor");
    if (locInterp != -1) rlSetUniform(locInterp, &renderAlpha, RL_SHADER_UNIFORM_FLOAT, 1);

    // Bind Buffers for Vertex Shader (Binding 1 = Indices, 2 = Commands)
    // We bind PREVIOUS slots because those are what the physics-synced MDI just finished.
    m_visibleBuffer.BindPreviousNoSync(1);

    rlEnableVertexArray(m_quadVAO);
    
    // Bind Indirect Buffer
    int bufferCount = m_commandBuffer.GetBufferCount();
    int prevSlot = (m_commandBuffer.GetCurrentSlot() - 1 + bufferCount) % bufferCount;
    size_t offset = (size_t)prevSlot * m_commandBuffer.GetSize();
    
    m_commandBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);

    // Draw
    // IMPORTANT: glDrawArraysIndirect offset is in BYTES!
    // Using GL_TRIANGLE_FAN since we have 4 vertices for a quad
    glDrawArraysIndirect(GL_TRIANGLE_FAN, (void*)offset);

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
    m_visibleBuffer.Destroy();
    m_commandBuffer.Destroy();
    
    // Shaders are managed by ResourceManager, simply reset IDs
    if (m_cullShader.id != 0) {
        m_cullShader.id = 0;
    }
    if (m_renderShader.id != 0) {
        m_renderShader.id = 0;
    }
}

} // namespace NoMoreDay::render
