#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <cstdint>
#include <vector>

namespace NoMoreDay::render {

// Matches GPUEntity in GPUData.hpp for zero-copy rendering
struct alignas(16) GPUInstanceData {
  Vector2 position; // 8 bytes  - Current physics position
  Vector2
      prevPosition; // 8 bytes  - Previous frame position (for interpolation)
  Vector2 velocity; // 8 bytes  - Used for auto-rotation in shader
  float radius;     // 4 bytes  - Used for scale calculation
  int32_t type;     // 4 bytes  - Texture Index
  uint32_t flags;   // 4 bytes  - Behavior flags
  float padding[7]; // 28 bytes - Padding to 64 bytes
};
// static_assert(alignof(GPUInstanceData) == 16,
//               "GPUInstanceData must be 16-byte aligned");
static_assert(sizeof(GPUInstanceData) == 64,
              "GPUInstanceData must match GPUEntity size");

struct DrawArraysIndirectCommand {
  uint32_t count;
  uint32_t instanceCount;
  uint32_t first;
  uint32_t baseInstance;
};

struct StatUpdateCmd {
  uint32_t index;
  float pad[3];
  components::GPUVisualStats stats;
};

class MDIRenderer {
public:
  static constexpr uint32_t kCullLocalSize = 256;
  [[nodiscard]] static constexpr uint32_t CalculateCullDispatchGroups(uint32_t entityCount) noexcept {
    return (entityCount + kCullLocalSize - 1) / kCullLocalSize;
  }

  [[deprecated("Use RenderContext injection instead")]]
  static MDIRenderer &Get();

  MDIRenderer() = default;
  ~MDIRenderer();

  // No copy, allow move
  MDIRenderer(const MDIRenderer &) = delete;
  MDIRenderer &operator=(const MDIRenderer &) = delete;
  MDIRenderer(MDIRenderer &&) = default;
  MDIRenderer &operator=(MDIRenderer &&) = default;

  // Initialize MDI renderer with resource manager and max entity capacity
  void Init(ResourceManager &rm, uint32_t maxEntities);

  /**
   * @brief Update visual-only stats (glow, status effects)
   */
  void Update(ResourceManager &rm,
              const NoMoreDay::render::PersistentBuffer &entityBuffer,
              float alpha);
  
  // Sparse Update API
  void UpdateStat(uint32_t entityIdx, const components::GPUVisualStats& stats);
  void FlushStatsUpdates(ResourceManager& rm);

  // Full Update API (Legacy/Alternative)
  void UpdateStats(const std::vector<components::GPUVisualStats> &stats,
                   int count);
  void UpdateStatsNoFlush(const std::vector<components::GPUVisualStats> &stats,
                          int count);
  void FlushStatsRange(size_t count);
  void ResetCommand();

  // Perform GPU culling and command generation
  // viewBounds: x=minX, y=minY, z=maxX, w=maxY (Axis Aligned)
  void Cull(ResourceManager &rm, const PersistentBuffer &entities, Vector4 viewBounds);

  // Execute indirect draw
  /**
   * @brief Perform MDI Draw call
   * @param entities The buffer containing entity data (Binding 0)
   */
  void Render(ResourceManager &rm, const PersistentBuffer &entities,
              float renderAlpha);

  void SetMaxActiveEntities(uint32_t count) { m_maxActiveEntities = count; }

  // Shutdown and release resources
  void Shutdown();

  bool IsInitialized() const { return m_quadVAO != 0; }
  int GetCurrentSlot() const { return m_commandBuffer.GetCurrentSlot(); }
  unsigned int GetId() const { return m_commandBuffer.GetId(); }
  size_t GetSize() const { return m_commandBuffer.GetSize(); }

private:
  static MDIRenderer *s_instance;

  PersistentBuffer m_visibleBuffer; // SSBO Binding 1 (Double Buffered)
  PersistentBuffer
      m_commandBuffer; // SSBO Binding 2 & Indirect (Double Buffered)
  PersistentBuffer
      m_statsBuffer; // SSBO Binding 3 (Double Buffered) - GPUVisualStats

  // Sparse Staging
  PersistentBuffer m_statsStaging;
  std::vector<StatUpdateCmd> m_pendingUpdates;

  Shader m_cullShader = {0};
  Shader m_renderShader = {0};
  Shader m_scatterShader = {0};

  uint32_t m_quadVAO = 0;
  uint32_t m_quadVBO = 0;
  uint32_t m_maxEntities = 0;
  uint32_t m_maxActiveEntities = 0;
};

} // namespace NoMoreDay::render
