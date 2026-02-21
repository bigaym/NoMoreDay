#include "doctest.h"

#include "engine/render/fluid/SPHReference.hpp"

#include <algorithm>
#include <vector>

namespace {

std::vector<NoMoreDay::render::fluid::reference::ParticleState>
BuildParticleFixture() {
  using NoMoreDay::render::fluid::reference::ParticleState;
  std::vector<ParticleState> particles = {};
  particles.reserve(256);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      ParticleState p = {};
      p.position = {x * 14.0f + ((x + y) % 3) * 0.35f,
                    y * 14.0f + ((x * 5 + y) % 4) * 0.25f};
      p.velocity = {0.0f, 0.0f};
      particles.push_back(p);
    }
  }
  return particles;
}

} // namespace

TEST_CASE("[Unit] Fluid SPH reference - Neighbor search correctness 1K class") {
  using namespace NoMoreDay::render::fluid::reference;

  auto particles = BuildParticleFixture();
  const float radius = 18.0f;

  const NeighborTable naive = BuildNeighborTableNaive(particles, radius);
  const NeighborTable hashed = BuildNeighborTableHashed(particles, radius);

  REQUIRE(naive.counts.size() == hashed.counts.size());
  for (size_t i = 0; i < naive.counts.size(); ++i) {
    const uint32_t naiveCount = naive.counts[i];
    const uint32_t hashedCount = hashed.counts[i];
    CHECK(naiveCount <= kMaxNeighbors);
    CHECK(hashedCount <= kMaxNeighbors);
    CHECK(naiveCount == hashedCount);

    std::vector<uint32_t> naiveNeighbors(naive.indices[i].begin(),
                                         naive.indices[i].begin() + naiveCount);
    std::vector<uint32_t> hashedNeighbors(hashed.indices[i].begin(),
                                          hashed.indices[i].begin() + hashedCount);
    std::sort(naiveNeighbors.begin(), naiveNeighbors.end());
    std::sort(hashedNeighbors.begin(), hashedNeighbors.end());
    CHECK(naiveNeighbors == hashedNeighbors);
  }
}

TEST_CASE("[Unit] Fluid SPH reference - Density error stays below 1%") {
  using namespace NoMoreDay::render::fluid::reference;

  auto particles = BuildParticleFixture();
  SimulationConfig config = {};
  config.smoothingRadius = 18.0f;
  config.restDensity = 1.0f;
  config.stiffness = 20.0f;
  config.viscosity = 0.15f;

  const NeighborTable naiveNeighbors =
      BuildNeighborTableNaive(particles, config.smoothingRadius);
  const NeighborTable hashedNeighbors =
      BuildNeighborTableHashed(particles, config.smoothingRadius);

  std::vector<ParticleState> naiveOutput = {};
  std::vector<ParticleState> hashedOutput = {};
  ComputeDensityPressure(particles, naiveNeighbors, config, naiveOutput);
  ComputeDensityPressure(particles, hashedNeighbors, config, hashedOutput);

  REQUIRE(naiveOutput.size() == hashedOutput.size());
  for (size_t i = 0; i < naiveOutput.size(); ++i) {
    const float reference = std::max(naiveOutput[i].density, 1e-6f);
    const float error =
        std::abs(naiveOutput[i].density - hashedOutput[i].density) / reference;
    CHECK(error < 0.01f);
  }
}
