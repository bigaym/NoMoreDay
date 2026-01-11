#include "game/data/BiomeRegistry.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

using json = nlohmann::json;

namespace NoMoreDay {

BiomeRegistry& BiomeRegistry::Get() {
    static BiomeRegistry instance;
    return instance;
}

void BiomeRegistry::LoadFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open biomes config: {}", path);
        return;
    }

    try {
        json j;
        file >> j;

        m_biomes.clear();
        for (const auto& item : j["biomes"]) {
            BiomeConfig config;
            config.id = item["id"];
            config.name = item["name"];
            
            auto fc = item["floorColor"];
            config.floorColor = { (unsigned char)fc[0], (unsigned char)fc[1], (unsigned char)fc[2], (unsigned char)fc[3] };
            
            auto wc = item["wallColor"];
            config.wallColor = { (unsigned char)wc[0], (unsigned char)wc[1], (unsigned char)wc[2], (unsigned char)wc[3] };
            
            config.wallProbability = item.value("wallProbability", 0.45f);
            config.smoothIterations = item.value("smoothIterations", 5);
            config.enemyPool = item["enemyPool"].get<std::vector<std::string>>();
            config.maxEnemies = item.value("maxEnemies", 50);
            config.isSafeZone = item.value("isSafeZone", false);

            m_biomes[config.id] = config;
            spdlog::info("Loaded biome: {} ({})", config.name, config.id);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error parsing biomes JSON: {}", e.what());
    }
}

const BiomeConfig& BiomeRegistry::GetBiome(const std::string& id) const {
    auto it = m_biomes.find(id);
    if (it != m_biomes.end()) {
        return it->second;
    }
    return m_defaultBiome;
}

bool BiomeRegistry::HasBiome(const std::string& id) const {
    return m_biomes.find(id) != m_biomes.end();
}

} // namespace NoMoreDay
