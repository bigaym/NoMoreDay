#include "engine/render/passes/ShadowBuildPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cstddef>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLRg16f = 0x822F;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kShadowGroupSize = 8u;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kShadowAtlasTileFragmentShader =
    "assets/shaders/lighting/shadow_atlas_tile.frag";

// Same-pass phase barrier: the SDF compute dispatch writes the SDF image and
// the occluder SSBO, and the Hybrid path reads them from fragment shaders in
// the SAME Execute (RenderAtlasTiles). Pass-entry barriers and cross-pass graph
// transitions fire before Execute and cannot cover this phase pair, so the bits
// are declared via AddPhaseBarrier(Compute, Fragment, ...) and emitted at the
// exact execution point (after dispatch, before tile draws).
constexpr uint32_t kShadowBuildBarrierBits =
    static_cast<uint32_t>(RenderConstants::Barrier::Image) |
    static_cast<uint32_t>(RenderConstants::Barrier::Buffer);

// B11 (RG-3 owner metadata): FramebufferManager registers every FBO under the
// generic "Scene" owner and ComputeBuffer under "Unknown"; the shadow backings
// must carry the RenderGraph owner contract (Shadow). The real owner
// (ShadowBuildPass) reclassifies the observer records right after it creates or
// recreates the backing (FramebufferManager::Resize and ComputeBuffer::Create
// internally destroy + recreate, so the reclassify must re-apply on every
// resize). Observer-only: no GL call, no ownership transfer, only the registry
// metadata record is updated.
void ReclassifyShadowFramebuffer(const resources::FramebufferHandle &handle) {
  auto &registry = resources::GPUResourceRegistry::Get();
  if (handle.fbo != 0) {
    registry.ReclassifyResourceOwner(handle.fbo, graph::ResourceKind::Framebuffer,
                                     graph::RenderOwnerTag::Shadow);
  }
  if (handle.colorTexture != 0) {
    registry.ReclassifyResourceOwner(handle.colorTexture,
                                     graph::ResourceKind::Texture2D,
                                     graph::RenderOwnerTag::Shadow);
  }
}

void ReclassifyShadowComputeBuffer(unsigned int id) {
  if (id != 0) {
    resources::GPUResourceRegistry::Get().ReclassifyResourceOwner(
        id, graph::ResourceKind::StorageBuffer, graph::RenderOwnerTag::Shadow);
  }
}

} // namespace

ShadowBuildPass::ShadowBuildPass() = default;

ShadowBuildPass::~ShadowBuildPass() { Shutdown(); }

