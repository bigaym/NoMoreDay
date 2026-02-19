#include "engine/render/passes/HeightShadowPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "game/components/Common.hpp"

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
constexpr const char *kHeightShadowFragmentShader =
    "assets/shaders/lighting/height_shadow_apply.frag";

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

} // namespace

HeightShadowPass::HeightShadowPass() = default;

HeightShadowPass::~HeightShadowPass() { Shutdown(); }

void HeightShadowPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::HeightShadow);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::HeightShadow);
}

bool HeightShadowPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_heightShadowShader =
      LoadShader(kFullscreenVertexShader, kHeightShadowFragmentShader);
  if (m_heightShadowShader.id == 0) {
    LOG_ERROR("HeightShadowPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_sceneTexLoc = GetShaderLocation(m_heightShadowShader, "uSceneTex");
  m_heightFieldTexLoc = GetShaderLocation(m_heightShadowShader, "uHeightFieldTex");
  m_stepsLoc = GetShaderLocation(m_heightShadowShader, "uHeightShadowSteps");
  m_selfShadowEnabledLoc =
      GetShaderLocation(m_heightShadowShader, "uSelfShadowEnabled");
  m_selfShadowStepsLoc =
      GetShaderLocation(m_heightShadowShader, "uSelfShadowSteps");
  m_pomEnabledLoc = GetShaderLocation(m_heightShadowShader, "uPomEnabled");
  m_pomLayersLoc = GetShaderLocation(m_heightShadowShader, "uPomLayers");
  m_cameraOffsetLoc = GetShaderLocation(m_heightShadowShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_heightShadowShader, "uScreenSize");
  m_heightWorldOriginLoc =
      GetShaderLocation(m_heightShadowShader, "uHeightWorldOrigin");
  m_heightWorldSizeLoc = GetShaderLocation(m_heightShadowShader, "uHeightWorldSize");

  m_initialized = true;
  return true;
}

bool HeightShadowPass::ReloadShaders() {
  Shader reloaded = LoadShader(kFullscreenVertexShader, kHeightShadowFragmentShader);
  if (reloaded.id == 0) {
    LOG_WARN("HeightShadowPass: shader reload failed, keeping previous program");
    return false;
  }

  if (m_heightShadowShader.id != 0) {
    UnloadShader(m_heightShadowShader);
  }
  m_heightShadowShader = reloaded;

  m_sceneTexLoc = GetShaderLocation(m_heightShadowShader, "uSceneTex");
  m_heightFieldTexLoc = GetShaderLocation(m_heightShadowShader, "uHeightFieldTex");
  m_stepsLoc = GetShaderLocation(m_heightShadowShader, "uHeightShadowSteps");
  m_selfShadowEnabledLoc =
      GetShaderLocation(m_heightShadowShader, "uSelfShadowEnabled");
  m_selfShadowStepsLoc =
      GetShaderLocation(m_heightShadowShader, "uSelfShadowSteps");
  m_pomEnabledLoc = GetShaderLocation(m_heightShadowShader, "uPomEnabled");
  m_pomLayersLoc = GetShaderLocation(m_heightShadowShader, "uPomLayers");
  m_cameraOffsetLoc = GetShaderLocation(m_heightShadowShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_heightShadowShader, "uScreenSize");
  m_heightWorldOriginLoc =
      GetShaderLocation(m_heightShadowShader, "uHeightWorldOrigin");
  m_heightWorldSizeLoc = GetShaderLocation(m_heightShadowShader, "uHeightWorldSize");
  LOG_INFO("HeightShadowPass: shader hot reloaded");
  return true;
}

void HeightShadowPass::Shutdown() {
  if (m_heightShadowShader.id != 0) {
    UnloadShader(m_heightShadowShader);
    m_heightShadowShader = {};
  }
  resources::FramebufferManager::Destroy(m_outputBuffer);
  m_heightField.Shutdown();
  m_heightFieldInitialized = false;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_initialized = false;
}

void HeightShadowPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
  if (!m_outputBuffer.IsValid()) {
    m_outputBuffer =
        resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
    return;
  }
  resources::FramebufferManager::Resize(m_outputBuffer, width, height);
}

