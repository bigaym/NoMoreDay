#include "engine/render/passes/RadianceCascadesPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/resource/TextureArrayManager.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture2DArray = 0x8C1A;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLReadWrite = 0x88BA;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLR16f = 0x822D;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLTextureMinFilter = 0x2801;
constexpr uint32_t kGLTextureMagFilter = 0x2800;
constexpr uint32_t kGLLinear = 0x2601;
constexpr uint32_t kGLTextureWrapS = 0x2802;
constexpr uint32_t kGLTextureWrapT = 0x2803;
constexpr uint32_t kGLClampToEdge = 0x812F;
constexpr uint32_t kGLComputeGroupSize = 8u;
constexpr uint32_t kTextureFetchBarrierBit = 0x00000008;
constexpr uint32_t kRadianceConfigBinding = 2u;
constexpr uint32_t kParticleCounterBinding = 0u;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

} // namespace

RadianceCascadesPass::RadianceCascadesPass() = default;

RadianceCascadesPass::~RadianceCascadesPass() { Shutdown(); }

void RadianceCascadesPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::RadianceCascades);
  builder.Read(graph::RenderResourceTag::DistanceField,
               graph::RenderOwnerTag::JFA);
  if (m_usesVfxEmissionSnapshot) {
    // Only declare the ParticleEmissive dependency when the VFX emission
    // snapshot pass is part of this graph; test graphs without the snapshot
    // pass must stay valid.
    builder.Read(graph::RenderResourceTag::ParticleEmissive,
                 graph::RenderOwnerTag::RadianceCascades,
                 graph::PipelineStage::Compute,
                 graph::ResourceUsage::ShaderRead);
  }
  builder.Write(graph::RenderResourceTag::EmissiveBuffer,
                graph::RenderOwnerTag::RadianceCascades);
  builder.Write(graph::RenderResourceTag::RadianceMap,
                graph::RenderOwnerTag::RadianceCascades);

  // Same-pass phase barrier: the emissive build / material / merge / cascade
  // compute dispatches all run inside this Execute and exchange data via image
  // loads and stores. Declared here and emitted via EmitPhaseBarrier at each
  // phase boundary (the exact execution points pass-entry barriers cannot
  // cover).
  builder.AddPhaseBarrier(graph::PipelineStage::Compute,
                          graph::PipelineStage::Compute,
                          static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                              kTextureFetchBarrierBit);
}

