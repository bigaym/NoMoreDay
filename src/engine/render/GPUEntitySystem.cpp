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

  for (int i = 0; i < 3; ++i) {
    m_slotToEntities[i].resize(m_maxEntities, entt::null);
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

    Vector4 viewBounds = {fminf(worldMin.x, worldMax.x) - 120.0f,
                          fminf(worldMin.y, worldMax.y) - 120.0f,
                          fmaxf(worldMin.x, worldMax.x) + 120.0f,
                          fmaxf(worldMin.y, worldMax.y) + 120.0f};

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

void GPUEntitySystem::Update(entt::registry &registry, float dt) {
  m_frameCounter++;

  // Ensure shadow buffer is sized correctly (lazy init if maxEntities changed,
  // though usually static)
  if (m_shadowBuffer.size() != m_maxEntities) {
    m_shadowBuffer.resize(m_maxEntities);
    m_visualStatsShadowBuffer.resize(
        m_maxEntities); // Ensure stats buffer is also resized
    // Initialize with zero radius to hide unused slots by default
    for (auto &e : m_shadowBuffer)
      e.radius = 0.0f;
  }

  // Get mapped pointer for the current frame's slot
  components::GPUEntity *gpuPtr =
      (components::GPUEntity *)m_persistentEntityBuffer.BeginWrite();

  auto group = registry.group<Position, Velocity, Radius, GPUIndex>();
  int index = 0;
  int currentWriteSlot = m_persistentEntityBuffer.GetCurrentSlot();
  m_activeCountPerSlot[currentWriteSlot] = 0;
  // No need to clear the whole vector, we will track active count
  // m_slotToEntities[currentWriteSlot].assign(m_maxEntities, entt::null);

  // Partial Update Optimization:
  // 1. Update m_shadowBuffer from Registry (CPU -> CPU), only for active
  // entities.
  // 2. We could optimize this further by only iterating dirty entities, but
  // iterating the Group
  //    is fast enough for 20k entities. The main saving is establishing the
  //    coherent ShadowBuffer. Actually, checking 'isDirty' prevents redundant
  //    component reads/writes to shadow buffer.

  constexpr float DIRTY_THRESHOLD_SQ = 0.5f * 0.5f;

  for (auto entity : group) {
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity) ||
        index >= m_maxEntities)
      continue;

    // Assign GPU Index dynamically
    group.get<GPUIndex>(entity).index = index;
    m_slotToEntities[currentWriteSlot][index] = entity;

    // Check Dirty Status (Optimization REMOVED for correctness)
    // EnTT group iteration order is NOT stable when entities are created/destroyed (swap-remove).
    // An entity at index 'i' this frame might be different from 'i' last frame.
    // If we skip update for static entities, m_shadowBuffer[i] contains STALE data from the previous occupant.
    // This caused the "Static Monster Flickering" bug.
    if (auto *dtComp = registry.try_get<DirtyTransform>(entity)) {
      dtComp->isDirty = false; // consume flag
    }

    const auto &pos = group.get<Position>(entity);
    const auto &vel = group.get<Velocity>(entity);
    const auto &radius = group.get<Radius>(entity);

    // Get or initialize previous position for interpolation
    auto &prevPos =
        registry.get_or_emplace<PrevPosition>(entity, pos.x, pos.y);

    // [FIX] Teleport/Spawn Glitch Prevention
    // If prevPos is too far from current pos (e.g. just spawned, recycled, or teleported),
    // the render interpolation (mix(prev, curr, alpha)) will cause visual artifacts.
    float dx = pos.x - prevPos.x;
    float dy = pos.y - prevPos.y;
    bool isTeleport = (dx * dx + dy * dy > 100.0f * 100.0f); // 100 pixel threshold

    if (isTeleport) {
        prevPos.x = pos.x;
        prevPos.y = pos.y;
    }

    // Update Shadow Buffer
    auto &gpuEntity = m_shadowBuffer[index];
    gpuEntity.position = {pos.x, pos.y};
    gpuEntity.prevPosition = {prevPos.x, prevPos.y};
    
    // [FIX] Zero-out velocity on teleport/spawn to prevent physics overshoot/oscillation
    // If we recycled an entity that was moving fast, we don't want that ghost velocity.
    if (isTeleport) {
        gpuEntity.velocity = {0.0f, 0.0f};
    } else {
        gpuEntity.velocity = {vel.vx, vel.vy};
    }
    gpuEntity.radius = radius.value;
    gpuEntity.type = (uint32_t)(registry.all_of<EnemyTag>(entity) ? 1 : 0);

    uint32_t flags = 0;
    if (registry.all_of<PlayerTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_KINEMATIC;
      flags |= GPU_ENTITY_FLAG_NO_RENDER;
    } else if (registry.all_of<SpriteComponent>(entity)) {
      flags |= GPU_ENTITY_FLAG_NO_RENDER;
    }

    if (auto *ai = registry.try_get<AIComponent>(entity)) {
      // Legacy flag for fast check (Keep for now or removing depending on preference, plan says keep for compat)
      if (ai->aiType == AIType::CHASE ||
          ai->aiType == AIType::NEMESIS_HUNTER) {
        flags |= GPU_ENTITY_FLAG_CHASING;
      }
      
      // Phase 2: AI State Sync (Bits 8-15)
      // Ensure AIType fits in 8 bits
      static_assert(static_cast<int>(AIType::TANK_BLOCK) < 256, "AI State must fit in 8 bits");
      uint8_t stateVal = static_cast<uint8_t>(ai->aiType);
      flags |= GPUFlags::PackAIState(stateVal);
    }

    gpuEntity.flags = flags;

    // Store current position for the next physics step's prevPosition
    prevPos.x = pos.x;
    prevPos.y = pos.y;

    // Always update stats for valid entities (or check dirty if stats have
    // dirty flag, but for now update always)
    auto &visualStats = m_visualStatsShadowBuffer[index];
    if (auto *stats = registry.try_get<CombatStats>(entity)) {
      AttributePipeline::ToGPU(*stats, visualStats);
    } else {
      visualStats = {};
    }

    index++;
  }
  m_activeCountPerSlot[currentWriteSlot] = index;

  // Optimized clearing: Only clear what was potentially used in the previous frame's slot
  // or simply keep a "lastIndex" and clear from index to lastIndex.
  // Actually, since we use triple buffering, it's safer to clear up to the max index 
  // reached in the last few frames, but for simplicity, let's just 
  // use a smaller limit if possible, or only zero what is strictly needed.
  static int lastMaxIndex = 0;
  int currentClearLimit = std::max(index + 100, lastMaxIndex); 
  currentClearLimit = std::min(currentClearLimit, m_maxEntities);

  for (int i = index; i < currentClearLimit; ++i) {
    m_shadowBuffer[i].radius = 0.0f;
    m_visualStatsShadowBuffer[i] = {};
  }
  lastMaxIndex = index;

  // Bulk Upload: Copy entire Shadow Buffer to GPU Mapped Memory
  memcpy(gpuPtr, m_shadowBuffer.data(),
         m_maxEntities * sizeof(components::GPUEntity));

  // Upload Visual Stats
  NoMoreDay::render::MDIRenderer::Get().UpdateStats(m_visualStatsShadowBuffer);

  m_persistentEntityBuffer.Flush();

  int gridCols = (int)m_mapBoundary / 32 + 1, gridRows = (int)m_mapBoundary / 32 + 1,
      numCells = gridCols * gridRows;
  rlEnableShader(m_gridClearShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridClearShader.id, "numCells"),
               &numCells, RL_SHADER_UNIFORM_INT, 1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  rlBindShaderBuffer(m_tempCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  // Only need SSBO barrier for subsequent counting
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  rlEnableShader(m_gridCountShader.id);
  float cellSize = 32.0f;
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "maxEntities"),
               &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "cellSize"),
               &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridCols"),
               &gridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridRows"),
               &gridRows, RL_SHADER_UNIFORM_INT, 1);
  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((index + 255) / 256, 1, 1);
  // Barrier for subsequent scan
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // 3-Pass GPU Prefix Sum (Scan)
  rlEnableShader(m_gridScanShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "numCells"), &numCells,
               RL_SHADER_UNIFORM_INT, 1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
  rlBindShaderBuffer(m_blockSumBuffer.GetId(), 6);

  // Pass 1: Local Scan
  int mode = 0;
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "mode"), &mode,
               RL_SHADER_UNIFORM_INT, 1);
  int blockCount = (numCells + 511) / 512;
  rlComputeShaderDispatch(blockCount, 1, 1);
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Pass 2: Block Scan
  mode = 1;
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "mode"), &mode,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "numCells"),
               &blockCount, RL_SHADER_UNIFORM_INT, 1); // numBlocks for this pass
  rlComputeShaderDispatch(1, 1, 1);
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Pass 3: Combine
  mode = 2;
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "mode"), &mode,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridScanShader.id, "numCells"), &numCells,
               RL_SHADER_UNIFORM_INT, 1);
  rlComputeShaderDispatch(blockCount, 1, 1);
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  rlEnableShader(m_gridSortShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "maxEntities"),
               &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "cellSize"), &cellSize,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridCols"), &gridCols,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridRows"), &gridRows,
               RL_SHADER_UNIFORM_INT, 1);
  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
  rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);
  rlBindShaderBuffer(m_tempCountBuffer.GetId(), 5);
  rlComputeShaderDispatch((index + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  GPUFlowFieldSystem::Get().UpdateCrowdDensity(m_persistentEntityBuffer,
                                               m_maxEntities, 10.0f);

  rlEnableShader(m_physicsShader.id);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "dt"), &dt,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "maxEntities"),
               &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "cellSize"), &cellSize,
               RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridCols"), &gridCols,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridRows"), &gridRows,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "mapBoundary"),
               &m_mapBoundary, RL_SHADER_UNIFORM_FLOAT, 1);
  
  uint32_t currentFrame = (uint32_t)m_frameCounter;
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "currentFrame"),
               &currentFrame, RL_SHADER_UNIFORM_UINT, 1);

  const auto &flowSystem = GPUFlowFieldSystem::Get();
  flowSystem.GetFlowBuffer().BindBase(6);
  int fw = flowSystem.GetWidth(), fh = flowSystem.GetHeight();
  Vector2 fo = flowSystem.GetGridOrigin();
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowWidth"), &fw,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowHeight"), &fh,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowOrigin"), &fo,
               RL_SHADER_UNIFORM_VEC2, 1);

  m_persistentEntityBuffer.BindBase(1); // InEntityBuffer
  rlBindShaderBuffer(m_physicsOutputBuffer.GetId(),
                     5); // OutEntityBuffer (Binding 5 matches shader)
  m_physicsOutputBuffer.BindBase(5); // Ensure current slot is bound

  // [CRITICAL FIX] Dispatch for ALL potential entities, not just active ones.
  // This ensures that non-active slots (radius <= 0) are correctly cleared in the output buffer.
  // Without this, "Ghost Entities" from N-2 frames (Triple Buffering) persist in the output,
  // causing invisible collisions and "teleportation" bugs.
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  rlDisableShader();
  m_persistentEntityBuffer.Lock();
  m_physicsOutputBuffer.Lock();
}

