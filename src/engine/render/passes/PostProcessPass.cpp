#include "engine/render/passes/PostProcessPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/TransientResourcePool.hpp"
#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLRgba8 = 0x8058;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kBrightExtractFragmentShader =
    "assets/shaders/postprocess/bright_extract.frag";
constexpr const char *kKawaseDownFragmentShader =
    "assets/shaders/postprocess/kawase_down.frag";
constexpr const char *kKawaseUpFragmentShader =
    "assets/shaders/postprocess/kawase_up.frag";
constexpr const char *kTonemapFragmentShader = "assets/shaders/postprocess/tonemap.frag";
constexpr const char *kFxaaFragmentShader = "assets/shaders/postprocess/fxaa.frag";
constexpr const char *kVignetteFragmentShader =
    "assets/shaders/postprocess/vignette.frag";
constexpr const char *kColorGradingFragmentShader =
    "assets/shaders/postprocess/color_grading.frag";

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

void ReleaseBuffer(resources::FramebufferHandle &handle, bool pooled) {
  if (!pooled) {
    resources::FramebufferManager::Destroy(handle);
  } else {
    handle = {};
  }
}

} // namespace

PostProcessPass::PostProcessPass() = default;

PostProcessPass::~PostProcessPass() { Shutdown(); }

void PostProcessPass::Setup(graph::RenderGraphBuilder &builder) {
  graph::TypedResourceDescriptor ldrDesc{};
  ldrDesc.tag = graph::RenderResourceTag::PostProcessLdrColor;
  ldrDesc.name = "PostProcessLdrColor";
  ldrDesc.kind = graph::ResourceKind::Texture2D;
  ldrDesc.format = graph::ResourceFormat::R8;
  ldrDesc.extentPolicy = graph::ExtentPolicy{graph::ExtentMode::MatchScreen};
  ldrDesc.usageFlags = graph::ResourceUsage::ColorAttachment;
  ldrDesc.lifetime = graph::ResourceLifetime::Transient;
  builder.DeclareResource(ldrDesc);

  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::PostProcess,
               graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Write(graph::RenderResourceTag::PostProcessLdrColor,
                graph::RenderOwnerTag::PostProcess,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);
}

bool PostProcessPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_brightExtractShader =
      LoadShader(kFullscreenVertexShader, kBrightExtractFragmentShader);
  m_kawaseDownShader = LoadShader(kFullscreenVertexShader, kKawaseDownFragmentShader);
  m_kawaseUpShader = LoadShader(kFullscreenVertexShader, kKawaseUpFragmentShader);
  m_tonemapShader = LoadShader(kFullscreenVertexShader, kTonemapFragmentShader);
  m_fxaaShader = LoadShader(kFullscreenVertexShader, kFxaaFragmentShader);
  m_vignetteShader = LoadShader(kFullscreenVertexShader, kVignetteFragmentShader);
  m_colorGradingShader =
      LoadShader(kFullscreenVertexShader, kColorGradingFragmentShader);

  if (m_brightExtractShader.id == 0 || m_kawaseDownShader.id == 0 ||
      m_kawaseUpShader.id == 0 || m_tonemapShader.id == 0 || m_fxaaShader.id == 0 ||
      m_vignetteShader.id == 0 || m_colorGradingShader.id == 0) {
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
  m_colorGradingSceneLoc = GetShaderLocation(m_colorGradingShader, "uSceneTexture");
  m_colorGradingLutLoc = GetShaderLocation(m_colorGradingShader, "uLutTexture");
  m_colorGradingIntensityLoc =
      GetShaderLocation(m_colorGradingShader, "uIntensity");
  m_colorGradingLutSizeLoc = GetShaderLocation(m_colorGradingShader, "uLutSize");

  if (!LoadColorGradingLUT(16)) {
    LOG_WARN("PostProcessPass: failed to preload neutral LUT");
  }

  m_initialized = true;
  return true;
}