bool RadianceCascadesPass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_emissiveBuildShader = resources.loadComputeShader(
      "v5_emissive_build_compute"_hs,
      "assets/shaders/lighting/v5_emissive_build.comp");
  m_materialEmissiveShader = resources.loadComputeShader(
      "v5_emissive_material_compute"_hs,
      "assets/shaders/lighting/v5_emissive_material.comp");
  m_particleEmissiveShader = resources.loadComputeShader(
      "v5_emissive_particle_compute"_hs,
      "assets/shaders/lighting/v5_emissive_particle.comp");
  m_emissiveMergeShader = resources.loadComputeShader(
      "v5_emissive_merge_compute"_hs,
      "assets/shaders/lighting/v5_emissive_merge.comp");
  m_radianceCascadeShader = resources.loadComputeShader(
      "v5_radiance_cascade_compute"_hs,
      "assets/shaders/lighting/v5_radiance_cascade.comp");

  if (m_emissiveBuildShader.id == 0 || m_materialEmissiveShader.id == 0 ||
      m_particleEmissiveShader.id == 0 || m_emissiveMergeShader.id == 0 ||
      m_radianceCascadeShader.id == 0) {
    Shutdown();
    return false;
  }

  m_emissiveResolutionLoc =
      rlGetLocationUniform(m_emissiveBuildShader.id, "uResolution");
  m_emissiveLightCountLoc =
      rlGetLocationUniform(m_emissiveBuildShader.id, "uLightCount");
  m_emissiveSceneTextureLoc =
      rlGetLocationUniform(m_emissiveBuildShader.id, "uSceneTexture");
  m_emissiveCameraOffsetLoc =
      rlGetLocationUniform(m_emissiveBuildShader.id, "uCameraOffset");
  m_emissiveScreenSizeLoc =
      rlGetLocationUniform(m_emissiveBuildShader.id, "uScreenSize");

  m_materialResolutionLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uResolution");
  m_materialMaskArrayLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uMaskArray");
  m_materialMaskLayerLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uMaskLayer");
  m_materialDispatchOriginLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uDispatchOrigin");
  m_materialDispatchSizeLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uDispatchSize");
  m_materialEmissionLoc =
      rlGetLocationUniform(m_materialEmissiveShader.id, "uEmission");

  m_particleResolutionLoc =
      rlGetLocationUniform(m_particleEmissiveShader.id, "uResolution");
  m_particleSceneTextureLoc =
      rlGetLocationUniform(m_particleEmissiveShader.id, "uSceneTexture");
  m_particleThresholdLoc =
      rlGetLocationUniform(m_particleEmissiveShader.id, "uThreshold");

  m_mergeResolutionLoc = rlGetLocationUniform(m_emissiveMergeShader.id, "uResolution");

  m_radianceFullResolutionLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uFullResolution");
  m_radianceCascadeResolutionLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uCascadeResolution");
  m_radianceCascadeLevelLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uCascadeLevel");
  m_radianceCascadeCountLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uCascadeCount");
  m_radianceRaysPerProbeLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uRaysPerProbe");
  m_radianceParentRaysLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uParentRaysPerProbe");
  m_radianceRayMinLengthLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uRayMinLength");
  m_radianceRayMaxLengthLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uRayMaxLength");
  m_radianceEmissiveTextureLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uEmissiveTexture");
  m_radianceParentTextureLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uParentRadiance");
  m_radianceParentValidLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uParentValid");
  m_radianceHolographicLoc =
      rlGetLocationUniform(m_radianceCascadeShader.id, "uHolographicMode");

  m_radianceConfigBuffer.Create(sizeof(::NoMoreDay::components::RadianceCascadeConfig),
                                nullptr, RL_DYNAMIC_DRAW);
  m_particleCounterBuffer.Create(sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  if (m_radianceConfigBuffer.GetId() == 0 || m_particleCounterBuffer.GetId() == 0) {
    Shutdown();
    return false;
  }

  m_initialized = true;
  return true;
}

void RadianceCascadesPass::DestroyCascadeTarget(CascadeRadianceTarget &target) {
  if (target.texture != 0u) {
    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &target.texture);
    target.texture = 0u;
  }
  target.width = 0;
  target.height = 0;
  target.directions = 0u;
}

void RadianceCascadesPass::Shutdown() {
  m_emissiveBuildShader = {};
  m_materialEmissiveShader = {};
  m_particleEmissiveShader = {};
  m_emissiveMergeShader = {};
  m_radianceCascadeShader = {};

  resources::FramebufferManager::Destroy(m_emissiveBase);
  resources::FramebufferManager::Destroy(m_particleEmissive);
  resources::FramebufferManager::Destroy(m_emissiveCombined);
  for (auto &level : m_cascadeRadiance) {
    DestroyCascadeTarget(level);
  }

  m_radianceConfigBuffer.Release();
  m_particleCounterBuffer.Release();

  m_emissiveResolutionLoc = -1;
  m_emissiveLightCountLoc = -1;
  m_emissiveSceneTextureLoc = -1;
  m_emissiveCameraOffsetLoc = -1;
  m_emissiveScreenSizeLoc = -1;

  m_materialResolutionLoc = -1;
  m_materialMaskArrayLoc = -1;
  m_materialMaskLayerLoc = -1;
  m_materialDispatchOriginLoc = -1;
  m_materialDispatchSizeLoc = -1;
  m_materialEmissionLoc = -1;

  m_particleResolutionLoc = -1;
  m_particleSceneTextureLoc = -1;
  m_particleThresholdLoc = -1;

  m_mergeResolutionLoc = -1;

  m_radianceFullResolutionLoc = -1;
  m_radianceCascadeResolutionLoc = -1;
  m_radianceCascadeLevelLoc = -1;
  m_radianceCascadeCountLoc = -1;
  m_radianceRaysPerProbeLoc = -1;
  m_radianceParentRaysLoc = -1;
  m_radianceRayMinLengthLoc = -1;
  m_radianceRayMaxLengthLoc = -1;
  m_radianceEmissiveTextureLoc = -1;
  m_radianceParentTextureLoc = -1;
  m_radianceParentValidLoc = -1;
  m_radianceHolographicLoc = -1;

  m_cachedFullWidth = 0;
  m_cachedFullHeight = 0;
  m_cachedCascadeLevels = 0u;
  m_cachedHalfResolution = false;
  m_cachedTier = core::QualityTier::Ultra;
  m_frameIndex = 0u;
  m_vfxEmissionSnapshotValid = false;
  m_vfxEmissionSnapshotVersion = 0u;
  m_lastMaterialStampCount = 0u;
  m_lastParticleWriteCount = 0u;
  m_initialized = false;
  m_barrierAuditLogged = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();
}

