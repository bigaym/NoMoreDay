#pragma once
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay {
struct SharedContext;
}

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
  static GPUEntitySystem &Get() {
    static GPUEntitySystem instance;
    return instance;
  }

  void Init(ResourceManager &rm, int maxEntities = 200000);

  void Update(entt::registry &registry, float dt);
  void SyncBack(entt::registry &registry);
    void Render(const NoMoreDay::SharedContext &context); // Render instanced entities
    void RenderLegacy(float alpha); // CPU-Instanced rendering (Fallback)
  
    void Shutdown();

private:
  GPUEntitySystem() = default;

  int m_maxEntities = 0;
  NoMoreDay::render::PersistentBuffer m_persistentEntityBuffer;
  NoMoreDay::render::PersistentBuffer m_physicsOutputBuffer;

  float m_mapBoundary = 5000.0f;

  // Grid Buffers
  NoMoreDay::core::ComputeBuffer m_cellCountBuffer;
  NoMoreDay::core::ComputeBuffer m_cellOffsetBuffer;
  NoMoreDay::core::ComputeBuffer m_entityIndicesBuffer; // Sorted entity IDs
  NoMoreDay::core::ComputeBuffer m_tempCountBuffer;
  NoMoreDay::core::ComputeBuffer m_blockSumBuffer;

  std::vector<NoMoreDay::components::GPUEntity> m_localData;
  std::vector<NoMoreDay::components::GPUEntity>
      m_shadowBuffer; // CPU-side shadow copy for incremental updates
  std::vector<NoMoreDay::components::GPUVisualStats>
      m_visualStatsShadowBuffer; // Cache for visual stats
  std::vector<uint32_t> m_gridCounts;
  std::vector<uint32_t> m_gridOffsets;

  // Compute Shaders
  Shader m_physicsShader;
  Shader m_gridClearShader;
  Shader m_gridCountShader;
  Shader m_gridSortShader;
  Shader m_gridScanShader;

  uint64_t m_frameCounter = 0;
  std::vector<entt::entity>
      m_slotToEntities[3]; // Stable tracking for each slot
  int m_activeCountPerSlot[3] = {0, 0, 0};

  static constexpr int BLOCK_SIZE = 1024;
  std::vector<bool> m_blockDirty;

  // Rendering
  Shader m_renderShader;
  unsigned int m_quadVAO = 0;
  unsigned int m_quadVBO = 0;
  void InitRender(ResourceManager &rm);
};

} // namespace NoMoreDay::systems