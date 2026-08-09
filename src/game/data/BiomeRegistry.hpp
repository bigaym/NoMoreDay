#pragma once

#include "game/components/MapComponent.hpp"
#include "game/data/BiomeTypes.hpp"
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
  NoMoreDay::BiomeStyle style = NoMoreDay::BiomeStyle::Open;
  Color floorColor = DARKBROWN;
  Color wallColor = DARKGRAY;
  Color ambientColor = WHITE;
  std::string backgroundShader;
  std::string visualFilterShader;
  float wallProbability = 0.45f;
  int smoothIterations = 5;
  int wallBirthLimit = 4;
  int wallDeathLimit = 3;
  uint32_t features = 0;
  float frictionMultiplier = 1.0f;
  float gravityMultiplier = 1.0f;
  float visionRadius = 0.0f;
  std::vector<std::string> enemyPool;
  int maxEnemies = 50;
  bool isSafeZone = false;

  [[nodiscard]] bool hasFeature(NoMoreDay::BiomeFeature feature) const noexcept {
    return NoMoreDay::HasBiomeFeature(features, feature);
  }
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
