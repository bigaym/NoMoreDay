#pragma once
#include "TestCommon.hpp"
#include "../src/systems/ProjectileSystem.hpp"
#include "../src/systems/CombatSystem.hpp"
#include "../src/utils/PhysicsUtils.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"

using namespace NoMoreDay;

TEST_CASE("Physics Interaction Logic") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1000, 1000, 50);

    SUBCASE("Knockback Application") {
        auto target = registry.create();
        registry.emplace<Position>(target, 100.0f, 100.0f);
        auto& vel = registry.emplace<Velocity>(target, 0.0f, 0.0f);

        // Source is to the left (-10, 0 relative to target)
        Utils::ApplyKnockback(registry, target, {90.0f, 100.0f}, 10.0f);

        // Expect knockback to the right (+X)
        CHECK(vel.vx == doctest::Approx(10.0f));
        CHECK(vel.vy == doctest::Approx(0.0f));
    }

    SUBCASE("Projectile Knockback") {
        auto attacker = registry.create();
        auto target = registry.create();
        
        registry.emplace<Position>(target, 200.0f, 200.0f);
        auto& tVel = registry.emplace<Velocity>(target, 0.0f, 0.0f);
        registry.emplace<HealthComponent>(target, 100.0f, 100.0f);

        auto projEnt = registry.create();
        auto& proj = registry.emplace<Projectile>(projEnt);
        proj.owner = attacker;
        proj.snapshot.knockback = 50.0f; // High knockback
        proj.radius = 10.0f;
        
        // Projectile is right on top of target
        registry.emplace<Position>(projEnt, 190.0f, 200.0f); // 10 units left
        registry.emplace<Velocity>(projEnt, 100.0f, 0.0f);
        
        // Build Grid
        auto view = registry.view<Position>();
        grid.rebuild(view, registry);

        ProjectileSystem::Update(registry, grid, 0.1f);

        // Target should have been hit and knocked back
        // Direction from 190,200 to 200,200 is (1,0)
        // Force 50
        CHECK(tVel.vx == doctest::Approx(50.0f));
        CHECK(tVel.vy == doctest::Approx(0.0f));
        
        // Projectile should be destroyed (no pierce)
        CHECK_FALSE(registry.valid(projEnt));
    }

    SUBCASE("Projectile Piercing") {
        auto attacker = registry.create();
        
        // Target 1
        auto t1 = registry.create();
        registry.emplace<Position>(t1, 100.0f, 100.0f);
        registry.emplace<Velocity>(t1);
        registry.emplace<HealthComponent>(t1, 100.0f, 100.0f);

        // Target 2
        auto t2 = registry.create();
        registry.emplace<Position>(t2, 105.0f, 100.0f); // Very close
        registry.emplace<Velocity>(t2);
        registry.emplace<HealthComponent>(t2, 100.0f, 100.0f);

        auto projEnt = registry.create();
        auto& proj = registry.emplace<Projectile>(projEnt);
        proj.owner = attacker;
        proj.radius = 20.0f; // Large radius to hit both
        proj.pierce = true;
        proj.pierceCount = 1; // Can hit 1 extra target

        registry.emplace<Position>(projEnt, 100.0f, 100.0f);
        registry.emplace<Velocity>(projEnt, 10.0f, 0.0f);

        // Build Grid
        auto view = registry.view<Position>();
        grid.rebuild(view, registry);

        ProjectileSystem::Update(registry, grid, 0.1f);

        // Verify hits by checking health damage
        // Targets started with 100 HP. Should take ~5 damage (min damage in system).
        auto& hp1 = registry.get<HealthComponent>(t1);
        auto& hp2 = registry.get<HealthComponent>(t2);
        
        CHECK(hp1.current < 100.0f);
        CHECK(hp2.current < 100.0f);

        // Verify destruction
        CHECK_FALSE(registry.valid(projEnt));
    }
}
