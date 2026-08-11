#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLReadFramebuffer = 0x8CA8;
constexpr uint32_t kGLDrawFramebuffer = 0x8CA9;
constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLColorBufferBit = 0x00004000;
constexpr uint32_t kGLComputeGroupSize = 8u;
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

void HashMix(uint64_t &hash, const uint64_t value) {
  hash ^= value;
  hash *= kFnvPrime;
}

} // namespace

GICompositePass::GICompositePass() = default;

GICompositePass::~GICompositePass() { Shutdown(); }

void GICompositePass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::GIComposite);
  builder.Read(graph::RenderResourceTag::RadianceMap,
               graph::RenderOwnerTag::RadianceCascades);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::GIComposite);

  // Same-pass phase barrier: the composite compute dispatch writes the output
  // scene image; the framebuffer blit that follows in this Execute reads it.
  // Declared here and emitted via EmitPhaseBarrier(Compute, Fragment) at the
  // exact execution point between dispatch and blit.
  builder.AddPhaseBarrier(
      graph::PipelineStage::Compute, graph::PipelineStage::Fragment,
      static_cast<uint32_t>(RenderConstants::Barrier::Image) |
          static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
}

bool GICompositePass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_compositeShader = resources.loadComputeShader(
      "v5_gi_composite_compute"_hs, "assets/shaders/lighting/v5_gi_composite.comp");
  if (m_compositeShader.id == 0) {
    Shutdown();
    return false;
  }

  m_sceneResolutionLoc = rlGetLocationUniform(m_compositeShader.id, "uSceneResolution");
  m_radianceResolutionLoc =
      rlGetLocationUniform(m_compositeShader.id, "uRadianceResolution");
  m_temporalWeightLoc = rlGetLocationUniform(m_compositeShader.id, "uTemporalWeight");
  m_giIntensityLoc = rlGetLocationUniform(m_compositeShader.id, "uGiIntensity");
  m_resetHistoryLoc = rlGetLocationUniform(m_compositeShader.id, "uResetHistory");
  m_cameraDeltaUvLoc = rlGetLocationUniform(m_compositeShader.id, "uCameraDeltaUv");
  m_zoomRatioLoc = rlGetLocationUniform(m_compositeShader.id, "uZoomRatio");
  m_occupancyEnabledLoc =
      rlGetLocationUniform(m_compositeShader.id, "uOccupancyEnabled");

  m_initialized = true;
  return true;
}

void GICompositePass::Shutdown() {
  m_compositeShader = {};
  resources::FramebufferManager::Destroy(m_outputScene);
  resources::FramebufferManager::Destroy(m_historyA);
  resources::FramebufferManager::Destroy(m_historyB);
  resources::FramebufferManager::Destroy(m_occupancyHistoryA);
  resources::FramebufferManager::Destroy(m_occupancyHistoryB);
  m_sceneResolutionLoc = -1;
  m_radianceResolutionLoc = -1;
  m_temporalWeightLoc = -1;
  m_giIntensityLoc = -1;
  m_resetHistoryLoc = -1;
  m_cameraDeltaUvLoc = -1;
  m_zoomRatioLoc = -1;
  m_occupancyEnabledLoc = -1;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_initialized = false;
  m_historyValid = false;
  m_readHistoryA = true;
  m_prevCameraValid = false;
  m_prevCameraTarget = {0.0f, 0.0f};
  m_prevCameraZoom = 0.0f;
  m_prevLightSignature = 0u;
  m_prevOccluderMaskVersion = 0u;
  m_prevVfxEmissionSnapshotVersion = 0u;
}

void GICompositePass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
}

