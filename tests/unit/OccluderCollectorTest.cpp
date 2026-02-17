#include "doctest.h"

#include "engine/render/shadow/OccluderCollector.hpp"

#include <algorithm>

namespace {

using NoMoreDay::render::shadow::OccluderCollector;
using NoMoreDay::render::shadow::OccluderCollectorConfig;
using NoMoreDay::render::shadow::OccluderEntry;
using NoMoreDay::render::shadow::ShadowChunkCoord;

bool ContainsCasterAt(const std::vector<NoMoreDay::components::GPUShadowCaster> &casters,
                      const float x, const float y) {
  return std::any_of(casters.begin(), casters.end(),
                     [x, y](const NoMoreDay::components::GPUShadowCaster &caster) {
                       return caster.posX == x && caster.posY == y;
                     });
}

} // namespace

TEST_CASE("[Unit] OccluderCollector - World chunk mapping is deterministic") {
  const ShadowChunkCoord c0 = OccluderCollector::WorldToChunk(0.0f, 0.0f, 16.0f);
  const ShadowChunkCoord c1 = OccluderCollector::WorldToChunk(15.9f, 0.0f, 16.0f);
  const ShadowChunkCoord c2 = OccluderCollector::WorldToChunk(16.0f, 16.0f, 16.0f);
  const ShadowChunkCoord c3 = OccluderCollector::WorldToChunk(-0.1f, -16.1f, 16.0f);

  CHECK(c0.x == 0);
  CHECK(c0.y == 0);
  CHECK(c1.x == 0);
  CHECK(c1.y == 0);
  CHECK(c2.x == 1);
  CHECK(c2.y == 1);
  CHECK(c3.x == -1);
  CHECK(c3.y == -2);
}

TEST_CASE("[Unit] OccluderCollector - Static chunk cache hit and LRU eviction") {
  OccluderCollector collector({
      .chunkSize = 10.0f,
      .cameraNeighborhoodRadius = 4.0f,
      .maxShadowCasters = 64u,
      .maxCachedStaticChunks = 1u,
  });

  CHECK(collector.UpsertOccluder(
      {.occluderId = 1u, .posX = 2.0f, .posY = 2.0f, .radius = 1.0f, .occluderHeight = 1.0f}));
  CHECK(collector.UpsertOccluder(
      {.occluderId = 2u, .posX = 24.0f, .posY = 2.0f, .radius = 1.0f, .occluderHeight = 1.0f}));

  collector.BeginFrame(1);
  const auto first = collector.CollectVisible(2.0f, 2.0f);
  CHECK(first.staticCasterCount == 1u);
  CHECK(collector.GetCachedChunkCount() == 1u);
  CHECK(collector.GetChunkUploadCount({.x = 0, .y = 0}) == 1u);

  collector.BeginFrame(2);
  const auto second = collector.CollectVisible(2.5f, 2.0f);
  CHECK(second.staticCasterCount == 1u);
  CHECK(collector.GetChunkUploadCount({.x = 0, .y = 0}) == 1u);

  collector.BeginFrame(3);
  const auto third = collector.CollectVisible(24.0f, 2.0f);
  CHECK(third.staticCasterCount == 1u);
  CHECK(third.evictedChunkCount == 1u);
  CHECK(!collector.HasChunkCached({.x = 0, .y = 0}));
  CHECK(collector.HasChunkCached({.x = 2, .y = 0}));
}

TEST_CASE("[Unit] OccluderCollector - Dynamic neighborhood filtering") {
  OccluderCollector collector({
      .chunkSize = 10.0f,
      .cameraNeighborhoodRadius = 10.0f,
      .maxShadowCasters = 64u,
      .maxCachedStaticChunks = 8u,
  });

  CHECK(collector.UpsertOccluder({.occluderId = 11u,
                                  .posX = 3.0f,
                                  .posY = 4.0f,
                                  .radius = 1.0f,
                                  .occluderHeight = 2.0f,
                                  .shapeIndex = 1u,
                                  .dynamicFlag = 1u}));
  CHECK(collector.UpsertOccluder({.occluderId = 12u,
                                  .posX = 100.0f,
                                  .posY = 0.0f,
                                  .radius = 1.0f,
                                  .occluderHeight = 2.0f,
                                  .shapeIndex = 2u,
                                  .dynamicFlag = 1u}));

  collector.BeginFrame(1);
  const auto result = collector.CollectVisible(0.0f, 0.0f);
  CHECK(result.staticCasterCount == 0u);
  CHECK(result.dynamicCasterCount == 1u);
  CHECK(result.totalCasterCount == 1u);
  CHECK(ContainsCasterAt(collector.GetStagingCasters(), 3.0f, 4.0f));
  CHECK(!ContainsCasterAt(collector.GetStagingCasters(), 100.0f, 0.0f));
}

TEST_CASE("[Unit] OccluderCollector - Staging consistency and upload batches") {
  OccluderCollector collector({
      .chunkSize = 10.0f,
      .cameraNeighborhoodRadius = 10.0f,
      .maxShadowCasters = 3u,
      .maxCachedStaticChunks = 8u,
  });

  CHECK(collector.UpsertOccluder({.occluderId = 21u,
                                  .posX = 1.0f,
                                  .posY = 1.0f,
                                  .radius = 2.0f,
                                  .occluderHeight = 3.0f,
                                  .shapeIndex = 7u,
                                  .dynamicFlag = 0u}));
  CHECK(collector.UpsertOccluder({.occluderId = 22u,
                                  .posX = 2.0f,
                                  .posY = 2.0f,
                                  .radius = 1.5f,
                                  .occluderHeight = 1.0f,
                                  .shapeIndex = 9u,
                                  .dynamicFlag = 1u}));
  CHECK(collector.UpsertOccluder({.occluderId = 23u,
                                  .posX = 3.0f,
                                  .posY = 3.0f,
                                  .radius = 1.0f,
                                  .occluderHeight = 1.0f,
                                  .shapeIndex = 10u,
                                  .dynamicFlag = 1u}));
  CHECK(collector.UpsertOccluder({.occluderId = 24u,
                                  .posX = 4.0f,
                                  .posY = 4.0f,
                                  .radius = 1.0f,
                                  .occluderHeight = 1.0f,
                                  .shapeIndex = 11u,
                                  .dynamicFlag = 1u}));

  collector.BeginFrame(42);
  const auto result = collector.CollectVisible(0.0f, 0.0f);
  CHECK(result.totalCasterCount == 3u);
  CHECK(result.truncated);

  const auto &staging = collector.GetStagingCasters();
  REQUIRE(staging.size() == 3u);
  CHECK(staging[0].posX == 1.0f);
  CHECK(staging[0].shapeIndex == 7u);
  CHECK(staging[0].dynamicFlag == 0u);

  const auto batches = collector.BuildUploadBatches(2u);
  REQUIRE(batches.size() == 2u);
  CHECK(batches[0].offset == 0u);
  CHECK(batches[0].count == 2u);
  CHECK(batches[1].offset == 2u);
  CHECK(batches[1].count == 1u);
}
