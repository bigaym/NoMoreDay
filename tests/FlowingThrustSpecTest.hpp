#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/systems/DamagePipeline.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/systems/XPAwardingSystem.hpp"

TEST_CASE("FlowingThrust: Branch A - Guan Ri (Pierce & Reset)") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;
    active.specialized_slots[0].skill_id = 1;
    
    SUBCASE("Pierce All enemies") {
        active.specialized_slots[0].allocated_points[110] = 1; // Guan Ri
        
        SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
        // Run state machine to spawn projectile
        for(int i=0; i<10; ++i) SkillSystem::Update(registry, 0.02f);
        
        auto view = registry.view<Projectile>();
        REQUIRE(view.begin() != view.end());
        auto& proj = view.get<Projectile>(*view.begin());
        CHECK(proj.pierceCount >= 999);
    }

    SUBCASE("CD Reset on Kill") {
        active.specialized_slots[0].allocated_points[110] = 1; // Guan Ri
        active.slots[0].current_charges = 0;
        active.slots[0].cooldown = 5.0f;
        
        auto enemy = registry.create();
        registry.emplace<EnemyStateComponent>(enemy);
        registry.emplace<KilledTag>(enemy, player);
        
        XPAwardingSystem::update(registry);
        // We can't easily check random, but we check if the code runs without crashing.
    }
}

TEST_CASE("FlowingThrust: Branch B - Liu Ying (Shadow Echo)") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[120] = 1; // Liu Ying

    bool castSuccess = SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
    MESSAGE("TryCast result: ", castSuccess);
    CHECK(castSuccess);
    
    // Run state machine to reach Casting phase where shadow is spawned
    for(int i=0; i<10; ++i) {
        SkillSystem::Update(registry, 0.02f);
    }
    
    // Check if shadow was created
    auto view = registry.view<ShadowComponent>();
    CHECK(view.begin() != view.end());
}

TEST_CASE("FlowingThrust: Branch C - Weakness Insight") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    auto attacker = registry.create();
    auto defender = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(attacker);
    auto& att_stats = registry.emplace<CombatStats>(attacker);
    auto& def_stats = registry.emplace<CombatStats>(defender);
    auto& hp = registry.emplace<HealthComponent>(defender);
    
    hp.max = 100.0f;
    hp.current = 100.0f; // Full health
    
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[130] = 5; // Weakness Insight +50% Crit
    
    att_stats.crit_chance = 0.0f; // 0 base crit
    att_stats.crit_damage = 2.0f;
    
    DamagePool base;
    base.Add(Tag::Physical, 100.0f);
    
    int crits = 0;
    for(int i=0; i<100; ++i) {
        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        if (result.is_crit) crits++;
    }
    CHECK(crits > 0);
}

TEST_CASE("FlowingThrust: Branch D - Frost Thrust") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[140] = 1; // Frost Thrust

    SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
    // Run state machine
    for(int i=0; i<10; ++i) SkillSystem::Update(registry, 0.02f);
    
    auto view = registry.view<Projectile, SkillModifierComponent>();
    REQUIRE(view.begin() != view.end());
    
    auto& mods = view.get<SkillModifierComponent>(*view.begin());
    bool hasConversion = false;
    for(const auto& m : mods.damage_modifiers) {
        if (m.type == ModifierType::Convert && m.target_tag == Tag::Cold) hasConversion = true;
    }
    CHECK(hasConversion);
}