void RadianceCascadesPass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedFullWidth = width;
  m_cachedFullHeight = height;
}

bool RadianceCascadesPass::PrepareVfxEmissionSnapshot(
    const graph::RenderContext &context) {
  m_vfxEmissionSnapshotValid = false;
  if (context.qualityManager == nullptr || context.resources == nullptr ||
      context.camera == nullptr || !context.hdrSceneBuffer.IsValid()) {
    return false;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled || config.giCascadeLevels == 0u) {
    return false;
  }
  if (!m_initialized && !Initialize(*context.resources)) {
    return false;
  }

  const auto tier = context.qualityManager->GetTier();
  const uint32_t cascadeLevels =
      std::clamp<uint32_t>(config.giCascadeLevels, 1u, kMaxCascadeLevels);
  if (!EnsureResources(context.hdrSceneBuffer.width, context.hdrSceneBuffer.height,
                       cascadeLevels, config.giHalfResolution, tier)) {
    return false;
  }

  if (!NoMoreDay::systems::GPUParticleSystem::Get().RenderEmissionSnapshot(
          *context.camera, m_particleEmissive.fbo, context.hdrSceneBuffer.fbo,
          m_particleEmissive.width, m_particleEmissive.height)) {
    return false;
  }
  ++m_vfxEmissionSnapshotVersion;
  m_vfxEmissionSnapshotValid = true;
  return true;
}

bool RadianceCascadesPass::EnsureResources(const int fullWidth, const int fullHeight,
                                           const uint32_t cascadeLevels,
                                           const bool halfResolution,
                                           const core::QualityTier tier) {
  if (fullWidth <= 0 || fullHeight <= 0 || cascadeLevels == 0u ||
      cascadeLevels > kMaxCascadeLevels) {
    return false;
  }

  const uint32_t emissiveFormat = RenderConstants::V5GI::kEmissiveFormat;
  const uint32_t radianceFormat = RenderConstants::V5GI::kRadianceFormat;

  if (!m_emissiveBase.IsValid()) {
    m_emissiveBase =
        resources::FramebufferManager::Create(fullWidth, fullHeight, emissiveFormat, false);
  } else if (m_emissiveBase.width != fullWidth || m_emissiveBase.height != fullHeight) {
    resources::FramebufferManager::Resize(m_emissiveBase, fullWidth, fullHeight);
  }

  if (!m_particleEmissive.IsValid()) {
    m_particleEmissive = resources::FramebufferManager::Create(
        fullWidth, fullHeight, emissiveFormat, false);
  } else if (m_particleEmissive.width != fullWidth ||
             m_particleEmissive.height != fullHeight) {
    resources::FramebufferManager::Resize(m_particleEmissive, fullWidth, fullHeight);
  }

  if (!m_emissiveCombined.IsValid()) {
    m_emissiveCombined = resources::FramebufferManager::Create(
        fullWidth, fullHeight, emissiveFormat, false);
  } else if (m_emissiveCombined.width != fullWidth ||
             m_emissiveCombined.height != fullHeight) {
    resources::FramebufferManager::Resize(m_emissiveCombined, fullWidth, fullHeight);
  }

  const int baseWidth = halfResolution ? std::max(1, (fullWidth + 1) / 2) : fullWidth;
  const int baseHeight =
      halfResolution ? std::max(1, (fullHeight + 1) / 2) : fullHeight;
  for (uint32_t level = 0; level < cascadeLevels; ++level) {
    const int levelWidth = std::max(1, baseWidth >> static_cast<int>(level));
    const int levelHeight = std::max(1, baseHeight >> static_cast<int>(level));
    const uint32_t directions = ResolveRaysPerProbe(level, cascadeLevels, tier);
    auto &target = m_cascadeRadiance[level];
    if (!target.IsValid() || target.width != levelWidth || target.height != levelHeight ||
        target.directions != directions) {
      DestroyCascadeTarget(target);
      uint32_t tex = 0u;
      NoMoreDay::utils::GPUUtils::GenTextures(1, &tex);
      NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, tex);
      NoMoreDay::utils::GPUUtils::TexStorage3D(kGLTexture2DArray, 1,
                                              radianceFormat, levelWidth, levelHeight,
                                              static_cast<int>(directions));
      NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2DArray,
                                                kGLTextureMinFilter,
                                                kGLLinear);
      NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2DArray,
                                                kGLTextureMagFilter,
                                                kGLLinear);
      NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2DArray,
                                                kGLTextureWrapS,
                                                kGLClampToEdge);
      NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2DArray,
                                                kGLTextureWrapT,
                                                kGLClampToEdge);
      NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, 0u);
      target.texture = tex;
      target.width = levelWidth;
      target.height = levelHeight;
      target.directions = directions;
    }
  }
  for (uint32_t level = cascadeLevels; level < kMaxCascadeLevels; ++level) {
    DestroyCascadeTarget(m_cascadeRadiance[level]);
  }

  const bool ok = m_emissiveBase.IsValid() && m_particleEmissive.IsValid() &&
                  m_emissiveCombined.IsValid() && m_cascadeRadiance[0].IsValid();
  if (!ok) {
    return false;
  }

  m_cachedFullWidth = fullWidth;
  m_cachedFullHeight = fullHeight;
  m_cachedCascadeLevels = cascadeLevels;
  m_cachedHalfResolution = halfResolution;
  m_cachedTier = tier;
  return true;
}

