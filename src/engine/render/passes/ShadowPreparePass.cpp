#include "engine/render/passes/ShadowPreparePass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/LightManager.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>

namespace NoMoreDay::render::passes {
namespace {

constexpr float kPriorityWeight = 1.0f;
constexpr float kInfluenceWeight = 1.0f;

} // namespace

void ShadowPreparePass::Setup(graph::RenderGraphBuilder &builder) {
  // CPU-only stage: it produces the occluder data that ShadowBuildPass's SDF
  // compute dispatch consumes. Declared as a Host write so the graph emits the
  // Host->Compute transition instead of relying on an implicit rlgl flush.
  builder.Write(graph::RenderResourceTag::ShadowOccluderSSBO,
                graph::RenderOwnerTag::Shadow, graph::PipelineStage::Host,
                graph::ResourceUsage::StorageWrite);
}

uint32_t ShadowPreparePass::BuildStableLightId(
    const components::GPULight &light, const uint32_t fallbackIndex) noexcept {
  // NOTE:
  // Shadow atlas identity must be stable across frames. Do NOT include per-frame
  // varying values such as position/intensity/flicker here, otherwise the same
  // logical light churns tile IDs every frame and quickly exhausts atlas slots
  // (observed as requested>0, allocated=0, overflow>0).
  uint32_t hash = 2166136261u;
  const auto mix = [&hash](const uint32_t v) noexcept {
    hash ^= v;
    hash *= 16777619u;
  };

  // fallbackIndex is sourced from active-light ordering and is the best
  // available runtime-stable key in current LightManager contract.
  mix(fallbackIndex);
  mix(light.lightType);
  mix(light.priority & 0xFFu);

  // Radius/type buckets help reduce accidental collisions without introducing
  // frame-to-frame instability.
  mix(std::bit_cast<uint32_t>(light.radius));
  if (hash == 0u) {
    hash = 1u;
  }
  return hash;
}

float ShadowPreparePass::ComputeScreenInfluence(const components::GPULight &light,
                                                const Camera2D &camera,
                                                const float screenWidth,
                                                const float screenHeight) noexcept {
  const float zoom = std::max(camera.zoom, 0.0001f);
  const float worldW = std::max(screenWidth, 1.0f) / zoom;
  const float worldH = std::max(screenHeight, 1.0f) / zoom;
  const float worldDiag = std::max(std::sqrt((worldW * worldW) + (worldH * worldH)),
                                   1.0f);
  const float dx = light.posX - camera.target.x;
  const float dy = light.posY - camera.target.y;
  const float distance = std::sqrt((dx * dx) + (dy * dy));
  const float normalizedRadius = std::clamp(light.radius / worldDiag, 0.0f, 1.0f);
  const float normalizedDistance = std::max(0.0f, distance / worldDiag);
  return std::clamp(normalizedRadius / (1.0f + normalizedDistance), 0.0f, 1.0f);
}

float ShadowPreparePass::ComputeCompositeScore(const uint8_t priority,
                                               const float screenInfluence) noexcept {
  const float priorityNorm = static_cast<float>(priority) / 255.0f;
  return (priorityNorm * kPriorityWeight) + (screenInfluence * kInfluenceWeight);
}

void ShadowPreparePass::EnsureAllocator(const uint32_t atlasSize) {
  m_atlasSize = std::max(1u, atlasSize);
  m_atlasTilesPerRow = std::max(1u, m_atlasSize / m_atlasTileSize);
  const uint32_t tileCount = m_atlasTilesPerRow * m_atlasTilesPerRow;
  if (m_atlasAllocator.GetTileCount() != tileCount) {
    m_atlasAllocator = shadow::ShadowAtlasAllocator(
        tileCount, RenderConstants::Shadow::kAtlasEvictionHysteresis);
  }
}

void ShadowPreparePass::ComputeAtlasRect(const uint32_t tileIndex,
                                         float outRect[4]) const noexcept {
  if (m_atlasTilesPerRow == 0u) {
    outRect[0] = outRect[1] = outRect[2] = outRect[3] = 0.0f;
    return;
  }
  const uint32_t tileX = tileIndex % m_atlasTilesPerRow;
  const uint32_t tileY = tileIndex / m_atlasTilesPerRow;
  const float inv = 1.0f / static_cast<float>(m_atlasTilesPerRow);
  outRect[0] = static_cast<float>(tileX) * inv;
  outRect[1] = static_cast<float>(tileY) * inv;
  outRect[2] = inv;
  outRect[3] = inv;
}

std::vector<ShadowPreparedLight> ShadowPreparePass::RankTopNForAtlas(
    const std::vector<ShadowPrepareLightInput> &inputs, const Camera2D &camera,
    const float screenWidth, const float screenHeight,
    const uint32_t maxShadowedLights) {
  if (inputs.empty() || maxShadowedLights == 0u) {
    return {};
  }

  std::vector<ShadowPreparedLight> ranked;
  ranked.reserve(inputs.size());

  for (const auto &input : inputs) {
    const float influence =
        ComputeScreenInfluence(input.gpuLight, camera, screenWidth, screenHeight);
    ranked.push_back({
        .lightId = BuildStableLightId(input.gpuLight, input.sourceIndex),
        .lightIndex = input.sourceIndex,
        .atlasTileIndex = 0u,
        .priorityScore = static_cast<float>(input.priority) / 255.0f,
        .screenInfluence = influence,
        .compositeScore = ComputeCompositeScore(input.priority, influence),
        .usesAtlas = false,
        .gpuLight = input.gpuLight,
    });
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const ShadowPreparedLight &lhs, const ShadowPreparedLight &rhs) {
              if (lhs.compositeScore != rhs.compositeScore) {
                return lhs.compositeScore > rhs.compositeScore;
              }
              if (lhs.priorityScore != rhs.priorityScore) {
                return lhs.priorityScore > rhs.priorityScore;
              }
              if (lhs.screenInfluence != rhs.screenInfluence) {
                return lhs.screenInfluence > rhs.screenInfluence;
              }
              return lhs.lightId < rhs.lightId;
            });

