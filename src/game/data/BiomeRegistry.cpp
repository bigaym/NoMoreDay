#include "game/data/BiomeRegistry.hpp"
#include "spdlog/spdlog.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

namespace NoMoreDay {

BiomeRegistry &BiomeRegistry::Get() {
  static BiomeRegistry instance;
  return instance;
}

void BiomeRegistry::LoadFromJSON(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    spdlog::error("Failed to open biomes config: {}", path);
    return;
  }

  try {
    json j;
    file >> j;

    m_biomes.clear();
    m_idToKey.clear();
    for (const auto &item : j["biomes"]) {
      BiomeConfig config;
      config.id = item["id"];
      config.numericId = static_cast<BiomeID>(item.value("numeric_id", 0));
      config.name = item["name"];

      auto fc = item["floorColor"];
      config.floorColor = {(unsigned char)fc[0], (unsigned char)fc[1],
                           (unsigned char)fc[2], (unsigned char)fc[3]};

      auto wc = item["wallColor"];
      config.wallColor = {(unsigned char)wc[0], (unsigned char)wc[1],
                          (unsigned char)wc[2], (unsigned char)wc[3]};

      config.wallProbability = item.value("wallProbability", 0.45f);
      config.smoothIterations = item.value("smoothIterations", 5);
      config.enemyPool = item["enemyPool"].get<std::vector<std::string>>();
      config.maxEnemies = item.value("maxEnemies", 50);
      config.isSafeZone = item.value("isSafeZone", false);

      m_biomes[config.id] = config;
      if (config.numericId != NoMoreDay::BiomeID::None) {
        m_idToKey[config.numericId] = config.id;
      }
      spdlog::info("Loaded biome: {} ({}, id: {})", config.name, config.id,
                   (int)config.numericId);
    }
  } catch (const std::exception &e) {
    spdlog::error("Error parsing biomes JSON: {}", e.what());
  }
}

const BiomeConfig &BiomeRegistry::GetBiome(const std::string &id) const {
  auto it = m_biomes.find(id);
  if (it != m_biomes.end()) {
    return it->second;
  }
  return m_defaultBiome;
}

const BiomeConfig &BiomeRegistry::GetBiome(BiomeID id) const {
  auto it = m_idToKey.find(id);
  if (it != m_idToKey.end()) {
    return GetBiome(it->second);
  }
  return m_defaultBiome;
}

bool BiomeRegistry::HasBiome(const std::string &id) const {
  return m_biomes.find(id) != m_biomes.end();
}

bool BiomeRegistry::HasBiome(BiomeID id) const {
  return m_idToKey.find(id) != m_idToKey.end();
}

} // namespace NoMoreDay
