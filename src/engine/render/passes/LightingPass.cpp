#include "engine/render/passes/LightingPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/LightManager.hpp"
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
  LOG_INFO("LightingPass: shader hot reloaded");
  return true;
}

void LightingPass::Shutdown() {
  if (m_lightAccumShader.id != 0) {
    UnloadShader(m_lightAccumShader);
    m_lightAccumShader = {};
  }
  resources::FramebufferManager::Destroy(m_litBuffer);
  m_cachedWidth = 0;
  m_cachedHeight = 0;
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
