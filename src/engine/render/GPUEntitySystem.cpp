#include "engine/render/GPUEntitySystem.hpp"
#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "raymath.h"
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

  m_persistentEntityBuffer.Create(m_maxEntities * sizeof(components::GPUEntity), 3);

  int gridCols = 5000 / 32 + 1;
  int gridRows = 5000 / 32 + 1;
  int numCells = gridCols * gridRows;

  m_cellCountBuffer.Create(numCells * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_cellOffsetBuffer.Create(numCells * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_entityIndicesBuffer.Create(m_maxEntities * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_tempCountBuffer.Create(numCells * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);

  m_localData.resize(m_maxEntities);
  m_gridCounts.resize(numCells);
  m_gridOffsets.resize(numCells);

  InitRender(resources);
  NoMoreDay::render::MDIRenderer::Get().Init(resources, m_maxEntities);
}

void GPUEntitySystem::InitRender(ResourceManager &rm) {
  m_renderShader = LoadShader("assets/shaders/entity.vert", "assets/shaders/entity.frag");
  m_renderShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_renderShader, "mvp");

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

void GPUEntitySystem::Render(const NoMoreDay::SharedContext& context) {
  bool useMDI = true;
  if (useMDI && NoMoreDay::render::MDIRenderer::Get().IsInitialized()) {
    Matrix mv = rlGetMatrixModelview();
    Matrix proj = rlGetMatrixProjection();
    Matrix mvp = MatrixMultiply(mv, proj);
    
    Matrix invMVP = MatrixInvert(mvp);
    Vector3 ndcMin = {-1.0f, -1.0f, 0.0f}, ndcMax = {1.0f, 1.0f, 0.0f};
    Vector3 worldMin = Vector3Transform(ndcMin, invMVP), worldMax = Vector3Transform(ndcMax, invMVP);

    Vector4 viewBounds = { fminf(worldMin.x, worldMax.x) - 120.0f, fminf(worldMin.y, worldMax.y) - 120.0f,
                           fmaxf(worldMin.x, worldMax.x) + 120.0f, fmaxf(worldMin.y, worldMax.y) + 120.0f };

    m_persistentEntityBuffer.BindPrevious(0);
    auto &mdi = NoMoreDay::render::MDIRenderer::Get();
    mdi.Cull(viewBounds);
    mdi.Render(mvp, context.renderAlpha);
  } else {
    RenderLegacy();
  }
}

void GPUEntitySystem::RenderLegacy() {
  if (m_maxEntities <= 0 || m_renderShader.id == 0) return;
  Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  rlEnableShader(m_renderShader.id);
  rlSetUniformMatrix(m_renderShader.locs[SHADER_LOC_MATRIX_MVP], mvp);
  m_persistentEntityBuffer.BindPrevious(1);
  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 4, m_maxEntities);
  rlDisableVertexArray();
  rlDisableShader();
}

void GPUEntitySystem::Update(entt::registry &registry, float dt) {
  NoMoreDay::utils::ScopedTimer timer("GPUEntity_Update", 1000);
  m_frameCounter++;
  components::GPUEntity *gpuPtr = (components::GPUEntity *)m_persistentEntityBuffer.BeginWrite();

  auto group = registry.group<Position, Velocity, Radius, GPUIndex>();
  int index = 0;
  int currentWriteSlot = m_persistentEntityBuffer.GetCurrentSlot();
  m_slotToEntities[currentWriteSlot].assign(m_maxEntities, entt::null);

  for (auto entity : group) {
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity) || index >= m_maxEntities) continue;
    
    const auto &pos = group.get<Position>(entity);
    const auto &vel = group.get<Velocity>(entity);
    const auto &radius = group.get<Radius>(entity);
    
    // Get or initialize previous position for interpolation
    auto& prevPos = registry.get_or_emplace<PrevPosition>(entity, pos.x, pos.y);
    
    group.get<GPUIndex>(entity).index = index;
    m_slotToEntities[currentWriteSlot][index] = entity;
    
    gpuPtr[index].position = {pos.x, pos.y};
    gpuPtr[index].prevPosition = {prevPos.x, prevPos.y};
    
    // Store current position for the next physics step's prevPosition
    prevPos.x = pos.x;
    prevPos.y = pos.y;

    gpuPtr[index].velocity = {vel.vx, vel.vy};
    gpuPtr[index].radius = radius.value;
    gpuPtr[index].type = (uint32_t)(registry.all_of<EnemyTag>(entity) ? 1 : 0);
    
    uint32_t flags = 0;
    if (registry.all_of<PlayerTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_KINEMATIC;
      flags |= GPU_ENTITY_FLAG_NO_RENDER;
    } else if (registry.all_of<SpriteComponent>(entity)) {
      // Disable MDI rendering for monsters with sprites to avoid double rendering (Sprite + Red Circle)
      flags |= GPU_ENTITY_FLAG_NO_RENDER;
    }
    gpuPtr[index].flags = flags;
    
    index++;
  }
  for (int i = index; i < m_maxEntities; ++i) gpuPtr[i].radius = 0.0f;

  m_persistentEntityBuffer.Flush();

  int gridCols = 5000 / 32 + 1, gridRows = 5000 / 32 + 1, numCells = gridCols * gridRows;
  rlEnableShader(m_gridClearShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridClearShader.id, "numCells"), &numCells, RL_SHADER_UNIFORM_INT, 1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  rlBindShaderBuffer(m_tempCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((numCells + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  rlEnableShader(m_gridCountShader.id);
  float cellSize = 32.0f;
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "maxEntities"), &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "cellSize"), &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridCols"), &gridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridCountShader.id, "gridRows"), &gridRows, RL_SHADER_UNIFORM_INT, 1);
  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  m_cellCountBuffer.Read(m_gridCounts.data(), numCells * sizeof(uint32_t));
  uint32_t currentOffset = 0;
  for (int i = 0; i < numCells; i++) { m_gridOffsets[i] = currentOffset; currentOffset += m_gridCounts[i]; }
  m_cellOffsetBuffer.Update(m_gridOffsets.data(), numCells * sizeof(uint32_t));

  rlEnableShader(m_gridSortShader.id);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "maxEntities"), &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "cellSize"), &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridCols"), &gridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridRows"), &gridRows, RL_SHADER_UNIFORM_INT, 1);
  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
  rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);
  rlBindShaderBuffer(m_tempCountBuffer.GetId(), 5);
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();

  GPUFlowFieldSystem::Get().UpdateCrowdDensity(m_persistentEntityBuffer, m_maxEntities, 10.0f);

  rlEnableShader(m_physicsShader.id);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "dt"), &dt, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "maxEntities"), &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "cellSize"), &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridCols"), &gridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridRows"), &gridRows, RL_SHADER_UNIFORM_INT, 1);

  const auto &flowSystem = GPUFlowFieldSystem::Get();
  flowSystem.GetFlowBuffer().BindBase(6);
  int fw = flowSystem.GetWidth(), fh = flowSystem.GetHeight();
  Vector2 fo = flowSystem.GetGridOrigin();
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowWidth"), &fw, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowHeight"), &fh, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "flowOrigin"), &fo, RL_SHADER_UNIFORM_VEC2, 1);

  m_persistentEntityBuffer.BindBase(1);
  rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
  rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
  rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);
  rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
  utils::GPUUtils::MemoryBarrier();
  rlDisableShader();
  m_persistentEntityBuffer.Lock();
}

