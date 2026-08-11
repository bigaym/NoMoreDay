#include "engine/render/passes/VolumetricLightPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"

#include "rlgl.h"

#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLReadFramebuffer = 0x8CA8;
constexpr uint32_t kGLDrawFramebuffer = 0x8CA9;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLColorBufferBit = 0x00004000;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kVolumetricFragmentShader =
    "assets/shaders/lighting/volumetric_light.frag";

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

} // namespace

VolumetricLightPass::VolumetricLightPass() = default;

VolumetricLightPass::~VolumetricLightPass() { Shutdown(); }

void VolumetricLightPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::Volumetric,
               graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::Volumetric,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);

  // Cluster SSBO consumption: the volumetric shader iterates the lights of the
  // current cluster via the cluster index buffers written by LightCullingPass.
  // Declared only when the clustered-lighting path is actually enabled so the
  // graph emits the cross-pass SSBO transition (0x2000) at this pass's entry.
  // The gate mirrors the Execute() runtime guard (config.v3Enabled &&
  // config.clusteredLightingEnabled); graphs built without a LightCullingPass
  // producer stay valid.
  const auto &config = core::QualityTierManager::Get().GetConfig();
  if (config.v3Enabled && config.clusteredLightingEnabled) {
    builder.Read(graph::RenderResourceTag::ClusterHeaderSSBO,
                 graph::RenderOwnerTag::LightCulling,
                 graph::PipelineStage::Fragment, graph::ResourceUsage::ShaderRead);
    builder.Read(graph::RenderResourceTag::ClusterLightIndexSSBO,
                 graph::RenderOwnerTag::LightCulling,
                 graph::PipelineStage::Fragment, graph::ResourceUsage::ShaderRead);
  }
}

bool VolumetricLightPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_volumetricShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      kFullscreenVertexShader, kVolumetricFragmentShader);
  if (m_volumetricShader.id == 0) {
    LOG_ERROR("VolumetricLightPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_sceneTexLoc = GetShaderLocation(m_volumetricShader, "uSceneTex");
  m_sampleCountLoc = GetShaderLocation(m_volumetricShader, "uSampleCount");
  m_scatteringLoc = GetShaderLocation(m_volumetricShader, "uScattering");
  m_decayLoc = GetShaderLocation(m_volumetricShader, "uDecay");
  m_exposureLoc = GetShaderLocation(m_volumetricShader, "uExposure");
  m_cameraOffsetLoc = GetShaderLocation(m_volumetricShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_volumetricShader, "uScreenSize");
  m_clusterGridXLoc = GetShaderLocation(m_volumetricShader, "uClusterGridX");
  m_clusterGridYLoc = GetShaderLocation(m_volumetricShader, "uClusterGridY");
  m_clusterGridZLoc = GetShaderLocation(m_volumetricShader, "uClusterGridZ");
  m_clusterTileSizeWorldLoc =
      GetShaderLocation(m_volumetricShader, "uClusterTileSizeWorld");
  m_layerBandWorldUnitsLoc =
      GetShaderLocation(m_volumetricShader, "uLayerBandWorldUnits");

  m_initialized = true;
  return true;
}

bool VolumetricLightPass::ReloadShaders() {
  Shader reloaded = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      kFullscreenVertexShader, kVolumetricFragmentShader);
  if (reloaded.id == 0) {
    LOG_WARN("VolumetricLightPass: shader reload failed, keeping previous program");
    return false;
  }
  if (m_volumetricShader.id != 0) {
    UnloadShader(m_volumetricShader);
  }
  m_volumetricShader = reloaded;

  m_sceneTexLoc = GetShaderLocation(m_volumetricShader, "uSceneTex");
  m_sampleCountLoc = GetShaderLocation(m_volumetricShader, "uSampleCount");
  m_scatteringLoc = GetShaderLocation(m_volumetricShader, "uScattering");
  m_decayLoc = GetShaderLocation(m_volumetricShader, "uDecay");
  m_exposureLoc = GetShaderLocation(m_volumetricShader, "uExposure");
  m_cameraOffsetLoc = GetShaderLocation(m_volumetricShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_volumetricShader, "uScreenSize");
  m_clusterGridXLoc = GetShaderLocation(m_volumetricShader, "uClusterGridX");
  m_clusterGridYLoc = GetShaderLocation(m_volumetricShader, "uClusterGridY");
  m_clusterGridZLoc = GetShaderLocation(m_volumetricShader, "uClusterGridZ");
  m_clusterTileSizeWorldLoc =
      GetShaderLocation(m_volumetricShader, "uClusterTileSizeWorld");
  m_layerBandWorldUnitsLoc =
      GetShaderLocation(m_volumetricShader, "uLayerBandWorldUnits");
  LOG_INFO("VolumetricLightPass: shader hot reloaded");
  return true;
}

