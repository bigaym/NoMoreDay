#pragma once

#include "engine/render/graph/RenderPass.hpp"

#include "raylib.h"
#include <string>

class ResourceManager;

namespace NoMoreDay::render::passes {

class LightCullingPass final : public graph::RenderPass {
public:
  LightCullingPass();
  ~LightCullingPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "LightCullingPass"; }

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
  [[nodiscard]] uint32_t GetLastOverflowCount() const noexcept {
    return m_lastOverflowCount;
  }
  [[nodiscard]] const std::string &GetLastFailureReason() const noexcept {
    return m_lastFailureReason;
  }

private:
  bool Initialize(::ResourceManager &resources);
  void Shutdown();
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
  std::string m_lastFailureReason;
};

} // namespace NoMoreDay::render::passes
