#include "engine/render/GPUEntitySystem.hpp"
#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
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
    // Added: Reclaim slot when Position or Radius is removed
    registry->on_destroy<Position>().connect<&GPUEntitySystem::OnGPUIndexDestroyed>(this);
    registry->on_destroy<Radius>().connect<&GPUEntitySystem::OnGPUIndexDestroyed>(this);
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

void GPUEntitySystem::Render(const NoMoreDay::SharedContext &context, const Camera2D &camera) {
  if (m_maxEntities > 0) {
    // Calculate View Bounds for Culling
    Vector2 worldMin = GetScreenToWorld2D({0, 0}, camera);
    Vector2 worldMax = GetScreenToWorld2D(
        {(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    Vector4 viewBounds = {fminf(worldMin.x, worldMax.x) - 500.0f,
                          fminf(worldMin.y, worldMax.y) - 500.0f,
                          fmaxf(worldMin.x, worldMax.x) + 500.0f,
                          fmaxf(worldMin.y, worldMax.y) + 500.0f};

    auto &mdi = NoMoreDay::render::MDIRenderer::Get();
    m_persistentEntityBuffer.BindPreviousNoSync(0); // Bind Binding 0 for Cull CS
    mdi.Cull(viewBounds);
    mdi.Render(*context.resources, m_persistentEntityBuffer, context.renderAlpha);
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

void GPUEntitySystem::OnGPUIndexDestroyed(entt::registry &registry,
                                        entt::entity entity) {
  auto* gpuIdx = registry.try_get<GPUIndex>(entity);
  if (gpuIdx && gpuIdx->index != -1) {
    int slot = gpuIdx->index;

    // Bounds Check: Prevent corruption if index is garbage
    if (slot < 0 || slot >= m_maxEntities) {
      LOG_ERROR("[GPUEntitySystem] CRITICAL: Invalid GPUIndex {} on entity {} "
                "during destruction!",
                slot, (uint32_t)entity);
      gpuIdx->index = -1;
      return;
    }

    m_freeSlots.push_back(slot);
    m_slotToEntity[slot] = entt::null;

    // Crucial: Clear shadow buffer IMMEDIATELY so the next Update uploads
    // radius=0
    m_shadowBuffer[slot].radius = 0.0f;
    m_shadowBuffer[slot].position = {0, 0};
    m_visualStatsShadowBuffer[slot] = {};

    gpuIdx->index = -1;
  }
}

void GPUEntitySystem::Update(entt::registry &registry, float dt) {
  m_frameCounter++;
  float currentTime = (float)GetTime();

  if (m_shadowBuffer.size() != m_maxEntities) {
    m_shadowBuffer.assign(m_maxEntities, {});
    m_visualStatsShadowBuffer.assign(m_maxEntities, {});
  }

  // Get mapped pointer for the current frame's slot
  components::GPUEntity *gpuPtr =
      (components::GPUEntity *)m_persistentEntityBuffer.BeginWrite();

  // Phase 1: Slot Reclamation & Assignment
  // Use view instead of group to avoid conflicting with the 4-component
  // Owned Group registered in GroupRegistry. Conflicting groups break EnTT
  // invariants and cause memory corruption.
  int highWaterMark = 0; // Track the maximum slot index used to optimize memcpy
  auto view = registry.view<Position, Radius, GPUIndex>();

  for (auto entity : view) {
    auto& gpuIdx = view.get<GPUIndex>(entity);
    
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity)) {
        if (gpuIdx.index != -1) {
            int slot = gpuIdx.index;
            if (slot >= 0 && slot < m_maxEntities) {
                m_freeSlots.push_back(slot);
                m_slotToEntity[slot] = entt::null;
                m_shadowBuffer[slot].radius = 0.0f;
                m_visualStatsShadowBuffer[slot] = {};
            }
            gpuIdx.index = -1;
        }
        continue;
    }

    if (gpuIdx.index == -1) {
        if (!m_freeSlots.empty()) {
            gpuIdx.index = m_freeSlots.back();
            m_freeSlots.pop_back();
            m_slotToEntity[gpuIdx.index] = entity;
            
            const auto& pos = view.get<Position>(entity);
            const auto& radius = view.get<Radius>(entity);
            m_shadowBuffer[gpuIdx.index].position = Vector2{pos.x, pos.y};
            m_shadowBuffer[gpuIdx.index].prevPosition = Vector2{pos.x, pos.y};
            m_shadowBuffer[gpuIdx.index].radius = radius.value;
            m_shadowBuffer[gpuIdx.index].frameId = (uint32_t)m_frameCounter;
        } else {
            continue; 
        }
    }
    
    int slot = gpuIdx.index;
    if (slot < 0 || slot >= m_maxEntities) continue;
    if (slot > highWaterMark) highWaterMark = slot;

    const auto &pos = view.get<Position>(entity);
    const auto &radius = view.get<Radius>(entity);
    
    auto* velPtr = registry.try_get<Velocity>(entity);
    Vector2 velocity = velPtr ? Vector2{velPtr->vx, velPtr->vy} : Vector2{0, 0};

    auto &gpuEntity = m_shadowBuffer[slot];
    
    float dx = pos.x - gpuEntity.position.x;
    float dy = pos.y - gpuEntity.position.y;
    if (dx*dx + dy*dy > 100.0f * 100.0f) {
        gpuEntity.position = Vector2{pos.x, pos.y};
        gpuEntity.prevPosition = Vector2{pos.x, pos.y};
    } else {
        gpuEntity.prevPosition = gpuEntity.position;
        gpuEntity.position = Vector2{pos.x, pos.y};
    }

    gpuEntity.velocity = velocity;
    gpuEntity.radius = radius.value;
    gpuEntity.frameId = (uint32_t)m_frameCounter;

    if (auto* sprite = registry.try_get<SpriteComponent>(entity)) {
        gpuEntity.type = sprite->textureLayerIndex;
    } else {
        gpuEntity.type = NoMoreDay::Constants::GPU::SDF_CIRCLE_TYPE;
    }

    uint32_t flags = 0;
    if (registry.all_of<PlayerTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_KINEMATIC | GPU_ENTITY_FLAG_NO_RENDER;
    }

    if (auto *ai = registry.try_get<AIComponent>(entity)) {
      uint8_t stateVal = static_cast<uint8_t>(ai->aiType);
      flags |= GPUFlags::PackAIState(stateVal);
    }
    gpuEntity.flags = flags;

    // Stats and Status Effects Sync (With Dirty Check or Logic limit)
    // Only full sync if StatsDirty component is present or every N frames
    bool needsStatsSync = registry.any_of<StatsDirty>(entity) || (m_frameCounter % 5 == 0);
    
    auto &visualStats = m_visualStatsShadowBuffer[slot];
    if (needsStatsSync) {
        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          AttributePipeline::ToGPU(*stats, visualStats);
        } else {
          visualStats = {};
        }
        
        visualStats.activeStatusMask = 0;
        if (auto* effects = registry.try_get<ActiveEffectsComponent>(entity)) {
            for (const auto& effect : effects->effects) {
                switch (effect.type) {
                    case BuffType::Freeze: visualStats.activeStatusMask |= NoMoreDay::Constants::GPU::STATUS_FROZEN; break;
                    case BuffType::Burn: visualStats.activeStatusMask |= NoMoreDay::Constants::GPU::STATUS_BURNING; break;
                    case BuffType::Poison: visualStats.activeStatusMask |= NoMoreDay::Constants::GPU::STATUS_POISONED; break;
                    case BuffType::Shock: visualStats.activeStatusMask |= NoMoreDay::Constants::GPU::STATUS_SHOCKED; break;
                    default: break;
                }
            }
        }
    }
    visualStats.statusTimer = currentTime;
  }

  // Bulk Upload
  // Optimization: Only copy up to the highest used slot index
  size_t copyCount = std::min((size_t)highWaterMark + 128, (size_t)m_maxEntities);
  
  memcpy(gpuPtr, m_shadowBuffer.data(),
         copyCount * sizeof(components::GPUEntity));

  NoMoreDay::render::MDIRenderer::Get().UpdateStats(m_visualStatsShadowBuffer, (int)copyCount);
  m_persistentEntityBuffer.Flush();

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