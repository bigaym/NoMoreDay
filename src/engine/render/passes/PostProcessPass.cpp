#include "engine/render/passes/PostProcessPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLRgba8 = 0x8058;

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

} // namespace

PostProcessPass::PostProcessPass() = default;

PostProcessPass::~PostProcessPass() { Shutdown(); }

void PostProcessPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("SceneColor");
  builder.Write("PostProcessColor");
}

bool PostProcessPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_brightExtractShader =
      LoadShader("assets/shaders/postprocess/fullscreen.vert",
                 "assets/shaders/postprocess/bright_extract.frag");
  m_kawaseDownShader = LoadShader("assets/shaders/postprocess/fullscreen.vert",
                                  "assets/shaders/postprocess/kawase_down.frag");
  m_kawaseUpShader = LoadShader("assets/shaders/postprocess/fullscreen.vert",
                                "assets/shaders/postprocess/kawase_up.frag");
  m_tonemapShader = LoadShader("assets/shaders/postprocess/fullscreen.vert",
                               "assets/shaders/postprocess/tonemap.frag");
  m_fxaaShader = LoadShader("assets/shaders/postprocess/fullscreen.vert",
                            "assets/shaders/postprocess/fxaa.frag");
  m_vignetteShader = LoadShader("assets/shaders/postprocess/fullscreen.vert",
                                "assets/shaders/postprocess/vignette.frag");

  if (m_brightExtractShader.id == 0 || m_kawaseDownShader.id == 0 ||
      m_kawaseUpShader.id == 0 || m_tonemapShader.id == 0 || m_fxaaShader.id == 0 ||
      m_vignetteShader.id == 0) {
    LOG_ERROR("PostProcessPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_bloomThresholdLoc = GetShaderLocation(m_brightExtractShader, "uThreshold");
  m_bloomKneeLoc = GetShaderLocation(m_brightExtractShader, "uKnee");
  m_bloomIntensityLoc = GetShaderLocation(m_tonemapShader, "uBloomIntensity");
  m_tonemapExposureLoc = GetShaderLocation(m_tonemapShader, "uExposure");
  m_fxaaTexelSizeLoc = GetShaderLocation(m_fxaaShader, "uTexelSize");
  m_vignetteIntensityLoc = GetShaderLocation(m_vignetteShader, "uIntensity");
  m_vignetteRadiusLoc = GetShaderLocation(m_vignetteShader, "uRadius");

  m_initialized = true;
  return true;
}

void PostProcessPass::Shutdown() {
  if (m_brightExtractShader.id != 0) {
    UnloadShader(m_brightExtractShader);
    m_brightExtractShader = {};
  }
  if (m_kawaseDownShader.id != 0) {
    UnloadShader(m_kawaseDownShader);
    m_kawaseDownShader = {};
  }
  if (m_kawaseUpShader.id != 0) {
    UnloadShader(m_kawaseUpShader);
    m_kawaseUpShader = {};
  }
  if (m_tonemapShader.id != 0) {
    UnloadShader(m_tonemapShader);
    m_tonemapShader = {};
  }
  if (m_fxaaShader.id != 0) {
    UnloadShader(m_fxaaShader);
    m_fxaaShader = {};
  }
  if (m_vignetteShader.id != 0) {
    UnloadShader(m_vignetteShader);
    m_vignetteShader = {};
  }

  DestroyBloomMips();
  resources::FramebufferManager::Destroy(m_ldrBuffer);
  resources::FramebufferManager::Destroy(m_pingPongBuffer);
  m_finalOutputBuffer = {};
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_cachedBloomMips = -1;
  m_initialized = false;
}

void PostProcessPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  resources::FramebufferManager::Resize(m_ldrBuffer, width, height);
  resources::FramebufferManager::Resize(m_pingPongBuffer, width, height);
}

void PostProcessPass::EnsureWorkingBuffers(int width, int height) {
  if (!m_ldrBuffer.IsValid()) {
    m_ldrBuffer = resources::FramebufferManager::Create(width, height, kGLRgba8, false);
  } else if (m_ldrBuffer.width != width || m_ldrBuffer.height != height) {
    resources::FramebufferManager::Resize(m_ldrBuffer, width, height);
  }

  if (!m_pingPongBuffer.IsValid()) {
    m_pingPongBuffer =
        resources::FramebufferManager::Create(width, height, kGLRgba8, false);
  } else if (m_pingPongBuffer.width != width || m_pingPongBuffer.height != height) {
    resources::FramebufferManager::Resize(m_pingPongBuffer, width, height);
  }
}

void PostProcessPass::DestroyBloomMips() {
  for (auto &mip : m_bloomMips) {
    resources::FramebufferManager::Destroy(mip.fbo);
  }
  m_bloomMips.clear();
}

void PostProcessPass::RebuildBloomMips(int baseWidth, int baseHeight, int mipLevels) {
  DestroyBloomMips();
  if (mipLevels <= 0 || baseWidth <= 0 || baseHeight <= 0) {
    return;
  }

  m_bloomMips.reserve(static_cast<size_t>(mipLevels));
  int width = baseWidth;
  int height = baseHeight;
  for (int level = 0; level < mipLevels; ++level) {
    width = std::max(width / 2, 1);
    height = std::max(height / 2, 1);
    BloomMip mip = {};
    mip.width = width;
    mip.height = height;
    mip.fbo = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
    if (!mip.fbo.IsValid()) {
      LOG_ERROR("PostProcessPass bloom mip {} creation failed", level);
      DestroyBloomMips();
      return;
    }
    m_bloomMips.push_back(mip);
  }
}

void PostProcessPass::DrawFullscreen(Shader shader, uint32_t sourceTexture) {
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, sourceTexture);
  BeginShaderMode(shader);
  resources::FullscreenQuad::Draw();
  EndShaderMode();
}

