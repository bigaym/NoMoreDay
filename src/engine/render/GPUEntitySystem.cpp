#include "engine/render/GPUEntitySystem.hpp"
#include "core/logging/Logger.hpp"
  #include "engine/render/RenderConstants.hpp"
  #include "engine/render/GPUUtils.hpp"
#include "raylib.h" // Added for GetFrameTime()
#include "rlgl.h"

namespace NoMoreDay::systems {

using namespace components;

void GPUEntitySystem::Init(ResourceManager &resources, int maxEntities) {
  m_maxEntities = maxEntities;
  LOG_INFO("Initializing GPUEntitySystem (Compute-based Physics) with {} "
           "entities...",
           maxEntities);

  // W5.5 (RG-3 contract): every acquisition step below is checked so a
  // mid-init failure rolls back through the idempotent Shutdown() and the
  // system reports itself uninitialized (m_maxEntities == 0).
  auto failInit = [this](const char *what) {
    LOG_ERROR("GPUEntitySystem::Init failed while acquiring {}; rolling back "
              "partially acquired resources",
              what);
    Shutdown();
  };

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
  if (m_persistentEntityBuffer.GetId() == 0) {
    failInit("entity persistent buffer");
    return;
  }
  m_physicsOutputBuffer.Create(m_maxEntities * sizeof(components::GPUEntity),
                               3);
  if (m_physicsOutputBuffer.GetId() == 0) {
    failInit("physics output buffer");
    return;
  }
  m_mapBoundary = 5000.0f; // NoMoreDay::Constants::World::MAP_BOUNDARY

  int gridCols = 5000 / 32 + 1;
  int gridRows = 5000 / 32 + 1;
  int numCells = gridCols * gridRows;

  m_cellCountBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                           RL_DYNAMIC_DRAW);
  if (m_cellCountBuffer.GetId() == 0) {
    failInit("cell count buffer");
    return;
  }
  m_cellOffsetBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                            RL_DYNAMIC_DRAW);
  if (m_cellOffsetBuffer.GetId() == 0) {
    failInit("cell offset buffer");
    return;
  }
  m_entityIndicesBuffer.Create(m_maxEntities * sizeof(uint32_t), nullptr,
                               RL_DYNAMIC_DRAW);
  if (m_entityIndicesBuffer.GetId() == 0) {
    failInit("entity indices buffer");
    return;
  }
  m_tempCountBuffer.Create(numCells * sizeof(uint32_t), nullptr,
                           RL_DYNAMIC_DRAW);
  if (m_tempCountBuffer.GetId() == 0) {
    failInit("temp count buffer");
    return;
  }
  m_blockSumBuffer.Create(((numCells + 511) / 512) * sizeof(uint32_t), nullptr,
                          RL_DYNAMIC_DRAW);
  if (m_blockSumBuffer.GetId() == 0) {
    failInit("block sum buffer");
    return;
  }

  m_localData.resize(m_maxEntities);
  m_gridCounts.resize(numCells);
  m_gridOffsets.resize(numCells);
  m_blockDirty.resize((m_maxEntities / BLOCK_SIZE) + 1, true);

  // W5.5 (RG-3 contract): the grid compute shaders are loaded (and cached) by
  // ResourceManager and must NOT be released here; a missing shader file makes
  // loadComputeShader return Shader{0}. Validate the whole dependency set
  // before the system is considered usable. The check runs after the local
  // buffers are acquired so the rollback below exercises releasing partially
  // created buffers when a shader dependency is missing.
  if (m_gridClearShader.id == 0 || m_gridCountShader.id == 0 ||
      m_gridSortShader.id == 0 || m_physicsShader.id == 0 ||
      m_gridScanShader.id == 0) {
    failInit("grid compute shader set");
    return;
  }

  // Slot Manager initialization moved to NoMoreDay::GPUEntityAdapter (Game
  // layer). Engine no longer owns ECS slot/physics/visual projection.

  // W5.5 (RG-3 contract): partial-init safety. If the locally owned render
  // shader/VAO/VBO acquisition fails midway, release every successfully
  // acquired object and reset state so a later Shutdown is a context-safe
  // no-op and the system reports itself uninitialized.
  if (!InitRender(resources)) {
    LOG_ERROR("GPUEntitySystem::Init failed during render resource acquisition; "
              "releasing partially acquired resources");
    Shutdown();
  }
}

