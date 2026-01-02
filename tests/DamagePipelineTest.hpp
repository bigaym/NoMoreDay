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
    for(auto& res : d_stats.resistances) res = 0.0f; 

    SUBCASE("Basic Multipliers (Inc & More)") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        // Use ModifierList for StatsSystem scaling
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical}); // +50% Inc Phys
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentMult, 20.0f, Tag::Physical}); // x1.2 More Phys
        
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
        auto result_melee = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        // (100+10) * 1.5 = 165
        CHECK(result_melee.total_damage == doctest::Approx(165.0f));
        
        // 2. Projectile Hit (Skill 2: Rending Wave)
        auto result_proj = DamagePipeline::Calculate(registry, attacker, defender, 2, base, Tag::Hit);
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
        // +50% Inc Physical (Currently not inherited by converted damage in Calculate logic)
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical});
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        
        // Base (100+10=110) -> converts to 110 Fire
        // scaling: 100% (fire) = +100% inc
        // 110 * 2.0 = 220
        CHECK(result.total_damage == doctest::Approx(220.0f));
    }

    SUBCASE("Critical Hits") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        mod_list.modifiers.clear();
        
        a_stats.crit_damage = 2.0f; 
        
        auto result_crit = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit | Tag::Critical);
        // (100+10) * 2.0 = 220
        CHECK(result_crit.total_damage == doctest::Approx(220.0f));
        CHECK(result_crit.is_crit);
    }
}