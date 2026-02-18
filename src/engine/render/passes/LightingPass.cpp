#include "engine/render/passes/LightingPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"

#include "GLFW/glfw3.h"
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
constexpr uint32_t kGLShaderStorageBarrierBit = 0x00002000;
constexpr double kClusterFallbackWarnWindowSeconds = 5.0;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kLightingFragmentShader =
    "assets/shaders/lighting/light_accumulation.frag";

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

} // namespace

LightingPass::LightingPass() = default;

LightingPass::~LightingPass() { Shutdown(); }

void LightingPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::Lighting);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::Lighting);
}

bool LightingPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_lightAccumShader = LoadShader(kFullscreenVertexShader, kLightingFragmentShader);
  if (m_lightAccumShader.id == 0) {
    LOG_ERROR("LightingPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_sceneTexLoc = GetShaderLocation(m_lightAccumShader, "uSceneTex");
  m_ambientColorLoc = GetShaderLocation(m_lightAccumShader, "uAmbientColor");
  m_ambientIntensityLoc =
      GetShaderLocation(m_lightAccumShader, "uAmbientIntensity");
  m_lightCountLoc = GetShaderLocation(m_lightAccumShader, "uLightCount");
  m_cameraOffsetLoc = GetShaderLocation(m_lightAccumShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_lightAccumShader, "uScreenSize");
  m_shadowMaskTexLoc = GetShaderLocation(m_lightAccumShader, "uShadowMaskTex");
  m_shadowEnabledLoc = GetShaderLocation(m_lightAccumShader, "uShadowEnabled");
  m_clusteredLightingEnabledLoc =
      GetShaderLocation(m_lightAccumShader, "uClusteredLightingEnabled");
  m_clusterGridXLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridX");
  m_clusterGridYLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridY");
  m_clusterGridZLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridZ");
  m_clusterTileSizeWorldLoc =
      GetShaderLocation(m_lightAccumShader, "uClusterTileSizeWorld");
  m_layerBandWorldUnitsLoc =
      GetShaderLocation(m_lightAccumShader, "uLayerBandWorldUnits");

  m_initialized = true;
  return true;
}

bool LightingPass::ReloadShaders() {
  Shader reloaded = LoadShader(kFullscreenVertexShader, kLightingFragmentShader);
  if (reloaded.id == 0) {
    LOG_WARN("LightingPass: shader reload failed, keeping previous program");
    return false;
  }

  if (m_lightAccumShader.id != 0) {
    UnloadShader(m_lightAccumShader);
  }
  m_lightAccumShader = reloaded;

  m_sceneTexLoc = GetShaderLocation(m_lightAccumShader, "uSceneTex");
  m_ambientColorLoc = GetShaderLocation(m_lightAccumShader, "uAmbientColor");
  m_ambientIntensityLoc = GetShaderLocation(m_lightAccumShader, "uAmbientIntensity");
  m_lightCountLoc = GetShaderLocation(m_lightAccumShader, "uLightCount");
  m_cameraOffsetLoc = GetShaderLocation(m_lightAccumShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_lightAccumShader, "uScreenSize");
  m_shadowMaskTexLoc = GetShaderLocation(m_lightAccumShader, "uShadowMaskTex");
  m_shadowEnabledLoc = GetShaderLocation(m_lightAccumShader, "uShadowEnabled");
  m_clusteredLightingEnabledLoc =
      GetShaderLocation(m_lightAccumShader, "uClusteredLightingEnabled");
  m_clusterGridXLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridX");
  m_clusterGridYLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridY");
  m_clusterGridZLoc = GetShaderLocation(m_lightAccumShader, "uClusterGridZ");
  m_clusterTileSizeWorldLoc =
      GetShaderLocation(m_lightAccumShader, "uClusterTileSizeWorld");
  m_layerBandWorldUnitsLoc =
      GetShaderLocation(m_lightAccumShader, "uLayerBandWorldUnits");
  LOG_INFO("LightingPass: shader hot reloaded");
  return true;
}

void LightingPass::Shutdown() {
  if (m_lightAccumShader.id != 0) {
    UnloadShader(m_lightAccumShader);
    m_lightAccumShader = {};
  }
  resources::FramebufferManager::Destroy(m_litBuffer);
  m_shadowMaskTexLoc = -1;
  m_shadowEnabledLoc = -1;
  m_clusteredLightingEnabledLoc = -1;
  m_clusterGridXLoc = -1;
  m_clusterGridYLoc = -1;
  m_clusterGridZLoc = -1;
  m_clusterTileSizeWorldLoc = -1;
  m_layerBandWorldUnitsLoc = -1;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_frameIndex = 0;
  m_lastShadowApplied = false;
  m_lastUsedV2Fallback = false;
  m_lastShadowFallbackReason.clear();
  m_lastClusteredApplied = false;
  m_lastClusteredFallback = false;
  m_lastClusteredFallbackReason.clear();
  m_lastClusteredWarnTime = -1000.0;
  m_initialized = false;
}

void LightingPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
  const double approxMb =
      (static_cast<double>(width) * static_cast<double>(height) * 8.0) /
      (1024.0 * 1024.0); // RGBA16F ~= 8 bytes/pixel
  if (!m_litBuffer.IsValid()) {
    m_litBuffer =
        resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
    if (m_litBuffer.IsValid()) {
      LOG_INFO("LightingPass: created lit buffer {}x{} (~{:.2f} MB)", width,
               height, approxMb);
    }
    return;
  }
  resources::FramebufferManager::Resize(m_litBuffer, width, height);
  LOG_INFO("LightingPass: resized lit buffer {}x{} (~{:.2f} MB)", width, height,
           approxMb);
}