bool PostProcessPass::ReloadShaders() {
  Shader brightExtract = LoadShader(kFullscreenVertexShader, kBrightExtractFragmentShader);
  Shader kawaseDown = LoadShader(kFullscreenVertexShader, kKawaseDownFragmentShader);
  Shader kawaseUp = LoadShader(kFullscreenVertexShader, kKawaseUpFragmentShader);
  Shader tonemap = LoadShader(kFullscreenVertexShader, kTonemapFragmentShader);
  Shader fxaa = LoadShader(kFullscreenVertexShader, kFxaaFragmentShader);
  Shader vignette = LoadShader(kFullscreenVertexShader, kVignetteFragmentShader);
  Shader colorGrading = LoadShader(kFullscreenVertexShader, kColorGradingFragmentShader);

  if (brightExtract.id == 0 || kawaseDown.id == 0 || kawaseUp.id == 0 ||
      tonemap.id == 0 || fxaa.id == 0 || vignette.id == 0 ||
      colorGrading.id == 0) {
    if (brightExtract.id != 0) {
      UnloadShader(brightExtract);
    }
    if (kawaseDown.id != 0) {
      UnloadShader(kawaseDown);
    }
    if (kawaseUp.id != 0) {
      UnloadShader(kawaseUp);
    }
    if (tonemap.id != 0) {
      UnloadShader(tonemap);
    }
    if (fxaa.id != 0) {
      UnloadShader(fxaa);
    }
    if (vignette.id != 0) {
      UnloadShader(vignette);
    }
    if (colorGrading.id != 0) {
      UnloadShader(colorGrading);
    }
    LOG_WARN("PostProcessPass: shader reload failed, keeping previous program");
    return false;
  }

  if (m_brightExtractShader.id != 0) {
    UnloadShader(m_brightExtractShader);
  }
  if (m_kawaseDownShader.id != 0) {
    UnloadShader(m_kawaseDownShader);
  }
  if (m_kawaseUpShader.id != 0) {
    UnloadShader(m_kawaseUpShader);
  }
  if (m_tonemapShader.id != 0) {
    UnloadShader(m_tonemapShader);
  }
  if (m_fxaaShader.id != 0) {
    UnloadShader(m_fxaaShader);
  }
  if (m_vignetteShader.id != 0) {
    UnloadShader(m_vignetteShader);
  }
  if (m_colorGradingShader.id != 0) {
    UnloadShader(m_colorGradingShader);
  }

  m_brightExtractShader = brightExtract;
  m_kawaseDownShader = kawaseDown;
  m_kawaseUpShader = kawaseUp;
  m_tonemapShader = tonemap;
  m_fxaaShader = fxaa;
  m_vignetteShader = vignette;
  m_colorGradingShader = colorGrading;

  m_bloomThresholdLoc = GetShaderLocation(m_brightExtractShader, "uThreshold");
  m_bloomKneeLoc = GetShaderLocation(m_brightExtractShader, "uKnee");
  m_bloomIntensityLoc = GetShaderLocation(m_tonemapShader, "uBloomIntensity");
  m_tonemapExposureLoc = GetShaderLocation(m_tonemapShader, "uExposure");
  m_fxaaTexelSizeLoc = GetShaderLocation(m_fxaaShader, "uTexelSize");
  m_vignetteIntensityLoc = GetShaderLocation(m_vignetteShader, "uIntensity");
  m_vignetteRadiusLoc = GetShaderLocation(m_vignetteShader, "uRadius");
  m_colorGradingSceneLoc = GetShaderLocation(m_colorGradingShader, "uSceneTexture");
  m_colorGradingLutLoc = GetShaderLocation(m_colorGradingShader, "uLutTexture");
  m_colorGradingIntensityLoc =
      GetShaderLocation(m_colorGradingShader, "uIntensity");
  m_colorGradingLutSizeLoc = GetShaderLocation(m_colorGradingShader, "uLutSize");

  LOG_INFO("PostProcessPass: shader hot reloaded");
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
  if (m_colorGradingShader.id != 0) {
    UnloadShader(m_colorGradingShader);
    m_colorGradingShader = {};
  }
  if (m_colorGradingLut.id != 0) {
    UnloadTexture(m_colorGradingLut);
    m_colorGradingLut = {};
  }

  DestroyBloomMips();
  ReleaseBuffer(m_ldrBuffer, m_ldrBufferPooled);
  ReleaseBuffer(m_pingPongBuffer, m_pingPongBufferPooled);
  m_ldrBufferPooled = false;
  m_pingPongBufferPooled = false;
  m_finalOutputBuffer = {};
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_cachedBloomMips = -1;
  m_cachedLutSize = 0;
  m_initialized = false;
}

void PostProcessPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  if (!m_ldrBufferPooled) {
    resources::FramebufferManager::Resize(m_ldrBuffer, width, height);
  }
  if (!m_pingPongBufferPooled) {
    resources::FramebufferManager::Resize(m_pingPongBuffer, width, height);
  }
}

void PostProcessPass::EnsureWorkingBuffers(const graph::RenderContext &context,
                                           int width, int height) {
  const bool usePool = context.transientPool != nullptr;

  if (usePool) {
    if (!m_ldrBufferPooled) {
      resources::FramebufferManager::Destroy(m_ldrBuffer);
    }
    if (!m_pingPongBufferPooled) {
      resources::FramebufferManager::Destroy(m_pingPongBuffer);
    }

    m_ldrBuffer = context.transientPool->AcquireColorTarget(width, height, kGLRgba8);
    m_pingPongBuffer =
        context.transientPool->AcquireColorTarget(width, height, kGLRgba8);
    m_ldrBufferPooled = m_ldrBuffer.IsValid();
    m_pingPongBufferPooled = m_pingPongBuffer.IsValid();
    if (m_ldrBufferPooled && m_pingPongBufferPooled) {
      return;
    }

    LOG_WARN("PostProcessPass: transient pool acquire failed, fallback to persistent buffers");
    m_ldrBufferPooled = false;
    m_pingPongBufferPooled = false;
  } else {
    if (m_ldrBufferPooled) {
      m_ldrBuffer = {};
      m_ldrBufferPooled = false;
    }
    if (m_pingPongBufferPooled) {
      m_pingPongBuffer = {};
      m_pingPongBufferPooled = false;
    }
  }

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
    ReleaseBuffer(mip.fbo, mip.pooled);
  }
  m_bloomMips.clear();
}

