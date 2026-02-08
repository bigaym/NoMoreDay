#pragma once

#include "game/systems/world/BiomeStrategies.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <memory>

namespace NoMoreDay {

class BiomeMapGenerator final : public ::MapGenerator {
public:
  MapData Generate(int width, int height, uint32_t seed, float wallProb,
                   int iterations) override;

  MapData GenerateForBiome(int width, int height, const BiomeConfig &config,
                           uint32_t seed);

private:
  std::unique_ptr<IBiomeStrategy> CreateStrategy(BiomeStyle style) const;
  void ApplyFeatures(std::vector<Tile> &grid, const BiomeConfig &config) const;
  void EnsureConnectivity(std::vector<Tile> &grid, int width, int height) const;
  void PlaceExits(std::vector<Tile> &grid, int width, int height,
                  uint32_t seed) const;
};

} // namespace NoMoreDay
