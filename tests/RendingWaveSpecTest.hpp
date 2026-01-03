#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/systems/DamagePipeline.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/systems/PhysicsSystem.hpp"

TEST_CASE("RendingWave: Branch A - Fen Hai (Extra Waves)") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    active.slots[0].id = 2; // Rending Wave
    active.slots[0].current_charges = 1;
    active.specialized_slots[1].skill_id = 2; // Index 1 is Rending Wave in GameplayState setup
    
    SUBCASE("Default 1 Wave") {
        SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
        for(int i=0; i<10; ++i) SkillSystem::Update(registry, grid, 0.02f);
        
        auto view = registry.view<Projectile>();
        int count = 0;
        for(auto ent : view) count++;
        CHECK(count >= 1);
    }

    SUBCASE("Fen Hai +2 Waves") {
        active.specialized_slots[1].allocated_points[210] = 2; // Fen Hai
        
        SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
        for(int i=0; i<10; ++i) SkillSystem::Update(registry, grid, 0.02f);
        
        auto view = registry.view<Projectile>();
        int count = 0;
        for(auto ent : view) count++;
        CHECK(count >= 3);
    }
}

TEST_CASE("RendingWave: Branch B - Fan Tian (Boomerang)") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    active.slots[0].id = 2;
    active.slots[0].current_charges = 1;
    active.specialized_slots[1].skill_id = 2;
    active.specialized_slots[1].allocated_points[220] = 1; // Fan Tian

    SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
    for(int i=0; i<10; ++i) SkillSystem::Update(registry, grid, 0.02f);
    
    auto view = registry.view<Projectile, BoomerangComponent, Velocity>();
    REQUIRE(view.begin() != view.end());
    
    auto entity = *view.begin();
    auto& bc = view.get<BoomerangComponent>(entity);
    auto& vel = view.get<Velocity>(entity);
    float initialVx = vel.vx;

    // Run physics until boomerang returns
    // bc.returnTimer is 0.5s. 0.02f * 30 = 0.6s
    for(int i=0; i<30; ++i) {
        PhysicsSystem::updateAll(registry, 0.02f, 2000, 2000, grid);
    }
    
    // Check if phase changed and velocity reversed (approx)
    // If it reached owner it might be destroyed, so check if valid
    if (registry.valid(entity)) {
        CHECK(bc.phase == BoomerangComponent::Returning);
        CHECK(vel.vx < 0); // Fired at +X, should return with -X
    }
}

TEST_CASE("RendingWave: Branch C - Intent Scaling") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.stacks = 10;
    
    active.slots[0].id = 2;
    active.slots[0].current_charges = 1;
    active.specialized_slots[1].skill_id = 2;
    active.specialized_slots[1].allocated_points[230] = 1; // 5% per stack -> +50% More

    SkillSystem::TryCast(registry, player, 0, {100.0f, 0.0f});
    for(int i=0; i<10; ++i) SkillSystem::Update(registry, grid, 0.02f);
    
    auto view = registry.view<Projectile, CombatStats>();
    REQUIRE(view.begin() != view.end());
    
    auto& stats = view.get<CombatStats>(*view.begin());
    // Default multiplier is 1.0 (or 0 if uninit, but SkillSystem ensures >0)
    // Our logic does: mult *= (1.0 + 0.5)
    CHECK(stats.damage_multipliers[0] >= 1.49f);
}
