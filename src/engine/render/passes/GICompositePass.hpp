#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <cstdint>
#include <string>

class ResourceManager;

namespace NoMoreDay::render::passes {

class OccluderExtractPass;
class RadianceCascadesPass;

class GICompositePass final : public graph::RenderPass {
public:
  GICompositePass();
  ~GICompositePass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "GICompositePass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);
  void InvalidateHistory() noexcept { m_historyValid = false; }
  bool IsHistoryValid() const noexcept { return m_historyValid; }
  void SetOccluderExtractPass(const OccluderExtractPass *pass) noexcept {
    m_occluderExtractPass = pass;
  }
  // M0-A R3: expose the radiance pass so its VFX emission snapshot version can
  // participate in temporal history rejection.
  void SetRadianceCascadesPass(const RadianceCascadesPass *pass) noexcept {
    m_radianceCascadesPass = pass;
  }

  // M0-A R3: occupancy/depth history resource contract. The current-frame
  // occupancy comes from the occluder mask owned by OccluderExtractPass; the
  // previous-frame occupancy is read from a persistent R8 ping-pong history
  // updated every execute. HasOccupancyHistory() is true only after at least
  // one successful Execute with valid resources.
  [[nodiscard]] bool HasOccupancyHistory() const noexcept {
    return m_occupancyHistoryA.IsValid() && m_occupancyHistoryB.IsValid();
  }
  [[nodiscard]] uint32_t GetOccupancyHistoryTexture() const noexcept {
    const auto &read = m_readHistoryA ? m_occupancyHistoryA : m_occupancyHistoryB;
    return read.colorTexture;
  }
  [[nodiscard]] int GetOccupancyHistoryWidth() const noexcept {
    return m_cachedWidth;
  }
  [[nodiscard]] int GetOccupancyHistoryHeight() const noexcept {
    return m_cachedHeight;
  }
  // M0-A R3: instrumentation for the history-rejection contract. Each Execute
  // that resets temporal history (extent/camera/zoom/light/occluder/emissive
  // or a fresh start) increments the counter and records the primary reason.
  [[nodiscard]] uint64_t GetHistoryResetCount() const noexcept {
    return m_historyResetCount;
  }
  [[nodiscard]] const std::string &GetLastResetReason() const noexcept {
    return m_lastResetReason;
  }

private:
  bool EnsureResources(int width, int height);
  uint64_t BuildLightSignature() const;

  Shader m_compositeShader = {};

  int m_sceneResolutionLoc = -1;
  int m_radianceResolutionLoc = -1;
  int m_temporalWeightLoc = -1;
  int m_giIntensityLoc = -1;
  int m_resetHistoryLoc = -1;
  int m_cameraDeltaUvLoc = -1;
  int m_zoomRatioLoc = -1;
  int m_occupancyEnabledLoc = -1;

  resources::FramebufferHandle m_outputScene = {};
  resources::FramebufferHandle m_historyA = {};
  resources::FramebufferHandle m_historyB = {};
  // M0-A R3: persistent R8 occupancy history ping-pong (current-frame occupancy
  // is the occluder mask; previous frame occupancy is compared under the
  // reprojected UV for disocclusion rejection).
  resources::FramebufferHandle m_occupancyHistoryA = {};
  resources::FramebufferHandle m_occupancyHistoryB = {};

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
  bool m_historyValid = false;
  bool m_readHistoryA = true;
  bool m_prevCameraValid = false;
  Vector2 m_prevCameraTarget = {0.0f, 0.0f};
  float m_prevCameraZoom = 0.0f;
  uint64_t m_prevLightSignature = 0u;
  const OccluderExtractPass *m_occluderExtractPass = nullptr;
  uint64_t m_prevOccluderMaskVersion = 0u;
  const RadianceCascadesPass *m_radianceCascadesPass = nullptr;
  uint64_t m_prevVfxEmissionSnapshotVersion = 0u;
  uint64_t m_historyResetCount = 0u;
  std::string m_lastResetReason = {};
};

} // namespace NoMoreDay::render::passes

