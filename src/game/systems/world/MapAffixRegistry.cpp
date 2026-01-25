#include "MapAffixRegistry.hpp"
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

namespace NoMoreDay {

void MapAffixRegistry::Initialize() {
    // No-op, all data is constexpr in header
}

std::vector<MapAffixType> MapAffixRegistry::GetAvailableAffixes(MapAffixCategory category) {
    std::vector<MapAffixType> results;
    // G_AFFIX_DEFINITIONS is accessible here as a private static member
    for (size_t i = 0; i < G_AFFIX_DEFINITIONS.size(); ++i) {
        const auto& def = G_AFFIX_DEFINITIONS[i];
        if (def.difficultyWeight > 0.0f || def.category == MapAffixCategory::Environment) {
             if (def.category == category) {
                 results.push_back(static_cast<MapAffixType>(i));
             }
        }
    }
    return results;
}

float MapAffixRegistry::CalculateValue(MapAffixType type, int tier) {
    const auto& def = GetDef(type);
    if (tier <= 1) return def.valT1;
    if (tier >= 10) return def.valT10;

    float t = (tier - 1) / 9.0f;
    return def.valT1 + t * (def.valT10 - def.valT1);
}

std::string MapAffixRegistry::FormatDescription(MapAffixType type, float value) {
    const auto& def = GetDef(type);
    std::string result(def.descriptionTemplate);
    
    char buf[32];
    // Format to 1 decimal place if it has a fractional part, otherwise integer
    if (std::abs(value - std::round(value)) < 0.01f) {
        snprintf(buf, sizeof(buf), "%d", (int)std::round(value));
    } else {
        snprintf(buf, sizeof(buf), "%.1f", value);
    }
    
    size_t pos = result.find("{value}");
    if (pos != std::string::npos) {
        result.replace(pos, 7, buf);
    }
    
    return result;
}

} // namespace NoMoreDay
