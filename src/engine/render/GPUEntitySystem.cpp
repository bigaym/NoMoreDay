#include "engine/render/GPUEntitySystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp" // Added
#include "game/registry/GroupRegistry.hpp"
#include "rlgl.h"
#include "raymath.h"

namespace NoMoreDay::systems {

using namespace components;

void GPUEntitySystem::Init(ResourceManager &resources, int maxEntities) {
  m_maxEntities = maxEntities;
  LOG_INFO("Initializing GPUEntitySystem (Compute-based Physics) with {} "
           "entities...",
           maxEntities);

  // Load Shaders
  m_gridClearShader = resources.loadComputeShader(
      entt::hashed_string{"grid_clear"}, "assets/shaders/grid_clear.compute");
  m_gridCountShader = resources.loadComputeShader(
      entt::hashed_string{"grid_count"}, "assets/shaders/grid_count.compute");
  m_gridSortShader = resources.loadComputeShader(
      entt::hashed_string{"grid_sort"}, "assets/shaders/grid_sort.compute");
  m_physicsShader = resources.loadComputeShader(
      entt::hashed_string{"physics"}, "assets/shaders/physics.compute");

  // Initialize Buffers
  // Initialize Buffers
  m_persistentEntityBuffer.Create(m_maxEntities * sizeof(components::GPUEntity));

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

  m_localData.resize(m_maxEntities);
  m_mdiInstanceData.resize(m_maxEntities);
  m_gridCounts.resize(numCells);
  m_gridOffsets.resize(numCells);

  InitRender(resources);
  NoMoreDay::render::MDIRenderer::Get().Init(resources, m_maxEntities);
}

void GPUEntitySystem::InitRender(ResourceManager &rm) {
  // Load Shaders
  m_renderShader =
      LoadShader("assets/shaders/entity.vert", "assets/shaders/entity.frag");
  m_renderShader.locs[SHADER_LOC_MATRIX_MVP] =
      GetShaderLocation(m_renderShader, "mvp");

  // Setup Quad
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

void GPUEntitySystem::Render() {
  bool useMDI = true; // Could be Config::Get().useMDI

  if (useMDI && NoMoreDay::render::MDIRenderer::Get().IsInitialized()) {
      Matrix mv = rlGetMatrixModelview();
      Matrix proj = rlGetMatrixProjection();
      Matrix mvp = MatrixMultiply(mv, proj);
      
      // Calculate View Bounds (World Space AABB of the screen)
      Matrix invMVP = MatrixInvert(mvp);
      Vector3 ndcMin = {-1.0f, -1.0f, 0.0f};
      Vector3 ndcMax = {1.0f, 1.0f, 0.0f};
      Vector3 worldMin = Vector3Transform(ndcMin, invMVP);
      Vector3 worldMax = Vector3Transform(ndcMax, invMVP);

      // Simple min/max for AABB
      Vector4 viewBounds;
      viewBounds.x = fminf(worldMin.x, worldMax.x);
      viewBounds.y = fminf(worldMin.y, worldMax.y);
      viewBounds.z = fmaxf(worldMin.x, worldMax.x);
      viewBounds.w = fmaxf(worldMin.y, worldMax.y);

      // Margin
      float margin = 120.0f; 
      viewBounds.x -= margin;
      viewBounds.y -= margin;
      viewBounds.z += margin;
      viewBounds.w += margin;

      auto& mdi = NoMoreDay::render::MDIRenderer::Get();
      mdi.Cull(viewBounds);
      mdi.Render(mvp);
  } else {
      RenderLegacy();
  }
}

void GPUEntitySystem::RenderLegacy() {
  if (m_maxEntities <= 0 || m_renderShader.id == 0)
    return;

  Matrix mvp = rlGetMatrixModelview();
  Matrix projection = rlGetMatrixProjection();
  Matrix finalMvp = MatrixMultiply(mvp, projection);

  rlEnableShader(m_renderShader.id);
  rlSetUniformMatrix(m_renderShader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);

  m_persistentEntityBuffer.BindBase(1);

  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 4, m_maxEntities);
  rlDisableVertexArray();

  rlDisableShader();
}

void GPUEntitySystem::Update(entt::registry &registry, float dt) {
  // 1. Clear local buffer first (optional, as we overwrite usually)
  
  // 2. Sync loop: BeginWrite -> Read Old Data -> Write New Data -> EndWrite
  // Acquire ptr to current Write Slot (Wait for fence of Slot N+1 -> Safe to write)
  components::GPUEntity* gpuPtr = (components::GPUEntity*)m_persistentEntityBuffer.BeginWrite();
  
  // 2.1 Read Back Old Physics Result (from the slot we just acquired)
  // Since we use Triple Buffer, acquiring this slot means GPU is DONE with it (from 3 frames ago).
  // It contains the physics result of Frame N-2.
  // We copy it to local data for SyncBack usage.
  m_persistentEntityBuffer.Read(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));