void ShadowBuildPass::Setup(graph::RenderGraphBuilder &builder) {
  graph::TypedResourceDescriptor atlasDesc;
  atlasDesc.name = "ShadowAtlas";
  atlasDesc.tag = graph::RenderResourceTag::ShadowAtlas;
  atlasDesc.ownerTag = graph::RenderOwnerTag::Shadow;
  atlasDesc.kind = graph::ResourceKind::Texture2D;
  atlasDesc.format = graph::ResourceFormat::RGBA16F;
  atlasDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(atlasDesc);

  graph::TypedResourceDescriptor sdfDesc;
  sdfDesc.name = "ShadowDistanceField";
  sdfDesc.tag = graph::RenderResourceTag::ShadowDistanceField;
  sdfDesc.ownerTag = graph::RenderOwnerTag::Shadow;
  sdfDesc.kind = graph::ResourceKind::Texture2D;
  // FramebufferManager::Create uses GL_RG16F for this backing texture.
  sdfDesc.format = graph::ResourceFormat::RG16F;
  sdfDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(sdfDesc);

  graph::TypedResourceDescriptor occluderDesc;
  occluderDesc.name = "ShadowOccluderSSBO";
  occluderDesc.tag = graph::RenderResourceTag::ShadowOccluderSSBO;
  occluderDesc.ownerTag = graph::RenderOwnerTag::Shadow;
  occluderDesc.kind = graph::ResourceKind::StorageBuffer;
  occluderDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(occluderDesc);

  // SDF is written by the compute dispatch and consumed by ShadowResolvePass
  // (cross-pass Compute->Fragment transition is graph-generated).
  builder.Write(graph::RenderResourceTag::ShadowDistanceField,
                graph::RenderOwnerTag::Shadow, graph::PipelineStage::Compute,
                graph::ResourceUsage::StorageWrite);
  builder.Write(graph::RenderResourceTag::ShadowAtlas,
                graph::RenderOwnerTag::Shadow, graph::PipelineStage::Fragment,
                graph::ResourceUsage::ColorAttachment);
  // Occluder data is produced on the host by the shadow prepare stage
  // (ShadowPreparePass) and consumed by this pass's SDF compute dispatch.
  builder.Read(graph::RenderResourceTag::ShadowOccluderSSBO,
               graph::RenderOwnerTag::Shadow, graph::PipelineStage::Compute,
               graph::ResourceUsage::StorageRead);

  // Same-pass phase barrier: SDF compute writes must be visible to the Hybrid
  // atlas tile fragment draws that follow in this Execute. Declared here and
  // emitted by Execute via RenderContext::EmitPhaseBarrier(Compute, Fragment)
  // at the exact execution point (after dispatch, before tile draw).
  builder.AddPhaseBarrier(graph::PipelineStage::Compute,
                          graph::PipelineStage::Fragment,
                          kShadowBuildBarrierBits);

  // Observer-only binding declarations for the manual BindBufferBase /
  // BindImageTexture calls kept in Execute. They give the graph visibility over
  // the GL binding points without issuing any GL call or changing ownership.
  builder.BindBufferBase(graph::RenderResourceTag::ShadowOccluderSSBO,
                         RenderConstants::ShadowCS::kOccluderBinding);
  builder.BindImageUnit(graph::RenderResourceTag::ShadowDistanceField,
                        RenderConstants::ShadowCS::kSdfImageBinding,
                        kGLWriteOnly, kGLRg16f);

  // External backing import contract: the SDF field and shadow atlas are
  // FramebufferManager framebuffers owned by this pass (OnResize/EnsureAtlasSize
  // create/resize them; Shutdown destroys them); the occluder SSBO is a
  // ComputeBuffer owned here too. These declarations are observer-only metadata
  // for the compiled plan: the graph must never allocate, resize, free, or
  // GL-bind imported backing, and manual binds stay authoritative until a future
  // phase swaps them in without changing ownership.
  graph::ResourceImportInfo atlasImport;
  atlasImport.resourceTag = graph::RenderResourceTag::ShadowAtlas;
  atlasImport.kind = graph::ResourceKind::Texture2D;
  atlasImport.format = graph::ResourceFormat::RGBA16F;
  atlasImport.backingOwner = graph::RenderOwnerTag::Shadow;
  atlasImport.resizeFollowsCapacity = true; // EnsureAtlasSize recreates on atlas size change
  atlasImport.colorAttachmentIndex = 0;
  builder.ImportResource(atlasImport);

  graph::ResourceImportInfo sdfImport;
  sdfImport.resourceTag = graph::RenderResourceTag::ShadowDistanceField;
  sdfImport.kind = graph::ResourceKind::Texture2D;
  sdfImport.format = graph::ResourceFormat::RG16F;
  sdfImport.backingOwner = graph::RenderOwnerTag::Shadow;
  sdfImport.resizeFollowsScreen = true; // OnResize recreates backing at screen size
  sdfImport.imageUnit = RenderConstants::ShadowCS::kSdfImageBinding;
  sdfImport.imageAccess = kGLWriteOnly;
  sdfImport.imageFormat = kGLRg16f;
  builder.ImportResource(sdfImport);

  graph::ResourceImportInfo occluderImport;
  occluderImport.resourceTag = graph::RenderResourceTag::ShadowOccluderSSBO;
  occluderImport.kind = graph::ResourceKind::StorageBuffer;
  occluderImport.backingOwner = graph::RenderOwnerTag::Shadow;
  occluderImport.resizeFollowsCapacity = true; // UploadOccluders recreates on capacity growth
  occluderImport.bindingPoint = RenderConstants::ShadowCS::kOccluderBinding;
  builder.ImportResource(occluderImport);
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
    ReclassifyShadowFramebuffer(m_sdfField);
    return;
  }
  resources::FramebufferManager::Resize(m_sdfField, width, height);
  ReclassifyShadowFramebuffer(m_sdfField);
}

