#include "engine/render/passes/ShadowBuildPass.hpp"

#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/ShadowCasterComponent.hpp"

#include "rlgl.h"

#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLRg16f = 0x822F;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kShadowGroupSize = 8u;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kShadowAtlasTileFragmentShader =
    "assets/shaders/lighting/shadow_atlas_tile.frag";

} // namespace

ShadowBuildPass::ShadowBuildPass() = default;

ShadowBuildPass::~ShadowBuildPass() { Shutdown(); }

void ShadowBuildPass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

bool ShadowBuildPass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_sdfComputeShader = resources.loadComputeShader(
      "shadow_sdf_compute"_hash, "assets/shaders/lighting/shadow_sdf.comp");
  if (m_sdfComputeShader.id == 0) {
    LOG_ERROR("ShadowBuildPass: failed to load SDF compute shader");
    Shutdown();
    return false;
  }

  m_resolutionLoc = rlGetLocationUniform(m_sdfComputeShader.id, "uResolution");
  m_occluderCountLoc =
      rlGetLocationUniform(m_sdfComputeShader.id, "uOccluderCount");
  m_cameraOffsetLoc = rlGetLocationUniform(m_sdfComputeShader.id, "uCameraOffset");
  m_screenSizeLoc = rlGetLocationUniform(m_sdfComputeShader.id, "uScreenSize");

  if (!InitializeAtlasPath(resources)) {
    LOG_WARN("ShadowBuildPass: atlas tile shader unavailable, atlas path disabled");
  }

  m_initialized = true;
  return true;
}

void ShadowBuildPass::ReportFailure(const char *reason) {
  if (reason == nullptr) {
    reason = "unknown";
  }
  m_lastExecuteFailure = true;
  m_lastExecuteSuccess = false;
  m_lastFailureReason = reason;
  LOG_LIMITED_WARN(
      1.0f, "ShadowFallback: frame={} reason={} fallback=V2Lighting",
      m_frameIndex, m_lastFailureReason);
}

void ShadowBuildPass::MarkSuccess() {
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = true;
  m_lastFailureReason.clear();
}

bool ShadowBuildPass::InitializeAtlasPath(ResourceManager &resources) {
  m_atlasTileShader = resources.loadShader("shadow_atlas_tile"_hash,
                                           kFullscreenVertexShader,
                                           kShadowAtlasTileFragmentShader);
  if (m_atlasTileShader.id == 0) {
    return false;
  }

  m_tileOriginLoc = GetShaderLocation(m_atlasTileShader, "uTileOriginPx");
  m_tileSizeLoc = GetShaderLocation(m_atlasTileShader, "uTileSize");
  m_lightPosLoc = GetShaderLocation(m_atlasTileShader, "uLightPos");
  m_lightRadiusLoc = GetShaderLocation(m_atlasTileShader, "uLightRadius");
  m_atlasCameraOffsetLoc = GetShaderLocation(m_atlasTileShader, "uCameraOffset");
  m_atlasScreenSizeLoc = GetShaderLocation(m_atlasTileShader, "uScreenSize");
  return true;
}

void ShadowBuildPass::Shutdown() {
  if (m_sdfComputeShader.id != 0) {
    UnloadShader(m_sdfComputeShader);
    m_sdfComputeShader = {};
  }
  if (m_atlasTileShader.id != 0) {
    UnloadShader(m_atlasTileShader);
    m_atlasTileShader = {};
  }
  m_occluderBuffer.Release();
  resources::FramebufferManager::Destroy(m_sdfField);
  resources::FramebufferManager::Destroy(m_shadowAtlas);
  m_occluderStaging.clear();
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_shadowAtlasSize = 0;
  m_frameIndex = 0;
  m_occluderCount = 0;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();
  m_resolutionLoc = -1;
  m_occluderCountLoc = -1;
  m_cameraOffsetLoc = -1;
  m_screenSizeLoc = -1;
  m_tileOriginLoc = -1;
  m_tileSizeLoc = -1;
  m_lightPosLoc = -1;
  m_lightRadiusLoc = -1;
  m_atlasCameraOffsetLoc = -1;
  m_atlasScreenSizeLoc = -1;
  m_initialized = false;
}

void ShadowBuildPass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  if (!m_sdfField.IsValid()) {
    m_sdfField = resources::FramebufferManager::Create(width, height, kGLRg16f, false);
    return;
  }
  resources::FramebufferManager::Resize(m_sdfField, width, height);
}