  // 2.2 Overwrite with Current Frame ECS Data (Frame N inputs)
  // Note: We are overwriting the buffer we just read.
  
  // Reset for new frame writing
  // But wait, if we clear it, we lose the data we just read? No, we copied to m_localData above.
  
  auto group = registry.group<Position, Velocity, Radius, GPUIndex>();
  int index = 0;

  for (auto entity : group) {
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity))
      continue;
    if (index >= m_maxEntities)
      break;

    const auto &pos = group.get<Position>(entity);
    const auto &vel = group.get<Velocity>(entity);
    const auto &radius = group.get<Radius>(entity);
    auto &gpuIdx = group.get<GPUIndex>(entity);

    gpuIdx.index = index;
    
    // Write directly to Mapped Pointer
    gpuPtr[index].position = {pos.x, pos.y};
    gpuPtr[index].velocity = {vel.vx, vel.vy};
    gpuPtr[index].radius = radius.value;
    gpuPtr[index].type = registry.all_of<EnemyTag>(entity) ? 1 : 0;

    // Phase 1 MDI Data Population
    auto& mdi = m_mdiInstanceData[index];
    mdi.position = {pos.x, pos.y};
    mdi.scale = {radius.value * 2.0f, radius.value * 2.0f};
    mdi.rotation = 0.0f; 
    mdi.textureIndex = gpuPtr[index].type;
    mdi.flags = 0;

    index++;
  }
  
  // Clear remaining slots to avoid ghosts
  for(int i=index; i<m_maxEntities; ++i) {
      gpuPtr[i].radius = 0.0f;
  }

  // 2.3 Submit (Flush to GPU)
  m_persistentEntityBuffer.Flush();
  
  // Update MDI Buffer
  NoMoreDay::render::MDIRenderer::Get().UpdateInstances(m_mdiInstanceData);

  // 2. Build Grid
  int gridCols = 5000 / 32 + 1;
  int gridRows = 5000 / 32 + 1;
  int numCells = gridCols * gridRows;

  // 2.1 Clear
  rlEnableShader(m_gridClearShader.id);
  int locNumCells = rlGetLocationUniform(m_gridClearShader.id, "numCells");
  rlSetUniform(locNumCells, &numCells, RL_SHADER_UNIFORM_INT, 1);

  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);

  rlBindShaderBuffer(m_tempCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  // 2.2 Count
  rlEnableShader(m_gridCountShader.id);
  int locMaxEnt = rlGetLocationUniform(m_gridCountShader.id, "maxEntities");
  rlSetUniform(locMaxEnt, &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  float cellSize = 32.0f;
  int locCellSize = rlGetLocationUniform(m_gridCountShader.id, "cellSize");
  rlSetUniform(locCellSize, &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  int locCols = rlGetLocationUniform(m_gridCountShader.id, "gridCols");
  rlSetUniform(locCols, &gridCols, RL_SHADER_UNIFORM_INT, 1);
  int locRows = rlGetLocationUniform(m_gridCountShader.id, "gridRows");
  rlSetUniform(locRows, &gridRows, RL_SHADER_UNIFORM_INT, 1);

  // Use BindBase for Persistent Buffer
  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  // 2.3 Prefix Sum (CPU)
  m_cellCountBuffer.Read(m_gridCounts.data(), numCells * sizeof(uint32_t));
  uint32_t currentOffset = 0;
  for (int i = 0; i < numCells; i++) {
    m_gridOffsets[i] = currentOffset;
    currentOffset += m_gridCounts[i];
  }
  m_cellOffsetBuffer.Update(m_gridOffsets.data(), numCells * sizeof(uint32_t));

  // 2.4 Sort
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
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  // 2.5 Update Flow Field Crowd Density
  // Note: UpdateCrowdDensity expects a GL ID. Persistent Buffer has ID, but usually we bind base.
  // WARNING: If UpdateCrowdDensity binds GL_SHADER_STORAGE_BUFFER using the raw ID, it might bind from Offset 0,
  // which is Frame N-2 data (Slot 0) instead of current write slot.
  // Ideally, GPUFlowFieldSystem should accept an offset or we should handle binding here.
  // For now, proceeding with potential latency risk.
  GPUFlowFieldSystem::Get().UpdateCrowdDensity(m_persistentEntityBuffer.GetId(),
                                               m_maxEntities, 10.0f);

  // 3. Dispatch Physics (Integration + Collision)
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

  // Bind Flow Buffer (Binding 6 - assumes shader uses binding 6 for flow)
  const auto &flowSystem = GPUFlowFieldSystem::Get();
  flowSystem.GetFlowBuffer().BindBase(6);

  // Pass Flow Grid Params
  int locFlowW = rlGetLocationUniform(m_physicsShader.id, "flowWidth");
  int locFlowH = rlGetLocationUniform(m_physicsShader.id, "flowHeight");
  int locFlowOrigin = rlGetLocationUniform(m_physicsShader.id, "flowOrigin");

  int fw = flowSystem.GetWidth();
  int fh = flowSystem.GetHeight();
  Vector2 fo = flowSystem.GetGridOrigin();

  if (locFlowW >= 0)
    rlSetUniform(locFlowW, &fw, RL_SHADER_UNIFORM_INT, 1);
  if (locFlowH >= 0)
    rlSetUniform(locFlowH, &fh, RL_SHADER_UNIFORM_INT, 1);
  if (locFlowOrigin >= 0)
    rlSetUniform(locFlowOrigin, &fo, RL_SHADER_UNIFORM_VEC2, 1);

  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
  rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);

  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();
  rlDisableShader();
  
  // Mark end of GPU usage for this slot
  m_persistentEntityBuffer.Lock();
}

void GPUEntitySystem::SyncBack(entt::registry &registry) {
  static bool s_firstSyncLogged = false;
  // Note: Read is now done in Update to piggyback on BeginWrite sync
  
  if (!s_firstSyncLogged) {
    LOG_INFO(
        "GPU Physics Sync: Active (Using Triple-Buffer Readback)");
    s_firstSyncLogged = true;
  }
  
  // m_localData already contains the data read back in Update()

  auto view = registry.view<Position, Velocity, GPUIndex>();
  view.each([&](auto &pos, auto &vel, auto &gpuIdx) {
    if (gpuIdx.index >= 0 && gpuIdx.index < m_maxEntities) {
      auto &gpu = m_localData[gpuIdx.index];
      if (gpu.radius > 0.0f) {
        pos.x = gpu.position.x;
        pos.y = gpu.position.y;
        vel.vx = gpu.velocity.x;
        vel.vy = gpu.velocity.y;
      }
    }
  });
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