void ShadowBuildPass::EnsureAtlasSize(const int atlasSize) {
  const int clampedAtlasSize = std::max(1, atlasSize);
  if (!m_shadowAtlas.IsValid()) {
    m_shadowAtlas = resources::FramebufferManager::Create(clampedAtlasSize,
                                                          clampedAtlasSize,
                                                          kGLRgba16f, false);
    ReclassifyShadowFramebuffer(m_shadowAtlas);
    m_shadowAtlasSize = clampedAtlasSize;
    return;
  }
  if (m_shadowAtlas.width != clampedAtlasSize ||
      m_shadowAtlas.height != clampedAtlasSize) {
    resources::FramebufferManager::Resize(m_shadowAtlas, clampedAtlasSize,
                                          clampedAtlasSize);
    ReclassifyShadowFramebuffer(m_shadowAtlas);
    m_shadowAtlasSize = clampedAtlasSize;
  }
}

bool ShadowBuildPass::UploadOccluders(
    const NoMoreDay::components::GPUShadowCaster *occluders,
    const uint32_t occluderCount) {
  const uint32_t uploadCount =
      std::min(occluderCount, RenderConstants::Shadow::kMaxShadowCasters);

  const size_t requiredBytes =
      std::max<size_t>(1u, uploadCount) * sizeof(NoMoreDay::components::GPUShadowCaster);
  if (m_occluderBuffer.GetId() == 0 || m_occluderBuffer.GetSize() < requiredBytes) {
    m_occluderBuffer.Create(requiredBytes, nullptr, RL_DYNAMIC_DRAW);
    // B11 (RG-3 owner metadata): ComputeBuffer registers owner Unknown; the
    // occluder backing must carry the RenderGraph owner contract (Shadow).
    ReclassifyShadowComputeBuffer(m_occluderBuffer.GetId());
  }
  if (m_occluderBuffer.GetId() == 0) {
    LOG_ERROR("ShadowBuildPass: failed to allocate occluder buffer");
    return false;
  }

  if (occluders != nullptr && uploadCount > 0u) {
    m_occluderBuffer.Update(
        occluders,
        static_cast<size_t>(uploadCount) *
            sizeof(NoMoreDay::components::GPUShadowCaster),
        0);
  }
  m_occluderCount = uploadCount;
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

  if (context.qualityManager == nullptr || context.camera == nullptr ||
      context.resources == nullptr) {
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

  if (!m_initialized && !Initialize(*context.resources)) {
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

  if (!UploadOccluders(context.occluders, context.occluderCount)) {
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

  // Same-pass phase barrier: emitted at the exact execution point (after the
  // SDF dispatch, before the Hybrid atlas tile draws). The graph resolves the
  // bits from the AddPhaseBarrier(Compute, Fragment, ...) declaration in Setup.
  // If the graph path is unavailable (standalone Execute), fall back to the
  // same fallback bits so visual output is unchanged.
  if (!context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                                graph::PipelineStage::Fragment)) {
    NoMoreDay::utils::GPUUtils::MemoryBarrier(kShadowBuildBarrierBits);
  }

  if (config.shadowMode == core::ShadowMode::Hybrid) {
    EnsureAtlasSize(static_cast<int>(config.shadowAtlasSize));
    RenderAtlasTiles(context);
  }

  MarkSuccess();
}

} // namespace NoMoreDay::render::passes