void HeightShadowPass::DrawFullscreen(Shader shader, uint32_t sourceTexture,
                                      uint32_t heightFieldTexture) {
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, sourceTexture);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1u);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, heightFieldTexture);
  BeginShaderMode(shader);
  resources::FullscreenQuad::Draw();
  EndShaderMode();
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
}

void HeightShadowPass::Execute(graph::RenderContext &context) {
  const auto *qualityManager = context.qualityManager;
  if (qualityManager == nullptr || context.camera == nullptr) {
    return;
  }
  const auto &config = qualityManager->GetConfig();
  if (!config.heightShadowEnabled || config.heightShadowSteps == 0) {
    return;
  }
  if (!context.hdrSceneBuffer.IsValid()) {
    return;
  }
  if (context.registry == nullptr) {
    return;
  }
  if (!m_initialized && !Initialize()) {
    return;
  }
  if (!m_heightFieldInitialized) {
    lighting::GlobalHeightField::Config cfg = {};
    cfg.textureWidth = 1024;
    cfg.textureHeight = 1024;
    cfg.chunkSize = 64;
    cfg.worldOriginX = 0.0f;
    cfg.worldOriginY = 0.0f;
    cfg.worldWidth = static_cast<float>(Constants::World::WORLD_WIDTH);
    cfg.worldHeight = static_cast<float>(Constants::World::WORLD_HEIGHT);
    m_heightFieldInitialized = m_heightField.Initialize(cfg);
  }
  if (m_heightFieldInitialized) {
    m_heightField.Update(*context.registry);
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

  const int sceneTexUnit = 0;
  if (m_sceneTexLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_sceneTexLoc, &sceneTexUnit,
                   SHADER_UNIFORM_INT);
  }
  const int heightFieldTexUnit = 1;
  if (m_heightFieldTexLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_heightFieldTexLoc, &heightFieldTexUnit,
                   SHADER_UNIFORM_INT);
  }

  const int heightSteps = static_cast<int>(std::clamp<uint32_t>(
      config.heightShadowSteps, 0u, 128u));
  if (m_stepsLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_stepsLoc, &heightSteps,
                   SHADER_UNIFORM_INT);
  }

  const int selfShadowEnabled = config.selfShadowEnabled ? 1 : 0;
  if (m_selfShadowEnabledLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_selfShadowEnabledLoc,
                   &selfShadowEnabled, SHADER_UNIFORM_INT);
  }
  const int selfShadowSteps =
      static_cast<int>(std::clamp<uint32_t>(config.selfShadowSteps, 0u, 32u));
  if (m_selfShadowStepsLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_selfShadowStepsLoc, &selfShadowSteps,
                   SHADER_UNIFORM_INT);
  }

  const int pomEnabled = config.pomEnabled ? 1 : 0;
  if (m_pomEnabledLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_pomEnabledLoc, &pomEnabled,
                   SHADER_UNIFORM_INT);
  }
  const int pomLayers =
      static_cast<int>(std::clamp<uint32_t>(config.pomLayers, 0u, 32u));
  if (m_pomLayersLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_pomLayersLoc, &pomLayers,
                   SHADER_UNIFORM_INT);
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};
  if (m_cameraOffsetLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_cameraOffsetLoc, cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_screenSizeLoc >= 0) {
    SetShaderValue(m_heightShadowShader, m_screenSizeLoc, screenSize,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_heightFieldInitialized) {
    const auto &cfg = m_heightField.GetConfig();
    const float origin[2] = {cfg.worldOriginX, cfg.worldOriginY};
    const float size[2] = {cfg.worldWidth, cfg.worldHeight};
    if (m_heightWorldOriginLoc >= 0) {
      SetShaderValue(m_heightShadowShader, m_heightWorldOriginLoc, origin,
                     SHADER_UNIFORM_VEC2);
    }
    if (m_heightWorldSizeLoc >= 0) {
      SetShaderValue(m_heightShadowShader, m_heightWorldSizeLoc, size,
                     SHADER_UNIFORM_VEC2);
    }
  }

  DrawFullscreen(m_heightShadowShader, context.hdrSceneBuffer.colorTexture,
                 m_heightField.GetTextureId());

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
