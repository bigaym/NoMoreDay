#include "engine/render/GPUEntitySystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/RenderConstants.hpp"
#include "raylib.h" // Added for GetFrameTime()
#include "rlgl.h"

namespace NoMoreDay::systems {

using namespace components;

void GPUEntitySystem::Init(ResourceManager &resources, int maxEntities) {
  m_maxEntities = maxEntities;
  LOG_INFO("Initializing GPUEntitySystem (Compute-based Physics) with {} "
           "entities...",
           maxEntities);

  m_gridClearShader = resources.loadComputeShader(
      entt::hashed_string{"grid_clear"}, "assets/shaders/grid_clear.compute");
  m_gridCountShader = resources.loadComputeShader(
      entt::hashed_string{"grid_count"}, "assets/shaders/grid_count.compute");
  m_gridSortShader = resources.loadComputeShader(
      entt::hashed_string{"grid_sort"}, "assets/shaders/grid_sort.compute");
  m_physicsShader = resources.loadComputeShader(
      entt::hashed_string{"physics"}, "assets/shaders/physics.compute");
  m_gridScanShader = resources.loadComputeShader(
      entt::hashed_string{"grid_scan"}, "assets/shaders/grid_scan.compute");

  m_persistentEntityBuffer.Create(m_maxEntities * sizeof(components::GPUEntity),
                                  3);
  m_physicsOutputBuffer.Create(m_maxEntities * sizeof(components::GPUEntity),
                               3);
  m_mapBoundary = 5000.0f; // NoMoreDay::Constants::World::MAP_BOUNDARY

  int gridCols = 5000 / 32 + 1;
  int gridRows = 5000 / 32 + 1;
  int numCells = gridCols * gridRows;

  m_cellCountBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                           RL_DYNAMIC_DRAW);
  m_cellOffsetBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                            RL_DYNAMIC_DRAW);
  m_entityIndicesBuffer.Create(m_maxEntities * sizeof(uint32_t), nullptr,
                               RL_DYNAMIC_DRAW);
  m_tempCountBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                           RL_DYNAMIC_DRAW);
  m_blockSumBuffer.Create(((numCells + 511) / 512) * sizeof(uint32_t), nullptr,
                          RL_DYNAMIC_DRAW);

  m_localData.resize(m_maxEntities);
  m_gridCounts.resize(numCells);
  m_gridOffsets.resize(numCells);
  m_blockDirty.resize((m_maxEntities / BLOCK_SIZE) + 1, true);

  // Slot Manager initialization moved to NoMoreDay::GPUEntityAdapter (Game
  // layer). Engine no longer owns ECS slot/physics/visual projection.

  InitRender(resources);
}

