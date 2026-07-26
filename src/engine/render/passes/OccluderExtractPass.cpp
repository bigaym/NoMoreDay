#include "engine/render/passes/OccluderExtractPass.hpp"

#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/ShadowCasterComponent.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLR8 = 0x8229;
constexpr uint32_t kComputeGroupSize = 8u;
constexpr uint32_t kTextureFetchBarrierBit = 0x00000008;
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

void HashAppend(uint64_t &hash, const uint64_t value) noexcept {
  hash ^= value;
  hash *= kFnvPrime;
}

uint64_t BuildOccluderWord(const components::GPUShadowCaster &caster) noexcept {
  const uint32_t qx = static_cast<uint32_t>(std::lround(caster.posX * 16.0f));
  const uint32_t qy = static_cast<uint32_t>(std::lround(caster.posY * 16.0f));
  const uint32_t qr = static_cast<uint32_t>(std::lround(caster.radius * 16.0f));
  const uint32_t qh =
      static_cast<uint32_t>(std::lround(caster.occluderHeight * 16.0f));

  uint64_t word = static_cast<uint64_t>(qx);
  word = (word << 16) ^ static_cast<uint64_t>(qy & 0xFFFFu);
  word = (word << 16) ^ static_cast<uint64_t>(qr & 0xFFFFu);
  word = (word << 16) ^ static_cast<uint64_t>(qh & 0xFFFFu);
  word ^= static_cast<uint64_t>(caster.shapeIndex) << 8;
  word ^= static_cast<uint64_t>(caster.dynamicFlag) << 1;
  return word;
}

uint64_t FinalizeSignature(std::vector<uint64_t> words) {
  std::sort(words.begin(), words.end());
  uint64_t hash = kFnvOffset;
  for (const uint64_t word : words) {
    HashAppend(hash, word);
  }
  HashAppend(hash, static_cast<uint64_t>(words.size()));
  return hash;
}

} // namespace

OccluderExtractPass::OccluderExtractPass() = default;

OccluderExtractPass::~OccluderExtractPass() { Shutdown(); }

void OccluderExtractPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Write(graph::RenderResourceTag::OccluderMask,
                graph::RenderOwnerTag::OccluderExtract);
}

bool OccluderExtractPass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_extractShader = resources.loadComputeShader(
      "v5_occluder_extract_compute"_hs,
      "assets/shaders/lighting/v5_occluder_extract.comp");
  m_composeShader = resources.loadComputeShader(
      "v5_occluder_compose_compute"_hs,
      "assets/shaders/lighting/v5_occluder_compose.comp");
  if (m_extractShader.id == 0 || m_composeShader.id == 0) {
    Shutdown();
    return false;
  }

  m_resolutionLoc = rlGetLocationUniform(m_extractShader.id, "uResolution");
  m_occluderCountLoc = rlGetLocationUniform(m_extractShader.id, "uOccluderCount");
  m_cameraOffsetLoc = rlGetLocationUniform(m_extractShader.id, "uCameraOffset");
  m_screenSizeLoc = rlGetLocationUniform(m_extractShader.id, "uScreenSize");
  m_dynamicOnlyLoc = rlGetLocationUniform(m_extractShader.id, "uDynamicOnly");

  m_composeResolutionLoc =
      rlGetLocationUniform(m_composeShader.id, "uResolution");

  m_initialized = true;
  return true;
}

void OccluderExtractPass::Shutdown() {
  m_extractShader = {};
  m_composeShader = {};

  m_occluderBuffer.Release();
  resources::FramebufferManager::Destroy(m_staticMask);
  resources::FramebufferManager::Destroy(m_dynamicMask);
  resources::FramebufferManager::Destroy(m_occluderMask);

  m_occluderStaging.clear();
  m_resolutionLoc = -1;
  m_occluderCountLoc = -1;
  m_cameraOffsetLoc = -1;
  m_screenSizeLoc = -1;
  m_dynamicOnlyLoc = -1;
  m_composeResolutionLoc = -1;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_frameIndex = 0;
  m_occluderCount = 0;
  m_lastStaticSignature = 0;
  m_lastDynamicSignature = 0;
  m_staticRebuildCount = 0;
  m_maskChangedThisFrame = false;
  m_staticRebuiltThisFrame = false;
  m_dynamicUpdatedThisFrame = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_initialized = false;
  m_lastFailureReason.clear();
}

