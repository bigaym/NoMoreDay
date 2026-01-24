#include "MapAffixCalculator.hpp"
#include "MapAffixRegistry.hpp"
#include "../../data/MosaicData.hpp"
#include "../../components/MapFragmentComponent.hpp"
#include "../../components/ItemComponent.hpp" 
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

int MapAffixCalculator::CalculateDifficultyScore(const std::vector<MapAffix>& affixes) {
    float totalScore = 0.0f;
    for (const auto& affix : affixes) {
        // Only count Debuffs (Challenges) for Difficulty Score
        if (affix.category == MapAffixCategory::Debuff) {
             const auto& def = MapAffixRegistry::GetDef(affix.type);
             float score = (float)affix.tier * 10.0f * def.difficultyWeight;
             totalScore += score;
        }
    }
    return (int)totalScore;
}

MapAffixCalculator::Rewards MapAffixCalculator::CalculateRewards(int difficultyScore) {
    Rewards rewards;
    rewards.rarityBonus = 0.0f;
    rewards.quantityBonus = 0.0f;

    if (difficultyScore <= 0) {
        return rewards;
    }
    
    rewards.rarityBonus = (float)difficultyScore * 0.015f;
    
    float ds = (float)difficultyScore;
    rewards.quantityBonus = 0.50f * std::log2(1.0f + ds / 40.0f);
    
    return rewards;
}

float MapAffixCalculator::CalculateLPProbabilityMultiplier(float rarityBonus) {
    return 1.0f + rarityBonus;
}

std::vector<MapAffix> MapAffixCalculator::GenerateAffixesFromGrid(const MosaicGrid& grid, entt::registry& registry) {
    std::vector<MapAffix> affixes;
    
    auto addAffix = [&](MapAffixType type, int tier, std::string source) {
        if (tier <= 0) return;
        if (tier > 10) tier = 10;
        
        MapAffix aff;
        aff.type = type;
        aff.tier = tier;
        aff.source = source;
        const auto& def = MapAffixRegistry::GetDef(type);
        aff.category = def.category;
        aff.value = MapAffixRegistry::CalculateValue(type, tier);
        affixes.push_back(aff);
    };

    for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
        auto entity = grid.cells[i];
        if (!registry.valid(entity)) continue;
        
        auto* frag = registry.try_get<MapFragmentComponent>(entity);
        if (!frag) continue;
        
        if (frag->enemyDensityMod > 1.01f) {
            int tier = (int)((frag->enemyDensityMod - 1.0f) * 10.0f);
            addAffix(MapAffixType::MonsterDensity, tier, "Fragment Density");
        }
        
        if (frag->monsterLevelMod > 0) {
            addAffix(MapAffixType::MonsterLevel, frag->monsterLevelMod, "Fragment Level");
        }
        
        int tierBase = 1;
        switch(frag->rarity) {
            case Rarity::Common: tierBase = 1; break;
            case Rarity::Magic: tierBase = 3; break;
            case Rarity::Rare: tierBase = 5; break;
            case Rarity::Epic: tierBase = 8; break; // Epic is pretty high
            case Rarity::Legendary: tierBase = 10; break;
        }

        switch(frag->element) {
            case FragmentElement::Fire: addAffix(MapAffixType::Enemy_ResistFire, tierBase, "Fire Element"); break;
            case FragmentElement::Cold: addAffix(MapAffixType::Enemy_ResistCold, tierBase, "Cold Element"); break;
            case FragmentElement::Lightning: addAffix(MapAffixType::Enemy_ResistLight, tierBase, "Lightning Element"); break;
            case FragmentElement::Shadow: addAffix(MapAffixType::Enemy_ResistVoid, tierBase, "Shadow Element"); break;
            default: break; 
        }
        
        // Simple logic to add flavor affixes based on Type/Rarity
        if (frag->type == FragmentType::Affix) {
            if (frag->rarity >= Rarity::Magic) {
                 addAffix(MapAffixType::Enemy_ExtraHealth, tierBase, "Magic Modifier");
            }
            if (frag->rarity >= Rarity::Rare) {
                 addAffix(MapAffixType::Enemy_ExtraDamage, tierBase, "Rare Modifier");
            }
            if (frag->rarity >= Rarity::Legendary) {
                 addAffix(MapAffixType::Player_ResistRedAll, tierBase / 2, "Legendary Curse");
            }
        }
    }
    
    return affixes;
}

} // namespace NoMoreDay
