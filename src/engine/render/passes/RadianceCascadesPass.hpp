#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>

class ResourceManager;

namespace NoMoreDay::render::passes {

class RadianceCascadesPass final : public graph::RenderPass {
public:
  RadianceCascadesPass();
  ~RadianceCascadesPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "RadianceCascadesPass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);

  [[nodiscard]] bool HasEmissiveBuffer() const noexcept {
    return m_emissiveCombined.IsValid();
  }
  [[nodiscard]] uint32_t GetEmissiveTexture() const noexcept {
    return m_emissiveCombined.colorTexture;
  }
  [[nodiscard]] int GetEmissiveWidth() const noexcept {
    return m_emissiveCombined.width;
  }
  [[nodiscard]] int GetEmissiveHeight() const noexcept {
    return m_emissiveCombined.height;
  }
  [[nodiscard]] bool HasRadianceMap() const noexcept {
    return m_cascadeRadiance[0].IsValid();
  }
  [[nodiscard]] uint32_t GetRadianceTexture() const noexcept {
    return m_cascadeRadiance[0].colorTexture;
  }
  [[nodiscard]] int GetRadianceWidth() const noexcept {
    return m_cascadeRadiance[0].width;
  }
  [[nodiscard]] int GetRadianceHeight() const noexcept {
    return m_cascadeRadiance[0].height;
  }

private:
  static constexpr uint32_t kMaxCascadeLevels = 6u;

  bool EnsureResources(int fullWidth, int fullHeight, uint32_t cascadeLevels,
                       bool halfResolution);
  bool ClearParticleCounter();
  uint32_t ReadParticleCounter() const;
  bool RunEmissiveBuild(const graph::RenderContext &context, int width, int height);
  bool RunMaterialEmissive(const graph::RenderContext &context, int width,
                           int height);
  bool RunParticleEmissive(const graph::RenderContext &context, int width,
                           int height);
  bool RunEmissiveMerge(int width, int height);
  bool RunCascadeTrace(const graph::RenderContext &context, uint32_t cascadeLevels,
                       bool holographicMode);
  void UploadConfig(const graph::RenderContext &context, uint32_t cascadeLevels,
                    bool halfResolution);
  uint32_t ResolveRaysPerProbe(uint32_t cascadeLevel,
                               uint32_t cascadeLevels) const noexcept;
  float ResolveRayMinLength(uint32_t cascadeLevel) const noexcept;
  float ResolveRayMaxLength(uint32_t cascadeLevel) const noexcept;
  void ReportFailure(const char *reason);
  void MarkSuccess();
  void LogBarrierAuditOnce();

  Shader m_emissiveBuildShader = {};
  Shader m_materialEmissiveShader = {};
  Shader m_particleEmissiveShader = {};
  Shader m_emissiveMergeShader = {};
  Shader m_radianceCascadeShader = {};

  int m_emissiveResolutionLoc = -1;
  int m_emissiveLightCountLoc = -1;
  int m_emissiveSceneTextureLoc = -1;
  int m_emissiveCameraOffsetLoc = -1;
  int m_emissiveScreenSizeLoc = -1;

  int m_materialResolutionLoc = -1;
  int m_materialMaskArrayLoc = -1;
  int m_materialMaskLayerLoc = -1;
  int m_materialDispatchOriginLoc = -1;
  int m_materialDispatchSizeLoc = -1;
  int m_materialEmissionLoc = -1;

  int m_particleResolutionLoc = -1;
  int m_particleSceneTextureLoc = -1;
  int m_particleThresholdLoc = -1;

  int m_mergeResolutionLoc = -1;

  int m_radianceFullResolutionLoc = -1;
  int m_radianceCascadeResolutionLoc = -1;
  int m_radianceCascadeLevelLoc = -1;
  int m_radianceCascadeCountLoc = -1;
  int m_radianceRaysPerProbeLoc = -1;
  int m_radianceRayMinLengthLoc = -1;
  int m_radianceRayMaxLengthLoc = -1;
  int m_radianceEmissiveTextureLoc = -1;
  int m_radianceParentTextureLoc = -1;
  int m_radianceParentValidLoc = -1;
  int m_radianceHolographicLoc = -1;

  resources::FramebufferHandle m_emissiveBase = {};
  resources::FramebufferHandle m_particleEmissive = {};
  resources::FramebufferHandle m_emissiveCombined = {};
  std::array<resources::FramebufferHandle, kMaxCascadeLevels> m_cascadeRadiance = {};

  ::NoMoreDay::core::ComputeBuffer m_radianceConfigBuffer = {};
  ::NoMoreDay::core::ComputeBuffer m_particleCounterBuffer = {};

  int m_cachedFullWidth = 0;
  int m_cachedFullHeight = 0;
  uint32_t m_cachedCascadeLevels = 0u;
  bool m_cachedHalfResolution = false;
  uint32_t m_frameIndex = 0u;
  uint32_t m_lastMaterialStampCount = 0u;
  uint32_t m_lastParticleWriteCount = 0u;
  bool m_initialized = false;
  bool m_barrierAuditLogged = false;
  bool m_lastExecuteFailure = false;
  bool m_lastExecuteSuccess = false;
  std::string m_lastFailureReason = {};
};

} // namespace NoMoreDay::render::passes
