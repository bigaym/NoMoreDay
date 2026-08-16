#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/core/RenderConstants.hpp"
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
  struct CascadeRadianceTarget {
    uint32_t texture = 0u;
    int width = 0;
    int height = 0;
    uint32_t directions = 0u;
    [[nodiscard]] bool IsValid() const noexcept {
      return texture != 0u && width > 0 && height > 0 && directions > 0u;
    }
  };

  RadianceCascadesPass();
  ~RadianceCascadesPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "RadianceCascadesPass"; }
  [[nodiscard]] graph::RenderPassType Type() const override {
    return graph::RenderPassType::RadianceCascades;
  }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);
  bool PrepareVfxEmissionSnapshot(const graph::RenderContext &context);
  [[nodiscard]] uint64_t GetVfxEmissionSnapshotVersion() const noexcept {
    return m_vfxEmissionSnapshotVersion;
  }

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
    return m_cascadeRadiance[0].texture;
  }
  [[nodiscard]] int GetRadianceWidth() const noexcept {
    return m_cascadeRadiance[0].width;
  }
  [[nodiscard]] int GetRadianceHeight() const noexcept {
    return m_cascadeRadiance[0].height;
  }
  [[nodiscard]] uint32_t GetRadianceDirections() const noexcept {
    return m_cascadeRadiance[0].directions;
  }
  [[nodiscard]] const CascadeRadianceTarget &GetCascadeTarget(size_t level) const noexcept {
    return m_cascadeRadiance[level < kMaxCascadeLevels ? level : 0u];
  }
  [[nodiscard]] uint32_t ReadParticleCounterForTesting() const;

  static uint32_t ResolveRaysPerProbe(
      uint32_t cascadeLevel, uint32_t cascadeLevels,
      core::QualityTier tier = core::QualityTier::Ultra) noexcept;
  static float ResolveRayMinLength(uint32_t cascadeLevel) noexcept;
  static float ResolveRayMaxLength(uint32_t cascadeLevel) noexcept;

private:
  static constexpr uint32_t kMaxCascadeLevels = 6u;

  bool EnsureResources(int fullWidth, int fullHeight, uint32_t cascadeLevels,
                       bool halfResolution, core::QualityTier tier);
  void DestroyCascadeTarget(CascadeRadianceTarget &target);
  bool ClearParticleCounter();
  bool RunEmissiveBuild(const graph::RenderContext &context, int width, int height);
  bool RunMaterialEmissive(const graph::RenderContext &context, int width,
                           int height);
  bool RunParticleEmissive(const graph::RenderContext &context, int width,
                           int height);
  bool RunEmissiveMerge(const graph::RenderContext &context, int width, int height);
  bool RunCascadeTrace(const graph::RenderContext &context, uint32_t cascadeLevels,
                       bool holographicMode, core::QualityTier tier);
  void UploadConfig(const graph::RenderContext &context, uint32_t cascadeLevels,
                    bool halfResolution, core::QualityTier tier);
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
  int m_radianceParentRaysLoc = -1;
  int m_radianceRayMinLengthLoc = -1;
  int m_radianceRayMaxLengthLoc = -1;
  int m_radianceEmissiveTextureLoc = -1;
  int m_radianceParentTextureLoc = -1;
  int m_radianceParentValidLoc = -1;
  int m_radianceHolographicLoc = -1;

  resources::FramebufferHandle m_emissiveBase = {};
  resources::FramebufferHandle m_particleEmissive = {};
  resources::FramebufferHandle m_emissiveCombined = {};
  std::array<CascadeRadianceTarget, kMaxCascadeLevels> m_cascadeRadiance = {};

  ::NoMoreDay::core::ComputeBuffer m_radianceConfigBuffer = {};
  ::NoMoreDay::core::ComputeBuffer m_particleCounterBuffer = {};

  int m_cachedFullWidth = 0;
  int m_cachedFullHeight = 0;
  uint32_t m_cachedCascadeLevels = 0u;
  bool m_cachedHalfResolution = false;
  core::QualityTier m_cachedTier = core::QualityTier::Ultra;
  uint32_t m_frameIndex = 0u;
  uint32_t m_lastMaterialStampCount = 0u;
  uint32_t m_lastParticleWriteCount = 0u;
  bool m_initialized = false;
  bool m_vfxEmissionSnapshotValid = false;
  uint64_t m_vfxEmissionSnapshotVersion = 0u;
  bool m_barrierAuditLogged = false;
  bool m_lastExecuteFailure = false;
  bool m_lastExecuteSuccess = false;
  std::string m_lastFailureReason = {};
};

} // namespace NoMoreDay::render::passes
