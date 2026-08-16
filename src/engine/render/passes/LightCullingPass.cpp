#include "engine/render/passes/LightCullingPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cassert>
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLShaderStorageBarrierBit = 0x00002000;
constexpr uint32_t kGLCopyReadBuffer = 0x8F36;
constexpr uint32_t kGLCopyWriteBuffer = 0x8F37;
constexpr uint32_t kGLStreamRead = 0x88E1;
constexpr uint32_t kGLSyncGpuCommandsComplete = 0x9117;
constexpr uint32_t kGLAlreadySignaled = 0x9118;
constexpr uint32_t kGLConditionSatisfied = 0x9119;
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

LightCullingPass::SnapshotPollOutcome LightCullingPass::TryPublishReadySnapshot(
    const bool slotArmed, const bool frameEligible, const bool fenceSignaled,
    const size_t readIndex, const size_t ringDepth,
    const uint32_t pendingSnapshot, const uint32_t currentSnapshot) noexcept {
  assert(ringDepth > 0u);
  SnapshotPollOutcome outcome;
  outcome.snapshot = currentSnapshot;
  outcome.nextReadIndex = readIndex;
  if (slotArmed && frameEligible && fenceSignaled) {
    outcome.published = true;
    outcome.snapshot = pendingSnapshot;
    outcome.nextReadIndex = (readIndex + 1u) % ringDepth;
  }
  return outcome;
}

