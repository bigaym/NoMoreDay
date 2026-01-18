#pragma once
#include "doctest.h"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/RegenerationSystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include <entt/entt.hpp>

TEST_CASE("Hybrid Barrier: ES Mode Regeneration") {
    entt::registry registry;
    auto entity = registry.create();
    auto& stats = registry.emplace<NoMoreDay::CombatStats>(entity);
    registry.emplace<HealthComponent>(entity, 100.0f, 100.0f);
    auto& barrier = registry.emplace<BarrierComponent>(entity);
    
    // Disable other regens to avoid interference
    stats.health_regen = 0.0f;
    stats.mana_regen = 0.0f;
    
    // Setup ES Barrier stats directly
    stats.max_barrier = 50.0f;
    stats.barrier_regen = 10.0f;
    stats.barrier_delay = 1.0f;
    stats.barrier = 0.0f;
    
    barrier.last_damage_time = 0.0f; // Long ago
    
    // 1. Initial regen after delay
    float now = (float)GetTime();
    barrier.last_damage_time = now - 2.0f; // 2s ago, more than 1s delay
    
    NoMoreDay::RegenerationSystem::update(registry, 0.1f);
    CHECK(stats.barrier == doctest::Approx(1.0f)); // 10 regen * 0.1s
    
    // 2. Regen stops at max_barrier
    stats.barrier = 49.5f;
    NoMoreDay::RegenerationSystem::update(registry, 0.1f);
    CHECK(stats.barrier == 50.0f);
}

TEST_CASE("Hybrid Barrier: Damage Absorption & Delay") {
    entt::registry registry;
    auto entity = registry.create();
    auto& stats = registry.emplace<NoMoreDay::CombatStats>(entity);
    auto& hp = registry.emplace<HealthComponent>(entity, 100.0f, 100.0f);
    registry.emplace<BarrierComponent>(entity);
    
    // Disable other regens
    stats.health_regen = 0.0f;
    stats.mana_regen = 0.0f;
    
    stats.max_barrier = 50.0f;
    stats.barrier = 50.0f;
    stats.barrier_delay = 2.0f;
    
    // 1. Barrier absorbs damage
    CombatSystem::ApplyDamage(registry, entity, 30.0f, entt::null, false);
    CHECK(stats.barrier == 20.0f);
    CHECK(hp.current == 100.0f);
    
    // 2. Barrier resets delay
    auto& barrier = registry.get<BarrierComponent>(entity);
    float hitTime = barrier.last_damage_time;
    CHECK(hitTime > 0.0f);
    
    // Simulate time passing (but less than delay)
    NoMoreDay::RegenerationSystem::update(registry, 0.1f);
    CHECK(stats.barrier == 20.0f); // No regen yet
    
    // 3. Overflow damage hits health
    CombatSystem::ApplyDamage(registry, entity, 30.0f, entt::null, false);
    CHECK(stats.barrier == 0.0f);
    CHECK(hp.current == 90.0f);
}

TEST_CASE("Hybrid Barrier: Ward Mode Decay") {
    entt::registry registry;
    auto entity = registry.create();
    auto& stats = registry.emplace<NoMoreDay::CombatStats>(entity);
    registry.emplace<BarrierComponent>(entity);
    
    stats.max_barrier = 50.0f;
    stats.barrier = 100.0f; // Temporary ward
    stats.barrier_decay = 0.2f; // 20% per second
    stats.barrier_retention = 0.0f;
    
    // 1. Decay logic
    // Expected loss: (100 - 50) * 0.2 * 0.1s = 50 * 0.02 = 1.0
    NoMoreDay::RegenerationSystem::update(registry, 0.1f);
    CHECK(stats.barrier == doctest::Approx(99.0f));
    
    // 2. Decay stops at max_barrier (using large DT to ensure it hits limit)
    stats.barrier = 50.5f;
    NoMoreDay::RegenerationSystem::update(registry, 10.0f);
    CHECK(stats.barrier == 50.0f);
    
    // 3. Retention reduces decay
    stats.barrier = 100.0f;
    stats.barrier_retention = 1.0f; // 100% retention -> half decay
    // Effective Decay = 0.2 / (1 + 1) = 0.1
    // Expected loss: (100 - 50) * 0.1 * 0.1s = 0.5
    NoMoreDay::RegenerationSystem::update(registry, 0.1f);
    CHECK(stats.barrier == doctest::Approx(99.5f));
}

TEST_CASE("Hybrid Barrier: Stats Calculation") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<NoMoreDay::CombatStats>(entity);
    auto& primary = registry.emplace<NoMoreDay::PrimaryStats>(entity);
    auto& mods = registry.emplace<NoMoreDay::ModifierList>(entity);
    
    primary.intelligence = 100.0f; // Should give 100% retention (1.0)
    
    mods.modifiers.push_back({NoMoreDay::StatType::MaxBarrier, NoMoreDay::ModifierMode::Flat, 500.0f});
    mods.modifiers.push_back({NoMoreDay::StatType::BarrierRegen, NoMoreDay::ModifierMode::Flat, 25.0f});
    mods.modifiers.push_back({NoMoreDay::StatType::BarrierDelay, NoMoreDay::ModifierMode::Flat, -0.5f}); // Base 2.0 -> 1.5
    mods.modifiers.push_back({NoMoreDay::StatType::BarrierDecay, NoMoreDay::ModifierMode::PercentAdd, 50.0f}); // Base 20% + 50% = 30% (0.3)
    
    NoMoreDay::StatsSystem::Recalculate(registry, entity);
    
    auto& stats = registry.get<NoMoreDay::CombatStats>(entity);
    
    CHECK(stats.max_barrier == 500.0f);
    CHECK(stats.barrier_regen == 25.0f);
    CHECK(stats.barrier_delay == 1.5f);
    // Base 20% (0.2) * (1 + 50%) = 0.3
    CHECK(stats.barrier_decay == doctest::Approx(0.3f));
    CHECK(stats.barrier_retention == doctest::Approx(1.0f));
    
    CHECK(registry.all_of<BarrierComponent>(entity));
}