bool GPUEntitySystem::InitRender(ResourceManager &rm) {
  m_renderShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/entity.vert", "assets/shaders/entity.frag");
  if (m_renderShader.id == 0) {
    LOG_ERROR("GPUEntitySystem failed to load entity.vert/entity.frag");
    return false;
  }
  m_renderShader.locs[SHADER_LOC_MATRIX_MVP] =
      GetShaderLocation(m_renderShader, "mvp");

  float vertices[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
  m_quadVAO = rlLoadVertexArray();
  if (m_quadVAO == 0) {
    LOG_ERROR("GPUEntitySystem failed to create quad VAO");
    UnloadShader(m_renderShader);
    m_renderShader = Shader{0};
    return false;
  }
  rlEnableVertexArray(m_quadVAO);
  {
    m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    if (m_quadVBO == 0) {
      LOG_ERROR("GPUEntitySystem failed to create quad VBO");
      rlDisableVertexArray();
      rlUnloadVertexArray(m_quadVAO);
      m_quadVAO = 0;
      UnloadShader(m_renderShader);
      m_renderShader = Shader{0};
      return false;
    }
    rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
  }
  rlDisableVertexArray();

  // W5.5 (RG-3 contract): observe the locally owned raw shader and VAO/VBO.
  // The registry only records; GPUEntitySystem (via Game::cleanup -> Shutdown)
  // remains the sole releaser before context loss.
  auto &registry = NoMoreDay::render::resources::GPUResourceRegistry::Get();
  registry.RegisterResource(static_cast<uint32_t>(m_renderShader.id),
                            NoMoreDay::render::graph::ResourceKind::ShaderProgram,
                            NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
                            "GPUEntityRenderShader");
  registry.RegisterResource(m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray,
                            NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
                            "GPUEntityQuadVAO");
  registry.RegisterResource(m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer,
                            NoMoreDay::render::graph::RenderOwnerTag::Unknown, sizeof(vertices),
                            "GPUEntityQuadVBO");
  return true;
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
  // W5.5 (RG-3 contract): explicit, idempotent pre-context-loss cleanup.
  // Every locally owned object is released here; the five ResourceManager
  // compute shaders (m_gridClearShader/m_gridCountShader/m_gridSortShader/
  // m_physicsShader/m_gridScanShader) are NOT touched - ResourceManager
  // owns them and releases them in unloadAll(), which Game::cleanup calls
  // after this Shutdown. A second call and later default destruction must
  // perform no GL work, so every raw handle is zeroed below.
  LOG_INFO("Shutting down GPUEntitySystem...");

  // 1. Locally owned raw render shader.
  if (m_renderShader.id != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        static_cast<uint32_t>(m_renderShader.id),
        NoMoreDay::render::graph::ResourceKind::ShaderProgram);
    UnloadShader(m_renderShader);
    m_renderShader = Shader{0};
  }

  // 2. Locally owned quad VAO/VBO.
  if (m_quadVBO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer);
    rlUnloadVertexBuffer(m_quadVBO);
    m_quadVBO = 0;
  }
  if (m_quadVAO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray);
    rlUnloadVertexArray(m_quadVAO);
    m_quadVAO = 0;
  }

  // 3. Persistent buffers (register/unregister handled inside the wrappers).
  m_persistentEntityBuffer.Destroy();
  m_physicsOutputBuffer.Destroy();

  // 4. Grid compute buffers (register/unregister handled inside the wrapper).
  m_cellCountBuffer.Release();
  m_cellOffsetBuffer.Release();
  m_entityIndicesBuffer.Release();
  m_tempCountBuffer.Release();
  m_blockSumBuffer.Release();

  // 5. Reset initialization/allocation state so the object reports itself
  // uninitialized and repeated Shutdown + later member destruction are
  // context-safe no-ops.
  m_maxEntities = 0;
  m_highWaterMark = 0;
  m_mapBoundary = 5000.0f;
  m_blockDirty.clear();
  m_localData.clear();
  m_shadowBuffer.clear();
  m_visualStatsShadowBuffer.clear();
  m_updatedStatsIndices.clear();
  m_gridCounts.clear();
  m_gridOffsets.clear();
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
