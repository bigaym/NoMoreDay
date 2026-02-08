#pragma once

#include "game/data/BiomeRegistry.hpp"
#include <cstdint>
#include <vector>

namespace NoMoreDay {

struct BiomeGenerationParams {
  int width = 0;
  int height = 0;
  const BiomeConfig *config = nullptr;
  uint32_t seed = 0;
};

class IBiomeStrategy {
public:
  virtual ~IBiomeStrategy() = default;

  virtual void GenerateTerrain(std::vector<Tile> &grid,
                               const BiomeGenerationParams &params) = 0;
  virtual void PlaceSpecialStructures(std::vector<Tile> &grid,
                                      const BiomeGenerationParams &params) = 0;
};

class OpenBiomeStrategy final : public IBiomeStrategy {
public:
  void GenerateTerrain(std::vector<Tile> &grid,
                       const BiomeGenerationParams &params) override;
  void PlaceSpecialStructures(std::vector<Tile> &grid,
                              const BiomeGenerationParams &params) override;
};

class MazeBiomeStrategy final : public IBiomeStrategy {
public:
  void GenerateTerrain(std::vector<Tile> &grid,
                       const BiomeGenerationParams &params) override;
  void PlaceSpecialStructures(std::vector<Tile> &grid,
                              const BiomeGenerationParams &params) override;

private:
  void CarveCorridors(std::vector<Tile> &grid, int width, int height,
                      uint32_t seed);
  void ProcessDeadEnds(std::vector<Tile> &grid, int width, int height,
                       uint32_t seed);
};

class SpecialBiomeStrategy final : public IBiomeStrategy {
public:
  void GenerateTerrain(std::vector<Tile> &grid,
                       const BiomeGenerationParams &params) override;
  void PlaceSpecialStructures(std::vector<Tile> &grid,
                              const BiomeGenerationParams &params) override;

private:
  void GenerateFloatingPlatforms(std::vector<Tile> &grid, int width, int height,
                                 uint32_t seed);
  void GenerateBridges(std::vector<Tile> &grid, int width, int height,
                       uint32_t seed);
  void GenerateHolyArena(std::vector<Tile> &grid, int width, int height);
  void GenerateHiveNest(std::vector<Tile> &grid, int width, int height,
                        uint32_t seed);
};

} // namespace NoMoreDay
