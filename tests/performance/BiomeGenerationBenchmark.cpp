#include "doctest.h"
#include "game/components/Common.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/BiomeMapGenerator.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
std::filesystem::path ResolveBiomeJsonPathForBenchmark() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }

  throw std::runtime_error("Unable to locate assets/data/biomes.json from test cwd");
}
} // namespace

TEST_CASE("[Performance] BiomeMapGenerator - 256x256 Generation Timing") {
  using namespace NoMoreDay;
  BiomeRegistry::Get().LoadFromJSON(ResolveBiomeJsonPathForBenchmark().string());
  BiomeMapGenerator generator;

  struct BenchmarkCase {
    const char *id;
    const char *label;
  };
  constexpr std::array<BenchmarkCase, 3> kCases = {{
      {"sun_prairie", "Open"},
      {"gloom_spire", "Maze"},
      {"floating_isle", "Special"},
  }};

  constexpr int kRuns = 20;
  for (const auto &entry : kCases) {
    const auto &config = BiomeRegistry::Get().GetBiome(entry.id);
    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();
    for (int i = 0; i < kRuns; ++i) {
      (void)generator.GenerateForBiome(256, 256, config, 202600u + static_cast<uint32_t>(i));
    }
    auto end = clock::now();
    const auto totalUs =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const double avgMs = static_cast<double>(totalUs) / static_cast<double>(kRuns) / 1000.0;

    std::printf("[Benchmark] 256x256 %s biome avg generation: %.3f ms (%d runs)\n",
                entry.label, avgMs, kRuns);

    CHECK(avgMs > 0.0);
  }
}
