#include "engine/render/lighting/ClusteredLightingState.hpp"

#include "core/logging/Logger.hpp"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NoMoreDay::render::lighting {

ClusteredLightingState &ClusteredLightingState::Get() {
  static ClusteredLightingState state;
  return state;
}

uint32_t ClusteredLightingState::SafeMulU32(const uint32_t lhs,
                                            const uint32_t rhs) noexcept {
  if (lhs == 0u || rhs == 0u) {
    return 0u;
  }
  const uint64_t product = static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs);
  if (product > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    return 0u;
  }
  return static_cast<uint32_t>(product);
}

ClusteredLightingState::ClusterGridDimensions
ClusteredLightingState::ComputeClusterGridDimensions(const uint32_t screenWidth,
                                                     const uint32_t screenHeight,
                                                     const uint32_t tileSize,
                                                     const uint32_t zSliceCount) noexcept {
  const uint32_t safeTileSize = std::max(1u, tileSize);
  const uint32_t safeZSlices = std::max(1u, zSliceCount);
  if (screenWidth == 0u || screenHeight == 0u) {
    return {};
  }

  ClusterGridDimensions grid = {};
  grid.tilesX = (screenWidth + safeTileSize - 1u) / safeTileSize;
  grid.tilesY = (screenHeight + safeTileSize - 1u) / safeTileSize;
  grid.slicesZ = safeZSlices;

  const uint32_t layerCount = SafeMulU32(grid.tilesX, grid.tilesY);
  grid.clusterCount = SafeMulU32(layerCount, grid.slicesZ);
  if (grid.clusterCount == 0u) {
    grid = {};
  }
  return grid;
}

uint32_t ClusteredLightingState::BuildClusterIndex(const uint32_t tileX,
                                                   const uint32_t tileY,
                                                   const uint32_t sliceZ,
                                                   const uint32_t tilesX,
                                                   const uint32_t tilesY) noexcept {
  const uint32_t layerArea = tilesX * tilesY;
  return tileX + tileY * tilesX + sliceZ * layerArea;
}

bool ClusteredLightingState::DecodeClusterIndex(const uint32_t clusterIndex,
                                                const uint32_t tilesX,
                                                const uint32_t tilesY,
                                                const uint32_t slicesZ,
                                                uint32_t &outTileX,
                                                uint32_t &outTileY,
                                                uint32_t &outSliceZ) noexcept {
  if (tilesX == 0u || tilesY == 0u || slicesZ == 0u) {
    return false;
  }
  const uint64_t layerArea = static_cast<uint64_t>(tilesX) * static_cast<uint64_t>(tilesY);
  const uint64_t clusterCount = layerArea * static_cast<uint64_t>(slicesZ);
  if (clusterIndex >= clusterCount) {
    return false;
  }

  outSliceZ = static_cast<uint32_t>(clusterIndex / layerArea);
  const uint32_t layerOffset = static_cast<uint32_t>(clusterIndex % layerArea);
  outTileY = layerOffset / tilesX;
  outTileX = layerOffset % tilesX;
  return true;
}

int32_t ClusteredLightingState::MapWorldYToRenderLayer(
    const float worldY, const float layerBandWorldUnits) noexcept {
  const float safeBand = (layerBandWorldUnits > 0.0f) ? layerBandWorldUnits : 1.0f;
  const float rawLayer = std::floor(worldY / safeBand);
  const float clamped = std::clamp(rawLayer, static_cast<float>(kRenderLayerMin),
                                   static_cast<float>(kRenderLayerMax));
  return static_cast<int32_t>(clamped);
}

uint32_t ClusteredLightingState::MapRenderLayerToZSlice(
    const int32_t renderLayer, const uint32_t zSliceCount) noexcept {
  const uint32_t safeSlices = std::max(1u, zSliceCount);
  const int32_t clampedLayer = std::clamp(renderLayer, kRenderLayerMin, kRenderLayerMax);
  const uint32_t normalizedLayer =
      static_cast<uint32_t>(clampedLayer - kRenderLayerMin);
  const uint32_t layerSpan =
      static_cast<uint32_t>(kRenderLayerMax - kRenderLayerMin + 1);
  return (normalizedLayer * safeSlices) / layerSpan;
}

