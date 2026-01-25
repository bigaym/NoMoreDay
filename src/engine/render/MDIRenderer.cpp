#include "engine/render/MDIRenderer.hpp"
#include "core/logging/Logger.hpp"
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

#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(unsigned int mode,
                                            const void *indirect);
static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = nullptr;

typedef void (*PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);
static PFNGLBINDBUFFERPROC glBindBuffer = nullptr;

typedef void(APIENTRY *PFNGLACTIVETEXTUREPROC)(unsigned int texture);
typedef void(APIENTRY *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);
static PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
static PFNGLMEMORYBARRIERPROC glMemoryBarrier = nullptr;

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

namespace NoMoreDay::render {

MDIRenderer::~MDIRenderer() { Shutdown(); }

void MDIRenderer::Init(ResourceManager &rm, uint32_t maxEntities) {
  if (m_quadVAO != 0)
    return; // Already initialized

  m_maxEntities = maxEntities;

  // Load OpenGL extensions
  if (!glDrawArraysIndirect) {
    glDrawArraysIndirect =
        (PFNGLDRAWARRAYSINDIRECTPROC)glfwGetProcAddress("glDrawArraysIndirect");
    if (!glDrawArraysIndirect) {
      std::cerr << "[MDIRenderer] Failed to load glDrawArraysIndirect"
                << std::endl;
    }
  }
  if (!glBindBuffer) {
    glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
  }
  if (!glActiveTexture) {
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");
  }
  if (!glMemoryBarrier) {
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
  }

  // Binding 1: Visible Indices (Double Buffered)
  m_visibleBuffer.Create(maxEntities * sizeof(uint32_t), 3);

  // Binding 2: Indirect Command (Double Buffered)
  m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand), 3);

  // Binding 3: Visual Stats (Double Buffered)
  m_statsBuffer.Create(maxEntities *
                       sizeof(NoMoreDay::components::GPUVisualStats), 3);

  // Load Shaders
  // Using hashed string for IDs as per ResourceManager usage in GPUEntitySystem
  m_cullShader = rm.loadComputeShader(entt::hashed_string{"mdi_cull"},
                                      "assets/shaders/cull.compute");
  m_renderShader = rm.loadShader(entt::hashed_string{"mdi_render"},
                                 "assets/shaders/entity_mdi.vert",
                                 "assets/shaders/entity_mdi.frag");

  // Create Quad VAO (6 vertices for GL_TRIANGLES)
  float vertices[] = {
      -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
      -0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f};

  m_quadVAO = rlLoadVertexArray();
  rlEnableVertexArray(m_quadVAO);

  m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  rlSetVertexAttribute(0, 2, RL_FLOAT, 0, 0, 0); // Location 0: position (vec2)
  rlEnableVertexAttribute(0);

  rlDisableVertexArray();
}

void MDIRenderer::UpdateStats(
    const std::vector<NoMoreDay::components::GPUVisualStats> &stats) {
  if (stats.empty())
    return;

  size_t dataSize =
      stats.size() * sizeof(NoMoreDay::components::GPUVisualStats);
  if (dataSize > m_statsBuffer.GetSize()) {
    std::cerr << "[MDIRenderer] Stats data lager than buffer context!"
              << std::endl;
    // Optional: Resize? For now just clamp or return to avoid crash.
    dataSize = m_statsBuffer.GetSize();
  }

  void *ptr = m_statsBuffer.BeginWrite();
  if (ptr) {
    memcpy(ptr, stats.data(), dataSize);
    m_statsBuffer.Flush();
    m_statsBuffer.Lock();
  }
}

void MDIRenderer::ResetCommand() {
  auto *cmd = (DrawArraysIndirectCommand *)m_commandBuffer.BeginWrite();
  cmd->count = 6;
  cmd->instanceCount = 0; // RESET TO 0 - let Cull fill it
  cmd->first = 0;
  cmd->baseInstance = 0;
  m_commandBuffer.Flush();
}

