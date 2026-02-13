#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"

#include <cstddef>

using namespace NoMoreDay;

TEST_CASE("[Unit] TrailRenderer - ABI Layout") {
  CHECK(sizeof(components::GPUTrailPoint) == 32);
  CHECK(offsetof(components::GPUTrailPoint, posX) == 0);
  CHECK(offsetof(components::GPUTrailPoint, width) == 16);
  CHECK(offsetof(components::GPUTrailPoint, colorPacked) == 24);

  CHECK(sizeof(components::GPUTrailHeader) == 32);
  CHECK(offsetof(components::GPUTrailHeader, headIndex) == 0);
  CHECK(offsetof(components::GPUTrailHeader, maxLifetime) == 12);
  CHECK(offsetof(components::GPUTrailHeader, colorEnd) == 28);
}

TEST_CASE("[Unit] TrailRenderer - Allocation And Recycle") {
  auto &renderer = render::GPUTrailRenderer::Get();
  renderer.Shutdown();
  renderer.Init(2, 8);

  components::GPUTrailHeader config = {};
  config.maxPoints = 8;
  config.maxLifetime = 0.4f;
  config.widthStart = 6.0f;
  config.widthEnd = 1.0f;

  const int id0 = renderer.AllocateTrail(config);
  const int id1 = renderer.AllocateTrail(config);
  const int id2 = renderer.AllocateTrail(config);

  CHECK(id0 >= 0);
  CHECK(id1 >= 0);
  CHECK(id2 == -1);
  CHECK(renderer.GetActiveTrailCount() == 2);

  renderer.FreeTrail(id0);
  CHECK(renderer.GetActiveTrailCount() == 1);

  const int recycled = renderer.AllocateTrail(config);
  CHECK(recycled == id0);
  CHECK(renderer.GetActiveTrailCount() == 2);

  renderer.Shutdown();
}

TEST_CASE("[Unit] TrailRenderer - AppendPoint Ring Buffer Smoke") {
  auto &renderer = render::GPUTrailRenderer::Get();
  renderer.Shutdown();
  renderer.Init(1, 4);

  components::GPUTrailHeader config = {};
  config.maxPoints = 4;
  config.maxLifetime = 0.2f;
  const int trailId = renderer.AllocateTrail(config);
  REQUIRE(trailId >= 0);

  for (int i = 0; i < 12; ++i) {
    renderer.AppendPoint(trailId, Vector2{float(i), 0.0f}, Vector2{1.0f, 0.0f},
                         4.0f, 0xFFFFFFFFu);
  }

  renderer.Update(0.016f);
  CHECK(renderer.GetActiveTrailCount() == 1);
  renderer.Shutdown();
}
