#include "engine/render/GPUFlowFieldSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
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
  std::vector<uint32_t> initialCost(cellCount, 255);
  m_costBuffer.Create(cellCount * sizeof(uint32_t), initialCost.data(),
                      RL_DYNAMIC_DRAW);

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
  // Initialize with zero
  std::vector<Vector2> initialFlow(cellCount, {0.0f, 0.0f});
  m_flowBuffer.Create(cellCount * sizeof(Vector2), initialFlow.data(),
                      RL_DYNAMIC_DRAW);

  // Initialize Shadow Buffer
  m_flowFieldShadow = initialFlow;

  LOG_INFO("GPUFlowFieldSystem buffers allocated.");
}

void GPUFlowFieldSystem::Update(const std::vector<unsigned char> &fullCostMap,
                                int mapW, int mapH, Vector2 targetPos,
                                Vector2 gridOrigin) {
  m_syncedThisFrame = false;
  if (m_width == 0 || m_height == 0)
    return;

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

  for (int y = 0; y < m_height; ++y) {
    int my = startY + y;
    if (my < 0 || my >= mapH)
      continue;

    for (int x = 0; x < m_width; ++x) {
      int mx = startX + x;
      if (mx < 0 || mx >= mapW)
        continue;

      size_t fullIdx = (size_t)my * mapW + mx;
      m_costCache[y * m_width + x] = (uint32_t)fullCostMap[fullIdx];
    }
  }

  m_costBuffer.Update(m_costCache.data(),
                      m_costCache.size() * sizeof(uint32_t));

  // Bind Buffers to Bindings (Must match shader layout binding = X)
  // Binding 0: Cost (readonly)
  // Binding 1: Integration (readwrite)
  // Binding 2: Flow (writeonly)
  m_costBuffer.BindBase(0);
  m_integrationBuffer.BindBase(1);
  m_flowBuffer.BindBase(2);

  // 1. Reset Integration Field
  rlEnableShader(m_resetShader.id);

  m_integrationBuffer.BindBase(1);

  Vector2 targetGrid = {(targetPos.x - gridOrigin.x) / m_cellSize,
                        (targetPos.y - gridOrigin.y) / m_cellSize};
  int locW = rlGetLocationUniform(m_resetShader.id, "width");
  int locH = rlGetLocationUniform(m_resetShader.id, "height");
  int locTarget = rlGetLocationUniform(m_resetShader.id, "targetPos");

  int targetX = (int)targetGrid.x;
  int targetY = (int)targetGrid.y;
  int targetIVec[2] = {targetX, targetY};

  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locTarget, targetIVec, RL_SHADER_UNIFORM_IVEC2, 1);

  m_integrationBuffer.BindBase(1);
  rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

  m_integrationBuffer2.BindBase(1);
  rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

  utils::GPUUtils::MemoryBarrier();

  // 2. Integration (Iterative Relaxation)
  rlEnableShader(m_integrationShader.id);
  locW = rlGetLocationUniform(m_integrationShader.id, "width");
  locH = rlGetLocationUniform(m_integrationShader.id, "height");
  int locWeight = rlGetLocationUniform(m_integrationShader.id, "densityWeight");

  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);
  if (locWeight >= 0)
    rlSetUniform(locWeight, &m_densityWeight, RL_SHADER_UNIFORM_FLOAT, 1);

  int passes = 256;

  for (int i = 0; i < passes; ++i) {
    bool isEven = (i % 2 == 0);
    const auto &readBuf = isEven ? m_integrationBuffer : m_integrationBuffer2;
    const auto &writeBuf = isEven ? m_integrationBuffer2 : m_integrationBuffer;

    m_costBuffer.BindBase(0);
    readBuf.BindBase(1);
    m_densityBuffer.BindBase(3);
    writeBuf.BindBase(4);

    rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
    utils::GPUUtils::MemoryBarrier();
  }

  // 3. Vector Field Generation
  rlEnableShader(m_flowShader.id);
  locW = rlGetLocationUniform(m_flowShader.id, "width");
  locH = rlGetLocationUniform(m_flowShader.id, "height");
  rlSetUniform(locW, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locH, &m_height, RL_SHADER_UNIFORM_INT, 1);

  m_integrationBuffer.BindBase(1);
  m_flowBuffer.BindBase(2);

  rlComputeShaderDispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
  utils::GPUUtils::MemoryBarrier();

  rlDisableShader();
}

void GPUFlowFieldSystem::UpdateCrowdDensity(unsigned int entityBufferId,
                                            int entityCount, float cellSize) {
  if (m_width <= 0 || m_height <= 0)
    return;

  int numCells = m_width * m_height;

  // 1. Clear density buffer
  rlEnableShader(m_gridClearShader.id);
  int locNumCells = rlGetLocationUniform(m_gridClearShader.id, "numCells");
  rlSetUniform(locNumCells, &numCells, RL_SHADER_UNIFORM_INT, 1);

  m_densityBuffer.BindBase(2); // grid_clear uses binding 2
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

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

  rlBindShaderBuffer(entityBufferId,
                     1);       // grid_count uses binding 1 for entities
  m_densityBuffer.BindBase(2); // and binding 2 for cell counts

  rlComputeShaderDispatch((entityCount + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  rlDisableShader();
}

void GPUFlowFieldSystem::SyncToCPU() {
  if (m_width <= 0 || m_height <= 0 || m_syncedThisFrame)
    return;
  size_t cellCount = (size_t)m_width * m_height;

  // Ensure shadow buffer size is correct (though it should be from Init)
  if (m_flowFieldShadow.size() != cellCount) {
    m_flowFieldShadow.resize(cellCount);
  }

  // Read data from GPU to CPU shadow buffer
  m_flowBuffer.Read(m_flowFieldShadow.data(), cellCount * sizeof(Vector2));
  m_syncedThisFrame = true;
}

std::vector<Vector2> GPUFlowFieldSystem::DownloadFlowField() const {

  size_t cellCount = (size_t)m_width * m_height;
  std::vector<Vector2> flowData(cellCount);
  m_flowBuffer.Read(flowData.data(), cellCount * sizeof(Vector2));
  return flowData;
}

void GPUFlowFieldSystem::Shutdown() {
  LOG_INFO("Shutting down GPUFlowFieldSystem...");
  m_costBuffer.Release();
  m_integrationBuffer.Release();
  // Added missing releases
  m_integrationBuffer2.Release();
  m_densityBuffer.Release();
  m_flowBuffer.Release();
}

} // namespace NoMoreDay::systems
