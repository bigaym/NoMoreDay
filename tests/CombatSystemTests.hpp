#pragma once
#include "TestCommon.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"
#include "game/systems/combat/XPAwardingSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

TEST_CASE("Damage Calculation - Armor Mitigation") {
    CombatStats attacker;
    CombatStats defender;
    
    float baseDamage = 100.0f;
    
    defender.armor = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(100.0f));
    
    defender.armor = 100.0f;
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(50.0f));
}

TEST_CASE("Damage Calculation - Resistance") {
    CombatStats attacker;
    CombatStats defender;
    
    float baseDamage = 100.0f;
    
    defender.resistances[(int)DamageType::Fire] = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(100.0f));
    
    defender.resistances[(int)DamageType::Fire] = 0.5f;
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(50.0f));
    
    defender.resistances[(int)DamageType::Fire] = 0.9f;
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(25.0f));
}

TEST_CASE("CombatSystem - ApplyDamage") {
    entt::registry registry;
    auto entity = registry.create();
    
    HealthComponent health;
    health.max = 100.0f;
    health.current = 100.0f;
    registry.emplace<HealthComponent>(entity, health);

    bool dead = CombatSystem::ApplyDamage(registry, entity, 30.0f);
    CHECK(dead == false);
    CHECK(registry.get<HealthComponent>(entity).current == doctest::Approx(70.0f));

    dead = CombatSystem::ApplyDamage(registry, entity, 80.0f);
    CHECK(dead == true);
    CHECK(registry.valid(entity) == true);
    CHECK(registry.all_of<KilledTag>(entity) == true);
}

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
    
    a_stats.crit_damage = 1.5f; 
    a_stats.crit_chance = 0.0f; 

    SUBCASE("Basic Multipliers (Inc & More)") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical});
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentMult, 20.0f, Tag::Physical});
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        CHECK(result.total_damage == doctest::Approx(198.0f));
    }

    SUBCASE("Conversion Logic") {
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        global_mods.modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});
        mod_list.modifiers.push_back({StatType::FireDamage, ModifierMode::PercentAdd, 100.0f, Tag::Fire});
        mod_list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Physical});
        
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        CHECK(result.total_damage == doctest::Approx(275.0f));
    }
}

TEST_CASE("ProgressionSystem Tests") {
    SUBCASE("XP Scaling") {
        CHECK(ProgressionSystem::CalculateRequiredXP(1) == doctest::Approx(100.0f));
        CHECK(ProgressionSystem::CalculateRequiredXP(2) > 100.0f);
    }

    SUBCASE("Level Up Logic") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<PlayerLevel>(player, 1);
        registry.emplace<PlayerStats>(player, 0ULL, 0ULL, 1, 0.0f, 100.0f, 0, 0, 10, 10, 10, 10);
        registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
        
        ProgressionSystem::AddExperience(registry, player, 110.0f);
        const auto& updatedStats = registry.get<PlayerStats>(player);
        CHECK(updatedStats.level == 2);
        CHECK(updatedStats.current_xp == doctest::Approx(10.0f));
    }
}

TEST_CASE("Increased Damage Bug Reproduction") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity, PrimaryStats{ .strength = 100.0f });
    
    ModifierList mods;
    mods.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f, Tag::Melee, ModifierSource::Skill});
    registry.emplace<ModifierList>(entity, mods);
    registry.emplace<StatsDirty>(entity);

    StatsSystem::update(registry);
    float damageWithTag = StatsSystem::GetStatWithTags(registry, entity, StatType::PhysicalDamage, Tag::Melee);
    CHECK(damageWithTag == doctest::Approx(250.0f));
}

TEST_CASE("Shadow Kill Array: Duplication Logic") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    auto& playerStats = registry.emplace<PlayerStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(player);

    stats.mana = 100.0f;
    playerStats.last_shadow_trigger_time = -10.0f;
    active.slots[1].id = 2; active.slots[1].current_charges = 3;

    SUBCASE("Basic Duplication") {
        registry.emplace<ShadowKillArrayReady>(player);
        CHECK(SkillSystem::TryCast(registry, player, 1, {100, 0}));
        CHECK(stats.mana == 77.5f);
        CHECK(registry.view<ShadowComponent, ShadowCloneComponent>().size_hint() == 1);
        CHECK_FALSE(registry.any_of<ShadowKillArrayReady>(player));
    }
}

} // namespace NoMoreDay