  if (ranked.size() > maxShadowedLights) {
    ranked.resize(maxShadowedLights);
  }
  return ranked;
}

void ShadowPreparePass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_preparedLights.clear();
  m_atlasOverflowCount = 0;
  m_atlasAllocatedCount = 0;

  if (context.qualityManager == nullptr || context.camera == nullptr) {
    return;
  }
  const auto &config = context.qualityManager->GetConfig();
  if (!config.v3Enabled || !config.shadowEnabled ||
      config.shadowMode != core::ShadowMode::Hybrid) {
    return;
  }

  EnsureAllocator(config.shadowAtlasSize);
  m_atlasAllocator.BeginFrame(m_frameIndex);

  const float screenWidth = static_cast<float>(GetScreenWidth());
  const float screenHeight = static_cast<float>(GetScreenHeight());
  const auto &activeLights =
      NoMoreDay::render::lighting::LightManager::Get().GetActiveLightRecordsCpu();
  std::vector<ShadowPrepareLightInput> inputs;
  inputs.reserve(activeLights.size());
  for (uint32_t i = 0; i < activeLights.size(); ++i) {
    inputs.push_back(
        {.sourceIndex = i, .priority = activeLights[i].priority, .gpuLight = activeLights[i].gpuLight});
  }
  const auto ranked = RankTopNForAtlas(inputs, *context.camera, screenWidth,
                                       screenHeight, config.maxShadowedLights);

  auto &lightManager = NoMoreDay::render::lighting::LightManager::Get();
  lightManager.ClearShadowMapIndices();
  std::vector<NoMoreDay::render::lighting::LightManager::ShadowMapAssignment>
      assignments;
  assignments.reserve(ranked.size());

  m_preparedLights.reserve(ranked.size());
  for (const ShadowPreparedLight &candidate : ranked) {
    const auto allocation = m_atlasAllocator.AcquireTile(
        {.lightId = candidate.lightId, .priorityScore = candidate.compositeScore});
    ShadowPreparedLight light = candidate;
    if (allocation.success) {
      light.atlasTileIndex = allocation.tileIndex;
      ComputeAtlasRect(allocation.tileIndex, light.atlasRect);
      light.usesAtlas = true;
      light.gpuLight.shadowMapIndex = allocation.tileIndex + 1u;
      light.gpuLight.flags |= 0x1u;
      assignments.push_back({.lightIndex = light.lightIndex,
                             .shadowMapIndex = light.gpuLight.shadowMapIndex});
      ++m_atlasAllocatedCount;
    } else {
      light.usesAtlas = false;
      light.gpuLight.shadowMapIndex = 0u;
      light.gpuLight.flags &= ~0x1u;
      ++m_atlasOverflowCount;
    }
    m_preparedLights.push_back(light);
  }
  lightManager.ApplyShadowMapAssignments(assignments);

  if (m_atlasOverflowCount > 0u || m_lastLoggedOverflow > 0u) {
    LOG_WARN(
        "ShadowAtlas: frame={} requested={} allocated={} overflow={} atlasSize={} "
        "tileSize={} tilesPerRow={}",
        m_frameIndex, static_cast<uint32_t>(ranked.size()), m_atlasAllocatedCount,
        m_atlasOverflowCount, m_atlasSize, m_atlasTileSize, m_atlasTilesPerRow);
    m_lastLoggedOverflow = m_atlasOverflowCount;
  }
}

} // namespace NoMoreDay::render::passes
