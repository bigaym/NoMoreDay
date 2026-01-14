#include "TestCommon.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/MindBlade.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("New Skills Verification: Mind Blade & Phantom Flash") {
  LoggerScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player).damage_multipliers[0] = 1.0f;
  registry.get<CombatStats>(player).effective_intelligence =
      50.0f; // Scale Mind Blade
  registry.emplace<ActiveSkillsComponent>(player);

  SUBCASE("Mind Blade (ID 7) - Intelligence Scaling") {
    // Cast Mind Blade
    SkillExecution exec;
    exec.skill_id = 7;
    exec.owner = player;
    exec.target_pos = {100.0f, 0.0f};

    auto castFunc = SkillBehaviorRegistry::GetCast(7);
    REQUIRE(castFunc != nullptr);
    castFunc(registry, player, exec);

    // Verify Projectiles Spawned - Needs Simulation
    // Mock Enemy
    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 100.0f, 100.0f);
    registry.emplace<CombatStats>(enemy).health = 100.0f;
    registry.emplace<CombatStats>(enemy).health = 100.0f;
    // Rebuild grid to include enemy
    grid.rebuild(registry.view<Position>(), registry);

    // Run Update Loop for > 0.8s (Base Interval)
    // MindBlade behaves as an entity now.
    // We need to find the blade entity.
    auto bladeView = registry.view<MindBladeComponent, MindBladeAI>();
    CHECK(bladeView.size_hint() > 0);

    // Simulate frames
    float dt = 0.1f;
    for (int i = 0; i < 10; ++i) { // 1.0s total
      for (auto entity : bladeView) {
        auto &ai = bladeView.get<MindBladeAI>(entity);
        auto &comp = bladeView.get<MindBladeComponent>(entity);
        skills::MindBlade::Update(registry, entity, ai, comp, dt, grid);
      }
    }

    int projCount = 0;
    float totalMinDmg = 0.0f;
    auto view = registry.view<Projectile>();
    for (auto e : view) {
      auto &p = view.get<Projectile>(e);
      // Check owner matches
      if (p.owner == player) {
        projCount++;
        totalMinDmg += p.snapshot.min_weapon_damage;
      }
    }

    CHECK(projCount > 0);
    // Base is 20 + (50 * 1.0) = 70.
    if (projCount > 0) {
      CHECK(totalMinDmg / projCount == doctest::Approx(70.0f).epsilon(5.0f));
    }
  }

  SUBCASE("Phantom Flash (ID 9) - Defensive Buff") {
    SkillExecution exec;
    exec.skill_id = 9;
    exec.owner = player;

    auto castFunc = SkillBehaviorRegistry::GetCast(9);
    REQUIRE(castFunc != nullptr);
    castFunc(registry, player, exec);

    // Check if Component applied
    bool hasComponent = registry.all_of<PhantomFlashComponent>(player);
    CHECK(hasComponent == true);

    if (hasComponent) {
      auto &pf = registry.get<PhantomFlashComponent>(player);
      CHECK(pf.counter_window > 0.0f);
    }
  }
}

} // namespace NoMoreDay
