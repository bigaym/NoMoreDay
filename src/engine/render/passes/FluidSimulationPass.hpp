#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/graph/RenderPass.hpp"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

class ResourceManager;

namespace NoMoreDay::render::passes {

class OccluderExtractPass;
class RadianceCascadesPass;

class FluidSimulationPass final : public graph::RenderPass {
public:
  FluidSimulationPass();
  ~FluidSimulationPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "FluidSimulationPass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);

  void SetOccluderExtractPass(const OccluderExtractPass *pass) noexcept {
    m_occluderExtractPass = pass;
  }
  void SetRadiancePass(const RadianceCascadesPass *pass) noexcept {
    m_radiancePass = pass;
  }
  void SetInteractionEnabledForTesting(const bool enabled) noexcept {
    m_interactionEnabled = enabled;
  }

private:
  bool EnsureRuntimeBuffers(uint32_t maxParticles, int width, int height,
                            const Camera2D &camera);
  void SeedParticles(uint32_t count, const Camera2D &camera, int width, int height);
  void UploadConfig(uint32_t maxParticles);
  bool DispatchGridHash(uint32_t particleCount);
  bool DispatchNeighborSearch(uint32_t particleCount);
  bool DispatchDensity(uint32_t particleCount, float deltaTime);
  bool DispatchForce(uint32_t particleCount, float deltaTime);
  bool DispatchIntegrate(const graph::RenderContext &context, uint32_t particleCount,
                         float deltaTime);
  void InjectEmissive(const graph::RenderContext &context, uint32_t particleCount);
  void InjectOccluderMask(const graph::RenderContext &context, uint32_t particleCount);
  void RenderParticles(const graph::RenderContext &context, uint32_t particleCount);
  bool ShouldEnableGiInteraction() const;
  bool LoadShaders();
  void UnloadShaders();
  void ReleaseRuntimeBuffers();
  uint32_t CurrentParticleBufferId() const;
  uint32_t AlternateParticleBufferId() const;
  void SwapParticleBuffers();

  const OccluderExtractPass *m_occluderExtractPass = nullptr;
  const RadianceCascadesPass *m_radiancePass = nullptr;

  Shader m_gridHashShader = {};
  Shader m_neighborSearchShader = {};
  Shader m_densityShader = {};
  Shader m_forceShader = {};
  Shader m_integrateShader = {};
  Shader m_emissiveInjectShader = {};
  Shader m_occluderInjectShader = {};
  Shader m_renderShader = {};

  int m_gridParticleCountLoc = -1;
  int m_gridCellSizeLoc = -1;
  int m_gridOriginLoc = -1;
  int m_gridDimLoc = -1;

  int m_neighborParticleCountLoc = -1;
  int m_neighborMaxNeighborsLoc = -1;
  int m_neighborRadiusLoc = -1;

  int m_densityParticleCountLoc = -1;
  int m_densityMaxNeighborsLoc = -1;

  int m_forceParticleCountLoc = -1;
  int m_forceMaxNeighborsLoc = -1;
  int m_forceDeltaTimeLoc = -1;

  int m_integrateParticleCountLoc = -1;
  int m_integrateDeltaTimeLoc = -1;
  int m_integrateBoundsMinLoc = -1;
  int m_integrateBoundsMaxLoc = -1;
  int m_integrateEmitterLoc = -1;
  int m_integrateFrameIndexLoc = -1;
  int m_integrateCameraOffsetLoc = -1;
  int m_integrateScreenSizeLoc = -1;
  int m_integrateDistanceFieldTexLoc = -1;
  int m_integrateUseDistanceFieldLoc = -1;

  int m_emissiveParticleCountLoc = -1;
  int m_emissiveResolutionLoc = -1;
  int m_emissiveCameraOffsetLoc = -1;
  int m_emissiveScreenSizeLoc = -1;
  int m_emissiveThresholdLoc = -1;

  int m_occluderParticleCountLoc = -1;
  int m_occluderResolutionLoc = -1;
  int m_occluderCameraOffsetLoc = -1;
  int m_occluderScreenSizeLoc = -1;
  int m_occluderDensityThresholdLoc = -1;

  int m_renderMvpLoc = -1;
  int m_renderRadiusLoc = -1;
  int m_renderParticleCountLoc = -1;
  int m_renderRadianceLoc = -1;
  int m_renderUseRadianceLoc = -1;
  int m_renderCameraOffsetLoc = -1;
  int m_renderScreenSizeLoc = -1;
  int m_renderRestDensityLoc = -1;

  NoMoreDay::core::ComputeBuffer m_particlePing;
  NoMoreDay::core::ComputeBuffer m_particlePong;
  NoMoreDay::core::ComputeBuffer m_cellCoordBuffer;
  NoMoreDay::core::ComputeBuffer m_cellCountBuffer;
  NoMoreDay::core::ComputeBuffer m_neighborListBuffer;
  NoMoreDay::core::ComputeBuffer m_neighborCountBuffer;
  NoMoreDay::core::ComputeBuffer m_configBuffer;

  std::vector<components::GPUFluidParticle> m_seedData = {};
  std::vector<uint32_t> m_zeroCellCounts = {};

  uint32_t m_quadVao = 0u;
  uint32_t m_quadVbo = 0u;
  uint32_t m_maxParticles = 0u;
  uint32_t m_frameIndex = 0u;
  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  int m_gridWidth = 0;
  int m_gridHeight = 0;
  bool m_initialized = false;
  bool m_runtimeBuffersReady = false;
  bool m_particlesReadPing = true;
  bool m_interactionEnabled = true;
  std::string m_lastFailureReason = {};

  static constexpr uint32_t kMaxNeighbors = 32u;
};

} // namespace NoMoreDay::render::passes
