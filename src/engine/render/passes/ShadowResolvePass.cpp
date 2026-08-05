#include "engine/render/passes/ShadowResolvePass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

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
  graph::TypedResourceDescriptor maskDesc;
  maskDesc.name = "ShadowMask";
  maskDesc.tag = graph::RenderResourceTag::ShadowMask;
  maskDesc.ownerTag = graph::RenderOwnerTag::Shadow;
  maskDesc.kind = graph::ResourceKind::Texture2D;
  maskDesc.format = graph::ResourceFormat::RGBA16F; // actual FBO format (see OnResize)
  maskDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(maskDesc);

  // SDF was written by ShadowBuildPass's compute dispatch; the graph generates
  // the cross-pass Compute->Fragment transition that replaces the previous
  // manual compute-to-fragment barrier.
  builder.Read(graph::RenderResourceTag::ShadowDistanceField,
               graph::RenderOwnerTag::Shadow, graph::PipelineStage::Fragment,
               graph::ResourceUsage::ShaderRead);
  builder.Write(graph::RenderResourceTag::ShadowMask,
                graph::RenderOwnerTag::Shadow,
                graph::PipelineStage::FramebufferAttachment,
                graph::ResourceUsage::ColorAttachment);

  // External backing import contract: ShadowMask is a FramebufferManager
  // framebuffer owned by this pass (OnResize recreates it at screen size,
  // Shutdown destroys it). Observer-only metadata: the graph never allocates,
  // resizes, frees, or GL-binds this backing.
  graph::ResourceImportInfo maskImport;
  maskImport.resourceTag = graph::RenderResourceTag::ShadowMask;
  maskImport.kind = graph::ResourceKind::Texture2D;
  maskImport.format = graph::ResourceFormat::RGBA16F;
  maskImport.backingOwner = graph::RenderOwnerTag::Shadow;
  maskImport.resizeFollowsScreen = true; // OnResize recreates backing at screen size
  maskImport.colorAttachmentIndex = 0;
  builder.ImportResource(maskImport);
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
  LOG_WARN("ShadowFallback: frame={} reason={} fallback=NoShadowMask", m_frameIndex,
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
    ReclassifyShadowMask();
    return;
  }
  resources::FramebufferManager::Resize(m_shadowMask, width, height);
  ReclassifyShadowMask();
}

void ShadowResolvePass::ReclassifyShadowMask() {
  // B11 (RG-3 owner metadata): FramebufferManager registers every FBO under the
  // generic "Scene" owner; the shadow mask must carry the RenderGraph owner
  // contract (Shadow). Observer-only: no GL call, no ownership transfer, only
  // the registry metadata record is updated.
  auto &registry = resources::GPUResourceRegistry::Get();
  if (m_shadowMask.fbo != 0) {
    registry.ReclassifyResourceOwner(m_shadowMask.fbo,
                                     graph::ResourceKind::Framebuffer,
                                     graph::RenderOwnerTag::Shadow);
  }
  if (m_shadowMask.colorTexture != 0) {
    registry.ReclassifyResourceOwner(m_shadowMask.colorTexture,
                                     graph::ResourceKind::Texture2D,
                                     graph::RenderOwnerTag::Shadow);
  }
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

  // The SDF Compute -> Fragment transition is now graph-generated:
  // ShadowBuildPass writes ShadowDistanceField (Compute) and this pass reads it
  // (Fragment), so the compiled plan emits the cross-pass transition and
  // RenderGraph::Execute issues it (with non-zero barrier bits, verified by the
  // B2/B3 gate) before this pass runs. The previous manual
  // compute-to-fragment barrier is removed (B3 final convergence, 2026-08-05).
  // The sampler texture bind below remains manual: the fragment shader samples
  // ShadowDistanceField as a regular texture, which the graph-driven binding
  // surface does not cover.
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
}

} // namespace NoMoreDay::render::passes
