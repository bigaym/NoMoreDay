#pragma once
#include "TestCommon.hpp"
#include "../src/systems/DamagePipeline.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Damage Pipeline Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto attacker = registry.create();
    auto defender = registry.create();

    auto& a_stats = registry.emplace<CombatStats>(attacker);
    auto& d_stats = registry.emplace<CombatStats>(defender);
    auto& global_mods = registry.emplace<GlobalModifierComponent>(attacker);
    auto& mod_list = registry.emplace<ModifierList>(attacker);
    
    // Initialize stats
    a_stats.crit_damage = 1.5f; 
    a_stats.crit_chance = 0.0f; // Ensure no expected crit damage in simulation
    for(auto& res : d_stats.resistances) res = 0.0f; 

    SUBCASE("Basic Multipliers (Inc & More)") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        // Use ModifierList for StatsSystem scaling
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical}); // +50% Inc Phys
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentMult, 20.0f, Tag::Physical}); // x1.2 More Phys
        
        // Verify StatsSystem Output directly
        float phys_mult = StatsSystem::GetStatWithTags(registry, attacker, StatType::PhysicalDamage, Tag::Hit | Tag::Physical, 1);
        CHECK_MESSAGE(phys_mult == doctest::Approx(180.0f), "StatsSystem Mult: ", phys_mult);

        // Skill 1: Flowing Thrust (Phys + Melee). Base Pool 100 + Skill Base 10 = 110.
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        
        // (110) * (1.0 + 0.5) * (1.2) = 110 * 1.5 * 1.2 = 198
        CHECK(result.total_damage == doctest::Approx(198.0f));
    }

    SUBCASE("Tag-specific Multipliers") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        mod_list.modifiers.clear();
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Melee}); 
        
        // 1. Melee Hit (Skill 1: Flowing Thrust)
        auto result_melee = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        // (100+10) * 1.5 = 165.0
        CHECK(result_melee.total_damage == doctest::Approx(165.0f));
        
        // 2. Projectile Hit (Skill 2: Rending Wave)
        auto result_proj = DamagePipeline::Calculate(registry, attacker, defender, 2, base, Tag::Hit, entt::null, true);
        // (100+25) = 125 (No Melee bonus)
        CHECK(result_proj.total_damage == doctest::Approx(125.0f)); 
    }

    SUBCASE("Conversion Logic") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        mod_list.modifiers.clear();
        global_mods.modifiers.clear();
        
        // 100% Phys converted to Fire
        global_mods.modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});
        
        // +100% Inc Fire
        mod_list.modifiers.push_back({StatType::FireDamage, ModifierMode::PercentAdd, 100.0f, Tag::Fire});
        // +50% Inc Physical (Inherited by converted damage due to source tagging)
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical});
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        
        // Base (100+10=110) -> converts to 110 Fire
        // scaling: 100% (fire) + 50% (phys inherited) = +150% inc
        // 110 * 2.5 = 275.0
        CHECK(result.total_damage == doctest::Approx(275.0f));
    }

    SUBCASE("Critical Hits") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        mod_list.modifiers.clear();
        
        a_stats.crit_damage = 2.0f; 
        
        // Pass Tag::Critical explicitly to force crit in logic, but simulation=true ensures consistency
        auto result_crit = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit | Tag::Critical, entt::null, true);
        // (100+10) * 2.0 = 220
        CHECK(result_crit.total_damage == doctest::Approx(220.0f));
        CHECK(result_crit.is_crit);
    }

    SUBCASE("Resistance Cap Logic") {
        DamagePool base;
        base.Add(Tag::Fire, 100.0f);
        mod_list.modifiers.clear();
        
        // 1. Over-capped Resistance (90% -> should be capped at 75%)
        d_stats.resistances[(int)DamageType::Fire] = 0.90f;
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        
        // Check ONLY Fire part to avoid noise from Skill's physical damage
        // Base 100 Fire.
        // Res 0.75 (Capped). Multiplier = 0.25.
        // Fire Damage = 100 * 0.25 = 25.0.
        float fire_dmg = result.final_pool.values[(int)DamageType::Fire];
        CHECK(fire_dmg == doctest::Approx(25.0f));

        // 2. Negative Resistance (-150% -> should be capped at -100% per design doc)
        d_stats.resistances[(int)DamageType::Fire] = -1.50f;
        auto result_neg = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        
        // Base 100 Fire.
        // Res -1.0 (Capped). Multiplier = (1 - (-1)) = 2.0.
        // Fire Damage = 100 * 2.0 = 200.0.
        float fire_dmg_neg = result_neg.final_pool.values[(int)DamageType::Fire];
        CHECK(fire_dmg_neg == doctest::Approx(200.0f));
    }
    
    SUBCASE("Armor Scaling Logic") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f); // 100 Phys
        mod_list.modifiers.clear();
        
        d_stats.armor = 100.0f;
        
        // Skill 1 adds 10 Phys. Total 110.
        // Current Formula: 100 / (100 + Armor) = 100 / 200 = 0.5 multiplier.
        // Damage = 110 * 0.5 = 55.
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        
        // Verification of current behavior: 55.0f
        CHECK(result.total_damage == doctest::Approx(55.0f));
    }

    SUBCASE("Edge Cases") {
        mod_list.modifiers.clear();
        
        // 1. Zero Damage
        DamagePool zero_pool;
        auto result_zero = DamagePipeline::Calculate(registry, attacker, defender, 0, zero_pool, Tag::Hit, entt::null, true);
        CHECK(result_zero.total_damage == 0.0f);

        // 2. Extremely High Damage (Overflow check)
        DamagePool high_pool;
        high_pool.Add(Tag::Physical, 1e10f); 
        auto result_high = DamagePipeline::Calculate(registry, attacker, defender, 0, high_pool, Tag::Hit, entt::null, true);
        CHECK(result_high.total_damage > 1e9f);

        // 3. 100% Resistance (Capped at 75%)
        DamagePool fire_pool;
        fire_pool.Add(Tag::Fire, 100.0f);
        d_stats.resistances[(int)DamageType::Fire] = 1.0f; // 100%
        auto result_res = DamagePipeline::Calculate(registry, attacker, defender, 0, fire_pool, Tag::Hit, entt::null, true);
        // Should be 100 * (1 - 0.75) = 25.0
        CHECK(result_res.total_damage == doctest::Approx(25.0f));
    }
}