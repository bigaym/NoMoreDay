#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/ItemStats.hpp"
#include <algorithm>
#include <vector>
#include <array>

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

// Helper to apply a generic StatModifier to the calculation array
static void ApplyStatModifier(std::array<StatCalculation, static_cast<size_t>(StatType::Count)>& calcs, StatType type, ModifierMode mode, float value) {
    auto& c = calcs[static_cast<size_t>(type)];
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
static void ApplyAffix(std::array<StatCalculation, static_cast<size_t>(StatType::Count)>& calcs, const Affix& affix) {
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
        default:
            break;
    }
}

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    resetCombatStats(combat); 
    
    std::array<StatCalculation, static_cast<size_t>(StatType::Count)> calcs;
    
    // Initialize with defaults
    calcs[static_cast<size_t>(StatType::MaxHealth)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::MaxMana)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::MoveSpeed)].base = 300.0f;
    calcs[static_cast<size_t>(StatType::Armor)].base = 0.0f;

    // 0. Base Stats from PrimaryStats Component
    if (registry.all_of<PrimaryStats>(entity)) {
        const auto& primary = registry.get<PrimaryStats>(entity);
        calcs[static_cast<size_t>(StatType::Strength)].base = primary.strength;
        calcs[static_cast<size_t>(StatType::Dexterity)].base = primary.dexterity;
        calcs[static_cast<size_t>(StatType::Intelligence)].base = primary.intelligence;
        calcs[static_cast<size_t>(StatType::Vitality)].base = primary.vitality;
    }

    // 1. Process Equipment
    if (registry.all_of<EquipmentComponent>(entity)) {
        const auto& equipment = registry.get<EquipmentComponent>(entity);
        for (const auto& itemEntity : equipment.slots) {
            if (registry.valid(itemEntity) && registry.all_of<ItemComponent>(itemEntity)) {
                const auto& item = registry.get<ItemComponent>(itemEntity);
                for (const auto& affix : item.implicits) ApplyAffix(calcs, affix);
                for (const auto& affix : item.affixes) ApplyAffix(calcs, affix);
                if (item.defense > 0) {
                    ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat, item.defense);
                }
            }
        }
    }

    // 2. Accumulate Generic Modifiers
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        for (const auto& mod : list.modifiers) {
            ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
        }
    }

    // 3. Resolve Primary Stats
    float str = calcs[static_cast<size_t>(StatType::Strength)].Result();
    float dex = calcs[static_cast<size_t>(StatType::Dexterity)].Result();
    float vit = calcs[static_cast<size_t>(StatType::Vitality)].Result();

    // 4. Apply Primary Stat Scaling
    calcs[static_cast<size_t>(StatType::Armor)].base += str * 1.0f;
    calcs[static_cast<size_t>(StatType::MaxHealth)].base += vit * 10.0f;
    calcs[static_cast<size_t>(StatType::MaxMana)].base += calcs[static_cast<size_t>(StatType::Intelligence)].Result() * 2.0f;

    // 5. Finalize Secondary Stats
    combat.max_health = calcs[static_cast<size_t>(StatType::MaxHealth)].Result();
    combat.max_mana = calcs[static_cast<size_t>(StatType::MaxMana)].Result();
    combat.armor = calcs[static_cast<size_t>(StatType::Armor)].Result();
    combat.move_speed = calcs[static_cast<size_t>(StatType::MoveSpeed)].Result();
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