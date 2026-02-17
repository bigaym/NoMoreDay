#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/shadow/ShadowSDFMath.hpp"

#include <array>
#include <limits>

TEST_CASE("[Unit] Shadow SDF - Boundary conditions") {
  NoMoreDay::components::GPUShadowCaster caster = {};
  caster.posX = 0.0f;
  caster.posY = 0.0f;
  caster.radius = 10.0f;

  CHECK(NoMoreDay::render::shadow::SignedDistanceToCaster(caster, 10.0f, 0.0f) ==
        doctest::Approx(0.0f));
  CHECK(NoMoreDay::render::shadow::SignedDistanceToCaster(caster, 11.0f, 0.0f) ==
        doctest::Approx(1.0f));
  CHECK(NoMoreDay::render::shadow::SignedDistanceToCaster(caster, 0.0f, 0.0f) ==
        doctest::Approx(-10.0f));
}

TEST_CASE("[Unit] Shadow SDF - Empty scene resolves to full light") {
  const std::array<NoMoreDay::components::GPUShadowCaster, 0> emptyCasters = {};
  const float sdf =
      NoMoreDay::render::shadow::ComputeSceneSDF(emptyCasters, 0.0f, 0.0f);
  CHECK(sdf == std::numeric_limits<float>::max());
  CHECK(NoMoreDay::render::shadow::ResolveShadowFactor(sdf, 1.0f) ==
        doctest::Approx(1.0f));
}

TEST_CASE("[Unit] Shadow SDF - Single occluder scene") {
  std::array<NoMoreDay::components::GPUShadowCaster, 1> casters = {};
  casters[0].posX = 0.0f;
  casters[0].posY = 0.0f;
  casters[0].radius = 5.0f;

  CHECK(NoMoreDay::render::shadow::ComputeShadowFactor(casters, 0.0f, 0.0f, 2.0f) ==
        doctest::Approx(0.0f));
  CHECK(NoMoreDay::render::shadow::ComputeShadowFactor(casters, 7.0f, 0.0f, 2.0f) ==
        doctest::Approx(1.0f));
}

TEST_CASE("[Unit] Shadow SDF - Multi occluder picks nearest caster") {
  std::array<NoMoreDay::components::GPUShadowCaster, 2> casters = {};
  casters[0].posX = -10.0f;
  casters[0].posY = 0.0f;
  casters[0].radius = 2.0f;
  casters[1].posX = 3.0f;
  casters[1].posY = 0.0f;
  casters[1].radius = 4.0f;

  const float sdfAtOrigin =
      NoMoreDay::render::shadow::ComputeSceneSDF(casters, 0.0f, 0.0f);
  CHECK(sdfAtOrigin == doctest::Approx(-1.0f));

  const float factorSoft1 =
      NoMoreDay::render::shadow::ComputeShadowFactor(casters, 8.0f, 0.0f, 1.0f);
  const float factorSoft4 =
      NoMoreDay::render::shadow::ComputeShadowFactor(casters, 8.0f, 0.0f, 4.0f);
  CHECK(factorSoft1 == doctest::Approx(1.0f));
  CHECK(factorSoft4 == doctest::Approx(0.25f));
}
