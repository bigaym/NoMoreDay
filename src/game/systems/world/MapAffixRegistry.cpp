#include "MapAffixRegistry.hpp"
#include <stdexcept>
#include <algorithm>

namespace NoMoreDay {

std::unordered_map<MapAffixType, MapAffixDefinition> MapAffixRegistry::definitions;

void MapAffixRegistry::Initialize() {
    definitions.clear();

    // --- BUFFS (System Derived) ---
    definitions[MapAffixType::DropRarity] = { "Gilded", "镀金之", MapAffixCategory::Buff, 0.0f, 20.0f, 150.0f, false };
    definitions[MapAffixType::DropQuantity] = { "Opulent", "富裕之", MapAffixCategory::Buff, 0.0f, 10.0f, 100.0f, false };

    // --- STATISTICAL (Prefixes, Weight 0.5) ---
    definitions[MapAffixType::MonsterDensity] = { "Swarming", "虫群之", MapAffixCategory::Debuff, 0.5f, 10.0f, 50.0f, false };
    definitions[MapAffixType::MonsterLevel] = { "Nightmare", "梦魇之", MapAffixCategory::Debuff, 0.5f, 1.0f, 5.0f, false };

    // --- ENEMY DEFENSE (Suffixes, Weight 1.0) ---
    definitions[MapAffixType::Enemy_ExtraHealth] = { "of the Colossus", "巨像之", MapAffixCategory::Debuff, 1.0f, 20.0f, 100.0f, true };
    definitions[MapAffixType::Enemy_ExtraBarrier] = { "of the Aegis", "庇护之", MapAffixCategory::Debuff, 1.0f, 20.0f, 80.0f, true };
    definitions[MapAffixType::Enemy_BarrierRegen] = { "of Restoration", "复苏之", MapAffixCategory::Debuff, 1.0f, 1.0f, 5.0f, true };
    definitions[MapAffixType::Enemy_Armor] = { "of Iron", "钢铁之", MapAffixCategory::Debuff, 1.0f, 500.0f, 3000.0f, true };
    definitions[MapAffixType::Enemy_Dodge] = { "of Mist", "迷雾之", MapAffixCategory::Debuff, 1.0f, 500.0f, 2500.0f, true };
    definitions[MapAffixType::Enemy_ResistAll] = { "of Prism", "棱镜之", MapAffixCategory::Debuff, 1.0f, 5.0f, 20.0f, true };
    definitions[MapAffixType::Enemy_ResistPhys] = { "of Granite", "花岗岩之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_ResistFire] = { "of Embers", "灰烬之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_ResistCold] = { "of Frost", "霜冻之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_ResistLight] = { "of Storms", "风暴之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_ResistPois] = { "of Venom", "剧毒之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_ResistVoid] = { "of Null", "虚无之", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true };
    definitions[MapAffixType::Enemy_CritResist] = { "of Adamant", "坚韧之", MapAffixCategory::Debuff, 1.0f, 30.0f, 60.0f, true };

    // --- ENEMY OFFENSE (Suffixes, Weight 2.0) ---
    definitions[MapAffixType::Enemy_ExtraDamage] = { "of Violence", "暴力之", MapAffixCategory::Debuff, 2.0f, 15.0f, 50.0f, true };
    definitions[MapAffixType::Enemy_Fast] = { "of Frenzy", "狂乱之", MapAffixCategory::Debuff, 2.0f, 10.0f, 35.0f, true };
    definitions[MapAffixType::Enemy_CritChance] = { "of Precision", "精准之", MapAffixCategory::Debuff, 2.0f, 20.0f, 100.0f, true };
    definitions[MapAffixType::Enemy_ArmorShred] = { "of Sundering", "穿透之", MapAffixCategory::Debuff, 2.0f, 5.0f, 15.0f, true };

    // --- PLAYER PENALTIES (Suffixes, Weight 3.0) ---
    definitions[MapAffixType::Player_ResistRedAll] = { "of Exposure", "暴露之", MapAffixCategory::Debuff, 3.0f, 5.0f, 20.0f, true };
    definitions[MapAffixType::Player_ResistRedSpecific] = { "of Vulnerability", "脆弱之", MapAffixCategory::Debuff, 3.0f, 15.0f, 40.0f, true };
    definitions[MapAffixType::Player_RedRecovery] = { "of Atrophy", "萎缩之", MapAffixCategory::Debuff, 3.0f, 30.0f, 60.0f, true };
    definitions[MapAffixType::Player_Fragile] = { "of Glass", "玻璃之", MapAffixCategory::Debuff, 3.0f, 10.0f, 25.0f, true };
    definitions[MapAffixType::Player_DodgePenalty] = { "of Unwavering", "迟缓之", MapAffixCategory::Debuff, 3.0f, 15.0f, 30.0f, true };

    // --- ENVIRONMENT (Special) ---
    definitions[MapAffixType::Env_Firestorm] = { "of Ash", "炼狱之", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true };
    definitions[MapAffixType::Env_Darkness] = { "of Void", "漆黑之", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true };
    definitions[MapAffixType::Env_GroundIce] = { "of Frost", "冰洁之", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true };
    definitions[MapAffixType::Env_LightningStorm] = { "of Fulgur", "雷霆之", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true };
}

const MapAffixDefinition& MapAffixRegistry::GetDef(MapAffixType type) {
    if (definitions.empty()) Initialize();
    auto it = definitions.find(type);
    if (it != definitions.end()) return it->second;
    throw std::runtime_error("MapAffix definition not found");
}

std::vector<MapAffixType> MapAffixRegistry::GetAvailableAffixes(MapAffixCategory category) {
    if (definitions.empty()) Initialize();
    std::vector<MapAffixType> results;
    for (const auto& [type, def] : definitions) {
        if (def.category == category) {
            results.push_back(type);
        }
    }
    return results;
}

float MapAffixRegistry::CalculateValue(MapAffixType type, int tier) {
    if (tier < 1) tier = 1;
    if (tier > 10) tier = 10;
    const auto& def = GetDef(type);
    float t = (float)(tier - 1) / 9.0f; // 0.0 to 1.0
    return def.valT1 + t * (def.valT10 - def.valT1);
}

} // namespace NoMoreDay
