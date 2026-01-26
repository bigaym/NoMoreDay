#include "engine/render/MDIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "raymath.h"
#include <iostream>
#include <vector>

namespace NoMoreDay::render {

using namespace NoMoreDay::RenderConstants;

MDIRenderer::~MDIRenderer() { Shutdown(); }

void MDIRenderer::Init(ResourceManager &rm, uint32_t maxEntities) {
  if (m_quadVAO != 0)
    return; // Already initialized

  m_maxEntities = maxEntities;
  LOG_INFO("Initializing MDI Renderer (Max Entities: {})...", maxEntities);

  if (!utils::GPUUtils::IsInitialized()) {
    LOG_ERROR("MDIRenderer: GPUUtils must be initialized before MDIRenderer!");
    return;
  }

  // 1. Load Shaders
  m_renderShader =
      rm.loadShader("mdi_render"_hash, "assets/shaders/entity_mdi.vert",
                    "assets/shaders/entity_mdi.frag");

  m_cullShader =
      rm.loadComputeShader("mdi_cull"_hash, "assets/shaders/cull.compute");

  // 2. Create Buffers
  m_visibleBuffer.Create(maxEntities * sizeof(uint32_t), 3);
  m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand), 3);
  m_statsBuffer.Create(maxEntities * sizeof(components::GPUVisualStats), 3);

  // 3. Create Quad VAO
  float quadVertices[] = {
      -0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, 1.0f, 0.0f,
      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, 0.5f,  0.0f, 1.0f,
  };
  unsigned int quadIndices[] = {0, 1, 2, 2, 3, 0};

  m_quadVAO = rlLoadVertexArray();
  rlEnableVertexArray(m_quadVAO);
  m_quadVBO = rlLoadVertexBuffer(quadVertices, sizeof(quadVertices), false);
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float),
                       (int)(2 * sizeof(float)));
  rlEnableVertexAttribute(0);
  rlEnableVertexAttribute(1);

  // Vertex array element buffer
  // Note: rlgl manages ID for us
  rlLoadVertexBufferElement(quadIndices, sizeof(quadIndices), false);
  rlDisableVertexArray();

  LOG_INFO("MDIRenderer: Initialized successfully.");
}

void MDIRenderer::Shutdown() {
  if (m_quadVAO == 0)
    return;
  rlUnloadVertexArray(m_quadVAO);
  rlUnloadVertexBuffer(m_quadVBO);
  m_quadVAO = 0;

  m_visibleBuffer.Destroy();
  m_commandBuffer.Destroy();
  m_statsBuffer.Destroy();
}

void MDIRenderer::Update(ResourceManager &rm, const PersistentBuffer &entities,
                         float alpha) {
  // Convenience wrapper
}

void MDIRenderer::ResetCommand() {
  void *ptr = m_commandBuffer.BeginWrite();
  if (ptr) {
    DrawArraysIndirectCommand cmd = {4, 0, 0, 0}; // 4 vertices for Quad
    memcpy(ptr, &cmd, sizeof(cmd));
  }
  m_commandBuffer.Flush();
  m_commandBuffer.Lock();
}

void MDIRenderer::UpdateStats(
    const std::vector<components::GPUVisualStats> &stats, int count) {
  if (count <= 0)
    return;
  void *ptr = m_statsBuffer.BeginWrite();
  memcpy(ptr, stats.data(), count * sizeof(components::GPUVisualStats));
  m_statsBuffer.Flush();
  m_statsBuffer.Lock();
}

void MDIRenderer::Cull(Vector4 viewBounds) {
  using namespace NoMoreDay::RenderConstants;

  if (m_cullShader.id == 0)
    return;

  // Reset Command Counter manually if needed or in shader
  // Assuming cull.compute handle this
  rlEnableShader(m_cullShader.id);

  // Set view bounds
  int locBounds = rlGetLocationUniform(m_cullShader.id, "viewBounds");
  if (locBounds != -1) {
    float bounds[4] = {viewBounds.x, viewBounds.y, viewBounds.z, viewBounds.w};
    rlSetUniform(locBounds, bounds, RL_SHADER_UNIFORM_VEC4, 1);
  }

  // Bind Buffers for Cull
  m_visibleBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
  m_commandBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND));

  // Dispatch - No barrier here, Render() will handle it as the Consumer
  utils::GPUUtils::DispatchComputeNoBarrier((m_maxEntities + 63) / 64, 1, 1);

  rlDisableShader();
}

void MDIRenderer::Render(ResourceManager &rm, const PersistentBuffer &entities,
                         float renderAlpha) {
  using namespace NoMoreDay::RenderConstants;

  if (m_renderShader.id == 0)
    return;

  // 1. Flush Raylib batch
  rlDrawRenderBatchActive();

  // 2. MVP
  Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

  rlEnableShader(m_renderShader.id);

  // 3. Bind Texture Array
  unsigned int texArray = rm.getEntityTextureArray();
  if (texArray != 0) {
    int locTex = rlGetLocationUniform(m_renderShader.id, "entityTextures");
    if (locTex != -1) {
      utils::GPUUtils::ActiveTexture(TextureUnit::TEX_ENTITY_ARRAY);
      utils::GPUUtils::BindTexture(GL::TEXTURE_2D_ARRAY, texArray);
      int unit = static_cast<int>(TextureUnit::TEX_ENTITY_ARRAY);
      rlSetUniform(locTex, &unit, RL_SHADER_UNIFORM_INT, 1);
    }
  }

  // 4. Set Uniforms
  int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
  if (locVP != -1)
    rlSetUniformMatrix(locVP, mvp);

  int locInterp =
      rlGetLocationUniform(m_renderShader.id, "interpolationFactor");
  if (locInterp != -1)
    rlSetUniform(locInterp, &renderAlpha, RL_SHADER_UNIFORM_FLOAT, 1);

  // 5. EXPLICITLY BIND ALL SSBOs
  entities.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA));
  m_visibleBuffer.BindPreviousNoSync(
      static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
  m_statsBuffer.BindPreviousNoSync(
      static_cast<uint32_t>(Binding::SSBO_VISUAL_STATS));

  // Bind Command Buffer for Indirect Draw
  m_commandBuffer.Bind(GL::DRAW_INDIRECT_BUFFER);

  // 6. Execute Indirect Draw
  rlEnableVertexArray(m_quadVAO);

  // Ensure all previous commands/writes are visible to Indirect Draw
  // Consumer Responsibility: Sync Command, SSBO, and Buffer updates
  utils::GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO |
                                 Barrier::Buffer);

  utils::GPUUtils::DrawArraysIndirect(GL::TRIANGLES, 0);
  rlDisableVertexArray();

  rlDisableShader();
}

} // namespace NoMoreDay::render
