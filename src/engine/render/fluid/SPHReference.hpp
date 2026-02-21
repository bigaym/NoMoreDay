#pragma once

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace NoMoreDay::render::fluid::reference {

inline constexpr uint32_t kMaxNeighbors = 32u;

struct ParticleState {
  Vector2 position = {0.0f, 0.0f};
  Vector2 velocity = {0.0f, 0.0f};
  float density = 0.0f;
  float pressure = 0.0f;
};

struct SimulationConfig {
  float smoothingRadius = 18.0f;
  float restDensity = 1.0f;
  float stiffness = 20.0f;
  float viscosity = 0.15f;
  Vector2 gravity = {0.0f, -9.8f};
};

struct NeighborTable {
  std::vector<std::array<uint32_t, kMaxNeighbors>> indices = {};
  std::vector<uint32_t> counts = {};
};

inline float LengthSquared(const Vector2 value) noexcept {
  return value.x * value.x + value.y * value.y;
}

inline NeighborTable BuildNeighborTableNaive(
    const std::vector<ParticleState> &particles, const float smoothingRadius) {
  NeighborTable table = {};
  table.indices.resize(particles.size());
  table.counts.resize(particles.size(), 0u);

  const float radiusSq = smoothingRadius * smoothingRadius;
  for (size_t i = 0; i < particles.size(); ++i) {
    uint32_t count = 0u;
    for (size_t j = 0; j < particles.size(); ++j) {
      if (i == j) {
        continue;
      }
      const Vector2 delta = {particles[i].position.x - particles[j].position.x,
                             particles[i].position.y - particles[j].position.y};
      if (LengthSquared(delta) > radiusSq) {
        continue;
      }
      if (count < kMaxNeighbors) {
        table.indices[i][count] = static_cast<uint32_t>(j);
        ++count;
      }
    }
    table.counts[i] = count;
  }
  return table;
}

inline NeighborTable BuildNeighborTableHashed(
    const std::vector<ParticleState> &particles, const float smoothingRadius) {
  NeighborTable table = {};
  table.indices.resize(particles.size());
  table.counts.resize(particles.size(), 0u);

  if (particles.empty() || smoothingRadius <= 0.0f) {
    return table;
  }

  Vector2 minP = particles.front().position;
  Vector2 maxP = particles.front().position;
  for (const auto &particle : particles) {
    minP.x = std::min(minP.x, particle.position.x);
    minP.y = std::min(minP.y, particle.position.y);
    maxP.x = std::max(maxP.x, particle.position.x);
    maxP.y = std::max(maxP.y, particle.position.y);
  }

  const float invCellSize = 1.0f / smoothingRadius;
  const int gridW = std::max(1, static_cast<int>((maxP.x - minP.x) * invCellSize) + 1);
  const int gridH = std::max(1, static_cast<int>((maxP.y - minP.y) * invCellSize) + 1);

  std::vector<uint32_t> hashedCells(particles.size(), 0u);
  std::vector<std::vector<uint32_t>> buckets(static_cast<size_t>(gridW * gridH));
  for (size_t i = 0; i < particles.size(); ++i) {
    const int cx = std::clamp(static_cast<int>((particles[i].position.x - minP.x) * invCellSize),
                              0, gridW - 1);
    const int cy = std::clamp(static_cast<int>((particles[i].position.y - minP.y) * invCellSize),
                              0, gridH - 1);
    const uint32_t cell = static_cast<uint32_t>(cy * gridW + cx);
    hashedCells[i] = cell;
    buckets[cell].push_back(static_cast<uint32_t>(i));
  }

  const float radiusSq = smoothingRadius * smoothingRadius;
  for (size_t i = 0; i < particles.size(); ++i) {
    const uint32_t cell = hashedCells[i];
    const int cx = static_cast<int>(cell % static_cast<uint32_t>(gridW));
    const int cy = static_cast<int>(cell / static_cast<uint32_t>(gridW));

    uint32_t count = 0u;
    for (int oy = -1; oy <= 1; ++oy) {
      for (int ox = -1; ox <= 1; ++ox) {
        const int nx = cx + ox;
        const int ny = cy + oy;
        if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) {
          continue;
        }
        const auto &bucket = buckets[static_cast<size_t>(ny * gridW + nx)];
        for (const uint32_t candidate : bucket) {
          if (candidate == i) {
            continue;
          }
          const Vector2 delta = {particles[i].position.x - particles[candidate].position.x,
                                 particles[i].position.y - particles[candidate].position.y};
          if (LengthSquared(delta) > radiusSq) {
            continue;
          }
          if (count < kMaxNeighbors) {
            table.indices[i][count] = candidate;
            ++count;
          }
        }
      }
    }
    table.counts[i] = count;
  }
  return table;
}

inline void ComputeDensityPressure(const std::vector<ParticleState> &input,
                                   const NeighborTable &neighbors,
                                   const SimulationConfig &config,
                                   std::vector<ParticleState> &output) {
  output = input;
  const float h = std::max(0.0001f, config.smoothingRadius);
  const float poly6Coeff = 4.0f / (3.14159265359f * h * h * h * h * h * h * h * h);
  const float selfContribution = poly6Coeff * h * h * h * h * h * h;

  for (size_t i = 0; i < input.size(); ++i) {
    float density = selfContribution;
    const uint32_t count =
        std::min<uint32_t>(neighbors.counts[i], static_cast<uint32_t>(kMaxNeighbors));
    for (uint32_t n = 0; n < count; ++n) {
      const uint32_t j = neighbors.indices[i][n];
      if (j >= input.size()) {
        continue;
      }
      const Vector2 delta = {input[i].position.x - input[j].position.x,
                             input[i].position.y - input[j].position.y};
      const float r2 = LengthSquared(delta);
      if (r2 >= h * h) {
        continue;
      }
      const float diff = (h * h) - r2;
      density += poly6Coeff * diff * diff * diff;
    }

    output[i].density = density;
    output[i].pressure =
        config.stiffness * std::max(0.0f, density - config.restDensity);
  }
}

} // namespace NoMoreDay::render::fluid::reference

