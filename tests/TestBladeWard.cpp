#include "TestCommon.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
// #include "game/systems/skill/behaviors/BladeWard.hpp" // Not available public
// header
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("Blade Ward Logic Verification") {
  LoggerScope scope;
  entt::registry registry;
  SkillBehaviorRegistry::Initialize();
  tools::Logger::SetLogLevel(spdlog::level::debug, 2); // Enable logs
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  SUBCASE("Blade Ward - Activation") {
    SkillExecution exec;
    exec.skill_id = 4;
    exec.owner = player;
    exec.active_nodes.set(400 % 100); // Trigger some talents?

    // Manually trigger cast since behavior registry might need linkage
    // But better to use registry if linked properly
    // NoMoreDay::skills::BladeWard::DoCast(registry, player, exec);
    auto castFunc = SkillBehaviorRegistry::GetCast(4);
    if (castFunc) {
      castFunc(registry, player, exec);
    } else {
      // Fallback not possible without internal header
      FAIL("BladeWard (ID 4) not registered!");
    }

    REQUIRE(registry.all_of<BladeWardComponent>(player));
    auto &ward = registry.get<BladeWardComponent>(player);
    CHECK(ward.sword_count == 3);
    CHECK(ward.interception_chance > 0.0f);
  }

  SUBCASE("Blade Ward - Counter Shot (Talent 470)") {
    SkillExecution exec;
    exec.skill_id = 4;
    exec.owner = player;
    exec.active_nodes.set(470 % 100); // Enable Counter Shot

    auto castFunc = SkillBehaviorRegistry::GetCast(4);
    if (castFunc) {
      castFunc(registry, player, exec);
    } else {
      FAIL("BladeWard (ID 4) not registered!");
    }

    auto &ward = registry.get<BladeWardComponent>(player);
    CHECK(ward.trigger_counter == true);
    CHECK(ward.counter_spin == false);

    // Simulate Projectile Interception
    // We need an enemy and a projectile owned by that enemy
    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 50.0f, 0.0f);
    registry.emplace<CombatStats>(enemy);
    registry.emplace<HealthComponent>(enemy, 100.0f, 100.0f);

    auto projectileEntity = registry.create();
    registry.emplace<Position>(projectileEntity, 5.0f, 0.0f); // Close to player
    registry.emplace<Projectile>(projectileEntity);
    auto &proj = registry.get<Projectile>(projectileEntity);
    proj.owner = enemy;
    registry.emplace<Velocity>(projectileEntity, -10.0f,
                               0.0f); // Moving towards player

    if (registry.any_of<BladeWardComponent>(enemy)) {
      registry.remove<BladeWardComponent>(enemy);
    }

    // We can't easily hook into ProjectileSystem's internal loop tick to force
    // interception chance to succeed. However, we can MANUALLY set interception
    // to 100% (or >1) if we hack the component for the test.
    ward.interception_chance = 20.0f; // Force success

    // We also need to simulate the collision logic.
    // ProjectileSystem::Update handles movement and collision.
    // Let's run ProjectileSystem::Update manually.

    // Rebuild grid so projectile can find player?
    // ProjectileSystem usually queries grid.
    // Rebuild grid so projectile can find player?
    grid.rebuild(registry.view<Position>(), registry);

    // Run system
    ProjectileSystem::Update(registry, grid, 0.1f);

    // Check consequences
    // 1. Sword count should decrease (unless solidified)
    CHECK(ward.sword_count == 2);

    // 2. Counter Shot should have triggered -> New SkillExecution for ID 2
    // (Rending Wave) on player? Wait, logic says: auto counter_ent =
    // registry.create(); registry.emplace<SkillExecution>(counter_ent);
    // exec.skill_id = 2; // Rending Wave

    bool foundCounter = false;
    auto view = registry.view<SkillExecution>();
    for (auto e : view) {
      auto &execution = view.get<SkillExecution>(e);
      LOG_INFO("Found SkillExecution: ID={}, Owner={}", execution.skill_id,
               (uint32_t)execution.owner);
      if (execution.skill_id == 2 && execution.owner == player) {
        foundCounter = true;
        break;
      }
    }
    CHECK(foundCounter == true);
  }

  SUBCASE("Blade Ward - Counter Spin (Talent 473)") {
    SkillExecution exec;
    exec.skill_id = 4;
    exec.owner = player;
    // Both 470 and 473 might be set, or just 473. Logic checks 470 first then
    // nested 473? Let's look at logic: if (ward->trigger_counter) { if
    // (ward->counter_spin) { ... } else { ... } } So we need 470
    // (trigger_counter) AND 473 (counter_spin) to get spin? Or just setting
    // flags in DoCast appropriately. In DoCast: if (exec.active_nodes.test(470
    // % 100)) ward.trigger_counter = true; if (exec.active_nodes.test(473 %
    // 100)) ward.counter_spin = true; So if we have 473, we usually also have
    // 470 in the tree, or at least we want the effect. BUT ProjectileSystem
    // checks: if (ward->trigger_counter) So for Spin to happen, trigger_counter
    // MUST be true. So active_nodes must have 470 set as well, OR DoCast must
    // ensure logic correctness. Let's set both for safety, or check if 473
    // implies 470.

    exec.active_nodes.set(470 % 100);
    exec.active_nodes.set(473 % 100);

    auto castFunc = SkillBehaviorRegistry::GetCast(4);
    if (castFunc) {
      castFunc(registry, player, exec);
    } else {
      FAIL("BladeWard (ID 4) not registered!");
    }

    auto &ward = registry.get<BladeWardComponent>(player);
    CHECK(ward.trigger_counter == true);
    CHECK(ward.counter_spin == true);
    ward.interception_chance = 20.0f; // Force success

    auto enemy = registry.create();
    registry.emplace<Position>(enemy, 50.0f, 0.0f);
    if (registry.any_of<BladeWardComponent>(enemy))
      registry.remove<BladeWardComponent>(enemy);

    auto projectileEntity = registry.create();
    auto &proj = registry.emplace<Projectile>(projectileEntity);
    proj.radius = 20.0f; // Increase radius to ensure detection
    proj.owner = enemy;

    registry.emplace<Position>(projectileEntity, 2.0f,
                               0.0f); // Move closer (2.0 vs 5.0)
    registry.emplace<Velocity>(projectileEntity, -10.0f, 0.0f);

    grid.rebuild(registry.view<Position>(), registry);
    ProjectileSystem::Update(registry, grid, 0.1f);

    // Check for Sword Array (ID 6) counter
    bool foundSpin = false;
    auto view = registry.view<SkillExecution>();
    for (auto e : view) {
      auto &execution = view.get<SkillExecution>(e);
      LOG_INFO("Found SkillExecution: ID={}, Owner={}", execution.skill_id,
               (uint32_t)execution.owner);
      if (execution.skill_id == 6 && execution.owner == player) {
        foundSpin = true;
        break;
      }
    }
    CHECK(foundSpin == true);
  }
}

} // namespace NoMoreDay
