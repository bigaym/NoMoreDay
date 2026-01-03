#pragma once
#include "TestCommon.hpp"
#include "../src/systems/DamagePipeline.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

TEST_CASE("SkillModifierSystem: Component Modifiers") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto attacker = registry.create();
    auto defender = registry.create();
    registry.emplace<CombatStats>(attacker);
    registry.emplace<CombatStats>(defender);
    
    DamagePool base_pool;
    base_pool.Add(Tag::Physical, 100.0f);

    SUBCASE("No Modifier") {
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base_pool, Tag::Hit);
        // Base damage pool (100) + Skill base (10) = 110
        CHECK(result.total_damage == doctest::Approx(110.0f));
    }

    SUBCASE("Attach SkillModifierComponent to Source") {
        auto source = registry.create();
        auto& modComp = registry.emplace<SkillModifierComponent>(source);
        
        // Add +50% Increased Physical Damage to the source entity
        modComp.stat_modifiers.push_back({
            StatType::PhysicalDamage,
            ModifierMode::PercentAdd,
            50.0f,
            Tag::Physical
        });

        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base_pool, Tag::Hit, source);
        // (100 + 10) * (1.0 + 0.5) = 165
        CHECK(result.total_damage == doctest::Approx(165.0f));
    }

    SUBCASE("Multiple Modifiers on Source") {
        auto source = registry.create();
        auto& modComp = registry.emplace<SkillModifierComponent>(source);
        
        // +100% Increased Physical (Stats system)
        modComp.stat_modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 100.0f});
        
        // x1.5 More Physical (Damage system - DamageModifier)
        modComp.damage_modifiers.push_back({Tag::Physical, Tag::None, 0.5f, ModifierType::More});

        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base_pool, Tag::Hit, source);
        // (110) * (1.0 + 1.0) * (1.5) = 110 * 2 * 1.5 = 330
        CHECK(result.total_damage == doctest::Approx(330.0f));
    }
}
