#pragma once
#include "TestCommon.hpp"
#include "systems/DamagePipeline.hpp"

using namespace NoMoreDay;

TEST_CASE("Damage Pipeline Logic") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();

    auto& a_stats = registry.emplace<CombatStats>(attacker);
    auto& d_stats = registry.emplace<CombatStats>(defender);
    auto& global_mods = registry.emplace<GlobalModifierComponent>(attacker);
    
    // Initialize stats
    a_stats.crit_chance = 0.0f; // No crit for base tests
    for(auto& res : d_stats.resistances) res = 0.0f; // No res for base tests

    SUBCASE("Basic Multipliers (Inc & More)") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        global_mods.modifiers.push_back({Tag::Physical, Tag::None, 0.5f, ModifierType::Increased}); // +50% Inc Phys
        global_mods.modifiers.push_back({Tag::Physical, Tag::None, 0.2f, ModifierType::More});      // x1.2 More Phys
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit | Tag::Melee);
        
        // Calculation: 100 * (1 + 0.5) * (1.2) = 100 * 1.5 * 1.2 = 180
        CHECK(result.total_damage == doctest::Approx(180.0f));
    }

    SUBCASE("Tag-specific Multipliers") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        global_mods.modifiers.clear();
        global_mods.modifiers.push_back({Tag::Melee, Tag::None, 0.5f, ModifierType::Increased}); // +50% Inc Melee
        global_mods.modifiers.push_back({Tag::Spell, Tag::None, 1.0f, ModifierType::Increased}); // +100% Inc Spell (should not apply)
        
        // Test Melee hit
        auto result_melee = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit | Tag::Melee);
        CHECK(result_melee.total_damage == doctest::Approx(150.0f));
        
        // Test Projectile hit (should not get Melee bonus)
        auto result_proj = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit | Tag::Projectile);
        CHECK(result_proj.total_damage == doctest::Approx(100.0f));
    }

    SUBCASE("Conversion Logic") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        global_mods.modifiers.clear();
        
        // 50% Phys converted to Fire
        global_mods.modifiers.push_back({Tag::Physical, Tag::Fire, 0.5f, ModifierType::Convert});
        
        // +100% Inc Phys
        global_mods.modifiers.push_back({Tag::Physical, Tag::None, 1.0f, ModifierType::Increased});
        
        // +100% Inc Fire
        global_mods.modifiers.push_back({Tag::Fire, Tag::None, 1.0f, ModifierType::Increased});
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit);
        
        CHECK(result.total_damage == doctest::Approx(250.0f));
    }

    SUBCASE("Critical Hits") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        a_stats.crit_damage = 2.0f; // 200% crit damage
        
        // Non-crit
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit);
        CHECK(result.total_damage == 100.0f);
        CHECK_FALSE(result.is_crit);
        
        // Crit (triggered by Tag::Critical for now)
        auto result_crit = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit | Tag::Critical);
        CHECK(result_crit.total_damage == 200.0f);
        CHECK(result_crit.is_crit);
    }
}