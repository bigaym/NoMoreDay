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
  float padding[3]; // 12 bytes - Padding to 48 bytes
};
static_assert(sizeof(GPUInstanceData) == 48,
              "GPUInstanceData must match GPUEntity size");

struct DrawArraysIndirectCommand {
  uint32_t count;
  uint32_t instanceCount;
  uint32_t first;
  uint32_t baseInstance;
};

class MDIRenderer {
public:
  static MDIRenderer &Get() {
    static MDIRenderer instance;
    return instance;
  }

  // Initialize MDI renderer with resource manager and max entity capacity
  void Init(ResourceManager &rm, uint32_t maxEntities);

  // Upload instance data to GPU
  void UpdateInstances(const std::vector<GPUInstanceData> &data);

  // Upload visual stats data to GPU (Binding 3)
  void
  UpdateStats(const std::vector<NoMoreDay::components::GPUVisualStats> &stats);

  // Perform GPU culling and command generation
  // viewBounds: x=minX, y=minY, z=maxX, w=maxY (Axis Aligned)
  void Cull(Vector4 viewBounds);

  // Execute indirect draw
  void Render(const Matrix &viewProj, float renderAlpha = 0.0f);

  // Shutdown and release resources
  void Shutdown();

  // Reset command buffer (instanceCount = 0)
  void ResetCommand();

  bool IsInitialized() const { return m_quadVAO != 0; }
  int GetCurrentSlot() const { return m_commandBuffer.GetCurrentSlot(); }
  unsigned int GetId() const { return m_commandBuffer.GetId(); }
  size_t GetSize() const { return m_commandBuffer.GetSize(); }

private:
  MDIRenderer() = default;
  ~MDIRenderer();

  // No copy/move
  MDIRenderer(const MDIRenderer &) = delete;
  MDIRenderer &operator=(const MDIRenderer &) = delete;

  PersistentBuffer m_visibleBuffer; // SSBO Binding 1 (Double Buffered)
  PersistentBuffer
      m_commandBuffer; // SSBO Binding 2 & Indirect (Double Buffered)
  PersistentBuffer
      m_statsBuffer; // SSBO Binding 3 (Double Buffered) - GPUVisualStats

  Shader m_cullShader;
  Shader m_renderShader;
  Shader m_resetShader; // Simple compute to reset counter if needed, or use
                        // separate kernel in cull

  uint32_t m_quadVAO = 0;
  uint32_t m_quadVBO = 0;
  uint32_t m_maxEntities = 0;
};

} // namespace NoMoreDay::render
