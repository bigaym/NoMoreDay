#pragma once

#include "game/components/MapComponent.hpp"
#include "raylib.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace NoMoreDay {

struct BiomeConfig {
  std::string id;
  NoMoreDay::BiomeID numericId = NoMoreDay::BiomeID::None;
  std::string name;
  Color floorColor = DARKBROWN;
  Color wallColor = DARKGRAY;
  float wallProbability = 0.45f;
  int smoothIterations = 5;
  std::vector<std::string> enemyPool;
  int maxEnemies = 50;
  bool isSafeZone = false;
};

class BiomeRegistry {
public:
  static BiomeRegistry &Get();

  void LoadFromJSON(const std::string &path);
  const BiomeConfig &GetBiome(const std::string &id) const;
  const BiomeConfig &GetBiome(BiomeID id) const;
  bool HasBiome(const std::string &id) const;
  bool HasBiome(BiomeID id) const;

private:
  BiomeRegistry() = default;
  std::unordered_map<std::string, BiomeConfig> m_biomes;
  std::unordered_map<BiomeID, std::string> m_idToKey;
  BiomeConfig m_defaultBiome;
};

} // namespace NoMoreDay
