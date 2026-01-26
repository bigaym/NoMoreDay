#pragma once
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUEntitySync.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay {
struct SharedContext;
}

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
  [[deprecated("Use RenderContext injection instead")]]
  static GPUEntitySystem &Get();

  GPUEntitySystem() = default;
  ~GPUEntitySystem() = default;

  // Disable copy, allow move
  GPUEntitySystem(const GPUEntitySystem &) = delete;
  GPUEntitySystem &operator=(const GPUEntitySystem &) = delete;
  GPUEntitySystem(GPUEntitySystem &&) = default;
  GPUEntitySystem &operator=(GPUEntitySystem &&) = default;

  void Init(ResourceManager &rm, int maxEntities = 200000,
            entt::registry *registry = nullptr);

  void Update(const NoMoreDay::SharedContext &context, float dt);
  void UpdateLogic(const NoMoreDay::SharedContext &context, float dt);
  void UploadGPU(const NoMoreDay::SharedContext &context);
  void SyncBack(entt::registry &registry);
  void Render(const NoMoreDay::SharedContext &context,
              const Camera2D &camera); // Render instanced entities
  void RenderLegacy(float alpha);      // CPU-Instanced rendering (Fallback)

  void Shutdown();

  // Phase 1: Accessors
  const render::GPUSlotManager &GetSlotManager() const { return m_slotManager; }
  const render::PersistentBuffer &GetEntityBuffer() const { return m_persistentEntityBuffer; }
  int GetMaxEntities() const { return m_maxEntities; }

private:
  static GPUEntitySystem *s_instance;

  int m_maxEntities = 0;
  int m_highWaterMark = 0;
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

  // Phase 1: Slot Manager
  NoMoreDay::render::GPUSlotManager m_slotManager;

  // Phase 2: Physics Sync
  render::GPUPhysicsSync m_physicsSync;

  // Phase 3: Visual Sync
  render::GPUVisualSync m_visualSync;

  // Legacy members removed
  // m_freeSlots removed
  // m_slotToEntity removed

  static constexpr int BLOCK_SIZE = 1024;
  std::vector<bool> m_blockDirty;

  // Rendering
  Shader m_renderShader = {0};
  unsigned int m_quadVAO = 0;
  unsigned int m_quadVBO = 0;
  void InitRender(ResourceManager &rm);
};

} // namespace NoMoreDay::systems