bool GICompositePass::EnsureResources(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  if (!m_outputScene.IsValid()) {
    m_outputScene = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_outputScene.width != width || m_outputScene.height != height) {
    resources::FramebufferManager::Resize(m_outputScene, width, height);
  }

  if (!m_historyA.IsValid()) {
    m_historyA = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_historyA.width != width || m_historyA.height != height) {
    resources::FramebufferManager::Resize(m_historyA, width, height);
  }

  if (!m_historyB.IsValid()) {
    m_historyB = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_historyB.width != width || m_historyB.height != height) {
    resources::FramebufferManager::Resize(m_historyB, width, height);
  }

  // M0-A R3: persistent R8 occupancy/depth history ping-pong. The current-frame
  // occupancy (occluder mask) is stored each execute so the next frame can
  // compare occupancy at the reprojected UV for disocclusion rejection.
  const uint32_t occupancyFormat = RenderConstants::V5GI::kOccluderMaskFormat;
  if (!m_occupancyHistoryA.IsValid()) {
    m_occupancyHistoryA =
        resources::FramebufferManager::Create(width, height, occupancyFormat, false);
  } else if (m_occupancyHistoryA.width != width ||
             m_occupancyHistoryA.height != height) {
    resources::FramebufferManager::Resize(m_occupancyHistoryA, width, height);
  }

  if (!m_occupancyHistoryB.IsValid()) {
    m_occupancyHistoryB =
        resources::FramebufferManager::Create(width, height, occupancyFormat, false);
  } else if (m_occupancyHistoryB.width != width ||
             m_occupancyHistoryB.height != height) {
    resources::FramebufferManager::Resize(m_occupancyHistoryB, width, height);
  }

  if (m_cachedWidth != width || m_cachedHeight != height) {
    m_historyValid = false;
    m_cachedWidth = width;
    m_cachedHeight = height;
  }

  return m_outputScene.IsValid() && m_historyA.IsValid() && m_historyB.IsValid() &&
         m_occupancyHistoryA.IsValid() && m_occupancyHistoryB.IsValid();
}

uint64_t GICompositePass::BuildLightSignature() const {
  const auto &lights = lighting::LightManager::Get().GetActiveLightsCpu();
  uint64_t signature = kFnvOffset;
  for (const auto &light : lights) {
    const int32_t px = static_cast<int32_t>(std::lround(light.posX * 8.0f));
    const int32_t py = static_cast<int32_t>(std::lround(light.posY * 8.0f));
    const int32_t radius = static_cast<int32_t>(std::lround(light.radius * 8.0f));
    const int32_t intensity =
        static_cast<int32_t>(std::lround(light.intensity * 32.0f));
    const int32_t colorR = static_cast<int32_t>(std::lround(light.colorR * 255.0f));
    const int32_t colorG = static_cast<int32_t>(std::lround(light.colorG * 255.0f));
    const int32_t colorB = static_cast<int32_t>(std::lround(light.colorB * 255.0f));
    const uint64_t packed = (static_cast<uint64_t>(static_cast<uint32_t>(px)) << 32u) ^
                            static_cast<uint64_t>(static_cast<uint32_t>(py));
    const uint64_t packed2 =
        (static_cast<uint64_t>(static_cast<uint32_t>(radius)) << 32u) ^
        static_cast<uint64_t>(static_cast<uint32_t>(intensity));
    const uint64_t packed3 = (static_cast<uint64_t>(static_cast<uint32_t>(colorR)) << 40u) ^
                             (static_cast<uint64_t>(static_cast<uint32_t>(colorG))
                              << 20u) ^
                             static_cast<uint64_t>(static_cast<uint32_t>(colorB));
    HashMix(signature, packed);
    HashMix(signature, packed2);
    HashMix(signature, packed3);
  }
  HashMix(signature, static_cast<uint64_t>(lights.size()));
  return signature;
}

