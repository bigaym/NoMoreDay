#include "engine/render/GPUFlowFieldSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
// RenderConstants::FlowFieldCS defines binding point semantics
#include "engine/render/RenderConstants.hpp"
#include "rlgl.h"

namespace NoMoreDay::systems {

void GPUFlowFieldSystem::Init(ResourceManager &resources, int width,
                              int height) {
  LOG_INFO("Initializing GPUFlowFieldSystem ({}x{})...", width, height);

  m_width = width;
  m_height = height;
  m_cellSize = 10.0f; // Default for MapSystem

  m_resetShader = resources.loadComputeShader(
      entt::hashed_string{"flow_reset"}, "assets/shaders/flow_reset.compute");
  m_flowShader = resources.loadComputeShader(
      entt::hashed_string{"flow_vector"}, "assets/shaders/flow_vector.compute");
  m_integrationShader =
      resources.loadComputeShader(entt::hashed_string{"flow_integration"},
                                  "assets/shaders/flow_integration.compute");
  m_gridCountShader = resources.loadComputeShader(
      entt::hashed_string{"grid_count"}, "assets/shaders/grid_count.compute");
  m_gridClearShader = resources.loadComputeShader(
      entt::hashed_string{"grid_clear"}, "assets/shaders/grid_clear.compute");

  size_t cellCount = (size_t)width * height;

  // 1. Cost Buffer (uint32_t for alignment)
  // Initialize with 255 (Wall)
  m_costBuffer.Create(cellCount * sizeof(uint32_t));
  uint32_t *costPtr = (uint32_t *)m_costBuffer.BeginWrite();
  for (size_t i = 0; i < cellCount; ++i)
    costPtr[i] = 255;
  m_costBuffer.Flush();

  // 2. Density Buffer
  std::vector<uint32_t> initialDensity(cellCount, 0);
  m_densityBuffer.Create(cellCount * sizeof(uint32_t), initialDensity.data(),
                         RL_DYNAMIC_DRAW);

  // 3. Integration Buffers (uint32_t)
  // Initialize with max int
  std::vector<uint32_t> initialInt(cellCount, 0xFFFFFFFF);
  m_integrationBuffer.Create(cellCount * sizeof(uint32_t), initialInt.data(),
                             RL_DYNAMIC_DRAW);
  m_integrationBuffer2.Create(cellCount * sizeof(uint32_t), initialInt.data(),
                              RL_DYNAMIC_DRAW);

  // 3. Flow Buffer (Vector2)
  // Initialize with zero (triple buffered for async readback ring)
  m_flowBuffer.Create(cellCount * sizeof(Vector2), 3);
  Vector2 *flowPtr = (Vector2 *)m_flowBuffer.BeginWrite();
  for (size_t i = 0; i < cellCount; ++i)
    flowPtr[i] = {0.0f, 0.0f};
  m_flowBuffer.Flush();

  // Initialize Shadow Buffer
  m_flowFieldShadow.assign(cellCount, {0.0f, 0.0f});

  LOG_INFO("GPUFlowFieldSystem buffers allocated.");
}

void GPUFlowFieldSystem::Update(const std::vector<unsigned char> &fullCostMap,
                                int mapW, int mapH, Vector2 targetPos,
                                Vector2 gridOrigin, const render::PersistentBuffer* entityBuffer, int entityCount) {
  m_syncedThisFrame = false;
  if (m_width == 0 || m_height == 0)
    return;

  // Change Detection for Optimization
  Vector2 targetGrid = {(targetPos.x - gridOrigin.x) / m_cellSize,
                        (targetPos.y - gridOrigin.y) / m_cellSize};

  bool gridChanged = (gridOrigin.x != m_lastGridOrigin.x ||
                      gridOrigin.y != m_lastGridOrigin.y);
  bool targetChanged = ((int)targetGrid.x != (int)m_lastTargetGridPos.x ||
                        (int)targetGrid.y != (int)m_lastTargetGridPos.y);

  if (!m_forceUpdate && !gridChanged && !targetChanged) {
    return;
  }

  // If we reach here, we are doing a REAL update.
  // This is the perfect time to update crowd density if buffer is provided.
  if (entityBuffer && entityCount > 0) {
      UpdateCrowdDensity(*entityBuffer, entityCount, m_cellSize);
  }

  m_lastGridOrigin = gridOrigin;
  m_lastTargetGridPos = {(float)(int)targetGrid.x, (float)(int)targetGrid.y};
  m_forceUpdate = false;

  m_gridOrigin = gridOrigin;

  // 0. Extract Window from Full Cost Map
  size_t neededSize = (size_t)m_width * m_height;
  if (m_costCache.size() != neededSize) {
    m_costCache.resize(neededSize);
  }
  // Reset to Wall (255) defaults
  std::fill(m_costCache.begin(), m_costCache.end(), 255);

  int startX = (int)(gridOrigin.x / m_cellSize);
  int startY = (int)(gridOrigin.y / m_cellSize);

  // Fast path: the window is fully inside the full map, so the per-cell bounds
  // checks can be hoisted to a single row-level test (uint8 -> uint32 widening
  // copy, compiler can vectorize).
  const bool windowInsideMap = (startX >= 0 && startY >= 0 &&
                                startX + m_width <= mapW &&
                                startY + m_height <= mapH);
  if (windowInsideMap) {
    for (int y = 0; y < m_height; ++y) {
      const unsigned char *srcRow =
          fullCostMap.data() + (size_t)(startY + y) * mapW + startX;
      uint32_t *dstRow = m_costCache.data() + (size_t)y * m_width;
      for (int x = 0; x < m_width; ++x) {
        dstRow[x] = (uint32_t)srcRow[x];
      }
    }
  } else {
    // Slow path: window partially outside the full map (per-cell bounds check).
    for (int y = 0; y < m_height; ++y) {
      int my = startY + y;
      if (my < 0 || my >= mapH)
        continue;

      for (int x = 0; x < m_width; ++x) {
        int mx = startX + x;
        if (mx < 0 || mx >= mapW)
          continue;

        size_t fullIdx = (size_t)my * mapW + mx;
        m_costCache[(size_t)y * m_width + x] = (uint32_t)fullCostMap[fullIdx];
      }
    }
  }

  uint32_t *costPtr = (uint32_t *)m_costBuffer.BeginWrite();
  memcpy(costPtr, m_costCache.data(), m_costCache.size() * sizeof(uint32_t));
  m_costBuffer.Flush();

  // Bind Buffers to Bindings (RenderConstants::FlowFieldCS semantics)
  // Binding 0: Cost (readonly)
  // Binding 1: Integration (readwrite)
  // Binding 2: Flow (writeonly)
  using namespace NoMoreDay::RenderConstants;
  m_costBuffer.BindBase(FlowFieldCS::COST_FIELD);
  m_integrationBuffer.BindBase(FlowFieldCS::INTEGRATION_READ);
  m_flowBuffer.BindBase(FlowFieldCS::FLOW_FIELD);

  // 1. Reset Integration Field (single dispatch; the in-place relaxation below
  //    only needs one buffer seeded to 0xFFFFFFFF / target = 0).
  rlEnableShader(m_resetShader.id);

  // Vector2 targetGrid already calculated at top of function

  int locW = rlGetLocationUniform(m_resetShader.id, "width");
  int locH = rlGetLocationUniform(m_resetShader.id, "height");
  int locTarget = rlGetLocationUniform(m_resetShader.id, "targetPos");

  int targetX = (int)targetGrid.x;
  int targetY = (int)targetGrid.y;
  int targetIVec[2] = {targetX, targetY};

  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locTarget, targetIVec, RL_SHADER_UNIFORM_IVEC2, 1);

