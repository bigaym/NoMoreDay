#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/shadow/ShadowAtlasAllocator.hpp"
#include "game/components/ShadowCasterComponent.hpp"

#include <cstddef>
#include <type_traits>

TEST_CASE("[Unit] Shadow Foundation - GPU shadow ABI layout snapshot") {
  using namespace NoMoreDay::components;

  CHECK(std::is_standard_layout_v<GPUShadowCaster>);
  CHECK(sizeof(GPUShadowCaster) == 32);
  CHECK(offsetof(GPUShadowCaster, posX) == 0);
  CHECK(offsetof(GPUShadowCaster, shapeIndex) == 16);

  CHECK(std::is_standard_layout_v<GPUShadowLight>);
  CHECK(sizeof(GPUShadowLight) == 48);
  CHECK(offsetof(GPUShadowLight, lightId) == 0);
  CHECK(offsetof(GPUShadowLight, penumbraParams) == 32);

  CHECK(std::is_standard_layout_v<GPUShadowAtlasMeta>);
  CHECK(sizeof(GPUShadowAtlasMeta) == 16);
  CHECK(offsetof(GPUShadowAtlasMeta, tileIndex) == 0);
  CHECK(offsetof(GPUShadowAtlasMeta, priorityScore) == 8);
}

TEST_CASE("[Unit] Shadow Foundation - Shadow caster component defaults") {
  using namespace NoMoreDay;

  const ShadowCasterComponent component{};
  CHECK(component.shape == ShadowOccluderShape::Circle);
  CHECK(component.occluderHeight == doctest::Approx(1.0f));
  CHECK(component.dynamicFlag == 0u);
  CHECK(std::is_standard_layout_v<ShadowCasterComponent>);
  CHECK(std::is_trivially_copyable_v<ShadowCasterComponent>);
}

TEST_CASE("[Unit] Shadow Foundation - Render constants contract") {
  using namespace NoMoreDay::RenderConstants::Shadow;

  CHECK(kMaxShadowCasters > 0u);
  CHECK(kShadowChunkSize > 0.0f);
  CHECK(kCameraNeighborhoodRadius >= kShadowChunkSize);
  CHECK(kAtlasEvictionHysteresis >= 1u);
}

TEST_CASE("[Unit] Shadow Foundation - Atlas allocator basic flow") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;
  using NoMoreDay::render::shadow::ShadowTileRequest;

  ShadowAtlasAllocator allocator(2, 0);
  allocator.BeginFrame(1);
  const auto a0 = allocator.AcquireTile({.lightId = 11u, .priorityScore = 1.0f});
  const auto a1 = allocator.AcquireTile({.lightId = 22u, .priorityScore = 2.0f});
  const auto a0Reuse =
      allocator.AcquireTile({.lightId = 11u, .priorityScore = 1.5f});

  CHECK(a0.success);
  CHECK(a1.success);
  CHECK(a0.tileIndex != a1.tileIndex);
  CHECK(a0Reuse.success);
  CHECK(a0Reuse.reusedExisting);
  CHECK(a0Reuse.tileIndex == a0.tileIndex);
  CHECK(allocator.GetAllocatedTileCount() == 2u);
}

TEST_CASE("[Unit] Shadow Foundation - Atlas eviction hysteresis") {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;
  using NoMoreDay::render::shadow::ShadowTileRequest;

  ShadowAtlasAllocator allocator(1, 2);
  allocator.BeginFrame(1);
  const auto base = allocator.AcquireTile({.lightId = 100u, .priorityScore = 1.0f});
  REQUIRE(base.success);

  allocator.BeginFrame(2);
  const auto firstTry =
      allocator.AcquireTile({.lightId = 200u, .priorityScore = 2.0f});
  CHECK(!firstTry.success);

  allocator.BeginFrame(3);
  const auto secondTry =
      allocator.AcquireTile({.lightId = 200u, .priorityScore = 2.0f});
  CHECK(!secondTry.success);

  allocator.BeginFrame(4);
  const auto thirdTry =
      allocator.AcquireTile({.lightId = 200u, .priorityScore = 2.0f});
  CHECK(thirdTry.success);
  CHECK(thirdTry.evicted);
  CHECK(thirdTry.evictedLightId == 100u);
}