void GICompositePass::Execute(graph::RenderContext &context) {
  if (context.qualityManager == nullptr || context.resources == nullptr ||
      context.camera == nullptr) {
    return;
  }
  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled || !context.hdrSceneBuffer.IsValid() ||
      context.giRadianceTexture == 0u) {
    return;
  }
  if (!m_initialized && !Initialize(*context.resources)) {
    return;
  }

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  const int previousWidth = m_cachedWidth;
  const int previousHeight = m_cachedHeight;
  if (!EnsureResources(width, height)) {
    return;
  }
  const bool extentChanged =
      (previousWidth != width || previousHeight != height);

  const uint64_t lightSignature = BuildLightSignature();
  const bool lightChanged = m_historyValid && (lightSignature != m_prevLightSignature);

  float temporalWeight = std::clamp(config.giTemporalWeight, 0.55f, 0.98f);
  Vector2 cameraDeltaUv = {0.0f, 0.0f};
  float zoomRatio = 1.0f;
  if (m_prevCameraValid) {
    const Vector2 delta = {
        context.camera->target.x - m_prevCameraTarget.x,
        context.camera->target.y - m_prevCameraTarget.y,
    };
    const float cameraDelta = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (cameraDelta > 0.0f) {
      temporalWeight -= std::min(cameraDelta * 0.015f, 0.35f);
      temporalWeight = std::max(temporalWeight, 0.55f);
    } else {
      temporalWeight = std::min(0.97f, temporalWeight + 0.02f);
    }

    const float zoom = std::max(context.camera->zoom, 0.0001f);
    const float prevZoom = std::max(m_prevCameraZoom, 0.0001f);
    zoomRatio = zoom / prevZoom;

    const float worldWidth = static_cast<float>(width) / zoom;
    const float worldHeight = static_cast<float>(height) / zoom;
    cameraDeltaUv.x = (worldWidth > 1e-4f) ? (delta.x / worldWidth) : 0.0f;
    cameraDeltaUv.y = (worldHeight > 1e-4f) ? (delta.y / worldHeight) : 0.0f;
  }

  const uint64_t occluderVersion =
      (m_occluderExtractPass != nullptr) ? m_occluderExtractPass->GetMaskVersion() : 0u;
  const bool occluderChanged =
      m_historyValid && (m_prevOccluderMaskVersion != 0u) &&
      (occluderVersion != m_prevOccluderMaskVersion);

  // M0-A R2 closure: the VFX emission snapshot version is a GI input version.
  // When the frozen emission layer changed since the last composite, history
  // produced against the previous emission must not be reused.
  const uint64_t vfxEmissionVersion =
      (m_radianceCascadesPass != nullptr)
          ? m_radianceCascadesPass->GetVfxEmissionSnapshotVersion()
          : 0u;
  const bool emissiveChanged =
      m_historyValid && (vfxEmissionVersion != m_prevVfxEmissionSnapshotVersion);

  const bool resetHistory =
      !m_historyValid || extentChanged || lightChanged || occluderChanged ||
      emissiveChanged;
  if (resetHistory) {
    temporalWeight = 0.0f;
  }

  const auto &historyRead = m_readHistoryA ? m_historyA : m_historyB;
  auto &historyWrite = m_readHistoryA ? m_historyB : m_historyA;
  if (!historyRead.IsValid() || !historyWrite.IsValid()) {
    return;
  }

  rlEnableShader(m_compositeShader.id);
  const int sceneResolution[2] = {width, height};
  const int radianceWidth = std::max(1, context.giRadianceWidth);
  const int radianceHeight = std::max(1, context.giRadianceHeight);
  const int radianceResolution[2] = {radianceWidth, radianceHeight};
  if (m_sceneResolutionLoc >= 0) {
    rlSetUniform(m_sceneResolutionLoc, sceneResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_radianceResolutionLoc >= 0) {
    rlSetUniform(m_radianceResolutionLoc, radianceResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_temporalWeightLoc >= 0) {
    rlSetUniform(m_temporalWeightLoc, &temporalWeight, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const float giIntensity = std::max(0.0f, config.giIntensity);
  if (m_giIntensityLoc >= 0) {
    rlSetUniform(m_giIntensityLoc, &giIntensity, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const int resetHistoryInt = resetHistory ? 1 : 0;
  if (m_resetHistoryLoc >= 0) {
    rlSetUniform(m_resetHistoryLoc, &resetHistoryInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_cameraDeltaUvLoc >= 0) {
    const float deltaArray[2] = {cameraDeltaUv.x, cameraDeltaUv.y};
    rlSetUniform(m_cameraDeltaUvLoc, deltaArray, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_zoomRatioLoc >= 0) {
    rlSetUniform(m_zoomRatioLoc, &zoomRatio, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  constexpr uint32_t kSceneInBinding = 0u;
  constexpr uint32_t kRadianceInBinding = 1u;
  constexpr uint32_t kHistoryInBinding = 2u;
  constexpr uint32_t kSceneOutBinding = 3u;
  constexpr uint32_t kHistoryOutBinding = 4u;
  constexpr uint32_t kOccupancyCurrInBinding = 5u;
  constexpr uint32_t kOccupancyPrevInBinding = 6u;
  constexpr uint32_t kOccupancyOutBinding = 7u;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kSceneInBinding,
                                               context.hdrSceneBuffer.colorTexture, 0,
                                               false, 0, kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kRadianceInBinding,
                                               context.giRadianceTexture, 0, false, 0,
                                               kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHistoryInBinding,
                                               historyRead.colorTexture, 0, false, 0,
                                               kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kSceneOutBinding,
                                               m_outputScene.colorTexture, 0, false, 0,
                                               kGLWriteOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHistoryOutBinding,
                                               historyWrite.colorTexture, 0, false, 0,
                                               kGLWriteOnly, kGLRgba16f);

  // M0-A R3: occupancy/depth disocclusion rejection. The current-frame
  // occupancy is the occluder mask texture (GL_R8, 0/1) owned by
  // OccluderExtractPass. Previous occupancy is read from the persistent R8
  // ping-pong history at the reprojected UV; any mismatch zeroes the history
  // weight. The current occupancy is always written to the outgoing history
  // slot for the next frame.
  const uint32_t occupancyFormat = RenderConstants::V5GI::kOccluderMaskFormat;
  const uint32_t currentOccupancyTexture =
      (m_occluderExtractPass != nullptr)
          ? m_occluderExtractPass->GetOccluderMaskTexture()
          : 0u;
  const auto &occupancyRead =
      m_readHistoryA ? m_occupancyHistoryA : m_occupancyHistoryB;
  auto &occupancyWrite = m_readHistoryA ? m_occupancyHistoryB : m_occupancyHistoryA;
  const bool occupancyAvailable = (currentOccupancyTexture != 0u) &&
                                  occupancyRead.IsValid() &&
                                  occupancyWrite.IsValid();
  if (occupancyAvailable) {
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyCurrInBinding,
                                                 currentOccupancyTexture, 0, false, 0,
                                                 kGLReadOnly, occupancyFormat);
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyPrevInBinding,
                                                 occupancyRead.colorTexture, 0, false,
                                                 0, kGLReadOnly, occupancyFormat);
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyOutBinding,
                                                 occupancyWrite.colorTexture, 0, false,
                                                 0, kGLWriteOnly, occupancyFormat);
  }
  const int occupancyEnabledInt = occupancyAvailable ? 1 : 0;
  if (m_occupancyEnabledLoc >= 0) {
    rlSetUniform(m_occupancyEnabledLoc, &occupancyEnabledInt, RL_SHADER_UNIFORM_INT, 1);
  }

  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("GIComposite");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
        DivUp(static_cast<uint32_t>(width), kGLComputeGroupSize),
        DivUp(static_cast<uint32_t>(height), kGLComputeGroupSize), 1u);
  }
  rlDisableShader();

  // Same-pass sync before the blit reads the composite output: emitted at this
  // exact execution point from the Setup AddPhaseBarrier(Compute, Fragment, ...)
  // declaration.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Fragment);

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLReadFramebuffer, m_outputScene.fbo);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLDrawFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    static_cast<int>(kGLColorBufferBit));
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, width, height);

  if (resetHistory) {
    ++m_historyResetCount;
    if (!m_historyValid || extentChanged) {
      m_lastResetReason = "extent";
    } else if (lightChanged) {
      m_lastResetReason = "light";
    } else if (occluderChanged) {
      m_lastResetReason = "occluder";
    } else if (emissiveChanged) {
      m_lastResetReason = "emissive";
    } else {
      m_lastResetReason = "initial";
    }
  }

  if (resetHistory && lightChanged) {
    LOG_INFO("GICompositePass: reset temporal history due light signature change");
  } else if (resetHistory && occluderChanged) {
    LOG_INFO("GICompositePass: reset temporal history due occluder mask version change ({})", occluderVersion);
  } else if (resetHistory && emissiveChanged) {
    LOG_INFO("GICompositePass: reset temporal history due VFX emission snapshot version change ({})", vfxEmissionVersion);
  } else if (resetHistory && extentChanged) {
    LOG_INFO("GICompositePass: reset temporal history due resolution change ({}x{})", width, height);
  }

  m_historyValid = true;
  m_readHistoryA = !m_readHistoryA;
  m_prevCameraValid = true;
  m_prevCameraTarget = context.camera->target;
  m_prevCameraZoom = context.camera->zoom;
  m_prevLightSignature = lightSignature;
  m_prevOccluderMaskVersion = occluderVersion;
  m_prevVfxEmissionSnapshotVersion = vfxEmissionVersion;
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes

