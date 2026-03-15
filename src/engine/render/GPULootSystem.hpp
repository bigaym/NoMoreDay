#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include <entt/entt.hpp>
#include <cstdint>
#include <raylib.h>
#include <vector>

namespace NoMoreDay::render {

class GPULootSystem {
public:
  static GPULootSystem &Get() {
    static GPULootSystem instance;
    return instance;
  }

  GPULootSystem() = default;

  void Init(uint32_t maxInstances = 8192);
  void Shutdown();

  void SyncDroppedItems(const entt::registry &registry);
  void Dispatch(const Camera2D &camera, int screenWidth, int screenHeight,
                bool enableForceDirected);
  void Render(const Matrix &viewProj, bool enableGlow) const;

  [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
  [[nodiscard]] uint32_t GetSyncedInstanceCount() const noexcept {
    return m_syncedInstanceCount;
  }
  [[nodiscard]] uint32_t GetMaxInstancesForTest() const noexcept {
    return m_maxInstances;
  }
  [[nodiscard]] uint32_t GetVisibleInstanceCount() const noexcept {
    return m_visibleInstanceCount;
  }
  
  struct DebugSnapshot {
    uint32_t required = 0;
    uint32_t synced = 0;
    uint32_t maxInstances = 0;
    uint32_t visible = 0;
  };
  [[nodiscard]] DebugSnapshot GetDebugSnapshot() const noexcept {
    return m_debugSnapshot;
  }

  [[nodiscard]] const NoMoreDay::core::ComputeBuffer &GetInstanceBuffer() const
      noexcept {
    return m_instanceBuffer;
  }

private:
  struct DrawArraysIndirectCommand {
    uint32_t count = 0;
    uint32_t instanceCount = 0;
    uint32_t first = 0;
    uint32_t baseInstance = 0;
  };

  void EnsureCapacity(uint32_t requiredInstances);
  void ResetDispatchState();

  bool m_initialized = false;
  uint32_t m_maxInstances = 0;
  uint32_t m_syncedInstanceCount = 0;
  uint32_t m_visibleInstanceCount = 0;
  DebugSnapshot m_debugSnapshot = {};
  std::vector<components::GPULootInstance> m_instances;

  Shader m_cullShader = {};
  Shader m_indirectArgsShader = {};
  Shader m_gridHashShader = {};
  Shader m_repulsionShader = {};
  Shader m_positionUpdateShader = {};
  Shader m_renderShader = {};

  int m_locCullCount = -1;
  int m_locCullViewRect = -1;
  int m_locIndirectMaxCount = -1;
  int m_locGridVisibleCount = -1;
  int m_locGridCellSize = -1;
  int m_locGridGridWidth = -1;
  int m_locGridGridHeight = -1;
  int m_locRepulsionVisibleCount = -1;
  int m_locRepulsionMinDist = -1;
  int m_locRepulsionStiffness = -1;
  int m_locRepulsionMaxForce = -1;
  int m_locRepulsionDamping = -1;
  int m_locUpdateVisibleCount = -1;
  int m_locUpdateDamping = -1;
  int m_locUpdateMaxOffset = -1;
  int m_locRenderMvp = -1;
  int m_locRenderGlowEnabled = -1;

  uint32_t m_vao = 0;
  uint32_t m_vbo = 0;

  uint32_t m_gridWidth = 0;
  uint32_t m_gridHeight = 0;
  float m_gridCellSize = 48.0f;

  NoMoreDay::core::ComputeBuffer m_instanceBuffer;
  NoMoreDay::core::ComputeBuffer m_visibleIndexBuffer;
  NoMoreDay::core::ComputeBuffer m_counterBuffer;
  NoMoreDay::core::ComputeBuffer m_indirectBuffer;
  NoMoreDay::core::ComputeBuffer m_forceBuffer;
  NoMoreDay::core::ComputeBuffer m_gridCountBuffer;
};

} // namespace NoMoreDay::render