void GPUEntitySystem::InitRender(ResourceManager &rm) {
  m_renderShader =
      LoadShader("assets/shaders/entity.vert", "assets/shaders/entity.frag");
  m_renderShader.locs[SHADER_LOC_MATRIX_MVP] =
      GetShaderLocation(m_renderShader, "mvp");

  float vertices[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
  m_quadVAO = rlLoadVertexArray();
  rlEnableVertexArray(m_quadVAO);
  {
    m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
  }
  rlDisableVertexArray();
}

void GPUEntitySystem::Render(const render::EntityRenderFrame &frame,
                             const Camera2D &camera) {
  if (m_maxEntities > 0) {
    // Calculate View Bounds for Culling
    Vector2 worldMin = GetScreenToWorld2D({0, 0}, camera);
    Vector2 worldMax = GetScreenToWorld2D(
        {(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    Vector4 viewBounds = {fminf(worldMin.x, worldMax.x) - 500.0f,
                          fminf(worldMin.y, worldMax.y) - 500.0f,
                          fmaxf(worldMin.x, worldMax.x) + 500.0f,
                          fmaxf(worldMin.y, worldMax.y) + 500.0f};

    m_persistentEntityBuffer.BindPreviousNoSync(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_ENTITY_DATA));

    if (frame.mdi != nullptr) {
      auto &mdi = *frame.mdi;
      mdi.ResetCommand(); // Reset before culling
      mdi.Cull(*frame.resources, m_persistentEntityBuffer, viewBounds);
      mdi.Render(*frame.resources, m_persistentEntityBuffer,
                 frame.renderAlpha);
    } else {
      auto &mdi = NoMoreDay::render::MDIRenderer::Get();
      mdi.ResetCommand(); // Reset before culling
      mdi.Cull(*frame.resources, m_persistentEntityBuffer, viewBounds);
      mdi.Render(*frame.resources, m_persistentEntityBuffer,
                 frame.renderAlpha);
    }
  } else {
    RenderLegacy(frame.renderAlpha);
  }
}

void GPUEntitySystem::RenderLegacy(float alpha) {
  if (m_maxEntities <= 0 || m_renderShader.id == 0)
    return;
  Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  rlEnableShader(m_renderShader.id);
  rlSetUniformMatrix(m_renderShader.locs[SHADER_LOC_MATRIX_MVP], mvp);

  // Set interpolation alpha
  int alphaLoc = rlGetLocationUniform(m_renderShader.id, "renderAlpha");
  rlSetUniform(alphaLoc, &alpha, RL_SHADER_UNIFORM_FLOAT, 1);

  m_persistentEntityBuffer.BindPrevious(static_cast<uint32_t>(
      NoMoreDay::RenderConstants::Binding::SSBO_VISIBLE_ID));
  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 4, m_maxEntities);
  rlDisableVertexArray();
  rlDisableShader();
}

void GPUEntitySystem::UploadGPU(const render::EntityRenderFrame &frame) {
  NoMoreDay::utils::ScopedTimer timer("Upload GPU", 50); // Keep sensitive to track improvements
  using namespace NoMoreDay::utils;
  
  // 1. Entity Data Upload (Full bulk for physics consistency, but optimized range)
  size_t activeCount = std::min((size_t)m_highWaterMark + 1, (size_t)m_maxEntities);
  size_t uploadSize = activeCount * sizeof(components::GPUEntity);

  if (uploadSize > 0) {
      components::GPUEntity *gpuPtr = (components::GPUEntity *)m_persistentEntityBuffer.BeginWrite();
      if (gpuPtr) {
          // Only memcpy the active range
          memcpy(gpuPtr, m_shadowBuffer.data(), uploadSize);
      }
      m_persistentEntityBuffer.FlushRange(0, uploadSize);
  }

  // 2. Visual Stats Upload (Sparse update via Scatter Compute)
  if (!m_updatedStatsIndices.empty()) {
      auto& mdi = (frame.mdi != nullptr) ? *frame.mdi : NoMoreDay::render::MDIRenderer::Get();
      mdi.FlushStatsUpdates(*frame.resources); // This handles m_updatedStatsIndices internally
  }
  
  // Update MDI active count even if no stats changed
  auto& mdi = (frame.mdi != nullptr) ? *frame.mdi : NoMoreDay::render::MDIRenderer::Get();
  mdi.SetMaxActiveEntities((uint32_t)activeCount);

  m_persistentEntityBuffer.Lock();
}

void GPUEntitySystem::Shutdown() {
  LOG_INFO("Shutting down GPUEntitySystem...");
  m_persistentEntityBuffer.Destroy();
  m_cellCountBuffer.Release();
  m_cellOffsetBuffer.Release();
  m_entityIndicesBuffer.Release();
  m_tempCountBuffer.Release();
}

components::GPUEntity *GPUEntitySystem::BeginShadowWrite() {
  if ((int)m_shadowBuffer.size() != m_maxEntities) {
    m_shadowBuffer.assign(m_maxEntities, {});
    m_visualStatsShadowBuffer.assign(m_maxEntities, {});
  }
  return m_shadowBuffer.data();
}

void GPUEntitySystem::SetHighWaterMark(int highWaterMark) {
  m_highWaterMark = highWaterMark;
}

void GPUEntitySystem::ApplyShadowFlags(int slot, uint32_t flags) {
  if (slot < 0 || slot >= (int)m_shadowBuffer.size()) {
    return;
  }
  const uint32_t noRenderMask = components::GPU_ENTITY_FLAG_NO_RENDER;
  m_shadowBuffer[slot].flags =
      (m_shadowBuffer[slot].flags & ~noRenderMask) | (flags & noRenderMask);
}

void GPUEntitySystem::SetUpdatedStatsIndices(
    const std::vector<uint32_t> &indices) {
  m_updatedStatsIndices = indices;
}

std::vector<components::GPUEntity> &GPUEntitySystem::ShadowBuffer() {
  return m_shadowBuffer;
}

std::vector<components::GPUVisualStats> &GPUEntitySystem::VisualStatsBuffer() {
  return m_visualStatsShadowBuffer;
}

} // namespace NoMoreDay::systems
