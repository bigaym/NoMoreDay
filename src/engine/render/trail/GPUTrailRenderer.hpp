#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"

#include <vector>

namespace NoMoreDay::render {

class GPUTrailRenderer {
public:
  static GPUTrailRenderer &Get();

  void Init(int maxTrails = 512, int maxPointsPerTrail = 64);
  void Shutdown();

  int AllocateTrail(const components::GPUTrailHeader &config);
  void FreeTrail(int trailId);

  void AppendPoint(int trailId, Vector2 position, Vector2 direction, float width,
                   uint32_t color);

  void Update(float dt);
  void Render(const Camera2D &camera);

  void ClearAll();

  [[nodiscard]] bool IsInitialized() const { return m_initialized; }
  [[nodiscard]] int GetActiveTrailCount() const;

private:
  Shader m_trailShader = {0};
  int m_mvpLoc = -1;
  int m_trailIndexLoc = -1;
  int m_maxPointsPerTrailLoc = -1;
  unsigned int m_dummyVAO = 0;

  NoMoreDay::core::ComputeBuffer m_headerSSBO;
  NoMoreDay::core::ComputeBuffer m_pointsSSBO;

  struct TrailSlot {
    bool active = false;
    components::GPUTrailHeader header = {};
    std::vector<components::GPUTrailPoint> points;
  };
  std::vector<TrailSlot> m_slots;

  int m_maxTrails = 512;
  int m_maxPointsPerTrail = 64;
  bool m_initialized = false;

  [[nodiscard]] bool IsTrailIdValid(int trailId) const;
};

} // namespace NoMoreDay::render
