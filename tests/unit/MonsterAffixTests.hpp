#include "TestCommon.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/data/MonsterAffixRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Monster Affix Persistence Test") {
    entt::registry registry;
    
    // 1. Create a dummy enemy with Berserker affix
    auto enemy = registry.create();
    registry.emplace<Position>(enemy, 100.0f, 100.0f);
    registry.emplace<HealthComponent>(enemy, 100.0f, 100.0f);
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD, EnemyArchetype::FODDER);
    
    auto& stats = registry.emplace<CombatStats>(enemy);
    auto& affix = registry.emplace<MonsterAffixComponent>(enemy);
    affix.AddAffix(MonsterAffixType::Berserker);
    
    // Initial state (not berserk)
    registry.emplace<StatsDirty>(enemy);
    StatsSystem::update(registry);
    float normalDamage = stats.min_weapon_damage;
    CHECK(normalDamage > 0.0f);

    // 2. Trigger Berserker (HP < 50%)
    {
        auto& hp = registry.get<HealthComponent>(enemy);
        hp.current = 10.0f; // 10% HP
    }
    
    MonsterAffixSystem::Update(registry, 0.1f);
    StatsSystem::update(registry); // Must update to see changes from isBerserk
    
    // Verify Berserker is active
    CHECK(affix.isBerserk == true);
    // Berserker should make it 2.0x normal damage
    CHECK(stats.min_weapon_damage == doctest::Approx(normalDamage * 2.0f));
    float berserkDamage = stats.min_weapon_damage;
    
    // 3. Trigger Recalculate (Simulation of a buff being added)
    registry.emplace<StatsDirty>(enemy);
    StatsSystem::update(registry);
    
    // Verify if damage is still berserk
    CHECK(stats.min_weapon_damage == doctest::Approx(berserkDamage));
}

TEST_CASE("Monster Affix Stat Mod Persistence Test") {
    entt::registry registry;
    
    // 1. Create enemy with Fast affix (+50% MoveSpeed)
    auto enemy = registry.create();
    registry.emplace<Position>(enemy, 100.0f, 100.0f);
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD, EnemyArchetype::FODDER);
    
    // Initial calculation to get base speed
    registry.emplace<CombatStats>(enemy);
    registry.emplace<StatsDirty>(enemy);
    StatsSystem::update(registry);
    
    auto& stats = registry.get<CombatStats>(enemy);
    float baseSpeed = stats.move_speed; // Undead base is 40.0f
    
    auto& affix = registry.emplace<MonsterAffixComponent>(enemy);
    affix.AddAffix(MonsterAffixType::Fast);
    
    // Trigger Recalculate to apply affix mods
    registry.emplace<StatsDirty>(enemy);
    StatsSystem::update(registry);
    
    float fastSpeed = baseSpeed * 1.5f;
    CHECK(stats.move_speed == doctest::Approx(fastSpeed));
    
    // 2. Trigger Recalculate again
    registry.emplace<StatsDirty>(enemy);
    StatsSystem::update(registry);
    
    // Verify if speed is still correct
    CHECK(stats.move_speed == doctest::Approx(fastSpeed));
}
