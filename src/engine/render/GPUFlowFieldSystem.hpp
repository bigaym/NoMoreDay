#pragma once
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include <vector>


namespace NoMoreDay::systems {

class GPUFlowFieldSystem {
public:
  static GPUFlowFieldSystem &Get() {
    static GPUFlowFieldSystem instance;
    return instance;
  }

  void Init(ResourceManager &rm, int width, int height);

  // Update with full cost map and target position
  // Will extract a window of m_width * m_height based on gridOrigin
  void Update(const std::vector<unsigned char> &fullCostMap, int mapW, int mapH,
              Vector2 targetPos, Vector2 gridOrigin, const render::PersistentBuffer* entityBuffer = nullptr, int entityCount = 0);

  // Update crowd density from GPU entity buffer
  void UpdateCrowdDensity(const render::PersistentBuffer &entityBuffer, int entityCount,
                          float cellSize);

  void Shutdown();

  // Accessors for Debugging
  const render::PersistentBuffer &GetFlowBuffer() const { return m_flowBuffer; }
  const core::ComputeBuffer &GetDensityBuffer() const {
    return m_densityBuffer;
  }
  const core::ComputeBuffer &GetIntegrationBuffer() const {
    return m_integrationBuffer;
  }
  const render::PersistentBuffer &GetCostBuffer() const { return m_costBuffer; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }
  Vector2 GetGridOrigin() const { return m_gridOrigin; }

  // Efficient CPU Access
  void SyncToCPU();
  const std::vector<Vector2> &GetFlowFieldCPU() const {
    return m_flowFieldShadow;
  }
  bool IsCoordinateInBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
  }
  void ResetSyncTag() { m_syncedThisFrame = false; }

  // Density Weight for Flanking
  float m_densityWeight = 10.0f;

  // Debugging
  bool m_debugDraw = false;
  void DownloadFlowField(std::vector<Vector2>& out) const;

private:
  GPUFlowFieldSystem() = default;

  Shader m_integrationShader;
  Shader m_flowShader;
  Shader m_resetShader;
  Shader m_gridCountShader;
  Shader m_gridClearShader;

  // Buffers (SSBOs)
  render::PersistentBuffer m_costBuffer;    // uint32_t[] (Static obstacles)
  core::ComputeBuffer m_densityBuffer; // uint32_t[] (Dynamic crowd density)

  core::ComputeBuffer m_integrationBuffer;  // Ping
  core::ComputeBuffer m_integrationBuffer2; // Pong
  render::PersistentBuffer m_flowBuffer;         // Vector2[] (Flow Direction)

  // CPU Shadow Buffer
  std::vector<Vector2> m_flowFieldShadow;
  // Cache for cost buffer updates to avoid repeated allocation
  std::vector<uint32_t> m_costCache;
  bool m_syncedThisFrame = false;

  int m_width = 0;
  int m_height = 0;
  float m_cellSize = 10.0f;
  Vector2 m_gridOrigin = {0, 0};

  // State Tracking for Optimization
  Vector2 m_lastGridOrigin = {-99999.0f, -99999.0f};
  Vector2 m_lastTargetGridPos = {-99999.0f, -99999.0f};
  bool m_forceUpdate = true; // Use accessors to set this if map changes
};

} // namespace NoMoreDay::systems
