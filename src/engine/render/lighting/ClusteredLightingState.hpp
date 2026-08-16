#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/core/RenderConstants.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay::render::lighting {

class ClusteredLightingState {
public:
  struct ClusterGridDimensions {
    uint32_t tilesX = 0;
    uint32_t tilesY = 0;
    uint32_t slicesZ = 0;
    uint32_t clusterCount = 0;
  };

  static ClusteredLightingState &Get();

  static ClusterGridDimensions
  ComputeClusterGridDimensions(uint32_t screenWidth, uint32_t screenHeight,
                               uint32_t tileSize, uint32_t zSliceCount) noexcept;
  static uint32_t BuildClusterIndex(uint32_t tileX, uint32_t tileY, uint32_t sliceZ,
                                    uint32_t tilesX, uint32_t tilesY) noexcept;
  static bool DecodeClusterIndex(uint32_t clusterIndex, uint32_t tilesX,
                                 uint32_t tilesY, uint32_t slicesZ,
                                 uint32_t &outTileX, uint32_t &outTileY,
                                 uint32_t &outSliceZ) noexcept;

  static int32_t MapWorldYToRenderLayer(float worldY,
                                        float layerBandWorldUnits) noexcept;
  static uint32_t MapRenderLayerToZSlice(int32_t renderLayer,
                                         uint32_t zSliceCount) noexcept;

  bool BeginFrame(uint32_t frameIndex, uint32_t screenWidth, uint32_t screenHeight,
                  uint32_t tileSize, uint32_t zSliceCount,
                  uint32_t lightCountEstimate);

  // Force allocation of the cluster SSBOs at the given grid capacity without
  // touching any per-frame state. Used by RenderSystem to pre-allocate the
  // backing before the per-frame imported-backing snapshot is captured, so the
  // graph never observes zero handles on the first offscreen-HDR frame.
  bool EnsureBuffersAllocated(uint32_t clusterCount, uint32_t maxLightBounds) {
    return EnsureBufferCapacity(clusterCount, maxLightBounds);
  }

  bool UploadLightBounds(std::span<const components::GPULightBounds> lightBounds);
  bool ReadBackClusterHeaders();
  bool ReadBackClusterLightIndices();
  void Shutdown();

  [[nodiscard]] uint32_t GetFrameIndex() const noexcept { return m_frameIndex; }
  [[nodiscard]] uint32_t GetClusterHeaderBufferId() const noexcept {
    return m_clusterHeaderBuffer.GetId();
  }
  [[nodiscard]] uint32_t GetClusterLightIndexBufferId() const noexcept {
    return m_clusterLightIndexBuffer.GetId();
  }
  [[nodiscard]] uint32_t GetClusterPackedLightBufferId() const noexcept {
    return m_clusterPackedLightBuffer.GetId();
  }
  [[nodiscard]] uint32_t GetLightBoundsBufferId() const noexcept {
    return m_lightBoundsBuffer.GetId();
  }
  [[nodiscard]] uint32_t GetCounterBufferId() const noexcept {
    return m_clusterCounterBuffer.GetId();
  }
  [[nodiscard]] uint32_t GetUploadedLightBoundsCount() const noexcept {
    return m_uploadedLightBoundsCount;
  }
  /// Snapshot delayed by >= 1 frame in production; same-frame when readbackEnabledForTesting is true.
  [[nodiscard]] uint32_t GetLastOverflowSum() const noexcept {
    return m_lastOverflowSum;
  }
  [[nodiscard]] uint32_t GetLastOverflowPoint() const noexcept {
    return m_lastOverflowPoint;
  }
  [[nodiscard]] uint32_t GetLastOverflowSpot() const noexcept {
    return m_lastOverflowSpot;
  }
  [[nodiscard]] uint32_t GetLastOverflowArea() const noexcept {
    return m_lastOverflowArea;
  }
  [[nodiscard]] uint32_t GetLastOverflowLine() const noexcept {
    return m_lastOverflowLine;
  }
  [[nodiscard]] uint32_t GetLastWrittenIndexCount() const noexcept {
    return m_lastWrittenIndexCount;
  }
  [[nodiscard]] uint32_t GetTileSize() const noexcept { return m_tileSize; }
  [[nodiscard]] uint32_t GetZSliceCount() const noexcept { return m_zSliceCount; }
  [[nodiscard]] const ClusterGridDimensions &GetGrid() const noexcept {
    return m_grid;
  }
  [[nodiscard]] const std::vector<components::GPUClusterHeader> &
  GetClusterHeadersReadback() const noexcept {
    return m_clusterHeadersReadback;
  }
  [[nodiscard]] const std::vector<components::GPUClusterLightIndex> &
  GetClusterLightIndicesReadback() const noexcept {
    return m_clusterLightIndicesReadback;
  }

  static constexpr float kDefaultLayerBandWorldUnits = 128.0f;
  static constexpr int32_t kRenderLayerMin = -32;
  static constexpr int32_t kRenderLayerMax = 31;

private:
  bool EnsureBufferCapacity(uint32_t clusterCount, uint32_t maxLightBounds);
  static uint32_t SafeMulU32(uint32_t lhs, uint32_t rhs) noexcept;

  uint32_t m_frameIndex = 0;
  uint32_t m_tileSize = core::kDefaultClusterTileSize;
  uint32_t m_zSliceCount = core::kDefaultClusterZSliceCount;
  uint32_t m_uploadedLightBoundsCount = 0;
  uint32_t m_lastOverflowSum = 0;
  uint32_t m_lastOverflowPoint = 0;
  uint32_t m_lastOverflowSpot = 0;
  uint32_t m_lastOverflowArea = 0;
  uint32_t m_lastOverflowLine = 0;
  uint32_t m_lastWrittenIndexCount = 0;
  ClusterGridDimensions m_grid = {};

  NoMoreDay::core::ComputeBuffer m_clusterHeaderBuffer;
  NoMoreDay::core::ComputeBuffer m_clusterLightIndexBuffer;
  NoMoreDay::core::ComputeBuffer m_clusterPackedLightBuffer;
  NoMoreDay::core::ComputeBuffer m_lightBoundsBuffer;
  NoMoreDay::core::ComputeBuffer m_clusterCounterBuffer;

  std::vector<components::GPUClusterHeader> m_clusterHeadersScratch;
  std::vector<components::GPUClusterHeader> m_clusterHeadersReadback;
  std::vector<components::GPUClusterLightIndex> m_clusterLightIndicesReadback;
};

} // namespace NoMoreDay::render::lighting
