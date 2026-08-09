#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>


using json = nlohmann::json;

namespace NoMoreDay {
namespace {
std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

Color ParseColor(const json &item, const char *key, Color fallback) {
  if (!item.contains(key) || !item[key].is_array() || item[key].size() != 4) {
    return fallback;
  }

  const auto &colorArray = item[key];
  auto toByte = [](const json &v) -> unsigned char {
    const int value = std::clamp(v.get<int>(), 0, 255);
    return static_cast<unsigned char>(value);
  };
  return {toByte(colorArray[0]), toByte(colorArray[1]), toByte(colorArray[2]),
          toByte(colorArray[3])};
}

BiomeStyle ParseBiomeStyle(const json &item) {
  const std::string style = ToLower(item.value("style", std::string("open")));
  if (style == "town") {
    return BiomeStyle::Town;
  }
  if (style == "open") {
    return BiomeStyle::Open;
  }
  if (style == "maze") {
    return BiomeStyle::Maze;
  }
  if (style == "special") {
    return BiomeStyle::Special;
  }

  LOG_WARN("Unknown biome style '{}', fallback to Open", style);
  return BiomeStyle::Open;
}

uint32_t ParseBiomeFeatures(const json &item) {
  static const std::unordered_map<std::string, BiomeFeature> kFeatureMap = {
      {"air_wall", BiomeFeature::AirWall},
      {"low_gravity", BiomeFeature::LowGravity},
      {"destructible", BiomeFeature::Destructible},
      {"dynamic_spawner", BiomeFeature::DynamicSpawner},
      {"limited_vision", BiomeFeature::LimitedVision},
      {"speed_zone", BiomeFeature::SpeedZone},
      {"friction_mod", BiomeFeature::FrictionMod},
      {"visual_filter", BiomeFeature::VisualFilter},
  };

  if (!item.contains("features")) {
    return 0;
  }
  if (!item["features"].is_array()) {
    LOG_WARN("Biome features should be an array, got invalid type");
    return 0;
  }

  uint32_t mask = 0;
  for (const auto &entry : item["features"]) {
    if (!entry.is_string()) {
      continue;
    }
    const std::string key = ToLower(entry.get<std::string>());
    const auto iter = kFeatureMap.find(key);
    if (iter == kFeatureMap.end()) {
      LOG_WARN("Unknown biome feature '{}', ignored", key);
      continue;
    }
    mask = AddBiomeFeature(mask, iter->second);
  }

  return mask;
}
} // namespace

BiomeRegistry &BiomeRegistry::Get() {
  static BiomeRegistry instance;
  return instance;
}

void BiomeRegistry::LoadFromJSON(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open biomes config: {}", path);
    return;
  }

  try {
    json j;
    file >> j;
    const int version = j.value("version", 1);
    if (version < 2) {
      LOG_WARN("Biome config version {} detected. Expected version 2.", version);
    }

    m_biomes.clear();
    m_idToKey.clear();
    for (const auto &item : j.value("biomes", json::array())) {
      BiomeConfig config;
      config.id = item.value("id", std::string{});
      config.numericId = static_cast<BiomeID>(item.value("numeric_id", 0));
      config.name = item.value("name", config.id);
      config.style = ParseBiomeStyle(item);
      config.floorColor = ParseColor(item, "floorColor", DARKBROWN);
      config.wallColor = ParseColor(item, "wallColor", DARKGRAY);
      config.ambientColor = ParseColor(item, "ambientColor", WHITE);
      config.backgroundShader = item.value("backgroundShader", std::string{});
      config.visualFilterShader = item.value("visualFilterShader", std::string{});

      config.wallProbability = item.value("wallProbability", 0.45f);
      config.smoothIterations = item.value("smoothIterations", 5);
      config.wallBirthLimit = item.value("wallBirthLimit", 4);
      config.wallDeathLimit = item.value("wallDeathLimit", 3);
      config.features = ParseBiomeFeatures(item);
      config.frictionMultiplier = item.value("frictionMultiplier", 1.0f);
      config.gravityMultiplier = item.value("gravityMultiplier", 1.0f);
      config.visionRadius = item.value("visionRadius", 0.0f);
      if (item.contains("enemyPool") && item["enemyPool"].is_array()) {
        config.enemyPool = item["enemyPool"].get<std::vector<std::string>>();
      } else {
        config.enemyPool.clear();
      }
      config.maxEnemies = item.value("maxEnemies", 50);
      config.isSafeZone = item.value("isSafeZone", false);

      if (config.id.empty()) {
        LOG_WARN("Skipped biome with empty id");
        continue;
      }

      m_biomes[config.id] = config;
      if (config.numericId != NoMoreDay::BiomeID::None) {
        m_idToKey[config.numericId] = config.id;
      }
      LOG_INFO("Loaded biome: {} ({}, id: {}, style: {}, features: 0x{:X})",
                   config.name, config.id, static_cast<int>(config.numericId),
                   static_cast<int>(config.style), config.features);
    }
  } catch (const std::exception &e) {
    LOG_ERROR("Error parsing biomes JSON: {}", e.what());
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
