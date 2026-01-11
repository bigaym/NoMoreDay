#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "raylib.h"
#include "game/components/MapComponent.hpp"

namespace NoMoreDay {

struct BiomeConfig {
    std::string id;
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
    static BiomeRegistry& Get();
    
    void LoadFromJSON(const std::string& path);
    const BiomeConfig& GetBiome(const std::string& id) const;
    bool HasBiome(const std::string& id) const;

private:
    BiomeRegistry() = default;
    std::unordered_map<std::string, BiomeConfig> m_biomes;
    BiomeConfig m_defaultBiome;
};

} // namespace NoMoreDay
