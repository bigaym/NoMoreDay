#include "engine/render/GPUEntitySystem.hpp"
#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/RenderContext.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include "raylib.h" // Added for GetFrameTime()
#include "rlgl.h"

namespace NoMoreDay::systems {

GPUEntitySystem *GPUEntitySystem::s_instance = nullptr;

GPUEntitySystem &GPUEntitySystem::Get() {
  if (!s_instance) {
    LOG_WARN("GPUEntitySystem::Get() called without initialization. "
             "Consider using RenderContext injection.");
    static GPUEntitySystem fallback;
    return fallback;
  }
  return *s_instance;
}

using namespace components;

void GPUEntitySystem::Init(ResourceManager &resources, int maxEntities,
                           entt::registry *registry) {
  s_instance = this;
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
  m_mapBoundary = NoMoreDay::Constants::World::MAP_BOUNDARY;

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

  // Initialize new SlotManager
  m_slotManager.Init(m_maxEntities, registry, [this](int slot) {
    if (slot >= 0 && slot < (int)m_shadowBuffer.size()) {
      m_shadowBuffer[slot].radius = 0.0f;
      m_shadowBuffer[slot].position = {0, 0};
      m_visualStatsShadowBuffer[slot] = {};
    }
  });

  // Legacy slot management removed in favor of GPUSlotManager
  // m_freeSlots/m_slotToEntity are now managed by m_slotManager

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

void GPUEntitySystem::Render(const NoMoreDay::SharedContext &context,
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

    if (context.renderContext) {
      auto &mdi = context.renderContext->MDI();
      mdi.ResetCommand(); // Reset before culling
      mdi.Cull(*context.resources, m_persistentEntityBuffer, viewBounds);
      mdi.Render(*context.resources, m_persistentEntityBuffer,
                 context.renderAlpha);
    } else {
      auto &mdi = NoMoreDay::render::MDIRenderer::Get();
      mdi.ResetCommand(); // Reset before culling
      mdi.Cull(*context.resources, m_persistentEntityBuffer, viewBounds);
      mdi.Render(*context.resources, m_persistentEntityBuffer,
                 context.renderAlpha);
    }
  } else {
    RenderLegacy(context.renderAlpha);
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

// OnGPUIndexDestroyed removed - Managed by GPUSlotManager internal callback

void GPUEntitySystem::Update(const NoMoreDay::SharedContext &context,
                             float dt) {
  UpdateLogic(context, dt);
  UploadGPU(context);
}

void GPUEntitySystem::UpdateLogic(const NoMoreDay::SharedContext &context, float dt) {
  NoMoreDay::utils::ScopedTimer timer("Logic Update", 100);
  using namespace NoMoreDay::utils;
  auto &registry = *context.registry;
  m_frameCounter++;
  float currentTime = (float)GetTime();

  if (m_shadowBuffer.size() != m_maxEntities) {
    m_shadowBuffer.assign(m_maxEntities, {});
    m_visualStatsShadowBuffer.assign(m_maxEntities, {});
  }

  // --- Step 1: CPU Logic (Perform all heavy lifting first without GPU locks) ---
  {
    // Phase 1: Slot Reclamation & Assignment via GPUSlotManager
    m_slotManager.Process(registry);

    // Phase 2: Physics Sync
    m_highWaterMark =
        m_physicsSync.Execute(registry, m_shadowBuffer, m_frameCounter);

    // Phase 3: Visual Sync
    m_updatedStatsIndices = m_visualSync.Execute(registry, m_visualStatsShadowBuffer, m_frameCounter,
                         currentTime);
  }

  if (context.levelManager) {
    const auto biomeId = context.levelManager->getCurrentBiomeID();
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(biomeId);
    if (biome.hasFeature(NoMoreDay::BiomeFeature::LimitedVision) &&
        biome.visionRadius > 0.0f) {
      auto enemyView = registry.view<EnemyTag, Position, GPUIndex>();
      for (auto entity : enemyView) {
        const auto &pos = enemyView.get<Position>(entity);
        const auto &gpuIdx = enemyView.get<GPUIndex>(entity);
        const int slot = gpuIdx.index;
        if (slot < 0 || slot >= (int)m_shadowBuffer.size()) {
          continue;
        }
        using namespace NoMoreDay::Constants::World;
        const int gx = static_cast<int>(pos.x / GRID_TILE_SIZE);
        const int gy = static_cast<int>(pos.y / GRID_TILE_SIZE);
        const bool visible = context.levelManager->getFogSystem().isVisible(gx, gy);
        if (visible) {
          m_shadowBuffer[slot].flags &= ~GPU_ENTITY_FLAG_NO_RENDER;
        } else {
          m_shadowBuffer[slot].flags |= GPU_ENTITY_FLAG_NO_RENDER;
        }
      }
    }
  }
}

void GPUEntitySystem::UploadGPU(const NoMoreDay::SharedContext &context) {
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
      auto& mdi = NoMoreDay::render::MDIRenderer::Get();
      mdi.FlushStatsUpdates(*context.resources); // This handles m_updatedStatsIndices internally
  }
  
  // Update MDI active count even if no stats changed
  auto& mdi = NoMoreDay::render::MDIRenderer::Get();
  mdi.SetMaxActiveEntities((uint32_t)activeCount);

  m_persistentEntityBuffer.Lock();
}
void GPUEntitySystem::SyncBack(entt::registry &registry) {
  // CPU Authority: SyncBack is disabled to prevent stale/extrapolated GPU data
  // from overwriting the CPU source of truth.
  // Implementation of Task 1.1 of the enemy-rendering-refactor track.
}

void GPUEntitySystem::Shutdown() {
  LOG_INFO("Shutting down GPUEntitySystem...");
  m_persistentEntityBuffer.Destroy();
  m_cellCountBuffer.Release();
  m_cellOffsetBuffer.Release();
  m_entityIndicesBuffer.Release();
  m_tempCountBuffer.Release();

  if (s_instance == this) {
    s_instance = nullptr;
  }
}

} // namespace NoMoreDay::systems
