#include "engine/render/passes/ShadowResolvePass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"

#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kShadowResolveFragmentShader =
    "assets/shaders/lighting/shadow_resolve.frag";

} // namespace

ShadowResolvePass::ShadowResolvePass() = default;

ShadowResolvePass::~ShadowResolvePass() { Shutdown(); }

void ShadowResolvePass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

bool ShadowResolvePass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_shadowResolveShader =
      LoadShader(kFullscreenVertexShader, kShadowResolveFragmentShader);
  if (m_shadowResolveShader.id == 0) {
    LOG_ERROR("ShadowResolvePass: failed to load resolve shader");
    Shutdown();
    return false;
  }

  m_shadowSdfTexLoc = GetShaderLocation(m_shadowResolveShader, "uShadowSdfTex");
  m_shadowSoftnessLoc = GetShaderLocation(m_shadowResolveShader, "uShadowSoftness");
  m_sdfTexelSizeLoc = GetShaderLocation(m_shadowResolveShader, "uSdfTexelSize");
  m_initialized = true;
  return true;
}

bool ShadowResolvePass::ReloadShaders() {
  Shader reloaded = LoadShader(kFullscreenVertexShader, kShadowResolveFragmentShader);
  if (reloaded.id == 0) {
    LOG_WARN("ShadowResolvePass: shader reload failed, keeping previous program");
    return false;
  }

  if (m_shadowResolveShader.id != 0) {
    UnloadShader(m_shadowResolveShader);
  }
  m_shadowResolveShader = reloaded;
  m_shadowSdfTexLoc = GetShaderLocation(m_shadowResolveShader, "uShadowSdfTex");
  m_shadowSoftnessLoc = GetShaderLocation(m_shadowResolveShader, "uShadowSoftness");
  m_sdfTexelSizeLoc = GetShaderLocation(m_shadowResolveShader, "uSdfTexelSize");
  LOG_INFO("ShadowResolvePass: shader hot reloaded");
  return true;
}

void ShadowResolvePass::ReportFailure(const std::string &reason) {
  m_shadowReadyThisFrame = false;
  m_lastExecuteFailed = true;
  m_lastFailureReason = reason;
  LOG_WARN("ShadowFallback: frame={} reason={} fallback=V2Lighting", m_frameIndex,
           m_lastFailureReason);
}

void ShadowResolvePass::Shutdown() {
  if (m_shadowResolveShader.id != 0) {
    UnloadShader(m_shadowResolveShader);
    m_shadowResolveShader = {};
  }
  resources::FramebufferManager::Destroy(m_shadowMask);
  m_shadowSdfTexLoc = -1;
  m_shadowSoftnessLoc = -1;
  m_sdfTexelSizeLoc = -1;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_frameIndex = 0;
  m_shadowReadyThisFrame = false;
  m_lastExecuteFailed = false;
  m_lastFailureReason.clear();
  m_initialized = false;
}

void ShadowResolvePass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  if (!m_shadowMask.IsValid()) {
    m_shadowMask =
        resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
    return;
  }
  resources::FramebufferManager::Resize(m_shadowMask, width, height);
}

void ShadowResolvePass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_shadowReadyThisFrame = false;
  m_lastExecuteFailed = false;
  m_lastFailureReason.clear();

  if (context.qualityManager == nullptr) {
    ReportFailure("missing quality manager context");
    return;
  }
  if (m_buildPass == nullptr) {
    ReportFailure("shadow build pass not bound");
    return;
  }
  const auto &config = context.qualityManager->GetConfig();
  if (!config.v3Enabled || !config.shadowEnabled ||
      config.shadowMode == core::ShadowMode::Off) {
    return;
  }

  if (m_buildPass->DidFailThisFrame()) {
    ReportFailure("shadow build stage failed: " + m_buildPass->GetLastFailureReason());
    return;
  }

  if (!m_buildPass->HasSdfField()) {
    ReportFailure("shadow sdf field unavailable");
    return;
  }

  if (!m_initialized && !Initialize()) {
    ReportFailure("failed to initialize shadow resolve shader");
    return;
  }

  const int width = m_buildPass->GetSdfWidth();
  const int height = m_buildPass->GetSdfHeight();
  if (width <= 0 || height <= 0) {
    ReportFailure("invalid shadow resolve resolution");
    return;
  }
  if (!m_shadowMask.IsValid() || m_cachedWidth != width || m_cachedHeight != height) {
    OnResize(width, height);
    if (!m_shadowMask.IsValid()) {
      ReportFailure("failed to allocate shadow mask framebuffer");
      return;
    }
  }

  core::ApplyComputeToFragmentBarrierTemplate();

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, m_shadowMask.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, m_shadowMask.width, m_shadowMask.height);

  const int sdfTexUnit = 0;
  if (m_shadowSdfTexLoc >= 0) {
    SetShaderValue(m_shadowResolveShader, m_shadowSdfTexLoc, &sdfTexUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_shadowSoftnessLoc >= 0) {
    const float shadowSoftness = std::max(0.0001f, config.shadowSoftness);
    SetShaderValue(m_shadowResolveShader, m_shadowSoftnessLoc, &shadowSoftness,
                   SHADER_UNIFORM_FLOAT);
  }
  if (m_sdfTexelSizeLoc >= 0) {
    const float texelSize[2] = {1.0f / static_cast<float>(width),
                                1.0f / static_cast<float>(height)};
    SetShaderValue(m_shadowResolveShader, m_sdfTexelSizeLoc, texelSize,
                   SHADER_UNIFORM_VEC2);
  }

  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_buildPass->GetSdfTexture());
  BeginShaderMode(m_shadowResolveShader);
  resources::FullscreenQuad::Draw();
  EndShaderMode();

  if (context.hdrSceneBuffer.IsValid()) {
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                                context.hdrSceneBuffer.fbo);
    NoMoreDay::utils::GPUUtils::Viewport(0, 0, context.hdrSceneBuffer.width,
                                         context.hdrSceneBuffer.height);
  }

  m_shadowReadyThisFrame = true;
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
