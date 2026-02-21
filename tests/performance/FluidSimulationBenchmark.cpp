#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/fluid/SPHReference.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<NoMoreDay::render::fluid::reference::ParticleState>
BuildParticles(const uint32_t count) {
  using NoMoreDay::render::fluid::reference::ParticleState;
  std::vector<ParticleState> particles = {};
  particles.reserve(count);

  const uint32_t columns =
      std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(std::sqrt(count))));
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t row = i / columns;
    const uint32_t col = i % columns;
    ParticleState p = {};
    p.position = {static_cast<float>(col) * 9.0f + static_cast<float>((i * 7u) % 5u) * 0.17f,
                  static_cast<float>(row) * 9.0f + static_cast<float>((i * 13u) % 7u) * 0.11f};
    p.velocity = {0.0f, 0.0f};
    particles.push_back(p);
  }
  return particles;
}

} // namespace

TEST_CASE("[Performance] FluidSimulation - SPH reference 10K exploration baseline") {
  using namespace NoMoreDay::render::fluid::reference;
  using NoMoreDay::tests::ScopedTimer;

  constexpr uint32_t kParticleCount = 10000u;
  constexpr uint32_t kFrames = 5u;

  SimulationConfig config = {};
  config.smoothingRadius = 18.0f;
  config.restDensity = 1.0f;
  config.stiffness = 20.0f;
  config.viscosity = 0.15f;

  auto particles = BuildParticles(kParticleCount);
  std::vector<ParticleState> output = {};
  std::vector<double> samples = {};
  samples.reserve(kFrames);

  uint64_t totalNeighbors = 0u;
  for (uint32_t frame = 0u; frame < kFrames; ++frame) {
    ScopedTimer timer(samples);
    const NeighborTable neighbors =
        BuildNeighborTableHashed(particles, config.smoothingRadius);
    ComputeDensityPressure(particles, neighbors, config, output);
    for (const uint32_t c : neighbors.counts) {
      totalNeighbors += c;
    }
  }

  const auto stats = NoMoreDay::tests::CalculateStats(samples);
  const double targetMs = 0.80;
  const bool hitTarget = stats.mean_ms <= targetMs;

  CHECK(stats.mean_ms > 0.0);
  CHECK(stats.p99_ms > 0.0);
  CHECK(totalNeighbors > 0u);

  std::cout << "RELEASE_GATE_METRIC fluid_reference_10k_mean_ms=" << stats.mean_ms
            << "\n";
  std::cout << "RELEASE_GATE_METRIC fluid_reference_10k_p99_ms=" << stats.p99_ms
            << "\n";
  std::cout << "RELEASE_GATE_METRIC fluid_reference_10k_target_hit="
            << (hitTarget ? 1 : 0) << "\n";
}