void LightingPass::DrawFullscreen(Shader shader, uint32_t sourceTexture) {
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, sourceTexture);
  BeginShaderMode(shader);
  resources::FullscreenQuad::Draw();
  EndShaderMode();
}

void LightingPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_lastShadowApplied = false;
  m_lastUsedV2Fallback = false;
  m_lastShadowFallbackReason.clear();
  m_lastClusteredApplied = false;
  m_lastClusteredFallback = false;
  m_lastClusteredFallbackReason.clear();

  const auto *qualityManager = context.qualityManager;
  if (qualityManager == nullptr || context.camera == nullptr) {
    return;
  }
  const auto &config = qualityManager->GetConfig();
  if (!config.dynamicLightingEnabled) {
    return;
  }
  if (!context.hdrSceneBuffer.IsValid()) {
    return;
  }
  if (!m_initialized && !Initialize()) {
    return;
  }

  NoMoreDay::render::core::ScopedGLState scopedState;

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (width <= 0 || height <= 0) {
    return;
  }
  if (!m_litBuffer.IsValid() || m_cachedWidth != width || m_cachedHeight != height) {
    OnResize(width, height);
    if (!m_litBuffer.IsValid()) {
      return;
    }
  }

  BindFramebufferAndViewport(m_litBuffer);
  lighting::LightManager::Get().Bind();

  const int sceneTexUnit = 0;
  if (m_sceneTexLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_sceneTexLoc, &sceneTexUnit,
                   SHADER_UNIFORM_INT);
  }

  const float ambientColor[3] = {config.ambientColorR, config.ambientColorG,
                                 config.ambientColorB};
  if (m_ambientColorLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_ambientColorLoc, ambientColor,
                   SHADER_UNIFORM_VEC3);
  }
  if (m_ambientIntensityLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_ambientIntensityLoc,
                   &config.ambientIntensity, SHADER_UNIFORM_FLOAT);
  }

  const int lightCount = lighting::LightManager::Get().GetActiveLightCount();
  if (m_lightCountLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_lightCountLoc, &lightCount,
                   SHADER_UNIFORM_INT);
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};

  if (m_cameraOffsetLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_cameraOffsetLoc, cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_screenSizeLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_screenSizeLoc, screenSize,
                   SHADER_UNIFORM_VEC2);
  }

  const bool wantsShadow = config.shadowEnabled &&
                           (config.shadowMode != core::ShadowMode::Off);
  int shadowEnabled = 0;
  if (wantsShadow && m_shadowResolvePass == nullptr) {
    m_lastUsedV2Fallback = true;
    m_lastShadowFallbackReason = "shadow resolve pass not bound";
  } else if (wantsShadow && m_shadowResolvePass->HadFailureThisFrame()) {
    m_lastUsedV2Fallback = true;
    m_lastShadowFallbackReason = m_shadowResolvePass->GetLastFailureReason();
  } else if (wantsShadow &&
             (!m_shadowResolvePass->IsShadowReadyForCurrentFrame() ||
              !m_shadowResolvePass->HasShadowMask())) {
    m_lastUsedV2Fallback = true;
    m_lastShadowFallbackReason = "shadow mask unavailable for current frame";
  } else if (wantsShadow) {
    constexpr uint32_t kGLTexture1 = 0x84C1;
    constexpr uint32_t kGLTexture2D = 0x0DE1;
    constexpr int kShadowMaskTexUnit = 1;
    shadowEnabled = 1;
    m_lastShadowApplied = true;
    core::ApplyComputeToFragmentBarrierTemplate();
    NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture1);
    NoMoreDay::utils::GPUUtils::BindTexture(
        kGLTexture2D, m_shadowResolvePass->GetShadowMaskTexture());
    if (m_shadowMaskTexLoc >= 0) {
      SetShaderValue(m_lightAccumShader, m_shadowMaskTexLoc, &kShadowMaskTexUnit,
                     SHADER_UNIFORM_INT);
    }
  }
  if (m_lastUsedV2Fallback && !m_lastShadowFallbackReason.empty()) {
    LOG_WARN("ShadowFallback: frame={} reason={} fallback=V2Lighting", m_frameIndex,
             m_lastShadowFallbackReason);
  }
  if (m_shadowEnabledLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_shadowEnabledLoc, &shadowEnabled,
                   SHADER_UNIFORM_INT);
  }

  int clusteredLightingEnabled = 0;
  if (config.v3Enabled && config.clusteredLightingEnabled) {
    if (m_lightCullingPass == nullptr) {
      m_lastClusteredFallback = true;
      m_lastClusteredFallbackReason = "light culling pass not bound";
    } else if (m_lightCullingPass->HadFailureThisFrame()) {
      m_lastClusteredFallback = true;
      m_lastClusteredFallbackReason = m_lightCullingPass->GetLastFailureReason();
    } else if (!m_lightCullingPass->IsClusterDataReadyForCurrentFrame()) {
      m_lastClusteredFallback = true;
      m_lastClusteredFallbackReason = "cluster data unavailable for current frame";
    } else {
      auto &clusterState = lighting::ClusteredLightingState::Get();
      const auto &grid = clusterState.GetGrid();
      if (grid.clusterCount == 0u || clusterState.GetClusterHeaderBufferId() == 0u ||
          clusterState.GetClusterLightIndexBufferId() == 0u) {
        m_lastClusteredFallback = true;
        m_lastClusteredFallbackReason = "cluster buffers unavailable";
      } else {
        uint32_t headerBinding = 0u;
        uint32_t indexBinding = 0u;
        if (!core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                               "CLUSTER_HEADER_OUT",
                                               headerBinding) ||
            !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                               "CLUSTER_INDEX_OUT",
                                               indexBinding)) {
          m_lastClusteredFallback = true;
          m_lastClusteredFallbackReason = "cluster binding resolution failed";
        } else {
          // Explicit compute->fragment SSBO sync point (auditable contract).
          NoMoreDay::utils::GPUUtils::MemoryBarrier(kGLShaderStorageBarrierBit);
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
            SetShaderValue(m_lightAccumShader, m_clusterGridXLoc, &gridX,
                           SHADER_UNIFORM_INT);
          }
          if (m_clusterGridYLoc >= 0) {
            SetShaderValue(m_lightAccumShader, m_clusterGridYLoc, &gridY,
                           SHADER_UNIFORM_INT);
          }
          if (m_clusterGridZLoc >= 0) {
            SetShaderValue(m_lightAccumShader, m_clusterGridZLoc, &gridZ,
                           SHADER_UNIFORM_INT);
          }
          if (m_clusterTileSizeWorldLoc >= 0) {
            SetShaderValue(m_lightAccumShader, m_clusterTileSizeWorldLoc,
                           &clusterTileSizeWorld, SHADER_UNIFORM_FLOAT);
          }
          if (m_layerBandWorldUnitsLoc >= 0) {
            SetShaderValue(m_lightAccumShader, m_layerBandWorldUnitsLoc,
                           &layerBandWorldUnits, SHADER_UNIFORM_FLOAT);
          }

          clusteredLightingEnabled = 1;
          m_lastClusteredApplied = true;
        }
      }
    }
  }
  if (m_clusteredLightingEnabledLoc >= 0) {
    SetShaderValue(m_lightAccumShader, m_clusteredLightingEnabledLoc,
                   &clusteredLightingEnabled, SHADER_UNIFORM_INT);
  }
  if (m_lastClusteredFallback && !m_lastClusteredFallbackReason.empty()) {
    const double now = GetTime();
    if ((now - m_lastClusteredWarnTime) >= kClusterFallbackWarnWindowSeconds) {
      LOG_WARN("ClusteredLightingFallback: frame={} reason={} fallback=V2Lighting",
               m_frameIndex, m_lastClusteredFallbackReason);
      m_lastClusteredWarnTime = now;
    }
  }

  DrawFullscreen(m_lightAccumShader, context.hdrSceneBuffer.colorTexture);

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLReadFramebuffer, m_litBuffer.fbo);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLDrawFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  rlBlitFramebuffer(0, 0, m_litBuffer.width, m_litBuffer.height, 0, 0, width,
                    height, static_cast<int>(kGLColorBufferBit));

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, width, height);
}

} // namespace NoMoreDay::render::passes