bool RadianceCascadesPass::ClearParticleCounter() {
  if (m_particleCounterBuffer.GetId() == 0) {
    return false;
  }
  const uint32_t zero = 0u;
  m_particleCounterBuffer.Update(&zero, sizeof(uint32_t), 0u);
  return true;
}

uint32_t RadianceCascadesPass::ReadParticleCounterForTesting() const {
  if (m_particleCounterBuffer.GetId() == 0) {
    return 0u;
  }
  uint32_t count = 0u;
  m_particleCounterBuffer.Read(&count, sizeof(uint32_t), 0u);
  return count;
}

bool RadianceCascadesPass::RunEmissiveBuild(const graph::RenderContext &context,
                                            const int width, const int height) {
  if (m_emissiveBuildShader.id == 0 || !m_emissiveBase.IsValid() ||
      !context.hdrSceneBuffer.IsValid() || context.camera == nullptr) {
    return false;
  }

  rlEnableShader(m_emissiveBuildShader.id);
  const int resolution[2] = {width, height};
  if (m_emissiveResolutionLoc >= 0) {
    rlSetUniform(m_emissiveResolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  const int sceneTexUnit = 0;
  if (m_emissiveSceneTextureLoc >= 0) {
    rlSetUniform(m_emissiveSceneTextureLoc, &sceneTexUnit, RL_SHADER_UNIFORM_INT, 1);
  }

  const int lightCount = lighting::LightManager::Get().GetActiveLightCount();
  if (m_emissiveLightCountLoc >= 0) {
    rlSetUniform(m_emissiveLightCountLoc, &lightCount, RL_SHADER_UNIFORM_INT, 1);
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom),
  };
  if (m_emissiveCameraOffsetLoc >= 0) {
    rlSetUniform(m_emissiveCameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_emissiveScreenSizeLoc >= 0) {
    rlSetUniform(m_emissiveScreenSizeLoc, screenSize, RL_SHADER_UNIFORM_VEC2, 1);
  }

  lighting::LightManager::Get().Bind();
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D,
                                          context.hdrSceneBuffer.colorTexture);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kEmissiveImageBinding, m_emissiveBase.colorTexture, 0,
      false, 0, kGLWriteOnly, kGLRgba16f);
  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("RadianceEmissiveBuild");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
        DivUp(static_cast<uint32_t>(width), kGLComputeGroupSize),
        DivUp(static_cast<uint32_t>(height), kGLComputeGroupSize), 1u);
  }
  rlDisableShader();

  // Same-pass sync before the material-emissive dispatches read the emissive
  // base image: emitted from the Setup AddPhaseBarrier(Compute, Compute, ...).
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  return true;
}