void LightCullingPass::Setup(graph::RenderGraphBuilder &builder) {
  const auto declareClusterBuffer = [&builder](graph::RenderResourceTag tag,
                                               const char *name) {
    graph::TypedResourceDescriptor desc;
    desc.name = name;
    desc.tag = tag;
    desc.ownerTag = graph::RenderOwnerTag::LightCulling;
    desc.kind = graph::ResourceKind::StorageBuffer;
    desc.lifetime = graph::ResourceLifetime::Persistent;
    builder.DeclareResource(desc);
  };
  declareClusterBuffer(graph::RenderResourceTag::ClusterHeaderSSBO,
                       "ClusterHeaderSSBO");
  declareClusterBuffer(graph::RenderResourceTag::ClusterLightIndexSSBO,
                       "ClusterLightIndexSSBO");
  declareClusterBuffer(graph::RenderResourceTag::ClusterPackedLightSSBO,
                       "ClusterPackedLightSSBO");
  declareClusterBuffer(graph::RenderResourceTag::ClusterCounterSSBO,
                       "ClusterCounterSSBO");
  declareClusterBuffer(graph::RenderResourceTag::LightBoundsSSBO,
                       "LightBoundsSSBO");

  builder.Write(graph::RenderResourceTag::ClusterHeaderSSBO,
                graph::RenderOwnerTag::LightCulling,
                graph::PipelineStage::Compute,
                graph::ResourceUsage::StorageWrite);
  builder.Write(graph::RenderResourceTag::ClusterLightIndexSSBO,
                graph::RenderOwnerTag::LightCulling,
                graph::PipelineStage::Compute,
                graph::ResourceUsage::StorageWrite);
  builder.Write(graph::RenderResourceTag::ClusterPackedLightSSBO,
                graph::RenderOwnerTag::LightCulling,
                graph::PipelineStage::Compute,
                graph::ResourceUsage::StorageWrite);
  builder.Write(graph::RenderResourceTag::ClusterCounterSSBO,
                graph::RenderOwnerTag::LightCulling,
                graph::PipelineStage::Compute,
                graph::ResourceUsage::StorageWrite);
  // Light bounds are uploaded by the CPU (ClusteredLightingState::UploadLightBounds)
  // before the compute dispatch reads them; the Host write must precede the
  // Compute read in declaration order so the graph sees a valid producer.
  builder.Write(graph::RenderResourceTag::LightBoundsSSBO,
                graph::RenderOwnerTag::LightCulling, graph::PipelineStage::Host,
                graph::ResourceUsage::StorageWrite);
  builder.Read(graph::RenderResourceTag::LightBoundsSSBO,
               graph::RenderOwnerTag::LightCulling, graph::PipelineStage::Compute,
               graph::ResourceUsage::StorageRead);

  // External backing import contract: every cluster/LightBuffer SSBO reached by
  // Execute is owned outside the graph (ClusteredLightingState / LightManager
  // own the GL buffers; EnsureBufferCapacity / OrphanAndUpload recreate them on
  // capacity growth). Declared here as observer-only metadata — the graph never
  // allocates, resizes, frees, or GL-binds imported backing, and the manual
  // BindBufferBase calls in Execute stay authoritative. Binding points mirror
  // the LightCulling binding domain symbols resolved in Execute.
  const auto resolveLightCullingBinding = [](const char *symbol,
                                             uint32_t fallback) -> uint32_t {
    uint32_t resolved = 0u;
    if (!core::BindingRegistry::TryResolve(core::BindingDomain::LightCulling,
                                           symbol, resolved)) {
      LOG_ERROR("LightCullingPass: failed to resolve binding '%s'", symbol);
      return fallback;
    }
    return resolved;
  };

  // Observer-only binding declarations mirroring the LightCulling binding
  // domain symbols resolved in Execute (BindingRegistry::LightCulling, points
  // 0-5). The graph-driven resolver admits them against the per-frame
  // imported-backing snapshot and issues behavior-equivalent binds right before
  // Execute; the manual BindBufferBase calls inside Execute stay authoritative
  // and untouched.
  builder.BindBufferBase(graph::RenderResourceTag::LightBufferSSBO,
                         resolveLightCullingBinding("LIGHT_LIST_IN", 0u));
  builder.BindBufferBase(graph::RenderResourceTag::ClusterHeaderSSBO,
                         resolveLightCullingBinding("CLUSTER_HEADER_OUT", 1u));
  builder.BindBufferBase(graph::RenderResourceTag::ClusterLightIndexSSBO,
                         resolveLightCullingBinding("CLUSTER_INDEX_OUT", 2u));
  builder.BindBufferBase(graph::RenderResourceTag::LightBoundsSSBO,
                         resolveLightCullingBinding("LIGHT_BOUNDS_IN", 3u));
  builder.BindBufferBase(graph::RenderResourceTag::ClusterCounterSSBO,
                         resolveLightCullingBinding("CLUSTER_COUNTER", 4u));
  builder.BindBufferBase(graph::RenderResourceTag::ClusterPackedLightSSBO,
                         resolveLightCullingBinding("CLUSTER_LIGHT_OUT", 5u));

  const auto importClusterBuffer =
      [&builder](graph::RenderResourceTag tag, uint32_t bindingPoint) {
        graph::ResourceImportInfo import;
        import.resourceTag = tag;
        import.kind = graph::ResourceKind::StorageBuffer;
        import.backingOwner = graph::RenderOwnerTag::LightCulling;
        import.resizeFollowsCapacity = true; // EnsureBufferCapacity recreates buffers
        import.bindingPoint = bindingPoint;
        builder.ImportResource(import);
      };
  importClusterBuffer(graph::RenderResourceTag::ClusterHeaderSSBO,
                      resolveLightCullingBinding("CLUSTER_HEADER_OUT", 1u));
  importClusterBuffer(graph::RenderResourceTag::ClusterLightIndexSSBO,
                      resolveLightCullingBinding("CLUSTER_INDEX_OUT", 2u));
  importClusterBuffer(graph::RenderResourceTag::ClusterPackedLightSSBO,
                      resolveLightCullingBinding("CLUSTER_LIGHT_OUT", 5u));
  importClusterBuffer(graph::RenderResourceTag::ClusterCounterSSBO,
                      resolveLightCullingBinding("CLUSTER_COUNTER", 4u));
  importClusterBuffer(graph::RenderResourceTag::LightBoundsSSBO,
                      resolveLightCullingBinding("LIGHT_BOUNDS_IN", 3u));

  graph::ResourceImportInfo lightBufferImport;
  lightBufferImport.resourceTag = graph::RenderResourceTag::LightBufferSSBO;
  lightBufferImport.kind = graph::ResourceKind::StorageBuffer;
  lightBufferImport.backingOwner = graph::RenderOwnerTag::Lighting; // LightManager
  lightBufferImport.resizeFollowsCapacity = true; // OrphanAndUpload on upload
  lightBufferImport.bindingPoint =
      resolveLightCullingBinding("LIGHT_LIST_IN", 0u);
  builder.ImportResource(lightBufferImport);
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

  constexpr size_t kCounterBytes = sizeof(components::GPUClusterCounters);

  for (auto &slot : m_overflowRing) {
    if (slot.counterReadbackBufferId == 0) {
      utils::GPUUtils::GenBuffers(1, &slot.counterReadbackBufferId);
      if (slot.counterReadbackBufferId != 0) {
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, slot.counterReadbackBufferId);
        utils::GPUUtils::BufferData(kGLCopyWriteBuffer,
                                    static_cast<ptrdiff_t>(kCounterBytes),
                                    nullptr, kGLStreamRead);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, 0);
      }
    }
    slot.fence = nullptr;
    slot.armed = false;
    slot.submittedFrame = 0;
  }
  m_ringWrite = 0;
  m_ringRead = 0;
  m_lastOverflowSnapshot = 0;

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
  m_lastOverflowSnapshot = 0;
  m_ringWrite = 0;
  m_ringRead = 0;
  m_readbackEnabledForTesting = false;
  m_lastFailureReason.clear();

  for (auto &slot : m_overflowRing) {
    if (slot.fence != nullptr) {
      utils::GPUUtils::DeleteSync(slot.fence);
      slot.fence = nullptr;
    }
    if (slot.counterReadbackBufferId != 0) {
      utils::GPUUtils::DeleteBuffers(1, &slot.counterReadbackBufferId);
      slot.counterReadbackBufferId = 0;
    }
    slot.armed = false;
    slot.submittedFrame = 0;
  }
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
      context.resources == nullptr) {
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

  if (!m_initialized && !Initialize(*context.resources)) {
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
  const uint32_t lightCount = static_cast<uint32_t>(records.size());
  if (!clusterState.BeginFrame(
          m_frameIndex, static_cast<uint32_t>(context.hdrSceneBuffer.width),
          static_cast<uint32_t>(context.hdrSceneBuffer.height), config.clusterTileSize,
          config.clusterZSliceCount, lightCount)) {
    ReportFailure("failed to allocate cluster state buffers");
    return;
  }

  const float zoom = std::max(context.camera->zoom, kSafeMinZoom);
  const float tileSizeWorld = static_cast<float>(config.clusterTileSize) / zoom;
  const float safetyPad = tileSizeWorld * 0.5f;

  std::vector<components::GPULightBounds> bounds;
  bounds.reserve(static_cast<size_t>(lightCount));
  for (uint32_t i = 0; i < lightCount; ++i) {
    const auto &record = records[static_cast<size_t>(i)];
    const components::GPULight &light = record.gpuLight;
    const float minX = light.posX - light.radius - safetyPad;
    const float minY = light.posY - light.radius - safetyPad;
    const float maxX = light.posX + light.radius + safetyPad;
    const float maxY = light.posY + light.radius + safetyPad;
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
    const int maxTotal = static_cast<int>(core::kMaxTotalClusteredLights);
    rlSetUniform(m_maxTotalClusteredLightsLoc, &maxTotal, RL_SHADER_UNIFORM_INT, 1);
  }

  // B4 final convergence (2026-08-05): in the graph path (the only path
  // production uses) graph-driven binding is the sole binding surface:
  // RenderGraph::Execute issued the admitted 6 BindBufferBase operations via
  // ApplyActivePassBindings just before this Execute. When the resolver could
  // not admit this pass's 6-point surface from the frame's imported-backing
  // snapshot (missing / zero handles), fail closed: skip the dispatch rather
  // than render garbage through unbound SSBOs. Standalone Execute (no active
  // graph: direct pass harness) falls back to the manual binds.
  if (context.activeGraph != nullptr) {
    if (!context.AreActivePassBindingsAdmitted()) {
      rlDisableShader();
      ReportFailure("graph-driven binding denied: imported backing snapshot "
                    "missing or invalid for this frame");
      return;
    }
  } else {
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
      rlDisableShader();
      ReportFailure("failed to resolve light culling bindings");
      return;
    }
    const uint32_t lightBufferId = lighting::LightManager::Get().GetLightBufferId();
    if (lightBufferId == 0u) {
      rlDisableShader();
      ReportFailure("light buffer unavailable");
      return;
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
  }

  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("LightCulling");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
        DivUp(grid.tilesX, kComputeGroupSizeX), DivUp(grid.tilesY, kComputeGroupSizeY),
        DivUp(grid.slicesZ, kComputeGroupSizeZ));
  }
  rlDisableShader();

  // Host readback sync for ReadBackClusterHeaders() below (per design §4.2 this
  // explicit barrier is retained; it is not a graph pass barrier).
  NoMoreDay::utils::GPUUtils::MemoryBarrier(kGLShaderStorageBarrierBit);

  if (m_readbackEnabledForTesting) {
    if (!clusterState.ReadBackClusterHeaders()) {
      ReportFailure("failed to read back cluster headers");
      return;
    }
    m_lastOverflowCount = clusterState.GetLastOverflowSum();
  } else {
    // 1. Poll FIFO read slot (non-blocking, timeout 0). The publish decision is
    // delegated to the pure TryPublishReadySnapshot contract; the GL side
    // effects (fence wait, counter readback, sync delete) stay here.
    auto &readSlot = m_overflowRing[m_ringRead];
    const bool frameEligible =
        readSlot.armed && m_frameIndex > readSlot.submittedFrame;
    bool fenceSignaled = false;
    if (frameEligible && readSlot.fence != nullptr) {
      const uint32_t status =
          utils::GPUUtils::ClientWaitSync(readSlot.fence, 0, 0); // timeout 0
      fenceSignaled = (status == kGLAlreadySignaled ||
                       status == kGLConditionSatisfied);
    }

    uint32_t pendingSnapshot = m_lastOverflowSnapshot;
    if (fenceSignaled) {
      components::GPUClusterCounters counters = {};
      utils::GPUUtils::BindBuffer(kGLCopyReadBuffer,
                                  readSlot.counterReadbackBufferId);
      utils::GPUUtils::GetBufferSubData(kGLCopyReadBuffer, 0, sizeof(counters),
                                        &counters);
      utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, 0);

      const uint64_t overflowSum =
          static_cast<uint64_t>(counters.overflowPoint) +
          static_cast<uint64_t>(counters.overflowSpot) +
          static_cast<uint64_t>(counters.overflowArea) +
          static_cast<uint64_t>(counters.overflowLine);
      pendingSnapshot = static_cast<uint32_t>(
          std::min<uint64_t>(overflowSum,
                             std::numeric_limits<uint32_t>::max()));
    }

    const SnapshotPollOutcome pollOutcome = TryPublishReadySnapshot(
        readSlot.armed, frameEligible, fenceSignaled, m_ringRead, kRingDepth,
        pendingSnapshot, m_lastOverflowSnapshot);
    if (pollOutcome.published) {
      m_lastOverflowSnapshot = pollOutcome.snapshot;
      utils::GPUUtils::DeleteSync(readSlot.fence);
      readSlot.fence = nullptr;
      readSlot.armed = false;
      m_ringRead = pollOutcome.nextReadIndex;
    }

    // 2. Submit copy to write slot if not armed (dropped when the ring is full):
    auto &writeSlot = m_overflowRing[m_ringWrite];
    if (CanSubmitReadbackCopy(writeSlot.armed)) {
      constexpr size_t counterBytes = sizeof(components::GPUClusterCounters);
      const uint32_t counterSrc = clusterState.GetCounterBufferId();

      if (counterSrc != 0 && writeSlot.counterReadbackBufferId != 0) {
        utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, counterSrc);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer,
                                    writeSlot.counterReadbackBufferId);
        utils::GPUUtils::CopyBufferSubData(
            kGLCopyReadBuffer, kGLCopyWriteBuffer, 0, 0,
            static_cast<ptrdiff_t>(counterBytes));

        utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, 0);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, 0);

        void *fence = utils::GPUUtils::FenceSync(kGLSyncGpuCommandsComplete, 0);
        if (fence != nullptr) {
          writeSlot.fence = fence;
          writeSlot.armed = true;
          writeSlot.submittedFrame = m_frameIndex;
          m_ringWrite = (m_ringWrite + 1) % kRingDepth;
        }
      }
    }
    m_lastOverflowCount = m_lastOverflowSnapshot;
  }
  m_clusterDataReadyForCurrentFrame = true;
  MarkSuccess();
}

} // namespace NoMoreDay::render::passes