  m_integrationBuffer.BindBase(FlowFieldCS::INTEGRATION_READ);
  utils::GPUUtils::ScopedDebugGroup debugGroupReset("FlowReset");
  rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

  utils::GPUUtils::MemoryBarrier();

  // 2. Integration (Iterative Relaxation): the sweeps run INSIDE the shader
  //    (flow_integration.compute loops `iterations` times per dispatch with a
  //    workgroup barrier between sweeps; the integration field is relaxed in
  //    place, Gauss-Seidel style, converging to the same distance field as the
  //    previous ping-pong Jacobi loop). barrier() only synchronizes a single
  //    workgroup, so sweeps that must cross 16x16 tile boundaries need a
  //    global SSBO barrier between dispatches. We therefore run 16 sweeps per
  //    dispatch and issue 4 dispatches for 64 sweeps total (same propagation
  //    depth as the original 64 one-iteration dispatches, ~3.5 ms on a
  //    256x224 grid because every dispatch + barrier fully serialized the GPU
  //    pipeline).
  rlEnableShader(m_integrationShader.id);
  locW = rlGetLocationUniform(m_integrationShader.id, "width");
  locH = rlGetLocationUniform(m_integrationShader.id, "height");
  int locWeight = rlGetLocationUniform(m_integrationShader.id, "densityWeight");
  int locIterations = rlGetLocationUniform(m_integrationShader.id, "iterations");

  const int kTotalSweeps = 64;       // Same total count as before: usually
                                     // enough for local propagation
  const int kSweepsPerDispatch = 16; // 4 dispatches + 4 SSBO barriers total
  const int sweepsPerDispatch = kSweepsPerDispatch;
  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
  if (locWeight >= 0)
    rlSetUniform(locWeight, &m_densityWeight, RL_SHADER_UNIFORM_FLOAT, 1);
  if (locIterations >= 0)
    rlSetUniform(locIterations, &sweepsPerDispatch, RL_SHADER_UNIFORM_INT, 1);

  m_costBuffer.BindBase(FlowFieldCS::COST_FIELD);
  m_densityBuffer.BindBase(FlowFieldCS::DENSITY_FIELD);
  // Single buffer, read and written in place through binding 1.
  m_integrationBuffer.BindBase(FlowFieldCS::INTEGRATION_READ);

