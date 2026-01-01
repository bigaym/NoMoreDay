#include "DamagePipeline.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay {

DamageResult DamagePipeline::Calculate(
    entt::registry& registry,
    entt::entity attacker,
    entt::entity defender,
    const DamagePool& base_pool,
    Tag hit_tags
) {
    // 1. Collect Modifiers
    std::vector<DamageModifier> all_mods;
    
    if (auto* global = registry.try_get<GlobalModifierComponent>(attacker)) {
        all_mods.insert(all_mods.end(), global->modifiers.begin(), global->modifiers.end());
    }
    
    // Note: Skill modifiers would be gathered here if we had a specific skill entity
    // For now, we assume attacker might have them or they are passed in somehow.
    // In a full implementation, we might pass the skill entity as well.

    // 2. Conversion & Gain Extra
    DamagePool converted_pool = ApplyConversion(base_pool, all_mods);

    // 3 & 4. Apply Multipliers (Inc & More)
    DamagePool multiplied_pool = ApplyMultipliers(converted_pool, all_mods, hit_tags);

    // 5. Final Settlement
    const auto& attacker_stats = registry.get<CombatStats>(attacker);
    const auto& defender_stats = registry.get<CombatStats>(defender);

    return Settle(multiplied_pool, attacker_stats, defender_stats, hit_tags);
}

DamagePool DamagePipeline::ApplyConversion(const DamagePool& pool, const std::vector<DamageModifier>& mods) {
    // Basic implementation for now (placeholder)
    return pool;
}

DamagePool DamagePipeline::ApplyMultipliers(const DamagePool& pool, const std::vector<DamageModifier>& mods, Tag hit_tags) {
    DamagePool result = pool;
    
    for (int i = 0; i < 16; ++i) {
        if (result.values[i] <= 0.0f) continue;
        
        Tag type_tag = static_cast<Tag>(1ULL << i);
        Tag combined_tags = type_tag | hit_tags;
        
        float increased = 0.0f;
        float more = 1.0f;
        
        for (const auto& mod : mods) {
            // Check if mod applies to these tags
            if (HasTag(combined_tags, mod.source_tag)) {
                if (mod.type == ModifierType::Increased) {
                    increased += mod.value;
                } else if (mod.type == ModifierType::More) {
                    more *= (1.0f + mod.value);
                }
            }
        }
        
        result.values[i] *= (1.0f + increased) * more;
    }
    
    return result;
}

DamageResult DamagePipeline::Settle(const DamagePool& pool, const CombatStats& attacker_stats, const CombatStats& defender_stats, Tag hit_tags) {
    DamageResult result;
    result.final_pool = pool;
    
    float total = 0.0f;
    for (int i = 0; i < 16; ++i) {
        if (pool.values[i] <= 0.0f) continue;
        
        // Apply Resistances
        float res = 0.0f;
        if (i < 6) { // Matching DamageType enum in Stats.hpp
            res = defender_stats.resistances[i];
        }
        
        float damage_after_res = pool.values[i] * (1.0f - res);
        total += damage_after_res;
    }
    
    result.total_damage = total;
    return result;
}

} // namespace NoMoreDay
