#include "engine/render/GPUEntitySystem.hpp"
#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include "raymath.h"
#include "rlgl.h"
#include "raylib.h" // Added for GetFrameTime()

namespace NoMoreDay::systems {

using namespace components;

void GPUEntitySystem::Init(ResourceManager &resources, int maxEntities, entt::registry* registry) {
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

  m_freeSlots.reserve(m_maxEntities);
  m_freeSlots.clear(); // Ensure clean start
  for (int i = m_maxEntities - 1; i >= 0; --i) {
    m_freeSlots.push_back(i);
  }
  m_slotToEntity.assign(m_maxEntities, entt::null);

  if (registry) {
    registry->on_destroy<GPUIndex>().connect<&GPUEntitySystem::OnGPUIndexDestroyed>(this);
  }

  InitRender(resources);
  NoMoreDay::render::MDIRenderer::Get().Init(resources, m_maxEntities);
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

void GPUEntitySystem::Render(const NoMoreDay::SharedContext &context) {
  bool useMDI = true;
  if (useMDI && NoMoreDay::render::MDIRenderer::Get().IsInitialized()) {
    Matrix mv = rlGetMatrixModelview();
    Matrix proj = rlGetMatrixProjection();
    Matrix mvp = MatrixMultiply(mv, proj);

    Matrix invMVP = MatrixInvert(mvp);
    Vector3 ndcMin = {-1.0f, -1.0f, 0.0f}, ndcMax = {1.0f, 1.0f, 0.0f};
    Vector3 worldMin = Vector3Transform(ndcMin, invMVP),
            worldMax = Vector3Transform(ndcMax, invMVP);

    Vector4 viewBounds = {fminf(worldMin.x, worldMax.x) - 500.0f,
                          fminf(worldMin.y, worldMax.y) - 500.0f,
                          fmaxf(worldMin.x, worldMax.x) + 500.0f,
                          fmaxf(worldMin.y, worldMax.y) + 500.0f};

    m_persistentEntityBuffer.BindPreviousNoSync(0);
    auto &mdi = NoMoreDay::render::MDIRenderer::Get();
    mdi.Cull(viewBounds);
    mdi.Render(mvp, context.renderAlpha);
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

  m_persistentEntityBuffer.BindPrevious(1);
  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 4, m_maxEntities);
  rlDisableVertexArray();
  rlDisableShader();
}

void GPUEntitySystem::OnGPUIndexDestroyed(entt::registry &registry, entt::entity entity) {
  auto& gpuIdx = registry.get<GPUIndex>(entity);
  if (gpuIdx.index != -1) {
    int slot = gpuIdx.index;
    m_freeSlots.push_back(slot);
    m_slotToEntity[slot] = entt::null;
    
    // Crucial: Clear shadow buffer IMMEDIATELY so the next Update uploads radius=0
    m_shadowBuffer[slot].radius = 0.0f;
    m_shadowBuffer[slot].position = {0, 0};
    m_visualStatsShadowBuffer[slot] = {};
    
    gpuIdx.index = -1;
  }
}

void GPUEntitySystem::Update(entt::registry &registry, float dt) {
  m_frameCounter++;

  if (m_shadowBuffer.size() != m_maxEntities) {
    m_shadowBuffer.assign(m_maxEntities, {});
    m_visualStatsShadowBuffer.assign(m_maxEntities, {});
  }

  // Get mapped pointer for the current frame's slot
  components::GPUEntity *gpuPtr =
      (components::GPUEntity *)m_persistentEntityBuffer.BeginWrite();

  // Phase 1: Slot Reclamation & Assignment
  auto group = registry.group<Position, Radius, GPUIndex>();
  
  // Identify active entities and ensure they have slots
  std::vector<int> currentActiveSlots;
  currentActiveSlots.reserve(group.size());

  for (auto entity : group) {
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity)) {
        // Entity should NOT be on GPU (Projectiles are short-lived, killed are gone)
        auto& gpuIdx = group.get<GPUIndex>(entity);
        if (gpuIdx.index != -1) {
            int slot = gpuIdx.index;
            m_freeSlots.push_back(slot);
            m_slotToEntity[slot] = entt::null;
            m_shadowBuffer[slot].radius = 0.0f;
            m_visualStatsShadowBuffer[slot] = {};
            gpuIdx.index = -1;
        }
        continue;
    }

    auto& gpuIdx = group.get<GPUIndex>(entity);
    if (gpuIdx.index == -1) {
        if (!m_freeSlots.empty()) {
            gpuIdx.index = m_freeSlots.back();
            m_freeSlots.pop_back();
            m_slotToEntity[gpuIdx.index] = entity;
            
            const auto& pos = group.get<Position>(entity);
            m_shadowBuffer[gpuIdx.index].position = {pos.x, pos.y};
            m_shadowBuffer[gpuIdx.index].prevPosition = {pos.x, pos.y};
        } else {
            continue; 
        }
    }
    
    int slot = gpuIdx.index;
    const auto &pos = group.get<Position>(entity);
    const auto &radius = group.get<Radius>(entity);
    const auto &vel = registry.get_or_emplace<Velocity>(entity, 0.0f, 0.0f);

    auto &gpuEntity = m_shadowBuffer[slot];
    
    // [TELEPORT SNAP] Detect massive jumps (Recycling/Spawn/Logic Teleport)
    // If moved > 100px in one frame, snap prevPosition to avoid "Stretch/Ghosting"
    float dx = pos.x - gpuEntity.position.x;
    float dy = pos.y - gpuEntity.position.y;
    if (dx*dx + dy*dy > 100.0f * 100.0f) {
        gpuEntity.position = {pos.x, pos.y};
        gpuEntity.prevPosition = {pos.x, pos.y};
    } else {
        gpuEntity.prevPosition = gpuEntity.position;
        gpuEntity.position = {pos.x, pos.y};
    }

    gpuEntity.velocity = {vel.vx, vel.vy};
    gpuEntity.radius = radius.value;
    gpuEntity.type = (uint32_t)(registry.all_of<EnemyTag>(entity) ? 1 : 0);
    gpuEntity.frameId = (uint32_t)m_frameCounter;

    uint32_t flags = 0;
    if (registry.all_of<PlayerTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_KINEMATIC | GPU_ENTITY_FLAG_NO_RENDER;
    } else if (registry.all_of<SpriteComponent>(entity)) {
      flags |= GPU_ENTITY_FLAG_NO_RENDER;
    }

    if (auto *ai = registry.try_get<AIComponent>(entity)) {
      uint8_t stateVal = static_cast<uint8_t>(ai->aiType);
      flags |= GPUFlags::PackAIState(stateVal);
    }
    gpuEntity.flags = flags;

    // Stats
    auto &visualStats = m_visualStatsShadowBuffer[slot];
    if (auto *stats = registry.try_get<CombatStats>(entity)) {
      AttributePipeline::ToGPU(*stats, visualStats);
    } else {
      visualStats = {};
    }
  }

  // Bulk Upload
  memcpy(gpuPtr, m_shadowBuffer.data(),
         m_maxEntities * sizeof(components::GPUEntity));

  NoMoreDay::render::MDIRenderer::Get().UpdateStats(m_visualStatsShadowBuffer);
  m_persistentEntityBuffer.Flush();

  // Update Crowd Density for Flow Field (Uses internal grid in GPUFlowFieldSystem)
  GPUFlowFieldSystem::Get().UpdateCrowdDensity(m_persistentEntityBuffer,
                                               m_maxEntities, 10.0f);

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
  NoMoreDay::render::MDIRenderer::Get().Shutdown();
}

} // namespace NoMoreDay::systems