void PostProcessPass::RebuildBloomMips(const graph::RenderContext &context,
                                       int baseWidth, int baseHeight,
                                       int mipLevels) {
  DestroyBloomMips();
  if (mipLevels <= 0 || baseWidth <= 0 || baseHeight <= 0) {
    return;
  }

  const bool usePool = context.transientPool != nullptr;
  m_bloomMips.reserve(static_cast<size_t>(mipLevels));
  int width = baseWidth;
  int height = baseHeight;
  for (int level = 0; level < mipLevels; ++level) {
    width = std::max(width / 2, 1);
    height = std::max(height / 2, 1);
    BloomMip mip = {};
    mip.width = width;
    mip.height = height;
    if (usePool) {
      mip.fbo =
          context.transientPool->AcquireColorTarget(width, height, kGLRgba16f);
      mip.pooled = mip.fbo.IsValid();
    }
    if (!mip.fbo.IsValid()) {
      mip.pooled = false;
      mip.fbo = resources::FramebufferManager::Create(width, height, kGLRgba16f,
                                                      false);
    }
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
  const float exposure = config.adaptiveQuality.exposure;
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
  std::swap(m_ldrBufferPooled, m_pingPongBufferPooled);
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
  std::swap(m_ldrBufferPooled, m_pingPongBufferPooled);
}

bool PostProcessPass::LoadColorGradingLUT(int lutSize) {
  const char *primaryPath = nullptr;
  int primarySize = lutSize;
  switch (lutSize) {
  case 32:
    primaryPath = "assets/luts/nightmare_32.png";
    break;
  case 16:
  default:
    primaryPath = "assets/luts/cinematic_warm_16.png";
    primarySize = 16;
    break;
  }

  Texture2D loaded = LoadTexture(primaryPath);
  int loadedSize = primarySize;
  if (loaded.id == 0) {
    LOG_WARN("PostProcessPass: LUT '{}' load failed, fallback to neutral_16",
             primaryPath);
    loaded = LoadTexture("assets/luts/neutral_16.png");
    loadedSize = 16;
  }
  if (loaded.id == 0) {
    LOG_ERROR("PostProcessPass: neutral LUT load failed");
    return false;
  }

  if (m_colorGradingLut.id != 0) {
    UnloadTexture(m_colorGradingLut);
  }
  m_colorGradingLut = loaded;
  m_cachedLutSize = loadedSize;
  return true;
}

void PostProcessPass::ExecuteColorGrading(const graph::RenderContext &context) {
  const auto &config = context.qualityManager->GetConfig();
  if (!config.colorGradingEnabled || config.colorGradingLutSize <= 0 ||
      m_colorGradingShader.id == 0) {
    return;
  }
  if (m_colorGradingLut.id == 0 || m_cachedLutSize != config.colorGradingLutSize) {
    if (!LoadColorGradingLUT(config.colorGradingLutSize)) {
      return;
    }
  }

  BindFramebufferAndViewport(m_pingPongBuffer);

  const int sceneUnit = 0;
  const int lutUnit = 1;
  const float intensity = std::clamp(config.colorGradingIntensity, 0.0f, 1.0f);
  const int lutSize = m_cachedLutSize;

  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_ldrBuffer.colorTexture);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_colorGradingLut.id);

  BeginShaderMode(m_colorGradingShader);
  if (m_colorGradingSceneLoc >= 0) {
    SetShaderValue(m_colorGradingShader, m_colorGradingSceneLoc, &sceneUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_colorGradingLutLoc >= 0) {
    SetShaderValue(m_colorGradingShader, m_colorGradingLutLoc, &lutUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_colorGradingIntensityLoc >= 0) {
    SetShaderValue(m_colorGradingShader, m_colorGradingIntensityLoc, &intensity,
                   SHADER_UNIFORM_FLOAT);
  }
  if (m_colorGradingLutSizeLoc >= 0) {
    SetShaderValue(m_colorGradingShader, m_colorGradingLutSizeLoc, &lutSize,
                   SHADER_UNIFORM_INT);
  }
  resources::FullscreenQuad::Draw();
  EndShaderMode();

  std::swap(m_ldrBuffer, m_pingPongBuffer);
  std::swap(m_ldrBufferPooled, m_pingPongBufferPooled);
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
  EnsureWorkingBuffers(context, width, height);

  const int bloomMipLevels = std::max(0, config.bloomMipLevels);
  const bool useTransientPool = context.transientPool != nullptr;
  bool bloomNeedsRebuild = (m_cachedBloomMips != bloomMipLevels);
  if (!bloomNeedsRebuild && !useTransientPool && !m_bloomMips.empty()) {
    const int expectedFirstMipWidth = std::max(width / 2, 1);
    const int expectedFirstMipHeight = std::max(height / 2, 1);
    bloomNeedsRebuild =
        (m_bloomMips.front().width != expectedFirstMipWidth ||
         m_bloomMips.front().height != expectedFirstMipHeight);
  }
  if (useTransientPool || bloomNeedsRebuild) {
    m_cachedBloomMips = bloomMipLevels;
    RebuildBloomMips(context, width, height, bloomMipLevels);
  }

  if (config.bloomEnabled && !m_bloomMips.empty()) {
    ExecuteBloom(context);
  }
  ExecuteTonemap(context);

  if (config.vignetteEnabled) {
    ExecuteVignette(context);
  }
  if (config.colorGradingEnabled && config.colorGradingLutSize > 0) {
    ExecuteColorGrading(context);
  }
  if (config.fxaaEnabled) {
    ExecuteFXAA(context);
  }

  m_finalOutputBuffer = m_ldrBuffer;
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
}

} // namespace NoMoreDay::render::passes
