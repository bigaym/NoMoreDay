#pragma once
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay::render {

// Frame-scoped render/upload inputs injected by the caller (RenderContext).
struct EntityRenderFrame {
  ResourceManager *resources = nullptr;
  MDIRenderer *mdi = nullptr;
  float renderAlpha = 0.0f;
};

} // namespace NoMoreDay::render

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
  GPUEntitySystem() = default;
  ~GPUEntitySystem() = default;

  // Disable copy, allow move
  GPUEntitySystem(const GPUEntitySystem &) = delete;
  GPUEntitySystem &operator=(const GPUEntitySystem &) = delete;
  GPUEntitySystem(GPUEntitySystem &&) = default;
  GPUEntitySystem &operator=(GPUEntitySystem &&) = default;

  void Init(ResourceManager &rm, int maxEntities = 200000);

  void UploadGPU(const render::EntityRenderFrame &frame);
  void Render(const render::EntityRenderFrame &frame,
              const Camera2D &camera); // Render instanced entities
  void RenderLegacy(float alpha);      // CPU-Instanced rendering (Fallback)

  void Shutdown();

  // Phase 1: Accessors
  const render::PersistentBuffer &GetEntityBuffer() const { return m_persistentEntityBuffer; }
  int GetMaxEntities() const { return m_maxEntities; }

  // Game adapter write contract: zero-copy projection into the shadow buffer.
  components::GPUEntity *BeginShadowWrite();
  void SetHighWaterMark(int highWaterMark);
  void ApplyShadowFlags(int slot, uint32_t flags);
  void SetUpdatedStatsIndices(const std::vector<uint32_t> &indices);
  std::vector<components::GPUEntity> &ShadowBuffer();
  std::vector<components::GPUVisualStats> &VisualStatsBuffer();

private:
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
  std::vector<uint32_t> m_updatedStatsIndices;
  std::vector<uint32_t> m_gridCounts;
  std::vector<uint32_t> m_gridOffsets;

  // Compute Shaders
  Shader m_physicsShader;
  Shader m_gridClearShader;
  Shader m_gridCountShader;
  Shader m_gridSortShader;
  Shader m_gridScanShader;

  static constexpr int BLOCK_SIZE = 1024;
  std::vector<bool> m_blockDirty;

  // Rendering
  Shader m_renderShader = {0};
  unsigned int m_quadVAO = 0;
  unsigned int m_quadVBO = 0;
  void InitRender(ResourceManager &rm);
};

} // namespace NoMoreDay::systems