void PostProcessPass::ExecuteBloom(const graph::RenderContext &context) {
  const auto &config = context.qualityManager->GetConfig();
  if (m_bloomMips.empty()) {
    return;
  }

  BindFramebufferAndViewport(m_bloomMips[0].fbo);
  if (m_bloomThresholdLoc >= 0) {
    SetShaderValue(m_brightExtractShader, m_bloomThresholdLoc, &config.bloomThreshold,
                   SHADER_UNIFORM_FLOAT);
  }
  if (m_bloomKneeLoc >= 0) {
    SetShaderValue(m_brightExtractShader, m_bloomKneeLoc, &config.bloomKnee,
                   SHADER_UNIFORM_FLOAT);
  }
  DrawFullscreen(m_brightExtractShader, context.hdrSceneBuffer.colorTexture);

  for (size_t i = 1; i < m_bloomMips.size(); ++i) {
    BindFramebufferAndViewport(m_bloomMips[i].fbo);
    DrawFullscreen(m_kawaseDownShader, m_bloomMips[i - 1].fbo.colorTexture);
  }

  for (size_t i = m_bloomMips.size(); i > 1; --i) {
    BindFramebufferAndViewport(m_bloomMips[i - 2].fbo);
    DrawFullscreen(m_kawaseUpShader, m_bloomMips[i - 1].fbo.colorTexture);
  }
}

void PostProcessPass::ExecuteTonemap(const graph::RenderContext &context) {
  const auto &config = context.qualityManager->GetConfig();
  BindFramebufferAndViewport(m_ldrBuffer);

  if (m_bloomIntensityLoc >= 0) {
    SetShaderValue(m_tonemapShader, m_bloomIntensityLoc, &config.bloomIntensity,
                   SHADER_UNIFORM_FLOAT);
  }
  const float exposure = 1.0f;
  if (m_tonemapExposureLoc >= 0) {
    SetShaderValue(m_tonemapShader, m_tonemapExposureLoc, &exposure,
                   SHADER_UNIFORM_FLOAT);
  }

  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D,
                                          context.hdrSceneBuffer.colorTexture);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1);
  NoMoreDay::utils::GPUUtils::BindTexture(
      kGLTexture2D,
      m_bloomMips.empty() ? context.hdrSceneBuffer.colorTexture
                          : m_bloomMips.front().fbo.colorTexture);

  BeginShaderMode(m_tonemapShader);
  const int hdrUnit = 0;
  const int bloomUnit = 1;
  SetShaderValue(m_tonemapShader, GetShaderLocation(m_tonemapShader, "uHDRScene"),
                 &hdrUnit, SHADER_UNIFORM_INT);
  SetShaderValue(m_tonemapShader,
                 GetShaderLocation(m_tonemapShader, "uBloomTexture"), &bloomUnit,
                 SHADER_UNIFORM_INT);
  resources::FullscreenQuad::Draw();
  EndShaderMode();
}

void PostProcessPass::ExecuteFXAA(const graph::RenderContext &context) {
  (void)context;
  BindFramebufferAndViewport(m_pingPongBuffer);

  const float texelSize[2] = {1.0f / static_cast<float>(m_ldrBuffer.width),
                              1.0f / static_cast<float>(m_ldrBuffer.height)};
  if (m_fxaaTexelSizeLoc >= 0) {
    SetShaderValue(m_fxaaShader, m_fxaaTexelSizeLoc, texelSize, SHADER_UNIFORM_VEC2);
  }
  DrawFullscreen(m_fxaaShader, m_ldrBuffer.colorTexture);

  std::swap(m_ldrBuffer, m_pingPongBuffer);
}

void PostProcessPass::ExecuteVignette(const graph::RenderContext &context) {
  const auto &config = context.qualityManager->GetConfig();
  BindFramebufferAndViewport(m_pingPongBuffer);

  if (m_vignetteIntensityLoc >= 0) {
    SetShaderValue(m_vignetteShader, m_vignetteIntensityLoc,
                   &config.vignetteIntensity, SHADER_UNIFORM_FLOAT);
  }
  if (m_vignetteRadiusLoc >= 0) {
    SetShaderValue(m_vignetteShader, m_vignetteRadiusLoc, &config.vignetteRadius,
                   SHADER_UNIFORM_FLOAT);
  }
  DrawFullscreen(m_vignetteShader, m_ldrBuffer.colorTexture);

  std::swap(m_ldrBuffer, m_pingPongBuffer);
}

void PostProcessPass::Execute(graph::RenderContext &context) {
  if (!m_initialized && !Initialize()) {
    return;
  }
  if (!context.hdrSceneBuffer.IsValid() || context.qualityManager == nullptr) {
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (width <= 0 || height <= 0) {
    return;
  }

  if (m_cachedWidth != width || m_cachedHeight != height) {
    m_cachedWidth = width;
    m_cachedHeight = height;
    OnResize(width, height);
  }
  EnsureWorkingBuffers(width, height);

  const int bloomMipLevels = std::max(0, config.bloomMipLevels);
  if (m_cachedBloomMips != bloomMipLevels) {
    m_cachedBloomMips = bloomMipLevels;
    RebuildBloomMips(width, height, bloomMipLevels);
  }

  if (config.bloomEnabled && !m_bloomMips.empty()) {
    ExecuteBloom(context);
  }
  ExecuteTonemap(context);

  if (config.fxaaEnabled) {
    ExecuteFXAA(context);
  }
  if (config.vignetteEnabled) {
    ExecuteVignette(context);
  }

  m_finalOutputBuffer = m_ldrBuffer;
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
}

} // namespace NoMoreDay::render::passes
