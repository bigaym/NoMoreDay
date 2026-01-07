#pragma once
#include "TestCommon.hpp"
#include "../src/systems/ProjectileSystem.hpp"
#include "../src/systems/DamagePipeline.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Projectile Snapshotting Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto attacker = registry.create();
    auto defender = registry.create();
    auto proj_ent = registry.create();

    auto& a_stats = registry.emplace<CombatStats>(attacker);
    auto& d_stats = registry.emplace<CombatStats>(defender);
    auto& mod_list = registry.emplace<ModifierList>(attacker);
    
    // 1. Initial State: Attacker has +50% Inc Physical
    mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::None});
    StatsSystem::Recalculate(registry, attacker);
    
    // Verify baked multiplier is 1.5
    CHECK(registry.get<CombatStats>(attacker).damage_multipliers[0] == doctest::Approx(1.5f));

    // 2. Fire Projectile: Snapshot attacker stats
    auto& proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = attacker;
    proj.snapshot = registry.get<CombatStats>(attacker);
    registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    registry.emplace<SkillComponent>(proj_ent, 2u, attacker); // Skill 2: Rending Wave
    
    // 3. Change Attacker Stats: Remove bonus, add huge penalty
    mod_list.modifiers.clear();
    mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, -90.0f, Tag::None});
    StatsSystem::Recalculate(registry, attacker);
    
    // Verify attacker is now weak (0.1 multiplier)
    CHECK(registry.get<CombatStats>(attacker).damage_multipliers[0] == doctest::Approx(0.1f));

    // 4. Calculate Damage for the IN-FLIGHT projectile
    // It should still use the SNAPSHOTTED stats (1.5 multiplier)
    DamagePool base;
    // Rending Wave base damage is 25.0
    auto result = DamagePipeline::Calculate(registry, proj_ent, defender, 2, base, Tag::Hit | Tag::Projectile, proj_ent);
    
    // Expected: 25.0 * 1.5 = 37.5
    // If it incorrectly used attacker's current stats: 25.0 * 0.1 = 2.5
    CHECK(result.total_damage == doctest::Approx(37.5f));
    
    // 5. Cleanup
    registry.destroy(attacker);
    registry.destroy(defender);
    registry.destroy(proj_ent);
}