bool RadianceCascadesPass::RunMaterialEmissive(
    const graph::RenderContext &context, const int width, const int height) {
  m_lastMaterialStampCount = 0u;
  if (m_materialEmissiveShader.id == 0 || !m_emissiveBase.IsValid() ||
      context.camera == nullptr) {
    return false;
  }

  auto &textureArrays = NoMoreDay::render::TextureArrayManager::Get();
  if (!textureArrays.IsInitialized()) {
    textureArrays.Initialize();
  }
  const uint32_t maskArrayTexture =
      textureArrays.GetTextureId(NoMoreDay::render::TextureArraySemantic::Mask);
  if (maskArrayTexture == 0u) {
    return true;
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom),
  };

  rlEnableShader(m_materialEmissiveShader.id);
  const int resolution[2] = {width, height};
  if (m_materialResolutionLoc >= 0) {
    rlSetUniform(m_materialResolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  const int maskTextureUnit = 0;
  if (m_materialMaskArrayLoc >= 0) {
    rlSetUniform(m_materialMaskArrayLoc, &maskTextureUnit, RL_SHADER_UNIFORM_INT, 1);
  }

  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, maskArrayTexture);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kEmissiveImageBinding, m_emissiveBase.colorTexture, 0,
      false, 0, kGLReadWrite, kGLRgba16f);

  uint32_t stampCount = 0u;
  for (uint32_t stampIndex = 0u; stampIndex < context.emissiveStampCount;
       ++stampIndex) {
    const auto &stamp = context.emissiveStamps[stampIndex];
    const int halfExtentPixels =
        std::max(2, static_cast<int>(std::ceil(stamp.worldHalfExtent * zoom)));
    const int dispatchSize[2] = {halfExtentPixels * 2, halfExtentPixels * 2};
    const int centerPx[2] = {
        static_cast<int>(std::floor((stamp.worldPos.x - cameraOffset[0]) * zoom)),
        static_cast<int>(std::floor((stamp.worldPos.y - cameraOffset[1]) * zoom)),
    };
    const int dispatchOrigin[2] = {centerPx[0] - halfExtentPixels,
                                   centerPx[1] - halfExtentPixels};
    if (dispatchOrigin[0] >= width || dispatchOrigin[1] >= height ||
        (dispatchOrigin[0] + dispatchSize[0]) <= 0 ||
        (dispatchOrigin[1] + dispatchSize[1]) <= 0) {
      continue;
    }

    if (m_materialMaskLayerLoc >= 0) {
      rlSetUniform(m_materialMaskLayerLoc, &stamp.maskLayer, RL_SHADER_UNIFORM_INT,
                   1);
    }
    if (m_materialDispatchOriginLoc >= 0) {
      rlSetUniform(m_materialDispatchOriginLoc, dispatchOrigin, RL_SHADER_UNIFORM_IVEC2,
                   1);
    }
    if (m_materialDispatchSizeLoc >= 0) {
      rlSetUniform(m_materialDispatchSizeLoc, dispatchSize, RL_SHADER_UNIFORM_IVEC2, 1);
    }
    const float emission[4] = {stamp.emissionRGBA.x, stamp.emissionRGBA.y,
                               stamp.emissionRGBA.z, stamp.emissionRGBA.w};
    if (m_materialEmissionLoc >= 0) {
      rlSetUniform(m_materialEmissionLoc, emission, RL_SHADER_UNIFORM_VEC4, 1);
    }

    {
      NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("RadianceMaterialStamp");
      NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
          DivUp(static_cast<uint32_t>(dispatchSize[0]), kGLComputeGroupSize),
          DivUp(static_cast<uint32_t>(dispatchSize[1]), kGLComputeGroupSize), 1u);
    }
    // Same-pass sync: successive stamp dispatches read-modify-write the same
    // emissive image; emitted at this exact execution point.
    context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                             graph::PipelineStage::Compute);
    ++stampCount;
  }

  rlDisableShader();
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  // Same-pass sync before the emissive merge dispatch reads the emissive base.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  m_lastMaterialStampCount = stampCount;
  return true;
}

bool RadianceCascadesPass::RunParticleEmissive(const graph::RenderContext &context,
                                               const int width, const int height) {
  (void)context;
  (void)width;
  (void)height;
  return m_particleEmissive.IsValid() && m_vfxEmissionSnapshotValid;
}