void ShadowBuildPass::EnsureAtlasSize(const int atlasSize) {
  const int clampedAtlasSize = std::max(1, atlasSize);
  if (!m_shadowAtlas.IsValid()) {
    m_shadowAtlas = resources::FramebufferManager::Create(clampedAtlasSize,
                                                          clampedAtlasSize,
                                                          kGLRgba16f, false);
    m_shadowAtlasSize = clampedAtlasSize;
    return;
  }
  if (m_shadowAtlas.width != clampedAtlasSize ||
      m_shadowAtlas.height != clampedAtlasSize) {
    resources::FramebufferManager::Resize(m_shadowAtlas, clampedAtlasSize,
                                          clampedAtlasSize);
    m_shadowAtlasSize = clampedAtlasSize;
  }
}

bool ShadowBuildPass::UploadOccluders(entt::registry &registry,
                                      const uint32_t maxShadowCasters) {
  const uint32_t maxCount = std::max(
      1u, std::min(maxShadowCasters, RenderConstants::Shadow::kMaxShadowCasters));
  m_occluderStaging.clear();
  m_occluderStaging.reserve(maxCount);

  auto view = registry.view<Position, NoMoreDay::ShadowCasterComponent>();
  for (const entt::entity entity : view) {
    if (m_occluderStaging.size() >= maxCount) {
      break;
    }

    const auto &[position, shadowCaster] =
        view.get<Position, NoMoreDay::ShadowCasterComponent>(entity);

    float radius = 24.0f;
    if (const auto *vision = registry.try_get<VisionComponent>(entity);
        vision != nullptr && vision->radius > 0.0f) {
      radius = vision->radius;
    }

    m_occluderStaging.push_back({
        .posX = position.x,
        .posY = position.y,
        .radius = radius,
        .occluderHeight = shadowCaster.occluderHeight,
        .shapeIndex = static_cast<uint32_t>(shadowCaster.shape),
        .dynamicFlag = shadowCaster.dynamicFlag,
        .reserved0 = 0u,
        .reserved1 = 0u,
    });
  }

  const size_t requiredBytes =
      static_cast<size_t>(maxCount) * sizeof(NoMoreDay::components::GPUShadowCaster);
  if (m_occluderBuffer.GetId() == 0 || m_occluderBuffer.GetSize() < requiredBytes) {
    m_occluderBuffer.Create(requiredBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_occluderBuffer.GetId() == 0) {
    LOG_ERROR("ShadowBuildPass: failed to allocate occluder buffer");
    return false;
  }

  if (!m_occluderStaging.empty()) {
    m_occluderBuffer.Update(
        m_occluderStaging.data(),
        m_occluderStaging.size() * sizeof(NoMoreDay::components::GPUShadowCaster), 0);
  }
  m_occluderCount = static_cast<uint32_t>(m_occluderStaging.size());
  return true;
}

void ShadowBuildPass::RenderAtlasTiles(const graph::RenderContext &context) {
  if (m_preparePass == nullptr || m_atlasTileShader.id == 0 || !m_shadowAtlas.IsValid() ||
      context.camera == nullptr) {
    return;
  }

  const auto &prepared = m_preparePass->GetPreparedLights();
  if (prepared.empty()) {
    return;
  }

  const int atlasSize = m_shadowAtlas.width;
  const uint32_t tileSize = m_preparePass->GetAtlasTileSize();
  const uint32_t tilesPerRow = std::max(1u, m_preparePass->GetAtlasTilesPerRow());

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, m_shadowAtlas.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, atlasSize, atlasSize);
  ClearBackground(BLACK);

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(m_cachedWidth) / zoom,
                               static_cast<float>(m_cachedHeight) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};

  BeginShaderMode(m_atlasTileShader);
  if (m_atlasCameraOffsetLoc >= 0) {
    SetShaderValue(m_atlasTileShader, m_atlasCameraOffsetLoc, cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_atlasScreenSizeLoc >= 0) {
    SetShaderValue(m_atlasTileShader, m_atlasScreenSizeLoc, screenSize,
                   SHADER_UNIFORM_VEC2);
  }

  for (const ShadowPreparedLight &light : prepared) {
    if (!light.usesAtlas) {
      continue;
    }
    const uint32_t tileIndex = light.atlasTileIndex;
    const uint32_t tileX = tileIndex % tilesPerRow;
    const uint32_t tileY = tileIndex / tilesPerRow;
    const int originPx[2] = {static_cast<int>(tileX * tileSize),
                             static_cast<int>(tileY * tileSize)};
    NoMoreDay::utils::GPUUtils::Viewport(originPx[0], originPx[1],
                                         static_cast<int>(tileSize),
                                         static_cast<int>(tileSize));

    const float originPxF[2] = {static_cast<float>(originPx[0]),
                                static_cast<float>(originPx[1])};
    const float tileSizeF = static_cast<float>(tileSize);
    const float lightPos[2] = {light.gpuLight.posX, light.gpuLight.posY};
    const float lightRadius = std::max(light.gpuLight.radius, 0.001f);

    if (m_tileOriginLoc >= 0) {
      SetShaderValue(m_atlasTileShader, m_tileOriginLoc, originPxF,
                     SHADER_UNIFORM_VEC2);
    }
    if (m_tileSizeLoc >= 0) {
      SetShaderValue(m_atlasTileShader, m_tileSizeLoc, &tileSizeF,
                     SHADER_UNIFORM_FLOAT);
    }
    if (m_lightPosLoc >= 0) {
      SetShaderValue(m_atlasTileShader, m_lightPosLoc, lightPos, SHADER_UNIFORM_VEC2);
    }
    if (m_lightRadiusLoc >= 0) {
      SetShaderValue(m_atlasTileShader, m_lightRadiusLoc, &lightRadius,
                     SHADER_UNIFORM_FLOAT);
    }

    resources::FullscreenQuad::Draw();
  }

  EndShaderMode();

  if (context.hdrSceneBuffer.IsValid()) {
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                                context.hdrSceneBuffer.fbo);
    NoMoreDay::utils::GPUUtils::Viewport(0, 0, context.hdrSceneBuffer.width,
                                         context.hdrSceneBuffer.height);
  }
}

void ShadowBuildPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();

  if (context.registry == nullptr || context.qualityManager == nullptr ||
      context.camera == nullptr || context.shared == nullptr ||
      context.shared->resources == nullptr) {
    ReportFailure("missing render context prerequisites");
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.v3Enabled || !config.shadowEnabled ||
      config.shadowMode == core::ShadowMode::Off) {
    m_occluderCount = 0;
    MarkSuccess();
    return;
  }

  if (!context.hdrSceneBuffer.IsValid()) {
    m_occluderCount = 0;
    ReportFailure("hdr scene buffer unavailable");
    return;
  }

  if (!m_initialized && !Initialize(*context.shared->resources)) {
    m_occluderCount = 0;
    ReportFailure("failed to initialize shadow build shaders");
    return;
  }

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (width <= 0 || height <= 0) {
    m_occluderCount = 0;
    ReportFailure("invalid shadow build resolution");
    return;
  }
  if (!m_sdfField.IsValid() || m_cachedWidth != width || m_cachedHeight != height) {
    OnResize(width, height);
  }
  if (!m_sdfField.IsValid()) {
    m_occluderCount = 0;
    ReportFailure("failed to allocate shadow sdf framebuffer");
    return;
  }

  if (!UploadOccluders(*context.registry, RenderConstants::Shadow::kMaxShadowCasters)) {
    m_occluderCount = 0;
    ReportFailure("failed to upload shadow occluders");
    return;
  }

  rlEnableShader(m_sdfComputeShader.id);

  const int resolution[2] = {width, height};
  if (m_resolutionLoc >= 0) {
    rlSetUniform(m_resolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  const int occluderCount = static_cast<int>(m_occluderCount);
  if (m_occluderCountLoc >= 0) {
    rlSetUniform(m_occluderCountLoc, &occluderCount, RL_SHADER_UNIFORM_INT, 1);
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};

  if (m_cameraOffsetLoc >= 0) {
    rlSetUniform(m_cameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_screenSizeLoc >= 0) {
    rlSetUniform(m_screenSizeLoc, screenSize, RL_SHADER_UNIFORM_VEC2, 1);
  }

  NoMoreDay::utils::GPUUtils::BindBufferBase(RenderConstants::ShadowCS::kOccluderBinding,
                                             m_occluderBuffer.GetId());
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::ShadowCS::kSdfImageBinding, m_sdfField.colorTexture, 0, false, 0,
      kGLWriteOnly, kGLRg16f);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      (static_cast<uint32_t>(width) + (kShadowGroupSize - 1u)) / kShadowGroupSize,
      (static_cast<uint32_t>(height) + (kShadowGroupSize - 1u)) / kShadowGroupSize, 1);
  rlDisableShader();

  constexpr uint32_t kShadowBuildBarrier =
      static_cast<uint32_t>(RenderConstants::Barrier::Image) |
      static_cast<uint32_t>(RenderConstants::Barrier::Buffer);
  NoMoreDay::utils::GPUUtils::MemoryBarrier(kShadowBuildBarrier);

  if (config.shadowMode == core::ShadowMode::Hybrid) {
    EnsureAtlasSize(static_cast<int>(config.shadowAtlasSize));
    RenderAtlasTiles(context);
  }

  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