  const int dispatchGroups = (kTotalSweeps + kSweepsPerDispatch - 1) / kSweepsPerDispatch;
  utils::GPUUtils::ScopedDebugGroup debugGroupIntegrate("FlowIntegrate");
  for (int i = 0; i < dispatchGroups; ++i) {
    rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
    // Global SSBO sync so the next batch of sweeps sees cross-tile updates.
    // Explicitly only sync SSBO writes.
    utils::GPUUtils::MemoryBarrier(Barrier::SSBO);
  }

  // 3. Vector Field Generation
  rlEnableShader(m_flowShader.id);
  locW = rlGetLocationUniform(m_flowShader.id, "width");
  locH = rlGetLocationUniform(m_flowShader.id, "height");
  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);

  m_integrationBuffer.BindBase(FlowFieldCS::INTEGRATION_READ);
  m_flowBuffer.BindBase(FlowFieldCS::FLOW_FIELD);

  utils::GPUUtils::ScopedDebugGroup debugGroupFlow("FlowVectorField");
  rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
  utils::GPUUtils::MemoryBarrier();

  rlDisableShader();

  m_costBuffer.Lock();
  m_flowBuffer.Lock();
}

void GPUFlowFieldSystem::UpdateCrowdDensity(
    const render::PersistentBuffer &entityBuffer, int entityCount,
    float cellSize) {
  if (m_width <= 0 || m_height <= 0)
    return;

  int numCells = m_width * m_height;

  using namespace NoMoreDay::RenderConstants;
  // 1. Clear density buffer
  rlEnableShader(m_gridClearShader.id);
  int locNumCells = rlGetLocationUniform(m_gridClearShader.id, "numCells");
  rlSetUniform(locNumCells, &numCells, RL_SHADER_UNIFORM_INT, 1);

  m_densityBuffer.BindBase(
      FlowFieldCS::FLOW_FIELD); // grid_clear uses binding 2
  utils::GPUUtils::ScopedDebugGroup debugGroupClear("FlowGridClear");
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier(Barrier::SSBO);

  // 2. Count
  rlEnableShader(m_gridCountShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "maxEntities"),
               &entityCount, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "cellSize"),
               &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridCols"), &m_width,
               RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridRows"),
               &m_height, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridOrigin"),
               &m_gridOrigin, RL_SHADER_UNIFORM_VEC2, 1);

  entityBuffer.BindBase(
      FlowFieldCS::INTEGRATION_READ); // grid_count uses binding 1 for entities
  m_densityBuffer.BindBase(
      FlowFieldCS::FLOW_FIELD); // and binding 2 for cell counts

  utils::GPUUtils::ScopedDebugGroup debugGroupCount("FlowGridCount");
  rlComputeShaderDispatch((entityCount + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier(Barrier::SSBO);

  rlDisableShader();
}

void GPUFlowFieldSystem::SyncToCPU() {
  if (m_width <= 0 || m_height <= 0 || m_syncedThisFrame)
    return;
  size_t cellCount = (size_t)m_width * m_height;

  // Ensure shadow buffer size is correct (though it should be from Init)
  if (m_flowFieldShadow.size() != cellCount) {
    m_flowFieldShadow.assign(cellCount, {0.0f, 0.0f});
  }

  // Zero CPU-GPU sync stalls in production: poll delayed slot without blocking
  if (m_flowBuffer.TryReadNonBlocking(m_flowFieldShadow.data(),
                                      cellCount * sizeof(Vector2), 1)) {
    m_hasValidSnapshot = true;
  }
  // AI uses the latest ready CPU snapshot
  m_syncedThisFrame = true;
}

void GPUFlowFieldSystem::DownloadFlowField(std::vector<Vector2>& out) const {
  size_t cellCount = (size_t)m_width * m_height;
  if (out.size() != cellCount) out.resize(cellCount);
  // T6.7: never block the main thread when no ready snapshot exists. Fall back
  // to the zero-initialized CPU shadow (safe until the first SyncToCPU).
  if (!m_flowBuffer.TryReadNonBlocking(out.data(), cellCount * sizeof(Vector2), 1)) {
    if (!m_flowFieldShadow.empty()) {
      std::memcpy(out.data(), m_flowFieldShadow.data(), cellCount * sizeof(Vector2));
    }
  }
}

void GPUFlowFieldSystem::Shutdown() {
  LOG_INFO("Shutting down GPUFlowFieldSystem...");
  m_costBuffer.Destroy();
  m_integrationBuffer.Release();
  m_integrationBuffer2.Release();
  m_densityBuffer.Release();
  m_flowBuffer.Destroy();
  
  m_flowFieldShadow.clear();
  m_flowFieldShadow.shrink_to_fit();
  m_costCache.clear();
  m_costCache.shrink_to_fit();
  m_hasValidSnapshot = false;
}

} // namespace NoMoreDay::systems
