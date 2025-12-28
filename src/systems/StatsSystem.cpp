#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include "../components/PlayerState.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay {

// Helper: Reset CombatStats to default state
static void resetCombatStats(CombatStats& combat) {
    // --- Offensive ---
    combat.damage_multipliers.fill(1.0f); 
    combat.flat_damage.fill(0.0f);
    combat.crit_chance = 0.05f;           
    combat.crit_damage = 1.50f;           
    combat.attack_speed = 1.0f;
    combat.cast_speed = 1.0f;
    combat.min_weapon_damage = 0.0f; // Reset weapon dmg too, will be re-added
    combat.max_weapon_damage = 0.0f;
    
    // --- Defensive ---
    combat.armor = 0.0f;
    combat.dodge_chance = 0.0f;
    combat.resistances.fill(0.0f);
    
    // --- Resources ---
    combat.max_health = 100.0f; // Default base
    combat.max_mana = 100.0f;
    combat.health_regen = 0.0f;
    
    // --- Utility ---
    combat.move_speed = 300.0f; 
    combat.cast_range = 0.0f;
}

// Helper: Apply a single affix to either PrimaryStats or CombatStats
static void applyAffix(const Affix& affix, PrimaryStats& primary, CombatStats& combat) {
    switch (affix.type) {
        // Primary
        case AffixType::Strength:     primary.strength += affix.value; break;
        case AffixType::Dexterity:    primary.dexterity += affix.value; break;
        case AffixType::Intelligence: primary.intelligence += affix.value; break;
        case AffixType::Vitality:     primary.vitality += affix.value; break;
        
        // Offensive Flat
        case AffixType::FlatPhysicalDamage:  combat.flat_damage[(int)DamageType::Physical] += affix.value; break;
        case AffixType::FlatFireDamage:      combat.flat_damage[(int)DamageType::Fire] += affix.value; break;
        case AffixType::FlatColdDamage:      combat.flat_damage[(int)DamageType::Cold] += affix.value; break;
        case AffixType::FlatLightningDamage: combat.flat_damage[(int)DamageType::Lightning] += affix.value; break;
        case AffixType::FlatPoisonDamage:    combat.flat_damage[(int)DamageType::Poison] += affix.value; break;
        case AffixType::FlatShadowDamage:    combat.flat_damage[(int)DamageType::Shadow] += affix.value; break;

        // Offensive Percent
        case AffixType::PercentPhysicalDamage:  combat.damage_multipliers[(int)DamageType::Physical] += affix.value / 100.0f; break;
        case AffixType::PercentFireDamage:      combat.damage_multipliers[(int)DamageType::Fire] += affix.value / 100.0f; break;
        case AffixType::PercentColdDamage:      combat.damage_multipliers[(int)DamageType::Cold] += affix.value / 100.0f; break;
        case AffixType::PercentLightningDamage: combat.damage_multipliers[(int)DamageType::Lightning] += affix.value / 100.0f; break;
        case AffixType::PercentPoisonDamage:    combat.damage_multipliers[(int)DamageType::Poison] += affix.value / 100.0f; break;
        case AffixType::PercentShadowDamage:    combat.damage_multipliers[(int)DamageType::Shadow] += affix.value / 100.0f; break;

        // Crit / Speed
        case AffixType::CritChance:   combat.crit_chance += affix.value / 100.0f; break;
        case AffixType::CritDamage:   combat.crit_damage += affix.value / 100.0f; break;
        case AffixType::AttackSpeed:  combat.attack_speed += affix.value / 100.0f; break;
        case AffixType::CastSpeed:    combat.cast_speed += affix.value / 100.0f; break;

        // Defensive
        case AffixType::FlatArmor:    combat.armor += affix.value; break;
        case AffixType::FlatHealth:   combat.max_health += affix.value; break;
        case AffixType::FlatMana:     combat.max_mana += affix.value; break;
        
        // Utility
        case AffixType::MoveSpeed:    combat.move_speed *= (1.0f + affix.value / 100.0f); break; // Multiplicative usually? Or Additive base?
                                      // Let's treat it as multiplier to base speed (300).
                                      // Actually, move_speed in CombatStats is final value.
                                      // So if base is 300, +10% means += 30.
                                      // But we reset to 300. So += 300 * (val/100).
                                      combat.move_speed += 300.0f * (affix.value / 100.0f);
                                      break;

        default: break;
    }
}

// Helper: Convert Primary Stats to Combat Stats (The "Scaling" logic)
static void applyPrimaryToCombat(const PrimaryStats& primary, CombatStats& combat) {
    // STR: +1% Phys Dmg, +5 Armor
    combat.damage_multipliers[(int)DamageType::Physical] += primary.strength * 0.01f;
    combat.armor += primary.strength * 5.0f;

    // DEX: +0.1% Crit, +0.1% Dodge, +1% AtkSpeed
    combat.crit_chance += primary.dexterity * 0.001f;
    combat.dodge_chance += primary.dexterity * 0.001f;
    combat.attack_speed += primary.dexterity * 0.01f;

    // INT: +1% Elem Dmg, +0.05% Res, +2 Max Mana
    float int_dmg_bonus = primary.intelligence * 0.01f;
    combat.damage_multipliers[(int)DamageType::Fire]      += int_dmg_bonus;
    combat.damage_multipliers[(int)DamageType::Cold]      += int_dmg_bonus;
    combat.damage_multipliers[(int)DamageType::Lightning] += int_dmg_bonus;
    
    float res_val = primary.intelligence * 0.0005f; 
    for(auto& r : combat.resistances) r += res_val;

    combat.max_mana += (primary.intelligence * 2.0f);

    // VIT: +10 Max HP, +0.2 HP Regen
    combat.max_health += (primary.vitality * 10.0f);
    combat.health_regen += primary.vitality * 0.2f;
}