void VolumetricLightPass::Shutdown() {
  if (m_volumetricShader.id != 0) {
    UnloadShader(m_volumetricShader);
    m_volumetricShader = {};
  }
  resources::FramebufferManager::Destroy(m_outputBuffer);
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_initialized = false;
}

void VolumetricLightPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
  const double approxMb =
      (static_cast<double>(width) * static_cast<double>(height) * 8.0) /
      (1024.0 * 1024.0); // RGBA16F ~= 8 bytes/pixel
  if (!m_outputBuffer.IsValid()) {
    m_outputBuffer =
        resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
    if (m_outputBuffer.IsValid()) {
      LOG_INFO("VolumetricLightPass: created output buffer {}x{} (~{:.2f} MB)",
               width, height, approxMb);
    }
    return;
  }
  resources::FramebufferManager::Resize(m_outputBuffer, width, height);
  LOG_INFO("VolumetricLightPass: resized output buffer {}x{} (~{:.2f} MB)", width,
           height, approxMb);
}

void VolumetricLightPass::DrawFullscreen(Shader shader, uint32_t sourceTexture) {
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, sourceTexture);
  BeginShaderMode(shader);
  resources::FullscreenQuad::Draw();
  EndShaderMode();
}

void VolumetricLightPass::Execute(graph::RenderContext &context) {
  const auto *qualityManager = context.qualityManager;
  if (qualityManager == nullptr || context.camera == nullptr) {
    return;
  }
  const auto &config = qualityManager->GetConfig();
  if (!config.volumetricLightEnabled || config.volumetricSampleCount <= 0) {
    return;
  }
  if (!context.hdrSceneBuffer.IsValid()) {
    return;
  }
  if (!m_initialized && !Initialize()) {
    return;
  }

  // The volumetric shader iterates the current cluster's light indices, so it
  // depends on the clustered-lighting path (same semantics as LightCullingPass:
  // fail closed instead of silently reading a raw light list).
  if (!config.v3Enabled || !config.dynamicLightingEnabled ||
      !config.clusteredLightingEnabled) {
    LOG_LIMITED_WARN(1.0f,
                     "VolumetricLightPass: clustered lighting disabled "
                     "(v3Enabled={}, dynamicLightingEnabled={}, "
                     "clusteredLightingEnabled={}); fail-closed, skipping "
                     "volumetric lighting",
                     config.v3Enabled, config.dynamicLightingEnabled,
                     config.clusteredLightingEnabled);
    return;
  }

  auto &clusterState = lighting::ClusteredLightingState::Get();
  const auto &grid = clusterState.GetGrid();
  if (grid.clusterCount == 0u || clusterState.GetClusterHeaderBufferId() == 0u ||
      clusterState.GetClusterLightIndexBufferId() == 0u) {
    LOG_LIMITED_WARN(1.0f,
                     "VolumetricLightPass: cluster buffers unavailable; "
                     "fail-closed, skipping volumetric lighting");
    return;
  }

  uint32_t headerBinding = 0u;
  uint32_t indexBinding = 0u;
  if (!core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_HEADER_OUT",
                                         headerBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_INDEX_OUT", indexBinding)) {
    LOG_LIMITED_WARN(1.0f,
                     "VolumetricLightPass: cluster binding resolution failed; "
                     "fail-closed, skipping volumetric lighting");
    return;
  }

  NoMoreDay::render::core::ScopedGLState scopedState;

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (width <= 0 || height <= 0) {
    return;
  }
  if (!m_outputBuffer.IsValid() || m_cachedWidth != width ||
      m_cachedHeight != height) {
    OnResize(width, height);
    if (!m_outputBuffer.IsValid()) {
      return;
    }
  }

  BindFramebufferAndViewport(m_outputBuffer);
  NoMoreDay::render::lighting::LightManager::Get().Bind();

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};

  // Cross-pass sync: the cluster SSBOs were written by LightCullingPass
  // (compute) in this frame. The graph emits the SSBO transition (0x2000) at
  // this pass's entry from the Setup Read declarations; no manual barrier here.
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      headerBinding, clusterState.GetClusterHeaderBufferId());
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      indexBinding, clusterState.GetClusterLightIndexBufferId());

  const int gridX = static_cast<int>(grid.tilesX);
  const int gridY = static_cast<int>(grid.tilesY);
  const int gridZ = static_cast<int>(grid.slicesZ);
  const float clusterTileSizeWorld =
      static_cast<float>(config.clusterTileSize) / zoom;
  const float layerBandWorldUnits =
      lighting::ClusteredLightingState::kDefaultLayerBandWorldUnits;
  if (m_clusterGridXLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_clusterGridXLoc, &gridX,
                   SHADER_UNIFORM_INT);
  }
  if (m_clusterGridYLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_clusterGridYLoc, &gridY,
                   SHADER_UNIFORM_INT);
  }
  if (m_clusterGridZLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_clusterGridZLoc, &gridZ,
                   SHADER_UNIFORM_INT);
  }
  if (m_clusterTileSizeWorldLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_clusterTileSizeWorldLoc,
                   &clusterTileSizeWorld, SHADER_UNIFORM_FLOAT);
  }
  if (m_layerBandWorldUnitsLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_layerBandWorldUnitsLoc,
                   &layerBandWorldUnits, SHADER_UNIFORM_FLOAT);
  }

  if (m_cameraOffsetLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_cameraOffsetLoc, cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_screenSizeLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_screenSizeLoc, screenSize,
                   SHADER_UNIFORM_VEC2);
  }

  const int sceneTexUnit = 0;
  if (m_sceneTexLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_sceneTexLoc, &sceneTexUnit,
                   SHADER_UNIFORM_INT);
  }

  const int sampleCount = std::clamp(config.volumetricSampleCount, 1, 96);
  if (m_sampleCountLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_sampleCountLoc, &sampleCount,
                   SHADER_UNIFORM_INT);
  }

  const float scattering = std::clamp(config.volumetricScattering, 0.0f, 1.0f);
  if (m_scatteringLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_scatteringLoc, &scattering,
                   SHADER_UNIFORM_FLOAT);
  }

  const float decay = std::clamp(config.volumetricDecay, 0.0f, 1.0f);
  if (m_decayLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_decayLoc, &decay, SHADER_UNIFORM_FLOAT);
  }

  const float exposure = 0.4f;
  if (m_exposureLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_exposureLoc, &exposure,
                   SHADER_UNIFORM_FLOAT);
  }

  if (m_cameraOffsetLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_cameraOffsetLoc, cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_screenSizeLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_screenSizeLoc, screenSize,
                   SHADER_UNIFORM_VEC2);
  }

  DrawFullscreen(m_volumetricShader, context.hdrSceneBuffer.colorTexture);

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLReadFramebuffer, m_outputBuffer.fbo);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLDrawFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  rlBlitFramebuffer(0, 0, m_outputBuffer.width, m_outputBuffer.height, 0, 0, width,
                    height, static_cast<int>(kGLColorBufferBit));

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, width, height);
}

} // namespace NoMoreDay::render::passes