bool RadianceCascadesPass::RunEmissiveMerge(const graph::RenderContext &context,
                                            const int width, const int height) {
  if (m_emissiveMergeShader.id == 0 || !m_emissiveBase.IsValid() ||
      !m_particleEmissive.IsValid() || !m_emissiveCombined.IsValid()) {
    return false;
  }

  rlEnableShader(m_emissiveMergeShader.id);
  const int resolution[2] = {width, height};
  if (m_mergeResolutionLoc >= 0) {
    rlSetUniform(m_mergeResolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  constexpr uint32_t kBaseBinding = 0u;
  constexpr uint32_t kParticleBinding = 1u;
  constexpr uint32_t kCombinedBinding = RenderConstants::V5GI::kEmissiveImageBinding;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kBaseBinding, m_emissiveBase.colorTexture,
                                               0, false, 0, kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      kParticleBinding, m_particleEmissive.colorTexture, 0, false, 0, kGLReadOnly,
      kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kCombinedBinding,
                                               m_emissiveCombined.colorTexture, 0,
                                               false, 0, kGLWriteOnly, kGLRgba16f);

  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("RadianceEmissiveMerge");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
        DivUp(static_cast<uint32_t>(width), kGLComputeGroupSize),
        DivUp(static_cast<uint32_t>(height), kGLComputeGroupSize), 1u);
  }
  rlDisableShader();

  // Same-pass sync before the cascade trace dispatch reads the combined
  // emissive image (and before the level-0 radiance result leaves this pass).
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  return true;
}

bool RadianceCascadesPass::RunCascadeTrace(const graph::RenderContext &context,
                                           const uint32_t cascadeLevels,
                                           const bool holographicMode,
                                           const core::QualityTier tier) {
  if (m_radianceCascadeShader.id == 0 || context.giDistanceFieldTexture == 0u ||
      !m_emissiveCombined.IsValid() || cascadeLevels == 0u) {
    return false;
  }

  const int fullResolution[2] = {m_cachedFullWidth, m_cachedFullHeight};
  const int emissiveTextureUnit = 0;
  const int parentTextureUnit = 1;
  rlEnableShader(m_radianceCascadeShader.id);
  if (m_radianceFullResolutionLoc >= 0) {
    rlSetUniform(m_radianceFullResolutionLoc, fullResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_radianceEmissiveTextureLoc >= 0) {
    rlSetUniform(m_radianceEmissiveTextureLoc, &emissiveTextureUnit,
                 RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_radianceParentTextureLoc >= 0) {
    rlSetUniform(m_radianceParentTextureLoc, &parentTextureUnit, RL_SHADER_UNIFORM_INT,
                 1);
  }
  const int cascadeCountInt = static_cast<int>(cascadeLevels);
  if (m_radianceCascadeCountLoc >= 0) {
    rlSetUniform(m_radianceCascadeCountLoc, &cascadeCountInt, RL_SHADER_UNIFORM_INT, 1);
  }
  const int holographic = holographicMode ? 1 : 0;
  if (m_radianceHolographicLoc >= 0) {
    rlSetUniform(m_radianceHolographicLoc, &holographic, RL_SHADER_UNIFORM_INT, 1);
  }

  m_radianceConfigBuffer.BindBase(kRadianceConfigBinding);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kDistanceFieldImageBinding, context.giDistanceFieldTexture,
      0, false, 0, kGLReadOnly, kGLR16f);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_emissiveCombined.colorTexture);

  for (int level = static_cast<int>(cascadeLevels) - 1; level >= 0; --level) {
    auto &target = m_cascadeRadiance[static_cast<size_t>(level)];
    if (!target.IsValid()) {
      rlDisableShader();
      return false;
    }

    const int levelResolution[2] = {target.width, target.height};
    if (m_radianceCascadeResolutionLoc >= 0) {
      rlSetUniform(m_radianceCascadeResolutionLoc, levelResolution,
                   RL_SHADER_UNIFORM_IVEC2, 1);
    }
    if (m_radianceCascadeLevelLoc >= 0) {
      rlSetUniform(m_radianceCascadeLevelLoc, &level, RL_SHADER_UNIFORM_INT, 1);
    }

    const uint32_t raysPerProbe = ResolveRaysPerProbe(static_cast<uint32_t>(level),
                                                      cascadeLevels, tier);
    const int raysPerProbeInt = static_cast<int>(raysPerProbe);
    if (m_radianceRaysPerProbeLoc >= 0) {
      rlSetUniform(m_radianceRaysPerProbeLoc, &raysPerProbeInt, RL_SHADER_UNIFORM_INT,
                   1);
    }

    const bool hasParent = (level + 1) < static_cast<int>(cascadeLevels) &&
                           m_cascadeRadiance[static_cast<size_t>(level + 1)].IsValid();
    const uint32_t parentRays = hasParent
        ? ResolveRaysPerProbe(static_cast<uint32_t>(level + 1), cascadeLevels, tier)
        : 0u;
    const int parentRaysInt = static_cast<int>(parentRays);
    if (m_radianceParentRaysLoc >= 0) {
      rlSetUniform(m_radianceParentRaysLoc, &parentRaysInt, RL_SHADER_UNIFORM_INT, 1);
    }

    const float rayMin = ResolveRayMinLength(static_cast<uint32_t>(level));
    const float rayMax = ResolveRayMaxLength(static_cast<uint32_t>(level));
    if (m_radianceRayMinLengthLoc >= 0) {
      rlSetUniform(m_radianceRayMinLengthLoc, &rayMin, RL_SHADER_UNIFORM_FLOAT, 1);
    }
    if (m_radianceRayMaxLengthLoc >= 0) {
      rlSetUniform(m_radianceRayMaxLengthLoc, &rayMax, RL_SHADER_UNIFORM_FLOAT, 1);
    }

    const int parentValid = hasParent ? 1 : 0;
    if (m_radianceParentValidLoc >= 0) {
      rlSetUniform(m_radianceParentValidLoc, &parentValid, RL_SHADER_UNIFORM_INT, 1);
    }

    NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1u);
    NoMoreDay::utils::GPUUtils::BindTexture(
        kGLTexture2DArray,
        hasParent ? m_cascadeRadiance[static_cast<size_t>(level + 1)].texture
                  : target.texture);
    NoMoreDay::utils::GPUUtils::BindImageTexture(
        RenderConstants::V5GI::kRadianceImageBinding, target.texture, 0, true, 0,
        kGLWriteOnly, RenderConstants::V5GI::kRadianceFormat);
    {
      NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("RadianceCascadeTrace");
      NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
          DivUp(static_cast<uint32_t>(target.width), kGLComputeGroupSize),
          DivUp(static_cast<uint32_t>(target.height), kGLComputeGroupSize), 1u);
    }
    // Same-pass sync: each cascade level's dispatch reads the coarser level
    // written by the previous iteration; emitted at this exact execution point.
    context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                             graph::PipelineStage::Compute);
  }

  rlDisableShader();
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1u);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, 0u);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  return true;
}

