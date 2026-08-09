#include "game/foundation/data/MonsterAffixRegistry.hpp"

namespace NoMoreDay {

const std::unordered_map<std::string_view, MonsterAffixType> MonsterAffixRegistry::kNameToType = {
    { "Fast", MonsterAffixType::Fast },
    { "Tanky", MonsterAffixType::Tanky },
    { "Powerful", MonsterAffixType::Powerful },
    { "Accurate", MonsterAffixType::Accurate },
    { "Molten", MonsterAffixType::Molten },
    { "Frozen", MonsterAffixType::Frozen },
    { "Storm", MonsterAffixType::Storm },
    { "Toxic", MonsterAffixType::Toxic },
    { "Void", MonsterAffixType::Void },
    { "Void Zone", MonsterAffixType::VoidZone },
    { "VoidZone", MonsterAffixType::VoidZone }, // Alias
    { "Storm Strider", MonsterAffixType::StormStrider },
    { "StormStrider", MonsterAffixType::StormStrider }, // Alias
    { "Teleporter", MonsterAffixType::Teleporter },
    { "Nullifier", MonsterAffixType::Nullifier },
    { "Shielding", MonsterAffixType::Shielding },
    { "Waller", MonsterAffixType::Waller },
    { "Vampiric", MonsterAffixType::Vampiric },
    { "Berserker", MonsterAffixType::Berserker },
    { "Avenger", MonsterAffixType::Avenger },
    { "Soul Link", MonsterAffixType::SoulLink },
    { "SoulLink", MonsterAffixType::SoulLink }, // Alias
    { "Mirror Image", MonsterAffixType::MirrorImage },
    { "MirrorImage", MonsterAffixType::MirrorImage }, // Alias
    { "Soul Eater", MonsterAffixType::SoulEater },
    { "SoulEater", MonsterAffixType::SoulEater }, // Alias
    { "Suppressor", MonsterAffixType::Suppressor },
    { "Mana Siphon", MonsterAffixType::ManaSiphon },
    { "ManaSiphon", MonsterAffixType::ManaSiphon }, // Alias
    { "Vortex", MonsterAffixType::Vortex },
    { "Entangler", MonsterAffixType::Entangler }
};

MonsterAffixType MonsterAffixRegistry::GetTypeFromName(std::string_view name) {
    auto it = kNameToType.find(name);
    if (it != kNameToType.end()) {
        return it->second;
    }
    return MonsterAffixType::None;
}

} // namespace NoMoreDay
