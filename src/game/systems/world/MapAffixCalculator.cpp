#include "MapAffixCalculator.hpp"
#include "MapAffixRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "../../foundation/data/MosaicData.hpp"
#include "../../foundation/components/MapFragmentComponent.hpp"
#include "../../foundation/components/ItemComponent.hpp" 
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

MapAffixCalculator::Rewards MapAffixCalculator::CalculateRewards(int difficultyScore, int depth) {
    Rewards rewards;
    rewards.rarityBonus = 0.0f;
    rewards.quantityBonus = 0.0f;

    if (difficultyScore <= 0) {
        return rewards;
    }
    
    float depthMult = 1.0f + (float)(depth - 1) * 0.1f;
    rewards.rarityBonus = (float)difficultyScore * 0.015f * depthMult;
    
    float ds = (float)difficultyScore;
    rewards.quantityBonus = 0.50f * std::log2(1.0f + ds / 40.0f) * depthMult;
    
    return rewards;
}

float MapAffixCalculator::CalculateLPProbabilityMultiplier(float rarityBonus) {
    return 1.0f + rarityBonus;
}

std::vector<AggregatedAffix> MapAffixCalculator::AggregateAffixes(const std::vector<MapAffix>& affixes) {
    std::unordered_map<MapAffixType, AggregatedAffix> aggMap;
    
    for (const auto& aff : affixes) {
        auto& agg = aggMap[aff.type];
        if (agg.sources.empty()) {
            agg.type = aff.type;
            agg.category = aff.category;
            agg.totalValue = 0.0f;
            agg.maxTier = 0;
        }
        
        agg.totalValue += aff.value;
        agg.maxTier = std::max(agg.maxTier, aff.tier);
        
        // Add source if not already present to avoid redundancy in display
        if (std::find(agg.sources.begin(), agg.sources.end(), aff.source) == agg.sources.end()) {
            agg.sources.push_back(aff.source);
        }
    }
    
    std::vector<AggregatedAffix> result;
    for (auto& pair : aggMap) {
        result.push_back(std::move(pair.second));
    }
    
    // Optional: Sort by category (Buff first, then Debuff, then Env) or importance
    std::sort(result.begin(), result.end(), [](const AggregatedAffix& a, const AggregatedAffix& b) {
        if (a.category != b.category) {
            return static_cast<int>(a.category) < static_cast<int>(b.category);
        }
        return a.type < b.type;
    });

    return result;
}

std::vector<MapAffix> MapAffixCalculator::GenerateAffixesFromSnapshots(const std::array<FragmentSnapshot, 9>& snapshots) {
    std::vector<MapAffix> affixes;
    affixes.reserve(snapshots.size() * 3); // Reserve space for at least 3 affixes per snapshot (density, level, base)
    
    
    auto addAffix = [&](MapAffixType type, int tier, std::string source) {
        if (tier <= 0) return;
        if (tier > 10) tier = 10;
        
        const auto& def = MapAffixRegistry::GetDef(type);
        
        MapAffix aff;
        aff.type = type;
        aff.tier = tier;
        aff.source = source;
        aff.category = def.category;
        aff.value = MapAffixRegistry::CalculateValue(type, tier);
        affixes.push_back(aff);
    };

    for (size_t i = 0; i < snapshots.size(); ++i) {
        const auto& snap = snapshots[i];
        if (!snap.hasFragment) continue;
        
        // Use a safe default name if snap.name is problematic or empty (though serialization should handle it)
        std::string safeName = (!snap.name.empty() && snap.name.capacity() < 1000) ? snap.name : "Fragment";

        if (snap.enemyDensityMod > 1.01f) {
            int tier = (int)((snap.enemyDensityMod - 1.0f) * 10.0f);
            addAffix(MapAffixType::MonsterDensity, tier, safeName + " (Density)");
        }
        
        if (snap.monsterLevelMod > 0) {
            addAffix(MapAffixType::MonsterLevel, snap.monsterLevelMod, safeName + " (Level)");
        }
        
        int tierBase = 1;
        switch(snap.rarity) {
            case Rarity::Common: tierBase = 1; break;
            case Rarity::Magic: tierBase = 3; break;
            case Rarity::Rare: tierBase = 5; break;
            case Rarity::Epic: tierBase = 8; break;
            case Rarity::Legendary: tierBase = 10; break;
            default: tierBase = 1; break;
        }

        switch(snap.element) {
            case FragmentElement::Fire: 
                addAffix(MapAffixType::Enemy_ResistFire, tierBase, safeName + " (Fire)"); 
                break;
            case FragmentElement::Cold: 
                addAffix(MapAffixType::Enemy_ResistCold, tierBase, safeName + " (Cold)"); 
                break;
            case FragmentElement::Lightning: 
                addAffix(MapAffixType::Enemy_ResistLight, tierBase, safeName + " (Light)"); 
                break;
            case FragmentElement::Shadow: 
                addAffix(MapAffixType::Enemy_ResistVoid, tierBase, safeName + " (Shadow)"); 
                break;
            default: break; 
        }
        
        if (snap.type == FragmentType::Affix) {
            if (snap.rarity >= Rarity::Magic) {
                 addAffix(MapAffixType::Enemy_ExtraHealth, tierBase, safeName + " (HP)");
            }
            if (snap.rarity >= Rarity::Rare) {
                 addAffix(MapAffixType::Enemy_ExtraDamage, tierBase, safeName + " (Damage)");
            }
            if (snap.rarity >= Rarity::Legendary) {
                 addAffix(MapAffixType::Player_ResistRedAll, tierBase / 2, safeName + " (Curse)");
            }
        }
    }
    return affixes;
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