void MDIRenderer::Cull(Vector4 viewBounds) {
  if (!m_cullShader.id) {
      LOG_LIMITED_ERROR(5.0f, "MDI Cull Fail: Shader ID is 0!");
      return;
  }

  // reset command buffer instance count
  ResetCommand();

  rlEnableShader(m_cullShader.id);

  // Bind buffers to CURRENT write slots
  m_visibleBuffer.BindBase(1);
  m_commandBuffer.BindBase(2);

  // Set Uniforms
  int locView = rlGetLocationUniform(m_cullShader.id, "viewBounds");
  if (locView != -1)
    rlSetUniform(locView, &viewBounds, RL_SHADER_UNIFORM_VEC4, 1);

  int locMax = rlGetLocationUniform(m_cullShader.id, "maxEntities");
  if (locMax != -1) {
    int me = (int)m_maxEntities;
    rlSetUniform(locMax, &me, RL_SHADER_UNIFORM_INT, 1);
  }

  // Dispatch Compute
  int groups = (m_maxEntities + 255) / 256;
  rlComputeShaderDispatch(groups, 1, 1);

  glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

  m_visibleBuffer.Lock();
  m_commandBuffer.Lock();
}

void MDIRenderer::Render(ResourceManager &rm, const PersistentBuffer &entities, float renderAlpha) {
  if (m_renderShader.id == 0 || !glDrawArraysIndirect) return;

  // 1. Flush Raylib batch
  rlDrawRenderBatchActive();

  // 2. Get current matrices from Raylib (ensure we are in BeginMode2D)
  Matrix modelview = rlGetMatrixModelview();
  Matrix projection = rlGetMatrixProjection();
  Matrix mvp = MatrixMultiply(modelview, projection);

  rlEnableShader(m_renderShader.id);

  // 3. Bind Texture Array
  unsigned int texArray = rm.getEntityTextureArray();
  if (texArray != 0) {
    int locTex = rlGetLocationUniform(m_renderShader.id, "entityTextures");
    if (locTex != -1 && glActiveTexture) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
      int unit = 0; 
      rlSetUniform(locTex, &unit, RL_SHADER_UNIFORM_INT, 1);
    }
  }

  // 4. Set Uniforms
  int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
  if (locVP != -1) rlSetUniformMatrix(locVP, mvp);

  int locInterp = rlGetLocationUniform(m_renderShader.id, "interpolationFactor");
  if (locInterp != -1) rlSetUniform(locInterp, &renderAlpha, RL_SHADER_UNIFORM_FLOAT, 1);

  // 5. EXPLICITLY BIND ALL SSBOs
  typedef void(APIENTRY *PFNGLBINDBUFFERBASEPROC)(unsigned int target, unsigned int index, unsigned int buffer);
  static PFNGLBINDBUFFERBASEPROC glBindBufferBasePtr = (PFNGLBINDBUFFERBASEPROC)glfwGetProcAddress("glBindBufferBase");

  // Binding 0: Entities (From external buffer)
  entities.BindPreviousNoSync(0);
  
  // Binding 1: Visible Indices
  m_visibleBuffer.BindPreviousNoSync(1);
  // Binding 3: Visual Stats
  m_statsBuffer.BindPreviousNoSync(3); 

  // 6. VAO & Indirect Buffer
  rlEnableVertexArray(m_quadVAO);

  int bufferCount = m_commandBuffer.GetBufferCount();
  int prevSlot = (m_commandBuffer.GetCurrentSlot() - 1 + bufferCount) % bufferCount;
  size_t offset = (size_t)prevSlot * m_commandBuffer.GetSize();

  m_commandBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);

  // 7. Final Draw
  glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  
  glDrawArraysIndirect(GL_TRIANGLES, (void *)offset);

  // Unbind
  if (glBindBuffer)
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
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
  m_statsBuffer.Destroy();

  // Shaders are managed by ResourceManager, simply reset IDs
  if (m_cullShader.id != 0) {
    m_cullShader.id = 0;
  }
  if (m_renderShader.id != 0) {
    m_renderShader.id = 0;
  }
}

} // namespace NoMoreDay::render