void StatsSystem::update(entt::registry& registry) {
    // LOG_TRACE("StatsSystem::update: Baking combat stats for entities");

    auto view = registry.view<CombatStats>();

    view.each([&](entt::entity entity, CombatStats& combat) {
        // 1. Reset
        resetCombatStats(combat);

        PrimaryStats currentPrimary = {0,0,0,0};
        float baseHealth = 100.0f;

        // 2. Base Stats (Player vs Monster)
        if (registry.all_of<PlayerStats>(entity)) {
            const auto& pStats = registry.get<PlayerStats>(entity);
            // LOG_TRACE("StatsSystem: Baking stats for Player (Level {})", pStats.level);
            // Example Growth: 10 base + 2 per level
            float baseStat = 10.0f + (pStats.level - 1) * 2.0f;
            currentPrimary = {baseStat, baseStat, baseStat, baseStat};
            baseHealth = 100.0f + (pStats.level - 1) * 10.0f;
        } else if (registry.all_of<PrimaryStats>(entity)) {
            // Non-scaling entities (monsters) use their stored PrimaryStats as base
            currentPrimary = registry.get<PrimaryStats>(entity);
            baseHealth = combat.max_health; // Keep what was set in reset? No, reset sets to 100.
                                            // We need to know "Base HP" for monsters.
                                            // Issue: resetCombatStats wipes monster HP config.
                                            // Fix: Don't wipe everything if it's not a player?
                                            // OR: Monsters should have a "BaseStats" component separate from "CombatStats" (Baked).
                                            // For now: assume monsters rely on manual setup and we don't fully wipe their "Base" if they don't have PlayerStats?
                                            // Let's just re-apply PrimaryStats for them.
        }
        
        // Fix for Monster HP: resetCombatStats sets max_health to 100.
        // If entity is NOT a player, we might overwrite their custom HP.
        // Solution: CombatStats shouldn't hold the *Configuration*. 
        // For monsters, maybe we shouldn't run this full logic if they don't have equipment?
        // Let's restrict: Only run full bake if entity has PlayerStats OR Equipment.
        // If it's a simple monster, maybe we skip or handle differently.
        // BUT, we want Attributes to work for monsters too.
        // Let's assume monsters have PrimaryStats configured, and we derive everything.
        
        combat.max_health = baseHealth;

        // 3. Base Weapon/Skill Stats (Range & Base Damage)
        // 无论是否穿戴装备，WeaponComponent 决定了基础的攻击距离和冷却参考
        if (registry.all_of<WeaponComponent>(entity)) {
             const auto& w = registry.get<WeaponComponent>(entity);
             combat.cast_range = w.range;
             
             // 默认基础伤害（如果没有装备覆盖）
             float spread = 0.1f;
             combat.min_weapon_damage = w.damage * (1.0f - spread);
             combat.max_weapon_damage = w.damage * (1.0f + spread);
        }

        // 4. Apply Equipment (Overrides/Adds to base)
        if (registry.all_of<EquipmentComponent>(entity)) {
            const auto& equip = registry.get<EquipmentComponent>(entity);
            for(auto itemEntity : equip.slots) {
                if(registry.valid(itemEntity) && registry.all_of<ItemComponent>(itemEntity)) {
                    const auto& item = registry.get<ItemComponent>(itemEntity);
                    
                    // 如果主手装备有攻击力，则覆盖基础伤害
                    if (item.slot == EquipmentSlot::MainHand && item.attack > 0) {
                         float spread = 0.1f;
                         combat.min_weapon_damage = item.attack * (1.0f - spread);
                         combat.max_weapon_damage = item.attack * (1.0f + spread);
                    }
                    
                    combat.armor += item.defense;
                    for(const auto& a : item.implicits) applyAffix(a, currentPrimary, combat);
                    for(const auto& a : item.affixes) applyAffix(a, currentPrimary, combat);
                }
            }
        }

        // 4. Commit Primary Stats
        if (registry.all_of<PrimaryStats>(entity)) {
            registry.replace<PrimaryStats>(entity, currentPrimary);
        }

        // 5. Final Calculation
        applyPrimaryToCombat(currentPrimary, combat);

        // 6. Update Runtime Health
        if (registry.all_of<HealthComponent>(entity)) {
            auto& hp = registry.get<HealthComponent>(entity);
            if (std::abs(hp.max - combat.max_health) > 0.1f) {
                LOG_DEBUG("StatsSystem: Entity {} max health changed from {:.1f} to {:.1f}", (uint32_t)entity, hp.max, combat.max_health);
                float ratio = hp.current / hp.max;
                hp.max = combat.max_health;
                hp.current = hp.max * ratio; // Keep %
            }
        }
    });
}

} // namespace NoMoreDay
