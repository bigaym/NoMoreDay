#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include <algorithm>
#include <vector>
#include <map>

namespace NoMoreDay {

struct StatCalculation {
    float base = 0.0f;
    float flat = 0.0f;
    float percent_add = 0.0f;
    float percent_mult = 1.0f;

    float Result() const {
        return (base + flat) * (1.0f + percent_add) * percent_mult;
    }
};

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

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    
    // Use a map to accumulate different stat types
    std::map<StatType, StatCalculation> calcs;
    
    // Initialize with defaults
    calcs[StatType::MaxHealth].base = 100.0f;
    calcs[StatType::MaxMana].base = 100.0f;
    calcs[StatType::MoveSpeed].base = 300.0f;
    calcs[StatType::Armor].base = 0.0f;

    // 1. Apply Primary Stats Scaling to Base
    if (registry.all_of<PrimaryStats>(entity)) {
        const auto& primary = registry.get<PrimaryStats>(entity);
        calcs[StatType::MaxHealth].base += primary.vitality * 10.0f;
        calcs[StatType::Armor].base += primary.strength * 1.0f;
        calcs[StatType::MaxMana].base += primary.intelligence * 2.0f;
        // Crit chance and others don't follow the same base/flat/mult formula easily if they are percentages,
        // but we can treat them as flat additions to a base 0.05.
    }

    // 2. Accumulate Modifiers
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        for (const auto& mod : list.modifiers) {
            auto& c = calcs[mod.type];
            switch (mod.mode) {
                case ModifierMode::Flat:
                    c.flat += mod.value;
                    break;
                case ModifierMode::PercentAdd:
                    c.percent_add += mod.value / 100.0f;
                    break;
                case ModifierMode::PercentMult:
                    c.percent_mult *= (1.0f + mod.value / 100.0f);
                    break;
            }
        }
    }

    // 3. Finalize
    combat.max_health = calcs[StatType::MaxHealth].Result();
    combat.max_mana = calcs[StatType::MaxMana].Result();
    combat.armor = calcs[StatType::Armor].Result();
    combat.move_speed = calcs[StatType::MoveSpeed].Result();

    // Handle others (DEX -> Crit)
    if (registry.all_of<PrimaryStats>(entity)) {
        const auto& primary = registry.get<PrimaryStats>(entity);
        combat.crit_chance = 0.05f + primary.dexterity * 0.001f;
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