void GPUEntitySystem::SyncBack(entt::registry &registry) {
  NoMoreDay::utils::ScopedTimer timer("GPUEntity_SyncBack", 500);
  if (m_frameCounter < 2) return;

  // Read back the result of the compute shader that just finished (in the previous Update)
  m_persistentEntityBuffer.Read(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));

  int bufferCount = m_persistentEntityBuffer.GetBufferCount();
  // PersistentBuffer::Read already accesses (m_writeSlot - 1), 
  // so we must use the same slot to find the matching entity list.
  int readSlot = (m_persistentEntityBuffer.GetCurrentSlot() - 1 + bufferCount) % bufferCount;
  const auto& entitiesInReadSlot = m_slotToEntities[readSlot];

  for (int i = 0; i < (int)entitiesInReadSlot.size(); ++i) {
    entt::entity entity = entitiesInReadSlot[i];
    if (entity == entt::null || !registry.valid(entity)) continue;
    if (registry.all_of<PlayerTag>(entity)) continue;

    auto& gpu = m_localData[i];
    if (gpu.radius > 0.0f) {
      if (auto* pos = registry.try_get<Position>(entity)) { 
        // Update PrevPosition for stable interpolation
        auto& prev = registry.get_or_emplace<PrevPosition>(entity, pos->x, pos->y);
        prev.x = pos->x;
        prev.y = pos->y;

        pos->x = gpu.position.x; 
        pos->y = gpu.position.y; 
      }
      if (auto* vel = registry.try_get<Velocity>(entity)) { vel->vx = gpu.velocity.x; vel->vy = gpu.velocity.y; }
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