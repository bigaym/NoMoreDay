#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/gi/JFADistanceFieldEvaluator.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"


#include "raylib.h"
#include <cstdint>
#include <string>
#include <vector>

class ResourceManager;

namespace NoMoreDay::render::passes {

class OccluderExtractPass final : public graph::RenderPass {
public:
  OccluderExtractPass();
  ~OccluderExtractPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "OccluderExtractPass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);

  [[nodiscard]] bool HasOccluderMask() const noexcept {
    return m_occluderMask.IsValid();
  }
  [[nodiscard]] uint32_t GetOccluderMaskTexture() const noexcept {
    return m_occluderMask.colorTexture;
  }
  [[nodiscard]] int GetMaskWidth() const noexcept { return m_cachedWidth; }
  [[nodiscard]] int GetMaskHeight() const noexcept { return m_cachedHeight; }
  [[nodiscard]] uint32_t GetOccluderCount() const noexcept {
    return m_occluderCount;
  }
  [[nodiscard]] bool WasMaskChangedThisFrame() const noexcept {
    return m_maskChangedThisFrame;
  }
  [[nodiscard]] bool RebuiltStaticLayerThisFrame() const noexcept {
    return m_staticRebuiltThisFrame;
  }
  [[nodiscard]] bool UpdatedDynamicLayerThisFrame() const noexcept {
    return m_dynamicUpdatedThisFrame;
  }
  [[nodiscard]] uint64_t GetStaticRebuildCount() const noexcept {
    return m_staticRebuildCount;
  }
  [[nodiscard]] uint64_t GetCameraInvalidateCount() const noexcept {
    return m_cameraInvalidateCount;
  }
  [[nodiscard]] uint64_t GetMaskVersion() const noexcept {
    return m_maskVersion;
  }
  [[nodiscard]] render::gi::JFARect GetCurrentOccluderScreenBounds() const noexcept {
    return m_currentOccluderBounds;
  }
  [[nodiscard]] render::gi::JFARect GetPreviousOccluderScreenBounds() const noexcept {
    return m_previousOccluderBounds;
  }
  [[nodiscard]] const std::string &GetLastFailureReason() const noexcept {
    return m_lastFailureReason;
  }

  void SetDebugVisualizationEnabledForTesting(bool enabled) noexcept {
    m_debugVisualizationEnabled = enabled;
  }

private:
  struct UploadStats {
    uint32_t totalCount = 0;
    uint32_t staticCount = 0;
    uint32_t dynamicCount = 0;
    uint64_t staticSignature = 0;
    uint64_t dynamicSignature = 0;
  };

  bool UploadOccluders(const NoMoreDay::components::GPUShadowCaster *occluders,
                       uint32_t occluderCount);
  bool EnsureMaskBuffers(int width, int height);
  bool RunExtractPass(const Camera2D &camera, bool dynamicOnly, uint32_t outputTexture,
                      uint32_t occluderCount);
  bool RunComposePass();
  void ReportFailure(const char *reason);
  void MarkSuccess();

  Shader m_extractShader = {};
  Shader m_composeShader = {};

  resources::FramebufferHandle m_staticMask = {};
  resources::FramebufferHandle m_dynamicMask = {};
  resources::FramebufferHandle m_occluderMask = {};

  NoMoreDay::core::ComputeBuffer m_occluderBuffer;

  int m_resolutionLoc = -1;
  int m_occluderCountLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;
  int m_dynamicOnlyLoc = -1;

  int m_composeResolutionLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  uint32_t m_frameIndex = 0;
  uint32_t m_occluderCount = 0;

  uint64_t m_lastStaticSignature = 0;
  uint64_t m_lastDynamicSignature = 0;
  uint64_t m_staticRebuildCount = 0;
  Vector2 m_lastCameraTarget = {0.0f, 0.0f};
  float m_lastCameraZoom = 0.0f;
  int m_lastViewportWidth = 0;
  int m_lastViewportHeight = 0;
  uint64_t m_cameraInvalidateCount = 0;
  uint64_t m_maskVersion = 1;

  render::gi::JFARect m_currentOccluderBounds = {};
  render::gi::JFARect m_previousOccluderBounds = {};

  bool m_maskChangedThisFrame = false;
  bool m_staticRebuiltThisFrame = false;

  bool m_dynamicUpdatedThisFrame = false;
  bool m_lastExecuteFailure = false;
  bool m_lastExecuteSuccess = false;
  bool m_initialized = false;
  bool m_debugVisualizationEnabled = false;
  std::string m_lastFailureReason;
};

} // namespace NoMoreDay::render::passes
