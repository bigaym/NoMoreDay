#include "GPUEntitySystem.hpp"
#include "GPUFlowFieldSystem.hpp"
#include "glad.h"
#include "rlgl.h"
#include "../components/Common.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay::systems {

void GPUEntitySystem::Init(ResourceManager& rm, int maxEntities) {
    m_maxEntities = maxEntities;
    m_localData.resize(maxEntities);
    for (auto& e : m_localData) e.radius = 0.0f; // Inactive
    
    LOG_INFO("Initializing GPUEntitySystem with {} entities", maxEntities);
    m_entityBuffer.Create(maxEntities * sizeof(components::GPUEntity), m_localData.data(), core::BufferUsage::Dynamic);
    
    // Grid Setup
    int numCells = (5000 / 32 + 1) * (5000 / 32 + 1);
    m_gridCounts.resize(numCells);
    m_gridOffsets.resize(numCells);

    m_cellCountBuffer.Create(numCells * sizeof(uint32_t));
    m_cellOffsetBuffer.Create(numCells * sizeof(uint32_t));
    m_tempCountBuffer.Create(numCells * sizeof(uint32_t));
    m_entityIndicesBuffer.Create(maxEntities * sizeof(uint32_t));

    m_physicsShader = rm.loadComputeShader(entt::hashed_string{"physics_update"}, "assets/shaders/physics.compute");
    m_gridClearShader = rm.loadComputeShader(entt::hashed_string{"grid_clear"}, "assets/shaders/grid_clear.compute");
    m_gridCountShader = rm.loadComputeShader(entt::hashed_string{"grid_count"}, "assets/shaders/grid_count.compute");
    m_gridSortShader = rm.loadComputeShader(entt::hashed_string{"grid_sort"}, "assets/shaders/grid_sort.compute");

    InitRender(rm);
}

void GPUEntitySystem::InitRender(ResourceManager& rm) {
    m_renderShader = rm.loadShader(entt::hashed_string{"entity_render"}, "assets/shaders/entity.vert", "assets/shaders/entity.frag");
    
    float quadVertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
        -0.5f, -0.5f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void GPUEntitySystem::Render() {
    if (m_renderShader.id == 0 || m_maxEntities == 0) return;

    glUseProgram(m_renderShader.id);
    
    // Get MVP from Raylib
    Matrix matView = rlGetMatrixModelview();
    Matrix matProj = rlGetMatrixProjection();
    Matrix matMVP = MatrixMultiply(matView, matProj);
    
    // Raylib Matrix is column-major float[16], compatible with OpenGL
    glUniformMatrix4fv(glGetUniformLocation(m_renderShader.id, "mvp"), 1, GL_FALSE, (float*)&matMVP);

    m_entityBuffer.BindBase(1);

    glBindVertexArray(m_quadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, m_maxEntities);
    glBindVertexArray(0);
    
    glUseProgram(0);
}

void GPUEntitySystem::Update(entt::registry& registry, float dt) {
    if (m_physicsShader.id == 0) return;

    // 1. Sync CPU -> GPU
    auto view = registry.view<Position, Velocity>();
    for (auto& e : m_localData) e.radius = 0.0f; 

    int index = 0;
    view.each([&](auto entity, auto& pos, auto& vel) {
        if (index >= m_maxEntities) return;
        registry.emplace_or_replace<GPUIndex>(entity, index);

        auto& gpu = m_localData[index];
        gpu.position = { pos.x, pos.y };
        gpu.velocity = { vel.vx, vel.vy };
        gpu.radius = 12.0f; 
        gpu.type = 1; 
        gpu.id = (int)entity;
        index++;
    });
    m_entityBuffer.Update(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));

    // 2. Build Grid
    int gridCols = 5000 / 32 + 1;
    int gridRows = 5000 / 32 + 1;
    int numCells = gridCols * gridRows;

    // 2.1 Clear
    glUseProgram(m_gridClearShader.id);
    glUniform1i(glGetUniformLocation(m_gridClearShader.id, "numCells"), numCells);
    m_cellCountBuffer.BindBase(2);
    glDispatchCompute((numCells + 255) / 256, 1, 1);
    m_tempCountBuffer.BindBase(2);
    glDispatchCompute((numCells + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 2.2 Count
    glUseProgram(m_gridCountShader.id);
    glUniform1i(glGetUniformLocation(m_gridCountShader.id, "maxEntities"), m_maxEntities);
    glUniform1f(glGetUniformLocation(m_gridCountShader.id, "cellSize"), 32.0f);
    glUniform1i(glGetUniformLocation(m_gridCountShader.id, "gridCols"), gridCols);
    glUniform1i(glGetUniformLocation(m_gridCountShader.id, "gridRows"), gridRows);
    m_entityBuffer.BindBase(1);
    m_cellCountBuffer.BindBase(2);
    glDispatchCompute((m_maxEntities + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 2.3 Prefix Sum (CPU)
    m_cellCountBuffer.Read(m_gridCounts.data(), numCells * sizeof(uint32_t));
    uint32_t currentOffset = 0;
    for (int i = 0; i < numCells; i++) {
        m_gridOffsets[i] = currentOffset;
        currentOffset += m_gridCounts[i];
    }
    m_cellOffsetBuffer.Update(m_gridOffsets.data(), numCells * sizeof(uint32_t));

    // 2.4 Sort
    glUseProgram(m_gridSortShader.id);
    glUniform1i(glGetUniformLocation(m_gridSortShader.id, "maxEntities"), m_maxEntities);
    glUniform1f(glGetUniformLocation(m_gridSortShader.id, "cellSize"), 32.0f);
    glUniform1i(glGetUniformLocation(m_gridSortShader.id, "gridCols"), gridCols);
    glUniform1i(glGetUniformLocation(m_gridSortShader.id, "gridRows"), gridRows);
    m_entityBuffer.BindBase(1);
    m_cellOffsetBuffer.BindBase(3);
    m_entityIndicesBuffer.BindBase(4);
    m_tempCountBuffer.BindBase(5);
    glDispatchCompute((m_maxEntities + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 3. Dispatch Physics (Integration + Collision)
    glUseProgram(m_physicsShader.id);
    glUniform1f(glGetUniformLocation(m_physicsShader.id, "dt"), dt);
    glUniform1i(glGetUniformLocation(m_physicsShader.id, "maxEntities"), m_maxEntities);
    // Add grid uniforms to physics shader
    glUniform1f(glGetUniformLocation(m_physicsShader.id, "cellSize"), 32.0f);
    glUniform1i(glGetUniformLocation(m_physicsShader.id, "gridCols"), gridCols);
    glUniform1i(glGetUniformLocation(m_physicsShader.id, "gridRows"), gridRows);
    
    // Bind Flow Texture (Unit 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GPUFlowFieldSystem::Get().GetFlowTexture());
    glUniform1i(glGetUniformLocation(m_physicsShader.id, "flowField"), 0);

    m_entityBuffer.BindBase(1);
    m_cellCountBuffer.BindBase(2);
    m_cellOffsetBuffer.BindBase(3);
    m_entityIndicesBuffer.BindBase(4);
    
    glDispatchCompute((m_maxEntities + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glUseProgram(0);
}

void GPUEntitySystem::SyncBack(entt::registry& registry) {
    m_entityBuffer.Read(m_localData.data(), m_maxEntities * sizeof(components::GPUEntity));
    
    auto view = registry.view<Position, Velocity, GPUIndex>();
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
    m_localData.clear();
}

} // namespace NoMoreDay::systems