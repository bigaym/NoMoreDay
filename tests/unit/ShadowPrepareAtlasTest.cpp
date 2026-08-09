#include "doctest.h"

#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/shadow/ShadowAtlasAllocator.hpp"

#include <vector>

namespace {

NoMoreDay::components::GPULight MakeLight(const float x, const float y,
                                          const float radius) {
  NoMoreDay::components::GPULight light = {};
  light.posX = x;
  light.posY = y;
  light.radius = radius;
  light.intensity = 1.0f;
  return light;
}

Camera2D MakeCamera() {
  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;
  return camera;
}

} // namespace

TEST_CASE("[Unit] Shadow Prepare - Top N ranking honors priority and cap") {
  using NoMoreDay::render::passes::ShadowPrepareLightInput;
  using NoMoreDay::render::passes::ShadowPreparePass;

  const Camera2D camera = MakeCamera();
  const std::vector<ShadowPrepareLightInput> inputs = {
      {.sourceIndex = 0u, .priority = 32u, .gpuLight = MakeLight(0.0f, 0.0f, 20.0f)},
      {.sourceIndex = 1u, .priority = 220u, .gpuLight = MakeLight(0.0f, 0.0f, 20.0f)},
      {.sourceIndex = 2u, .priority = 128u, .gpuLight = MakeLight(0.0f, 0.0f, 20.0f)},
  };

  const auto ranked =
      ShadowPreparePass::RankTopNForAtlas(inputs, camera, 200.0f, 200.0f, 2u);
  REQUIRE(ranked.size() == 2u);
  CHECK(ranked[0].lightIndex == 1u);
  CHECK(ranked[1].lightIndex == 2u);
  CHECK(ranked[0].compositeScore >= ranked[1].compositeScore);
}

TEST_CASE("[Unit] Shadow Prepare - Ranking order is deterministic for ties") {
  using NoMoreDay::render::passes::ShadowPrepareLightInput;
  using NoMoreDay::render::passes::ShadowPreparePass;

  const Camera2D camera = MakeCamera();
  const std::vector<ShadowPrepareLightInput> inputs = {
      {.sourceIndex = 0u, .priority = 100u, .gpuLight = MakeLight(10.0f, 5.0f, 12.0f)},
      {.sourceIndex = 1u, .priority = 100u, .gpuLight = MakeLight(10.0f, 5.0f, 12.0f)},
      {.sourceIndex = 2u, .priority = 100u, .gpuLight = MakeLight(10.0f, 5.0f, 12.0f)},
  };

  const auto rankedA =
      ShadowPreparePass::RankTopNForAtlas(inputs, camera, 320.0f, 180.0f, 3u);
  const auto rankedB =
      ShadowPreparePass::RankTopNForAtlas(inputs, camera, 320.0f, 180.0f, 3u);

  REQUIRE(rankedA.size() == 3u);
  REQUIRE(rankedB.size() == 3u);
  for (size_t i = 0; i < rankedA.size(); ++i) {
    CHECK(rankedA[i].lightId == rankedB[i].lightId);
    CHECK(rankedA[i].lightIndex == rankedB[i].lightIndex);
    CHECK(rankedA[i].compositeScore == doctest::Approx(rankedB[i].compositeScore));
  }
}

TEST_CASE("[Unit] Shadow Atlas - Eviction prefers lower priority then older frame") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(2u, 0u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 100u, .priorityScore = 2.0f}).success);

  allocator.BeginFrame(2u);
  REQUIRE(allocator.AcquireTile({.lightId = 200u, .priorityScore = 1.0f}).success);

  allocator.BeginFrame(3u);
  const auto promoted =
      allocator.AcquireTile({.lightId = 300u, .priorityScore = 3.0f});
  REQUIRE(promoted.success);
  CHECK(promoted.evicted);
  CHECK(promoted.evictedLightId == 200u);
}

