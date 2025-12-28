#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/ItemStats.hpp"
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

// Helper to apply a generic StatModifier to the calculation map
static void ApplyStatModifier(std::map<StatType, StatCalculation>& calcs, StatType type, ModifierMode mode, float value) {
    auto& c = calcs[type];
    switch (mode) {
        case ModifierMode::Flat:
            c.flat += value;
            break;
        case ModifierMode::PercentAdd:
            c.percent_add += value / 100.0f;
            break;
        case ModifierMode::PercentMult:
            c.percent_mult *= (1.0f + value / 100.0f);
            break;
    }
}

// Helper to convert AffixType to StatType and apply it
static void ApplyAffix(std::map<StatType, StatCalculation>& calcs, const Affix& affix) {
    switch (affix.type) {
        case AffixType::Strength:
            ApplyStatModifier(calcs, StatType::Strength, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Dexterity:
            ApplyStatModifier(calcs, StatType::Dexterity, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Intelligence:
            ApplyStatModifier(calcs, StatType::Intelligence, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Vitality:
            ApplyStatModifier(calcs, StatType::Vitality, ModifierMode::Flat, affix.value);
            break;
        case AffixType::FlatHealth:
            ApplyStatModifier(calcs, StatType::MaxHealth, ModifierMode::Flat, affix.value);
            break;
        case AffixType::PercentHealth:
            ApplyStatModifier(calcs, StatType::MaxHealth, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::FlatMana:
            ApplyStatModifier(calcs, StatType::MaxMana, ModifierMode::Flat, affix.value);
            break;
        case AffixType::FlatArmor:
            ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat, affix.value);
            break;
        case AffixType::PercentArmor:
            ApplyStatModifier(calcs, StatType::Armor, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::MoveSpeed:
            ApplyStatModifier(calcs, StatType::MoveSpeed, ModifierMode::PercentAdd, affix.value);
            break;
        // TODO: Handle Damage Affixes (need to expand StatType or handle separately)
        default:
            break;
    }
}

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    resetCombatStats(combat); // Ensure we start fresh
    
    // Use a map to accumulate different stat types
    std::map<StatType, StatCalculation> calcs;
    
    // Initialize with defaults
    calcs[StatType::MaxHealth].base = 100.0f;
    calcs[StatType::MaxMana].base = 100.0f;
    calcs[StatType::MoveSpeed].base = 300.0f;
    calcs[StatType::Armor].base = 0.0f;

    // 0. Base Stats from PrimaryStats Component (Base Values)
    if (registry.all_of<PrimaryStats>(entity)) {
        const auto& primary = registry.get<PrimaryStats>(entity);
        calcs[StatType::Strength].base = primary.strength;
        calcs[StatType::Dexterity].base = primary.dexterity;
        calcs[StatType::Intelligence].base = primary.intelligence;
        calcs[StatType::Vitality].base = primary.vitality;
    }

    // 1. Process Equipment (adds to Primary Stats and Secondary Stats)
    if (registry.all_of<EquipmentComponent>(entity)) {
        const auto& equipment = registry.get<EquipmentComponent>(entity);
        for (const auto& itemEntity : equipment.slots) {
            if (registry.valid(itemEntity) && registry.all_of<ItemComponent>(itemEntity)) {
                const auto& item = registry.get<ItemComponent>(itemEntity);
                
                // Implicits
                for (const auto& affix : item.implicits) {
                    ApplyAffix(calcs, affix);
                }
                
                // Explicits
                for (const auto& affix : item.affixes) {
                    ApplyAffix(calcs, affix);
                }
                
                // Base Item Stats (Weapon Damage, Armor)
                if (item.defense > 0) {
                    ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat, item.defense);
                }
                // Weapon damage usually handled separately or via specific StatType
            }
        }
    }

    // 2. Accumulate Generic Modifiers (Buffs, Passives)
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        for (const auto& mod : list.modifiers) {
            ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
        }
    }

    // 3. Resolve Primary Stats first (as they affect secondary)
    float str = calcs[StatType::Strength].Result();
    float dex = calcs[StatType::Dexterity].Result();
    float intel = calcs[StatType::Intelligence].Result();
    float vit = calcs[StatType::Vitality].Result();

    // 4. Apply Primary Stat Scaling to Secondary Bases
    // Strength -> Armor
    calcs[StatType::Armor].base += str * 1.0f;
    // Vitality -> HP
    calcs[StatType::MaxHealth].base += vit * 10.0f;
    // Intelligence -> Mana
    calcs[StatType::MaxMana].base += intel * 2.0f;

    // 5. Finalize Secondary Stats
    combat.max_health = calcs[StatType::MaxHealth].Result();
    combat.max_mana = calcs[StatType::MaxMana].Result();
    combat.armor = calcs[StatType::Armor].Result();
    combat.move_speed = calcs[StatType::MoveSpeed].Result();

    // Handle derived stats that didn't go through the calc map (Direct formula)
    combat.crit_chance = 0.05f + dex * 0.001f;
}

void StatsSystem::update(entt::registry& registry) {
    auto view = registry.view<StatsDirty>();
    for (auto entity : view) {
        Recalculate(registry, entity);
    }
    registry.clear<StatsDirty>();
}

} // namespace NoMoreDay
