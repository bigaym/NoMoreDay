#pragma once
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
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

  void Init(ResourceManager &rm, int maxEntities = 20000);

  void Update(entt::registry &registry, float dt);
  void SyncBack(entt::registry &registry);
  void
  Render(const NoMoreDay::SharedContext &context); // Render instanced entities
  void RenderLegacy(); // CPU-Instanced rendering (Fallback)

  void Shutdown();

private:
  GPUEntitySystem() = default;

  int m_maxEntities = 0;
  render::PersistentBuffer m_persistentEntityBuffer;

  // Grid Buffers
  core::ComputeBuffer m_cellCountBuffer;
  core::ComputeBuffer m_cellOffsetBuffer;
  core::ComputeBuffer m_entityIndicesBuffer; // Sorted entity IDs
  core::ComputeBuffer m_tempCountBuffer;

  std::vector<components::GPUEntity> m_localData;
  std::vector<components::GPUEntity>
      m_shadowBuffer; // CPU-side shadow copy for incremental updates
  std::vector<components::GPUVisualStats>
      m_visualStatsShadowBuffer; // Cache for visual stats
  std::vector<uint32_t> m_gridCounts;
  std::vector<uint32_t> m_gridOffsets;

  // Compute Shaders
  Shader m_physicsShader;
  Shader m_gridClearShader;
  Shader m_gridCountShader;
  Shader m_gridSortShader;

  uint64_t m_frameCounter = 0;
  std::vector<entt::entity>
      m_slotToEntities[3]; // Stable tracking for each slot

  static constexpr int BLOCK_SIZE = 1024;
  std::vector<bool> m_blockDirty;

  // Rendering
  Shader m_renderShader;
  unsigned int m_quadVAO = 0;
  unsigned int m_quadVBO = 0;
  void InitRender(ResourceManager &rm);
};

} // namespace NoMoreDay::systems