void GPUEntitySystem::SyncBack(entt::registry &registry) {
  if (m_frameCounter < 3)
    return;

  // Read back from the oldest buffer (N-2)
  int bufferCount = m_physicsOutputBuffer.GetBufferCount();
  int readSlot = (m_physicsOutputBuffer.GetCurrentSlot() - 2 + bufferCount) % bufferCount;
  
  m_physicsOutputBuffer.ReadFromSlot(m_localData.data(),
                                     m_maxEntities * sizeof(components::GPUEntity), 
                                     readSlot);

  const auto &entitiesInReadSlot = m_slotToEntities[readSlot];
  int activeCount = m_activeCountPerSlot[readSlot];

  for (int i = 0; i < activeCount; ++i) {
    entt::entity entity = entitiesInReadSlot[i];
    if (entity == entt::null || !registry.valid(entity))
      continue;
    if (registry.all_of<PlayerTag>(entity))
      continue;

    auto &gpu = m_localData[i];
    
    // [SAFETY] Stale Data Protection (Ring Buffer Sync)
    // If the GPU data corresponds to an older frame (because compute shader skipped it),
    // we MUST ignore it to prevent "Teleport to Past" artifacts.
    // We expect gpu.frameId to be roughly (m_frameCounter - 2).
    // Due to update order, gpu.frameId might be (m_frameCounter - 1) or equal.
    // Crucially, it must NOT be 0 (uninitialized/copied from inactive) and NOT old.
    uint32_t expectedFrame = (uint32_t)(m_frameCounter - 3); // Relaxed safety bound
    
    if (gpu.frameId == 0 || gpu.frameId < expectedFrame) {
         // Log for debugging (Limit frequency)
         static int staleLogCount = 0;
         if (staleLogCount++ < 100 && gpu.frameId != 0) { // Don't log 0s, they are expected for inactive
             LOG_WARN("SyncBack: Stale Data for Entity {}. GPU Frame: {}, Threshold: {}", 
                      (uint32_t)entity, gpu.frameId, expectedFrame);
         }
         continue;
    }

    // [SAFETY] Sanity check for valid GPU data
    // If radius is 0, it means the slot was empty/cleared or not written to.
    if (gpu.radius <= 0.001f) {
        continue;
    }

    if (auto *pos = registry.try_get<Position>(entity)) {
        // [SAFETY] Zero-Coordinate Glitch Protection
        // If GPU returns exact (0,0) but entity is far away, it's likely a buffer read error.
        if (std::abs(gpu.position.x) < 0.01f && std::abs(gpu.position.y) < 0.01f) {
            if (pos->x * pos->x + pos->y * pos->y > 100.0f) {
                // Log only once per second to avoid spam
                static int logTimer = 0;
                if (logTimer++ % 60 == 0) {
                     LOG_WARN("SyncBack: Ignored invalid GPU (0,0) for Entity {}. CPU Pos: ({}, {})", (uint32_t)entity, pos->x, pos->y);
                }
                continue;
            }
        }

        // [FIX] Dormancy Check
        // If the entity is dormant (placed at sentinel coordinates), do NOT sync back.
        // This prevents pulling dormant entities back into the world due to stale GPU data.
        if (pos->x <= -990.0f) {
            continue;
        }

        // Significant change threshold (0.5 pixel)
        float dx = gpu.position.x - pos->x;
        float dy = gpu.position.y - pos->y;
        float distSq = dx * dx + dy * dy;

        // [DEBUG] Log massive jumps
        if (distSq > 50.0f * 50.0f) {
             LOG_WARN("SyncBack: Huge Jump Detected for Entity {}. CPU: ({}, {}) -> GPU: ({}, {}). Slot: {}", 
                      (uint32_t)entity, pos->x, pos->y, gpu.position.x, gpu.position.y, readSlot);
        }

        if (distSq > 0.25f) { // 0.5^2
          registry.get_or_emplace<DirtyTransform>(entity).isDirty = true;
        }

        // Update PrevPosition for stable interpolation
        auto &prev =
            registry.get_or_emplace<PrevPosition>(entity, pos->x, pos->y);
        prev.x = pos->x;
        prev.y = pos->y;

        // [FIX] Extrapolate to compensate for N-2 Readback Latency (32ms)
        float simDt = GetFrameTime();
        if (simDt > 0.1f) simDt = 0.1f; 

        float predictedX = gpu.position.x + gpu.velocity.x * (2.0f * simDt);
        float predictedY = gpu.position.y + gpu.velocity.y * (2.0f * simDt);

        // Clamping to map boundary (simple safety)
        if(predictedX < 0) predictedX = 0;
        if(predictedY < 0) predictedY = 0;
        if(predictedX > m_mapBoundary) predictedX = m_mapBoundary;
        if(predictedY > m_mapBoundary) predictedY = m_mapBoundary;
        
        // [CRITICAL FIX] Teleport Protection / Desync Prevention
        float deltaX = predictedX - pos->x;
        float deltaY = predictedY - pos->y;
        float predDistSq = deltaX*deltaX + deltaY*deltaY;

        // Threshold: Increased from 64.0f (8px) to 10000.0f (100px)
        // This prevents rubberbanding when entities move fast or accelerate,
        // allowing the GPU physics to remain authoritative unless a massive teleport occurs.
        constexpr float TELEPORT_THRESHOLD_SQ = 100.0f * 100.0f;

        if (predDistSq > TELEPORT_THRESHOLD_SQ) {
            // Logic moved the entity. Trust CPU, ignore GPU this frame.
            prev.x = pos->x;
            prev.y = pos->y;
        } else {
            // Normal physics sync
            pos->x = predictedX;
            pos->y = predictedY;
            
            if (auto *vel = registry.try_get<Velocity>(entity)) {
                vel->vx = gpu.velocity.x;
                vel->vy = gpu.velocity.y;
            }
        }
    }
  }
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