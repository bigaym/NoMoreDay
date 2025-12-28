#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include <algorithm>
#include <vector>

namespace NoMoreDay {

static void resetCombatStats(CombatStats& combat) {
    combat.max_health = 100.0f;
    combat.max_mana = 100.0f;
    combat.armor = 0.0f;
    combat.move_speed = 300.0f;
    combat.crit_chance = 0.05f;
    combat.crit_damage = 1.50f;
    combat.attack_speed = 1.0f;
    combat.cast_speed = 1.0f;
    combat.resistances.fill(0.0f);
    combat.flat_damage.fill(0.0f);
    combat.damage_multipliers.fill(1.0f);
}

static void applyPrimaryScaling(const PrimaryStats& primary, CombatStats& combat) {
    // 1 VIT = 10 Max Health
    combat.max_health += primary.vitality * 10.0f;
    // 1 STR = 1 Armor
    combat.armor += primary.strength * 1.0f;
    // 1 INT = 2 Max Mana
    combat.max_mana += primary.intelligence * 2.0f;
    // 1 DEX = 0.001 Crit Chance
    combat.crit_chance += primary.dexterity * 0.001f;
}

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    resetCombatStats(combat);

    // 1. Apply Primary Stats Scaling
    if (registry.all_of<PrimaryStats>(entity)) {
        applyPrimaryScaling(registry.get<PrimaryStats>(entity), combat);
    }

    // 2. Apply Modifiers
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        
        // We need to apply mods in order: Flat -> PercentAdd -> PercentMult
        // For simplicity in this first pass, we just handle Flat
        for (const auto& mod : list.modifiers) {
            if (mod.mode == ModifierMode::Flat) {
                if (mod.type == StatType::MaxHealth) combat.max_health += mod.value;
                if (mod.type == StatType::Armor) combat.armor += mod.value;
            }
        }
    }
}

void StatsSystem::update(entt::registry& registry) {
    auto view = registry.view<StatsDirty>();
    for (auto entity : view) {
        Recalculate(registry, entity);
    }
    registry.clear<StatsDirty>();
}

} // namespace NoMoreDay