void RadianceCascadesPass::UploadConfig(const graph::RenderContext &context,
                                        const uint32_t cascadeLevels,
                                        const bool halfResolution,
                                        const core::QualityTier tier) {
  if (context.qualityManager == nullptr || m_radianceConfigBuffer.GetId() == 0) {
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  ::NoMoreDay::components::RadianceCascadeConfig payload = {};
  payload.numLevels = cascadeLevels;
  payload.raysPerProbe = ResolveRaysPerProbe(0u, cascadeLevels, tier);
  payload.baseInterval = 4.0f;
  payload.temporalWeight = std::clamp(config.giTemporalWeight, 0.0f, 0.98f);
  payload.halfResolution = halfResolution ? 1u : 0u;
  payload.sdfUpdateInterval = std::max<uint32_t>(1u, config.giSdfUpdateInterval);
  payload.giIntensity = std::max(0.0f, config.giIntensity);
  payload.reserved = 0u;
  m_radianceConfigBuffer.Update(&payload, sizeof(payload), 0u);
}

uint32_t RadianceCascadesPass::ResolveRaysPerProbe(
    const uint32_t cascadeLevel, const uint32_t cascadeLevels,
    const core::QualityTier tier) noexcept {
  if (tier == core::QualityTier::Low || tier == core::QualityTier::Medium) {
    return 1u;
  }
  if (tier == core::QualityTier::High) {
    return std::max(2u, 4u >> cascadeLevel);
  }
  // Ultra (or default, directions_k = max(2, 16 >> k)):
  return std::max(2u, 16u >> cascadeLevel);
}

float RadianceCascadesPass::ResolveRayMinLength(const uint32_t cascadeLevel) noexcept {
  if (cascadeLevel == 0u) {
    return 0.0f;
  }
  const float maxDistance = ResolveRayMaxLength(cascadeLevel);
  return maxDistance * 0.5f;
}

float RadianceCascadesPass::ResolveRayMaxLength(const uint32_t cascadeLevel) noexcept {
  const uint32_t scale = 1u << cascadeLevel;
  return 4.0f * static_cast<float>(scale);
}

void RadianceCascadesPass::ReportFailure(const char *reason) {
  m_lastExecuteFailure = true;
  m_lastExecuteSuccess = false;
  m_lastFailureReason = (reason != nullptr) ? reason : "unknown";
  LOG_WARN("RadianceCascadesPass fallback: frame={} reason={}", m_frameIndex,
           m_lastFailureReason);
}

void RadianceCascadesPass::MarkSuccess() {
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = true;
  m_lastFailureReason.clear();
}

void RadianceCascadesPass::LogBarrierAuditOnce() {
  if (m_barrierAuditLogged) {
    return;
  }
  m_barrierAuditLogged = true;
  LOG_INFO("RadianceCascadesPass barrier audit: emissive=Image|TextureFetch, "
           "material=Image|TextureFetch, "
           "particle=Image|Buffer|TextureFetch, merge=Image|TextureFetch, "
           "cascade=Image|TextureFetch");
}

void RadianceCascadesPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();
  m_lastMaterialStampCount = 0u;
  m_lastParticleWriteCount = 0u;

  if (context.qualityManager == nullptr || context.resources == nullptr) {
    ReportFailure("missing radiance prerequisites");
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled || config.giCascadeLevels == 0u) {
    MarkSuccess();
    return;
  }
  if (!context.hdrSceneBuffer.IsValid()) {
    ReportFailure("hdr scene buffer unavailable");
    return;
  }
  if (context.giDistanceFieldTexture == 0u) {
    ReportFailure("distance field texture unavailable");
    return;
  }
  if (!m_initialized && !Initialize(*context.resources)) {
    ReportFailure("failed to initialize radiance shaders");
    return;
  }

  const auto tier = context.qualityManager->GetTier();
  const uint32_t cascadeLevels = std::clamp<uint32_t>(config.giCascadeLevels, 1u,
                                                      kMaxCascadeLevels);
  const bool halfResolution = config.giHalfResolution;
  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (!EnsureResources(width, height, cascadeLevels, halfResolution, tier)) {
    ReportFailure("failed to allocate radiance resources");
    return;
  }

  UploadConfig(context, cascadeLevels, halfResolution, tier);
  if (!RunEmissiveBuild(context, width, height)) {
    ReportFailure("emissive build failed");
    return;
  }
  if (!RunMaterialEmissive(context, width, height)) {
    ReportFailure("material emissive pass failed");
    return;
  }
  if (!RunParticleEmissive(context, width, height)) {
    ReportFailure("particle emissive pass failed");
    return;
  }
  if (!RunEmissiveMerge(context, width, height)) {
    ReportFailure("emissive merge failed");
    return;
  }
  if (!RunCascadeTrace(context, cascadeLevels, config.giHolographicEnabled, tier)) {
    ReportFailure("radiance cascade trace failed");
    return;
  }

  context.giEmissiveTexture = m_emissiveCombined.colorTexture;
  context.giEmissiveWidth = m_emissiveCombined.width;
  context.giEmissiveHeight = m_emissiveCombined.height;
  context.giRadianceTexture = m_cascadeRadiance[0].texture;
  context.giRadianceWidth = m_cascadeRadiance[0].width;
  context.giRadianceHeight = m_cascadeRadiance[0].height;
  context.giRadianceDirections = m_cascadeRadiance[0].directions;

  if ((m_frameIndex % 120u) == 0u) {
    LOG_INFO(
        "RadianceCascadesPass debug: frame={} cascades={} halfRes={} "
        "radiance={}x{} dirs={} materialStamps={} particleWrites={} holographic={}",
        m_frameIndex, cascadeLevels, halfResolution ? 1 : 0, m_cascadeRadiance[0].width,
        m_cascadeRadiance[0].height, m_cascadeRadiance[0].directions,
        m_lastMaterialStampCount, m_lastParticleWriteCount,
        config.giHolographicEnabled ? 1 : 0);
  }

  LogBarrierAuditOnce();
  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