void OccluderExtractPass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  EnsureMaskBuffers(width, height);
}

bool OccluderExtractPass::EnsureMaskBuffers(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  const uint32_t format = RenderConstants::V5GI::kOccluderMaskFormat;
  if (!m_staticMask.IsValid()) {
    m_staticMask = resources::FramebufferManager::Create(width, height, format, false);
  } else if (m_staticMask.width != width || m_staticMask.height != height) {
    resources::FramebufferManager::Resize(m_staticMask, width, height);
  }

  if (!m_dynamicMask.IsValid()) {
    m_dynamicMask =
        resources::FramebufferManager::Create(width, height, format, false);
  } else if (m_dynamicMask.width != width || m_dynamicMask.height != height) {
    resources::FramebufferManager::Resize(m_dynamicMask, width, height);
  }

  if (!m_occluderMask.IsValid()) {
    m_occluderMask =
        resources::FramebufferManager::Create(width, height, format, false);
  } else if (m_occluderMask.width != width || m_occluderMask.height != height) {
    resources::FramebufferManager::Resize(m_occluderMask, width, height);
  }

  return m_staticMask.IsValid() && m_dynamicMask.IsValid() &&
         m_occluderMask.IsValid();
}

bool OccluderExtractPass::UploadOccluders(entt::registry &registry,
                                          UploadStats &stats) {
  m_occluderStaging.clear();

  std::vector<uint64_t> staticWords;
  std::vector<uint64_t> dynamicWords;
  staticWords.reserve(256);
  dynamicWords.reserve(256);

  auto view = registry.view<const Position, const NoMoreDay::ShadowCasterComponent>();
  m_occluderStaging.reserve(static_cast<size_t>(view.size_hint()));
  for (const entt::entity entity : view) {
    const auto &[position, casterComponent] =
        view.get<const Position, const NoMoreDay::ShadowCasterComponent>(entity);

    float radius = 24.0f;
    if (const auto *vision = registry.try_get<VisionComponent>(entity);
        vision != nullptr && vision->radius > 0.0f) {
      radius = vision->radius;
    }

    components::GPUShadowCaster caster = {
        .posX = position.x,
        .posY = position.y,
        .radius = radius,
        .occluderHeight = casterComponent.occluderHeight,
        .shapeIndex = static_cast<uint32_t>(casterComponent.shape),
        .dynamicFlag = casterComponent.dynamicFlag,
        .reserved0 = 0u,
        .reserved1 = 0u,
    };
    m_occluderStaging.push_back(caster);

    const uint64_t word = BuildOccluderWord(caster);
    if (caster.dynamicFlag != 0u) {
      dynamicWords.push_back(word);
      ++stats.dynamicCount;
    } else {
      staticWords.push_back(word);
      ++stats.staticCount;
    }
  }

  stats.totalCount = static_cast<uint32_t>(m_occluderStaging.size());
  stats.staticSignature = FinalizeSignature(std::move(staticWords));
  stats.dynamicSignature = FinalizeSignature(std::move(dynamicWords));

  const size_t requiredBytes =
      std::max<size_t>(1u, m_occluderStaging.size()) *
      sizeof(components::GPUShadowCaster);
  if (m_occluderBuffer.GetId() == 0 || m_occluderBuffer.GetSize() < requiredBytes) {
    m_occluderBuffer.Create(requiredBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_occluderBuffer.GetId() == 0) {
    return false;
  }

  if (!m_occluderStaging.empty()) {
    m_occluderBuffer.Update(
        m_occluderStaging.data(),
        m_occluderStaging.size() * sizeof(components::GPUShadowCaster), 0);
  }
  return true;
}

bool OccluderExtractPass::RunExtractPass(const Camera2D &camera,
                                         const bool dynamicOnly,
                                         const uint32_t outputTexture,
                                         const uint32_t occluderCount) {
  if (m_extractShader.id == 0 || outputTexture == 0u || m_cachedWidth <= 0 ||
      m_cachedHeight <= 0) {
    return false;
  }

  rlEnableShader(m_extractShader.id);
  const int resolution[2] = {m_cachedWidth, m_cachedHeight};
  if (m_resolutionLoc >= 0) {
    rlSetUniform(m_resolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  const int occluderCountInt = static_cast<int>(occluderCount);
  if (m_occluderCountLoc >= 0) {
    rlSetUniform(m_occluderCountLoc, &occluderCountInt, RL_SHADER_UNIFORM_INT, 1);
  }

  const float zoom = std::max(camera.zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(m_cachedWidth) / zoom,
                               static_cast<float>(m_cachedHeight) / zoom};
  const float cameraOffset[2] = {
      camera.target.x - (camera.offset.x / zoom),
      camera.target.y - (camera.offset.y / zoom),
  };
  if (m_cameraOffsetLoc >= 0) {
    rlSetUniform(m_cameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_screenSizeLoc >= 0) {
    rlSetUniform(m_screenSizeLoc, screenSize, RL_SHADER_UNIFORM_VEC2, 1);
  }
  const int dynamicOnlyInt = dynamicOnly ? 1 : 0;
  if (m_dynamicOnlyLoc >= 0) {
    rlSetUniform(m_dynamicOnlyLoc, &dynamicOnlyInt, RL_SHADER_UNIFORM_INT, 1);
  }

  NoMoreDay::utils::GPUUtils::BindBufferBase(RenderConstants::ShadowCS::kOccluderBinding,
                                             m_occluderBuffer.GetId());
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kOccluderMaskImageBinding, outputTexture, 0, false, 0,
      kGLWriteOnly, kGLR8);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(m_cachedWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(m_cachedHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

bool OccluderExtractPass::RunComposePass() {
  if (m_composeShader.id == 0 || !m_staticMask.IsValid() || !m_dynamicMask.IsValid() ||
      !m_occluderMask.IsValid()) {
    return false;
  }

  rlEnableShader(m_composeShader.id);
  const int resolution[2] = {m_cachedWidth, m_cachedHeight};
  if (m_composeResolutionLoc >= 0) {
    rlSetUniform(m_composeResolutionLoc, resolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  constexpr uint32_t kStaticLayerBinding = 0u;
  constexpr uint32_t kDynamicLayerBinding = 1u;
  constexpr uint32_t kOutputLayerBinding = 2u;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kStaticLayerBinding,
                                               m_staticMask.colorTexture, 0, false, 0,
                                               kGLReadOnly, kGLR8);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kDynamicLayerBinding,
                                               m_dynamicMask.colorTexture, 0, false, 0,
                                               kGLReadOnly, kGLR8);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kOutputLayerBinding,
                                               m_occluderMask.colorTexture, 0, false, 0,
                                               kGLWriteOnly, kGLR8);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(m_cachedWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(m_cachedHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

void OccluderExtractPass::ReportFailure(const char *reason) {
  m_lastExecuteFailure = true;
  m_lastExecuteSuccess = false;
  m_lastFailureReason = (reason != nullptr) ? reason : "unknown";
  LOG_WARN("OccluderExtractPass fallback: frame={} reason={}", m_frameIndex,
           m_lastFailureReason);
}

void OccluderExtractPass::MarkSuccess() {
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = true;
  m_lastFailureReason.clear();
}

void OccluderExtractPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_maskChangedThisFrame = false;
  m_staticRebuiltThisFrame = false;
  m_dynamicUpdatedThisFrame = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();
  m_occluderCount = 0u;

  if (context.registry == nullptr || context.shared == nullptr ||
      context.shared->resources == nullptr || context.qualityManager == nullptr ||
      context.camera == nullptr) {
    ReportFailure("missing render context prerequisites");
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled) {
    MarkSuccess();
    return;
  }
  if (!context.hdrSceneBuffer.IsValid()) {
    ReportFailure("hdr scene buffer unavailable");
    return;
  }
  if (!m_initialized && !Initialize(*context.shared->resources)) {
    ReportFailure("failed to initialize occluder extract shaders");
    return;
  }

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  if (width <= 0 || height <= 0) {
    ReportFailure("invalid occluder extraction resolution");
    return;
  }

  if (m_cachedWidth != width || m_cachedHeight != height || !m_occluderMask.IsValid()) {
    OnResize(width, height);
  }
  if (!EnsureMaskBuffers(width, height)) {
    ReportFailure("failed to allocate occluder mask buffers");
    return;
  }

  UploadStats stats = {};
  if (!UploadOccluders(*context.registry, stats)) {
    ReportFailure("failed to upload occluder buffer");
    return;
  }
  m_occluderCount = stats.totalCount;

  const bool cameraChanged = (context.camera == nullptr) ||
                             (context.camera->target.x != m_lastCameraTarget.x ||
                              context.camera->target.y != m_lastCameraTarget.y ||
                              context.camera->zoom != m_lastCameraZoom ||
                              width != m_lastViewportWidth ||
                              height != m_lastViewportHeight);
  if (cameraChanged) {
    ++m_cameraInvalidateCount;
  }

  const bool staticChanged = (stats.staticSignature != m_lastStaticSignature) || cameraChanged;
  const bool dynamicChanged = (stats.dynamicSignature != m_lastDynamicSignature) || cameraChanged;
  const bool firstFrame = (m_frameIndex <= 1u);

  if (firstFrame || staticChanged) {
    if (!RunExtractPass(*context.camera, false, m_staticMask.colorTexture,
                        stats.totalCount)) {
      ReportFailure("failed to rebuild static occluder layer");
      return;
    }
    m_staticRebuiltThisFrame = true;
    ++m_staticRebuildCount;
  }

  if (firstFrame || dynamicChanged) {
    if (!RunExtractPass(*context.camera, true, m_dynamicMask.colorTexture,
                        stats.totalCount)) {
      ReportFailure("failed to update dynamic occluder layer");
      return;
    }
    m_dynamicUpdatedThisFrame = true;
  }

  if (firstFrame || m_staticRebuiltThisFrame || m_dynamicUpdatedThisFrame) {
    if (!RunComposePass()) {
      ReportFailure("failed to compose occluder mask");
      return;
    }
    m_maskChangedThisFrame = true;
    ++m_maskVersion;
  }

  m_lastStaticSignature = stats.staticSignature;
  m_lastDynamicSignature = stats.dynamicSignature;

  m_previousOccluderBounds = m_currentOccluderBounds;
  render::gi::JFARect currentBounds{};
  if (context.camera != nullptr) {
    for (const auto &caster : m_occluderStaging) {
      if (caster.dynamicFlag != 0u) {
        Vector2 screenPos = GetWorldToScreen2D(Vector2{caster.posX, caster.posY}, *context.camera);
        float r = caster.radius;
        int minX = std::max(0, static_cast<int>(screenPos.x - r));
        int minY = std::max(0, static_cast<int>(screenPos.y - r));
        int maxX = std::min(width, static_cast<int>(screenPos.x + r));
        int maxY = std::min(height, static_cast<int>(screenPos.y + r));
        currentBounds = currentBounds.Union(render::gi::JFARect{minX, minY, maxX, maxY});
      }
    }
  }
  m_currentOccluderBounds = currentBounds;

  if (context.camera != nullptr) {
    m_lastCameraTarget = context.camera->target;
    m_lastCameraZoom = context.camera->zoom;
  }

  m_lastViewportWidth = width;
  m_lastViewportHeight = height;

  if (m_debugVisualizationEnabled && (m_frameIndex % 120u) == 0u) {
    LOG_INFO(
        "OccluderExtractPass debug: frame={} total={} static={} dynamic={} "
        "staticRebuilds={} maskChanged={}",
        m_frameIndex, stats.totalCount, stats.staticCount, stats.dynamicCount,
        m_staticRebuildCount, m_maskChangedThisFrame ? 1 : 0);
  }

  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
