#include "TestCommon.hpp"
#include "game/components/Combat.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/behaviors/BladeWard.cpp" // To register behavior? Or just component manually.


namespace NoMoreDay {

TEST_CASE("Defense Mechanics") {
  LoggerScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50);

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<Velocity>(defender, 0.0f, 0.0f); // Required for grid
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  registry.emplace<CombatStats>(defender);

  SUBCASE("Phantom Flash Counter") {
    // Setup Phantom Flash state
    auto &pf = registry.emplace<PhantomFlashComponent>(defender);
    pf.counter_window = 0.5f;
    pf.triggered = false;

    // Attack
    DamagePool pool;
    pool.Add(Tag::Physical, 50.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null);

    // Should take NO damage
    CHECK(result.total_damage == doctest::Approx(0.0f));

    // Should be triggered
    CHECK(pf.triggered == true);

    // Health should be full
    CHECK(registry.get<HealthComponent>(defender).current == 100.0f);
  }

  SUBCASE("Blade Ward Interception") {
    // Setup Blade Ward
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 100; // 100 * 15% > 100% chance to ensure intercept
    ward.interception_chance = 1.0f; // Force it

    // Projectile setup
    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 10.0f, 0.0f); // Same pos as defender
    registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f);
    auto &proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = attacker;
    proj.radius = 5.0f;
    proj.lifeTime = 1.0f;

    // Update ProjectileSystem
    // Note: ProjectileSystem checks grid. Need to rebuild grid.
    auto view = registry.view<Position>();
    grid.rebuild(view, registry);

    ProjectileSystem::Update(registry, grid, 0.1f);

    // Projectile should be destroyed (intercepted)
    CHECK_FALSE(registry.valid(proj_ent));

    // Sword count should decrease
    CHECK(ward.sword_count == 99);
  }
}

} // namespace NoMoreDay
