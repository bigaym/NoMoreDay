#pragma once
#include "TestCommon.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/Common.hpp"

TEST_CASE("Increased Damage Bug Reproduction") {
    entt::registry registry;
    auto entity = registry.create();

    // 1. Setup Base Stats
    registry.emplace<CombatStats>(entity);
    // Base damage is 100% (1.0 multiplier)
    
    // 2. Add Unconditional Increased Damage (e.g. from Strength or passive)
    // Strength 100 => +100% Physical Damage (1 point = 1%)
    registry.emplace<PrimaryStats>(entity, PrimaryStats{ .strength = 100.0f });
    
    // 3. Add Conditional Increased Damage (e.g. from Skill/Buff with Tag)
    // +50% Physical Damage if "Melee" tag is present
    
    ModifierList mods;
    mods.modifiers.push_back({
        StatType::PhysicalDamage, 
        ModifierMode::PercentAdd, 
        50.0f, 
        Tag::Melee, 
        ModifierSource::Skill
    });
    registry.emplace<ModifierList>(entity, mods);
    registry.emplace<StatsDirty>(entity);

    // 4. Run System to calculate base stats
    StatsSystem::update(registry);
    
    // Verify Unconditional Stats first
    const auto& combat = registry.get<CombatStats>(entity);
    // Base 100% + Strength 100% = 200% = 2.0x Multiplier
    CHECK(combat.damage_multipliers[(int)DamageType::Physical] == doctest::Approx(2.0f));

    // 5. Query with Tag
    // We expect: Base(100%) + Str(100%) + Mod(50%) = 250% = 2.5x
    // Current Bug: (Base(100%) + Str(100%)) * (100% + Mod(50%)) = 2.0 * 1.5 = 3.0x
    
    float damageWithTag = StatsSystem::GetStatWithTags(registry, entity, StatType::PhysicalDamage, Tag::Melee);
    
    // Assert the CORRECT value. This should FAIL if the bug is present.
    // 2.5x multiplier * 100.0f base = 250.0f
    CHECK(damageWithTag == doctest::Approx(250.0f));
}
