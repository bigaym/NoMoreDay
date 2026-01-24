#pragma once

#include "../../data/MapAffix.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace NoMoreDay {

struct MapAffixDefinition {
    std::string name;        // e.g. "Swarming", "of Iron"
    std::string nameZh;      // e.g. "虫群之", "钢铁之"
    MapAffixCategory category;
    float difficultyWeight;  // 0.5 to 3.0
    float valT1;             // Value at Tier 1
    float valT10;            // Value at Tier 10
    bool isSuffix;           // true if it goes after map name
};

class MapAffixRegistry {
public:
    static void Initialize();
    
    static const MapAffixDefinition& GetDef(MapAffixType type);
    static std::vector<MapAffixType> GetAvailableAffixes(MapAffixCategory category);
    static float CalculateValue(MapAffixType type, int tier);
    
private:
    static std::unordered_map<MapAffixType, MapAffixDefinition> definitions;
};

} // namespace NoMoreDay