bool ClusteredLightingState::EnsureBufferCapacity(const uint32_t clusterCount,
                                                  const uint32_t maxLightBounds) {
  if (clusterCount == 0u) {
    return false;
  }

  const size_t headerBytes =
      static_cast<size_t>(clusterCount) * sizeof(components::GPUClusterHeader);
  const size_t indexBytes =
      static_cast<size_t>(core::kMaxTotalClusteredLights) *
      sizeof(components::GPUClusterLightIndex);
  const size_t packedLightBytes =
      static_cast<size_t>(core::kMaxTotalClusteredLights) *
      sizeof(components::GPUClusterPackedLight);
  const size_t boundsBytes =
      static_cast<size_t>(std::max(1u, maxLightBounds)) *
      sizeof(components::GPULightBounds);
  constexpr size_t kCounterBytes = sizeof(components::GPUClusterCounters);

  if (m_clusterHeaderBuffer.GetId() == 0 || m_clusterHeaderBuffer.GetSize() < headerBytes) {
    m_clusterHeaderBuffer.Create(headerBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_clusterLightIndexBuffer.GetId() == 0 ||
      m_clusterLightIndexBuffer.GetSize() < indexBytes) {
    m_clusterLightIndexBuffer.Create(indexBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_clusterPackedLightBuffer.GetId() == 0 ||
      m_clusterPackedLightBuffer.GetSize() < packedLightBytes) {
    m_clusterPackedLightBuffer.Create(packedLightBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_lightBoundsBuffer.GetId() == 0 || m_lightBoundsBuffer.GetSize() < boundsBytes) {
    m_lightBoundsBuffer.Create(boundsBytes, nullptr, RL_DYNAMIC_DRAW);
  }
  if (m_clusterCounterBuffer.GetId() == 0 ||
      m_clusterCounterBuffer.GetSize() < kCounterBytes) {
    m_clusterCounterBuffer.Create(kCounterBytes, nullptr, RL_DYNAMIC_DRAW);
  }

  const bool buffersReady = m_clusterHeaderBuffer.GetId() != 0 &&
                            m_clusterLightIndexBuffer.GetId() != 0 &&
                            m_clusterPackedLightBuffer.GetId() != 0 &&
                            m_lightBoundsBuffer.GetId() != 0 &&
                            m_clusterCounterBuffer.GetId() != 0;
  if (!buffersReady) {
    LOG_ERROR("ClusteredLightingState: failed to allocate cluster buffers");
  }
  return buffersReady;
}

bool ClusteredLightingState::BeginFrame(const uint32_t frameIndex,
                                        const uint32_t screenWidth,
                                        const uint32_t screenHeight,
                                        const uint32_t tileSize,
                                        const uint32_t zSliceCount,
                                        const uint32_t lightCountEstimate) {
  m_frameIndex = frameIndex;
  m_tileSize = std::max(1u, tileSize);
  m_zSliceCount = std::max(1u, zSliceCount);
  m_grid = ComputeClusterGridDimensions(screenWidth, screenHeight, m_tileSize,
                                        m_zSliceCount);
  m_uploadedLightBoundsCount = 0;
  m_lastOverflowSum = 0;
  m_lastOverflowPoint = 0;
  m_lastOverflowSpot = 0;
  m_lastOverflowArea = 0;
  m_lastOverflowLine = 0;
  m_lastWrittenIndexCount = 0;
  m_clusterLightIndicesReadback.clear();
  if (m_grid.clusterCount == 0u) {
    return false;
  }

  if (!EnsureBufferCapacity(m_grid.clusterCount, lightCountEstimate)) {
    return false;
  }

  m_clusterHeadersScratch.assign(static_cast<size_t>(m_grid.clusterCount), {});
  m_clusterHeaderBuffer.Update(
      m_clusterHeadersScratch.data(),
      static_cast<size_t>(m_clusterHeadersScratch.size()) *
          sizeof(components::GPUClusterHeader),
      0);
  const components::GPUClusterCounters zeroCounters = {};
  m_clusterCounterBuffer.Update(&zeroCounters, sizeof(zeroCounters), 0);
  return true;
}

bool ClusteredLightingState::UploadLightBounds(
    const std::span<const components::GPULightBounds> lightBounds) {
  if (m_lightBoundsBuffer.GetId() == 0) {
    return false;
  }
  m_uploadedLightBoundsCount = static_cast<uint32_t>(lightBounds.size());
  if (lightBounds.empty()) {
    return true;
  }

  m_lightBoundsBuffer.Update(lightBounds.data(),
                             lightBounds.size_bytes(), 0);
  return true;
}

bool ClusteredLightingState::ReadBackClusterHeaders() {
  if (m_clusterHeaderBuffer.GetId() == 0 || m_grid.clusterCount == 0u) {
    return false;
  }

  m_clusterHeadersReadback.assign(static_cast<size_t>(m_grid.clusterCount), {});
  m_clusterHeaderBuffer.Read(
      m_clusterHeadersReadback.data(),
      static_cast<size_t>(m_clusterHeadersReadback.size()) *
          sizeof(components::GPUClusterHeader),
      0);

  components::GPUClusterCounters counters = {};
  m_clusterCounterBuffer.Read(&counters, sizeof(counters), 0);
  m_lastOverflowPoint = counters.overflowPoint;
  m_lastOverflowSpot = counters.overflowSpot;
  m_lastOverflowArea = counters.overflowArea;
  m_lastOverflowLine = counters.overflowLine;
  const uint64_t overflowSum =
      static_cast<uint64_t>(m_lastOverflowPoint) +
      static_cast<uint64_t>(m_lastOverflowSpot) +
      static_cast<uint64_t>(m_lastOverflowArea) +
      static_cast<uint64_t>(m_lastOverflowLine);
  m_lastOverflowSum = static_cast<uint32_t>(
      std::min<uint64_t>(overflowSum, std::numeric_limits<uint32_t>::max()));
  m_lastWrittenIndexCount =
      std::min(counters.writeCursor, core::kMaxTotalClusteredLights);
  return true;
}

bool ClusteredLightingState::ReadBackClusterLightIndices() {
  if (m_clusterLightIndexBuffer.GetId() == 0 || m_lastWrittenIndexCount == 0u) {
    m_clusterLightIndicesReadback.clear();
    return true;
  }

  m_clusterLightIndicesReadback.assign(
      static_cast<size_t>(m_lastWrittenIndexCount), {});
  m_clusterLightIndexBuffer.Read(
      m_clusterLightIndicesReadback.data(),
      static_cast<size_t>(m_clusterLightIndicesReadback.size()) *
          sizeof(components::GPUClusterLightIndex),
      0);
  return true;
}

void ClusteredLightingState::Shutdown() {
  m_clusterHeaderBuffer.Release();
  m_clusterLightIndexBuffer.Release();
  m_clusterPackedLightBuffer.Release();
  m_lightBoundsBuffer.Release();
  m_clusterCounterBuffer.Release();
  m_clusterHeadersScratch.clear();
  m_clusterHeadersReadback.clear();
  m_clusterLightIndicesReadback.clear();
  m_frameIndex = 0;
  m_tileSize = core::kDefaultClusterTileSize;
  m_zSliceCount = core::kDefaultClusterZSliceCount;
  m_uploadedLightBoundsCount = 0;
  m_lastOverflowSum = 0;
  m_lastOverflowPoint = 0;
  m_lastOverflowSpot = 0;
  m_lastOverflowArea = 0;
  m_lastOverflowLine = 0;
  m_lastWrittenIndexCount = 0;
  m_grid = {};
}

} // namespace NoMoreDay::render::lighting
