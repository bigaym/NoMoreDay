#include "doctest.h"

#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"

TEST_CASE("[Unit] ClusteredLightingState - Cluster index encode/decode") {
  using NoMoreDay::render::lighting::ClusteredLightingState;

  const auto grid =
      ClusteredLightingState::ComputeClusterGridDimensions(1920, 1080, 32, 4);
  REQUIRE(grid.tilesX == 60);
  REQUIRE(grid.tilesY == 34);
  REQUIRE(grid.slicesZ == 4);
  REQUIRE(grid.clusterCount == 8160);

  const uint32_t index =
      ClusteredLightingState::BuildClusterIndex(7, 5, 2, grid.tilesX, grid.tilesY);
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
  CHECK(ClusteredLightingState::DecodeClusterIndex(
      index, grid.tilesX, grid.tilesY, grid.slicesZ, x, y, z));
  CHECK(x == 7);
  CHECK(y == 5);
  CHECK(z == 2);
}

TEST_CASE("[Unit] ClusteredLightingState - Render layer to z-slice boundaries") {
  using NoMoreDay::render::lighting::ClusteredLightingState;

  constexpr uint32_t kSlices = 4;
  CHECK(ClusteredLightingState::MapRenderLayerToZSlice(-999, kSlices) == 0u);
  CHECK(ClusteredLightingState::MapRenderLayerToZSlice(
            ClusteredLightingState::kRenderLayerMin, kSlices) == 0u);
  CHECK(ClusteredLightingState::MapRenderLayerToZSlice(0, kSlices) == 2u);
  CHECK(ClusteredLightingState::MapRenderLayerToZSlice(
            ClusteredLightingState::kRenderLayerMax, kSlices) == 3u);
  CHECK(ClusteredLightingState::MapRenderLayerToZSlice(999, kSlices) == 3u);
}

TEST_CASE("[Unit] ClusteredLightingState - WorldY mapping boundaries") {
  using NoMoreDay::render::lighting::ClusteredLightingState;

  CHECK(ClusteredLightingState::MapWorldYToRenderLayer(
            -100000.0f, ClusteredLightingState::kDefaultLayerBandWorldUnits) ==
        ClusteredLightingState::kRenderLayerMin);
  CHECK(ClusteredLightingState::MapWorldYToRenderLayer(
            100000.0f, ClusteredLightingState::kDefaultLayerBandWorldUnits) ==
        ClusteredLightingState::kRenderLayerMax);
  CHECK(ClusteredLightingState::MapWorldYToRenderLayer(
            0.0f, ClusteredLightingState::kDefaultLayerBandWorldUnits) == 0);
}

TEST_CASE("[Unit] ClusteredLighting - Constants contract") {
  using namespace NoMoreDay::render::core;

  CHECK(kDefaultClusterTileSize == 32u);
  CHECK(kDefaultClusterZSliceCount == 4u);
  CHECK(kMaxLightsPerCluster == 64u);
  CHECK(kMaxTotalClusteredLights == 4096u);
}

