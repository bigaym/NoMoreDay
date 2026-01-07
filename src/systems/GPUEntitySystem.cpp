#include "GPUEntitySystem.hpp"
#include "GPUFlowFieldSystem.hpp"
#include "../tools/Logger.hpp"
#include "../utils/GPUUtils.hpp"
#include "../components/Common.hpp"
#include "../components/AIComponent.hpp"
#include "rlgl.h"

namespace NoMoreDay::systems {

using namespace components;

void GPUEntitySystem::Init(ResourceManager& resources, int maxEntities) {
    m_maxEntities = maxEntities;
    LOG_INFO("Initializing GPUEntitySystem (Compute-based Physics) with {} entities...", maxEntities);
    
    // Load Shaders
    m_gridClearShader = resources.loadComputeShader(entt::hashed_string{"grid_clear"}, "assets/shaders/grid_clear.compute");
    m_gridCountShader = resources.loadComputeShader(entt::hashed_string{"grid_count"}, "assets/shaders/grid_count.compute");
    m_gridSortShader = resources.loadComputeShader(entt::hashed_string{"grid_sort"}, "assets/shaders/grid_sort.compute");
    m_physicsShader = resources.loadComputeShader(entt::hashed_string{"physics"}, "assets/shaders/physics.compute");

    // Initialize Buffers
    m_entityBuffer.Create(m_maxEntities * sizeof(components::GPUEntity), nullptr, RL_DYNAMIC_DRAW);
    
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
}

void GPUEntitySystem::InitRender(ResourceManager& rm) {
    // Load Shaders
    m_renderShader = LoadShader("assets/shaders/entity.vert", "assets/shaders/entity.frag");
    m_renderShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_renderShader, "mvp");

    // Setup Quad
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f
    };

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
    if (m_maxEntities <= 0 || m_renderShader.id == 0) return;

    Matrix mvp = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    Matrix finalMvp = MatrixMultiply(mvp, projection);

    rlEnableShader(m_renderShader.id);
    rlSetUniformMatrix(m_renderShader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);
    
    m_entityBuffer.BindBase(1);
    
    rlEnableVertexArray(m_quadVAO);
    rlDrawVertexArrayInstanced(0, 4, m_maxEntities);
    rlDisableVertexArray();
    
    rlDisableShader();
}

void GPUEntitySystem::Update(entt::registry& registry, float dt) {
    // 1. Sync CPU -> GPU
    auto view = registry.view<Position, Velocity, Radius, GPUIndex>();
    int index = 0;
    view.each([&](auto entity, auto& pos, auto& vel, auto& radius, auto& gpuIdx) {
        if (index >= m_maxEntities) return;
        
        gpuIdx.index = index;
        m_localData[index].position = {pos.x, pos.y};
        m_localData[index].velocity = {vel.vx, vel.vy};
        m_localData[index].radius = radius.value;
        
        // Entity types (simplified for now)
        m_localData[index].type = registry.all_of<EnemyTag>(entity) ? 1 : 0;
        
        index++;
    });
    m_entityBuffer.Update(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));

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
    
    rlBindShaderBuffer(m_entityBuffer.GetId(), 1);
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
    rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "maxEntities"), &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "cellSize"), &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridCols"), &gridCols, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(m_gridSortShader.id, "gridRows"), &gridRows, RL_SHADER_UNIFORM_INT, 1);
    
    rlBindShaderBuffer(m_entityBuffer.GetId(), 1);
    rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
    rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);
    rlBindShaderBuffer(m_tempCountBuffer.GetId(), 5);
    rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
    utils::GPUUtils::MemoryBarrier();

    // 3. Dispatch Physics (Integration + Collision)
    rlEnableShader(m_physicsShader.id);
    rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "dt"), &dt, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "maxEntities"), &m_maxEntities, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "cellSize"), &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridCols"), &gridCols, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(rlGetLocationUniform(m_physicsShader.id, "gridRows"), &gridRows, RL_SHADER_UNIFORM_INT, 1);
    
    // Bind Flow Texture (Unit 0)
    rlActiveTextureSlot(0);
    rlEnableTexture(GPUFlowFieldSystem::Get().GetFlowTexture());
    int locFlowField = rlGetLocationUniform(m_physicsShader.id, "flowField");
    int texUnit = 0;
    rlSetUniform(locFlowField, &texUnit, RL_SHADER_UNIFORM_INT, 1);

    rlBindShaderBuffer(m_entityBuffer.GetId(), 1);
    rlBindShaderBuffer(m_cellCountBuffer.GetId(), 2);
    rlBindShaderBuffer(m_cellOffsetBuffer.GetId(), 3);
    rlBindShaderBuffer(m_entityIndicesBuffer.GetId(), 4);
    
    rlComputeShaderDispatch((m_maxEntities + 255) / 256, 1, 1);
    utils::GPUUtils::MemoryBarrier();
    rlDisableShader();
}

void GPUEntitySystem::SyncBack(entt::registry& registry) {
    static bool s_firstSyncLogged = false;
    m_entityBuffer.Read(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));
    
    auto view = registry.view<Position, Velocity, GPUIndex>();
    if (view.begin() != view.end() && !s_firstSyncLogged) {
        LOG_INFO("GPU Physics Sync: Active (Successfully reading data back to CPU)");
        s_firstSyncLogged = true;
    }

    view.each([&](auto& pos, auto& vel, auto& gpuIdx) {
        if (gpuIdx.index >= 0 && gpuIdx.index < m_maxEntities) {
            auto& gpu = m_localData[gpuIdx.index];
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
    m_entityBuffer.Release();
    m_cellCountBuffer.Release();
    m_cellOffsetBuffer.Release();
    m_entityIndicesBuffer.Release();
    m_tempCountBuffer.Release();
}

} // namespace NoMoreDay::systems