TEST_CASE("[Unit] Shadow Atlas - Tie break eviction is deterministic by light id") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(2u, 0u);
  allocator.BeginFrame(7u);
  REQUIRE(allocator.AcquireTile({.lightId = 40u, .priorityScore = 1.0f}).success);
  REQUIRE(allocator.AcquireTile({.lightId = 30u, .priorityScore = 1.0f}).success);

  allocator.BeginFrame(8u);
  const auto allocation =
      allocator.AcquireTile({.lightId = 99u, .priorityScore = 2.0f});
  REQUIRE(allocation.success);
  CHECK(allocation.evicted);
  CHECK(allocation.evictedLightId == 30u);
}

TEST_CASE("[Unit] Shadow Atlas - Hysteresis produces predictable overflow attempts") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(1u, 2u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 1u, .priorityScore = 1.0f}).success);

  uint32_t overflowCount = 0u;
  for (uint32_t frame = 2u; frame <= 3u; ++frame) {
    allocator.BeginFrame(frame);
    const auto allocation =
        allocator.AcquireTile({.lightId = 2u, .priorityScore = 2.0f});
    if (!allocation.success) {
      ++overflowCount;
    }
  }

  allocator.BeginFrame(4u);
  const auto finalAllocation =
      allocator.AcquireTile({.lightId = 2u, .priorityScore = 2.0f});
  CHECK(finalAllocation.success);
  CHECK(finalAllocation.evicted);
  CHECK(finalAllocation.evictedLightId == 1u);
  CHECK(overflowCount == 2u);
}

TEST_CASE("[Unit] Shadow Atlas - Sweep releases tiles not reused within retention") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(2u, 0u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 10u, .priorityScore = 2.0f}).success);
  REQUIRE(allocator.AcquireTile({.lightId = 20u, .priorityScore = 2.0f}).success);
  REQUIRE(allocator.GetAllocatedTileCount() == 2u);

  allocator.BeginFrame(2u);
  REQUIRE(allocator.AcquireTile({.lightId = 10u, .priorityScore = 2.0f}).success);

  allocator.BeginFrame(6u);
  allocator.SweepStaleTiles(6u, 5u);
  CHECK(allocator.GetAllocatedTileCount() == 1u);

  const auto reacquire =
      allocator.AcquireTile({.lightId = 20u, .priorityScore = 2.0f});
  REQUIRE(reacquire.success);
  CHECK_FALSE(reacquire.evicted);
  CHECK_FALSE(reacquire.reusedExisting);
  CHECK(allocator.GetAllocatedTileCount() == 2u);
}

TEST_CASE("[Unit] Shadow Atlas - Sweep is a no-op with zero retention") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(1u, 0u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 1u, .priorityScore = 1.0f}).success);

  allocator.BeginFrame(100u);
  allocator.SweepStaleTiles(100u, 0u);
  CHECK(allocator.GetAllocatedTileCount() == 1u);
}

TEST_CASE("[Unit] Shadow Atlas - Equal priority evicts lowest with sweep-friendly retry") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(1u, 2u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 1u, .priorityScore = 1.0f}).success);

  allocator.BeginFrame(2u);
  const auto first = allocator.AcquireTile({.lightId = 2u, .priorityScore = 1.0f});
  CHECK_FALSE(first.success);

  allocator.BeginFrame(3u);
  const auto second = allocator.AcquireTile({.lightId = 2u, .priorityScore = 1.0f});
  CHECK_FALSE(second.success);

  allocator.BeginFrame(4u);
  const auto third = allocator.AcquireTile({.lightId = 2u, .priorityScore = 1.0f});
  REQUIRE(third.success);
  CHECK(third.evicted);
  CHECK(third.evictedLightId == 1u);
}

TEST_CASE("[Unit] Shadow Atlas - Stale victims evict immediately without hysteresis") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(1u, 10u);
  allocator.BeginFrame(1u);
  REQUIRE(allocator.AcquireTile({.lightId = 1u, .priorityScore = 1.0f}).success);

  for (uint32_t frame = 2u; frame <= 10u; ++frame) {
    allocator.BeginFrame(frame);
  }

  allocator.BeginFrame(11u);
  const auto allocation =
      allocator.AcquireTile({.lightId = 2u, .priorityScore = 1.0f});
  REQUIRE(allocation.success);
  CHECK(allocation.evicted);
  CHECK(allocation.evictedLightId == 1u);
}
