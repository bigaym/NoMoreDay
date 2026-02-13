#include "engine/render/passes/VolumetricLightPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
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
  builder.Read("SceneColor");
  builder.Write("SceneColor");
}

bool VolumetricLightPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_volumetricShader = LoadShader(kFullscreenVertexShader, kVolumetricFragmentShader);
  if (m_volumetricShader.id == 0) {
    LOG_ERROR("VolumetricLightPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_sceneTexLoc = GetShaderLocation(m_volumetricShader, "uSceneTex");
  m_lightCountLoc = GetShaderLocation(m_volumetricShader, "uLightCount");
  m_sampleCountLoc = GetShaderLocation(m_volumetricShader, "uSampleCount");
  m_scatteringLoc = GetShaderLocation(m_volumetricShader, "uScattering");
  m_decayLoc = GetShaderLocation(m_volumetricShader, "uDecay");
  m_exposureLoc = GetShaderLocation(m_volumetricShader, "uExposure");
  m_cameraOffsetLoc = GetShaderLocation(m_volumetricShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_volumetricShader, "uScreenSize");

  m_initialized = true;
  return true;
}

bool VolumetricLightPass::ReloadShaders() {
  Shader reloaded = LoadShader(kFullscreenVertexShader, kVolumetricFragmentShader);
  if (reloaded.id == 0) {
    LOG_WARN("VolumetricLightPass: shader reload failed, keeping previous program");
    return false;
  }
  if (m_volumetricShader.id != 0) {
    UnloadShader(m_volumetricShader);
  }
  m_volumetricShader = reloaded;

  m_sceneTexLoc = GetShaderLocation(m_volumetricShader, "uSceneTex");
  m_lightCountLoc = GetShaderLocation(m_volumetricShader, "uLightCount");
  m_sampleCountLoc = GetShaderLocation(m_volumetricShader, "uSampleCount");
  m_scatteringLoc = GetShaderLocation(m_volumetricShader, "uScattering");
  m_decayLoc = GetShaderLocation(m_volumetricShader, "uDecay");
  m_exposureLoc = GetShaderLocation(m_volumetricShader, "uExposure");
  m_cameraOffsetLoc = GetShaderLocation(m_volumetricShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_volumetricShader, "uScreenSize");
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

  const int sceneTexUnit = 0;
  if (m_sceneTexLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_sceneTexLoc, &sceneTexUnit,
                   SHADER_UNIFORM_INT);
  }

  const int lightCount =
      NoMoreDay::render::lighting::LightManager::Get().GetActiveLightCount();
  if (m_lightCountLoc >= 0) {
    SetShaderValue(m_volumetricShader, m_lightCountLoc, &lightCount,
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

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(width) / zoom,
                               static_cast<float>(height) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom)};

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
