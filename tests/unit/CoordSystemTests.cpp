#include "doctest.h"

#include "engine/render/CoordSystem.hpp"

#include <cmath>

namespace NoMoreDay::render::coord {
namespace {

bool NearlyEqual(float a, float b, float eps = 0.01f) {
  return std::fabs(a - b) <= eps;
}

TEST_CASE("[Unit] CoordSystem - world/screen round-trip") {
  Camera2DTransform cam;
  cam.target = {120.0f, -45.0f};
  cam.offset = {320.0f, 240.0f};
  cam.zoom = 1.5f;

  const Vector2 world{100.0f, 50.0f};
  const Vector2 screen = WorldToScenePixel(cam, world);
  const Vector2 back = ScenePixelToWorld(cam, screen);

  CHECK(NearlyEqual(back.x, world.x));
  CHECK(NearlyEqual(back.y, world.y));

  CHECK(NearlyEqual(screen.x, (world.x - cam.target.x) * cam.zoom + cam.offset.x));
  CHECK(NearlyEqual(screen.y, (world.y - cam.target.y) * cam.zoom + cam.offset.y));
}

TEST_CASE("[Unit] CoordSystem - NativeYToGl flips once and only once") {
  CHECK(NativeYToGl(0.0f, 720.0f) == doctest::Approx(720.0f));
  CHECK(NativeYToGl(720.0f, 720.0f) == doctest::Approx(0.0f));
  CHECK(NativeYToGl(360.0f, 720.0f) == doctest::Approx(360.0f));

  const float y = 123.0f;
  CHECK(NativeYToGl(NativeYToGl(y, 720.0f), 720.0f) == doctest::Approx(y));
}

TEST_CASE("[Unit] CoordSystem - Build2DMvp matches legacy y-down ortho") {
  Camera2D cam{};
  cam.target = {100.0f, 200.0f};
  cam.offset = {40.0f, 60.0f};
  cam.rotation = 0.0f;
  cam.zoom = 2.0f;

  const float w = 1280.0f;
  const float h = 720.0f;
  const Matrix legacy = MatrixMultiply(
      GetCameraMatrix2D(cam),
      MatrixOrtho(0.0f, w, h, 0.0f, -1.0f, 1.0f));
  const Matrix unified =
      Build2DMvp(Camera2DTransform::From(cam), w, h);

  CHECK(legacy.m0 == doctest::Approx(unified.m0));
  CHECK(legacy.m1 == doctest::Approx(unified.m1));
  CHECK(legacy.m2 == doctest::Approx(unified.m2));
  CHECK(legacy.m3 == doctest::Approx(unified.m3));
  CHECK(legacy.m4 == doctest::Approx(unified.m4));
  CHECK(legacy.m5 == doctest::Approx(unified.m5));
  CHECK(legacy.m6 == doctest::Approx(unified.m6));
  CHECK(legacy.m7 == doctest::Approx(unified.m7));
  CHECK(legacy.m8 == doctest::Approx(unified.m8));
  CHECK(legacy.m9 == doctest::Approx(unified.m9));
  CHECK(legacy.m10 == doctest::Approx(unified.m10));
  CHECK(legacy.m11 == doctest::Approx(unified.m11));
  CHECK(legacy.m12 == doctest::Approx(unified.m12));
  CHECK(legacy.m13 == doctest::Approx(unified.m13));
  CHECK(legacy.m14 == doctest::Approx(unified.m14));
  CHECK(legacy.m15 == doctest::Approx(unified.m15));
}

} // namespace

TEST_CASE("[Unit] CoordSystem - MSDF bearing maps through the single helper") {
  // Synthetic values from tests/unit/LootTextBatcherTests.cpp: emSize 29.078125,
  // fontSize 2*emSize => scale=2, bearingBottom -0.12 => -0.24.
  const float em = 29.078125f;
  const float fontSize = 2.0f * em;
  CHECK(MsdfBearingToWorldOffset(-0.12f, em, fontSize) == doctest::Approx(-0.24f));
  CHECK(MsdfBearingToWorldOffset(0.05f, em, fontSize) == doctest::Approx(0.10f));
}

} // namespace NoMoreDay::render::coord
