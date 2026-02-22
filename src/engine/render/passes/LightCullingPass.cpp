#include "engine/render/passes/LightCullingPass.hpp"

#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "rlgl.h"

#include <algorithm>
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLShaderStorageBarrierBit = 0x00002000;
constexpr uint32_t kComputeGroupSizeX = 8u;
constexpr uint32_t kComputeGroupSizeY = 8u;
constexpr uint32_t kComputeGroupSizeZ = 1u;
constexpr float kSafeMinZoom = 0.0001f;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

} // namespace

LightCullingPass::LightCullingPass() = default;

LightCullingPass::~LightCullingPass() { Shutdown(); }

void LightCullingPass::Setup(graph::RenderGraphBuilder &builder) {
  (void)builder;
}

bool LightCullingPass::Initialize(::ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_lightCullingShader = resources.loadComputeShader(
      "light_culling_compute"_hs, m_computeShaderPath);
  if (m_lightCullingShader.id == 0) {
    LOG_ERROR("LightCullingPass: failed to load compute shader");
    Shutdown();
    return false;
  }

  m_clusterGridXLoc = rlGetLocationUniform(m_lightCullingShader.id, "uClusterGridX");
  m_clusterGridYLoc = rlGetLocationUniform(m_lightCullingShader.id, "uClusterGridY");
  m_clusterGridZLoc = rlGetLocationUniform(m_lightCullingShader.id, "uClusterGridZ");
  m_tileSizeWorldLoc =
      rlGetLocationUniform(m_lightCullingShader.id, "uTileSizeWorld");
  m_cameraOffsetLoc = rlGetLocationUniform(m_lightCullingShader.id, "uCameraOffset");
  m_lightCountLoc = rlGetLocationUniform(m_lightCullingShader.id, "uLightCount");
  m_maxLightsPerClusterLoc =
      rlGetLocationUniform(m_lightCullingShader.id, "uMaxLightsPerCluster");
  m_maxTotalClusteredLightsLoc =
      rlGetLocationUniform(m_lightCullingShader.id, "uMaxTotalClusteredLights");

  m_initialized = true;
  return true;
}

void LightCullingPass::Shutdown() {
  // Shader lifetime is owned by ResourceManager.
  m_lightCullingShader = {};
  m_clusterGridXLoc = -1;
  m_clusterGridYLoc = -1;
  m_clusterGridZLoc = -1;
  m_tileSizeWorldLoc = -1;
  m_cameraOffsetLoc = -1;
  m_lightCountLoc = -1;
  m_maxLightsPerClusterLoc = -1;
  m_maxTotalClusteredLightsLoc = -1;
  m_initialized = false;
  m_clusterDataReadyForCurrentFrame = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastOverflowCount = 0;
  m_readbackEnabledForTesting = true;
  m_lastFailureReason.clear();
}

void LightCullingPass::SetComputeShaderPathForTesting(const std::string &path) {
  m_computeShaderPath = path;
  m_initialized = false;
}

void LightCullingPass::ReportFailure(const char *reason) {
  m_clusterDataReadyForCurrentFrame = false;
  m_lastExecuteFailure = true;
  m_lastExecuteSuccess = false;
  m_lastFailureReason = (reason != nullptr) ? reason : "unknown";
  LOG_LIMITED_WARN(1.0f, "LightCullingPass fallback: frame={} reason={}",
                   m_frameIndex, m_lastFailureReason);
}

void LightCullingPass::MarkSuccess() {
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = true;
  m_lastFailureReason.clear();
}

void LightCullingPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_clusterDataReadyForCurrentFrame = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();
  m_lastOverflowCount = 0;

  if (context.qualityManager == nullptr || context.camera == nullptr ||
      context.shared == nullptr || context.shared->resources == nullptr) {
    ReportFailure("missing render context prerequisites");
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.v3Enabled || !config.dynamicLightingEnabled ||
      !config.clusteredLightingEnabled) {
    MarkSuccess();
    return;
  }

  if (!context.hdrSceneBuffer.IsValid()) {
    ReportFailure("hdr scene buffer unavailable");
    return;
  }

  if (!m_initialized && !Initialize(*context.shared->resources)) {
    ReportFailure("failed to initialize light culling shader");
    return;
  }

  auto &clusterState = lighting::ClusteredLightingState::Get();
  const auto grid = lighting::ClusteredLightingState::ComputeClusterGridDimensions(
      static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.width)),
      static_cast<uint32_t>(std::max(0, context.hdrSceneBuffer.height)),
      config.clusterTileSize, config.clusterZSliceCount);
  if (grid.clusterCount == 0u) {
    ReportFailure("invalid cluster dimensions");
    return;
  }

  const auto &records =
      lighting::LightManager::Get().GetActiveLightRecordsCpu();
  uint32_t lightCount = static_cast<uint32_t>(records.size());
  const bool useV4Clustering = config.clusteredLightingV4Enabled;
  if (!useV4Clustering) {
    lightCount = std::min<uint32_t>(lightCount, 256u);
  }
  if (!clusterState.BeginFrame(
          m_frameIndex, static_cast<uint32_t>(context.hdrSceneBuffer.width),
          static_cast<uint32_t>(context.hdrSceneBuffer.height), config.clusterTileSize,
          config.clusterZSliceCount, lightCount)) {
    ReportFailure("failed to allocate cluster state buffers");
    return;
  }

  std::vector<components::GPULightBounds> bounds;
  bounds.reserve(static_cast<size_t>(lightCount));
  for (uint32_t i = 0; i < lightCount; ++i) {
    const auto &record = records[static_cast<size_t>(i)];
    const components::GPULight &light = record.gpuLight;
    const float minX = light.posX - light.radius;
    const float minY = light.posY - light.radius;
    const float maxX = light.posX + light.radius;
    const float maxY = light.posY + light.radius;
    const int32_t minLayer = lighting::ClusteredLightingState::MapWorldYToRenderLayer(
        minY, lighting::ClusteredLightingState::kDefaultLayerBandWorldUnits);
    const int32_t maxLayer = lighting::ClusteredLightingState::MapWorldYToRenderLayer(
        maxY, lighting::ClusteredLightingState::kDefaultLayerBandWorldUnits);
    bounds.push_back({
        .minXY = {minX, minY},
        .maxXY = {maxX, maxY},
        .minLayer = static_cast<float>(std::min(minLayer, maxLayer)),
        .maxLayer = static_cast<float>(std::max(minLayer, maxLayer)),
        .lightIndex = i,
        .reserved = static_cast<uint32_t>(record.priority),
    });
  }
  if (!clusterState.UploadLightBounds(bounds)) {
    ReportFailure("failed to upload light bounds");
    return;
  }

  uint32_t lightListBinding = 0u;
  uint32_t headerBinding = 0u;
  uint32_t indexBinding = 0u;
  uint32_t packedLightBinding = 0u;
  uint32_t boundsBinding = 0u;
  uint32_t counterBinding = 0u;
  if (!core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "LIGHT_LIST_IN", lightListBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_HEADER_OUT", headerBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_INDEX_OUT", indexBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_LIGHT_OUT", packedLightBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "LIGHT_BOUNDS_IN", boundsBinding) ||
      !core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                         "CLUSTER_COUNTER", counterBinding)) {
    ReportFailure("failed to resolve light culling bindings");
    return;
  }

  const uint32_t lightBufferId = lighting::LightManager::Get().GetLightBufferId();
  if (lightBufferId == 0u) {
    ReportFailure("light buffer unavailable");
    return;
  }

  rlEnableShader(m_lightCullingShader.id);
  const int clusterX = static_cast<int>(grid.tilesX);
  const int clusterY = static_cast<int>(grid.tilesY);
  const int clusterZ = static_cast<int>(grid.slicesZ);
  if (m_clusterGridXLoc >= 0) {
    rlSetUniform(m_clusterGridXLoc, &clusterX, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_clusterGridYLoc >= 0) {
    rlSetUniform(m_clusterGridYLoc, &clusterY, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_clusterGridZLoc >= 0) {
    rlSetUniform(m_clusterGridZLoc, &clusterZ, RL_SHADER_UNIFORM_INT, 1);
  }

  const float zoom = std::max(context.camera->zoom, kSafeMinZoom);
  const float tileSizeWorld = static_cast<float>(config.clusterTileSize) / zoom;
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom),
  };
  if (m_tileSizeWorldLoc >= 0) {
    rlSetUniform(m_tileSizeWorldLoc, &tileSizeWorld, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_cameraOffsetLoc >= 0) {
    rlSetUniform(m_cameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_lightCountLoc >= 0) {
    const int lightCountInt = static_cast<int>(lightCount);
    rlSetUniform(m_lightCountLoc, &lightCountInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_maxLightsPerClusterLoc >= 0) {
    const int maxPerCluster = static_cast<int>(core::kMaxLightsPerCluster);
    rlSetUniform(m_maxLightsPerClusterLoc, &maxPerCluster, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_maxTotalClusteredLightsLoc >= 0) {
    const int maxTotal = static_cast<int>(
        useV4Clustering ? core::kMaxTotalClusteredLights : 256u);
    rlSetUniform(m_maxTotalClusteredLightsLoc, &maxTotal, RL_SHADER_UNIFORM_INT, 1);
  }

  NoMoreDay::utils::GPUUtils::BindBufferBase(lightListBinding, lightBufferId);
  NoMoreDay::utils::GPUUtils::BindBufferBase(headerBinding,
                                             clusterState.GetClusterHeaderBufferId());
  NoMoreDay::utils::GPUUtils::BindBufferBase(indexBinding,
                                             clusterState.GetClusterLightIndexBufferId());
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      packedLightBinding, clusterState.GetClusterPackedLightBufferId());
  NoMoreDay::utils::GPUUtils::BindBufferBase(boundsBinding,
                                             clusterState.GetLightBoundsBufferId());
  NoMoreDay::utils::GPUUtils::BindBufferBase(counterBinding,
                                             clusterState.GetCounterBufferId());

  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(grid.tilesX, kComputeGroupSizeX), DivUp(grid.tilesY, kComputeGroupSizeY),
      DivUp(grid.slicesZ, kComputeGroupSizeZ));
  rlDisableShader();

  NoMoreDay::utils::GPUUtils::MemoryBarrier(kGLShaderStorageBarrierBit);

  if (m_readbackEnabledForTesting) {
    if (!clusterState.ReadBackClusterHeaders()) {
      ReportFailure("failed to read back cluster headers");
      return;
    }
    m_lastOverflowCount = clusterState.GetLastOverflowSum();
  } else {
    m_lastOverflowCount = 0;
  }
  m_clusterDataReadyForCurrentFrame = true;
  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
