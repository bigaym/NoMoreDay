#pragma once

#include "engine/render/graph/RenderPass.hpp"

#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>

class ResourceManager;

namespace NoMoreDay::render::passes {

class LightCullingPass final : public graph::RenderPass {
public:
  struct OverflowRingSlot {
    uint32_t counterReadbackBufferId = 0;
    void *fence = nullptr; // GLsync
    bool armed = false;
    uint64_t submittedFrame = 0;
  };
  static constexpr size_t kRingDepth = 3;

  LightCullingPass();
  ~LightCullingPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "LightCullingPass"; }
  graph::RenderPassType Type() const override {
    return graph::RenderPassType::LightCulling;
  }

  [[nodiscard]] bool IsClusterDataReadyForCurrentFrame() const noexcept {
    return m_clusterDataReadyForCurrentFrame;
  }
  [[nodiscard]] bool HadFailureThisFrame() const noexcept {
    return m_lastExecuteFailure;
  }
  [[nodiscard]] bool SucceededThisFrame() const noexcept {
    return m_lastExecuteSuccess;
  }
  [[nodiscard]] uint32_t GetFrameIndex() const noexcept { return m_frameIndex; }

  /// Snapshot delayed by >= 1 frame in production; same-frame when readbackEnabledForTesting is true.
  [[nodiscard]] uint32_t GetLastOverflowCount() const noexcept {
    return m_lastOverflowCount;
  }
  [[nodiscard]] uint32_t GetLastOverflowSnapshot() const noexcept {
    return m_lastOverflowSnapshot;
  }
  [[nodiscard]] const std::string &GetLastFailureReason() const noexcept {
    return m_lastFailureReason;
  }
  void SetComputeShaderPathForTesting(const std::string &path);
  void SetReadbackEnabledForTesting(bool enabled) noexcept {
    m_readbackEnabledForTesting = enabled;
  }
  [[nodiscard]] bool IsReadbackEnabledForTesting() const noexcept {
    return m_readbackEnabledForTesting;
  }

  [[nodiscard]] size_t GetRingWriteIndex() const noexcept { return m_ringWrite; }
  [[nodiscard]] size_t GetRingReadIndex() const noexcept { return m_ringRead; }
  [[nodiscard]] const std::array<OverflowRingSlot, kRingDepth> &
  GetOverflowRing() const noexcept {
    return m_overflowRing;
  }
  [[nodiscard]] std::array<OverflowRingSlot, kRingDepth> &
  GetOverflowRingMutableForTesting() noexcept {
    return m_overflowRing;
  }
  void SetLastOverflowSnapshotForTesting(uint32_t snapshot) noexcept {
    m_lastOverflowSnapshot = snapshot;
  }
  void SetRingIndicesForTesting(size_t writeIdx, size_t readIdx) noexcept {
    m_ringWrite = writeIdx;
    m_ringRead = readIdx;
  }

  /// Outcome of a non-blocking readback ring poll attempt.
  struct SnapshotPollOutcome {
    bool published = false;
    uint32_t snapshot = 0;
    size_t nextReadIndex = 0;
  };

  // Readback ring poll contract (P0-1): an armed slot that is at least one
  // frame old and whose fence has already signaled publishes the pending
  // overflow snapshot and advances the read index; any other state preserves
  // the previous snapshot and leaves the read index unchanged. Pure (no GL
  // calls), so the poll decision is unit-testable without a context.
  static SnapshotPollOutcome
  TryPublishReadySnapshot(bool slotArmed, bool frameEligible,
                          bool fenceSignaled, size_t readIndex,
                          size_t ringDepth, uint32_t pendingSnapshot,
                          uint32_t currentSnapshot) noexcept;

  // Readback ring write contract (P0-1): a slot may accept a new copy only
  // when it is not armed (no pending sample). When the ring is full the new
  // sample is dropped rather than overwriting a pending slot.
  static bool CanSubmitReadbackCopy(bool slotArmed) noexcept {
    return !slotArmed;
  }

  bool Initialize(::ResourceManager &resources);
  void Shutdown();

private:
  void ReportFailure(const char *reason);
  void MarkSuccess();

  Shader m_lightCullingShader = {};
  int m_clusterGridXLoc = -1;
  int m_clusterGridYLoc = -1;
  int m_clusterGridZLoc = -1;
  int m_tileSizeWorldLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_lightCountLoc = -1;
  int m_maxLightsPerClusterLoc = -1;
  int m_maxTotalClusteredLightsLoc = -1;

  uint32_t m_frameIndex = 0;
  uint32_t m_lastOverflowCount = 0;
  bool m_initialized = false;
  bool m_clusterDataReadyForCurrentFrame = false;
  bool m_lastExecuteFailure = false;
  bool m_lastExecuteSuccess = false;
  bool m_readbackEnabledForTesting = false;
  std::string m_lastFailureReason;
  std::string m_computeShaderPath = "assets/shaders/lighting/light_culling.comp";

  std::array<OverflowRingSlot, kRingDepth> m_overflowRing{};
  size_t m_ringWrite = 0;
  size_t m_ringRead = 0;
  uint32_t m_lastOverflowSnapshot = 0;
};

} // namespace NoMoreDay::render::passes
