#include "doctest.h"

#include <entt/entt.hpp>

#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"

TEST_CASE("[Unit] CombatSystem - Player death restores HP/MP without KilledTag") {
  entt::registry registry;
  const auto player = registry.create();

  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerStats>(player);

  auto &hp = registry.emplace<HealthComponent>(player, 50.0f, 50.0f);
  auto &stats = registry.emplace<NoMoreDay::CombatStats>(player);
  stats.max_health = 123.0f;
  stats.health = 12.0f;
  stats.max_mana = 77.0f;
  stats.mana = 3.0f;
  stats.max_barrier = 42.0f;
  stats.barrier = 1.0f;

  const bool killed =
      CombatSystem::ApplyDamage(registry, player, 9999.0f, entt::null, false);

  CHECK(killed);
  CHECK_FALSE(registry.all_of<KilledTag>(player));
  CHECK(hp.current == doctest::Approx(hp.max));
  CHECK(stats.health == doctest::Approx(stats.max_health));
  CHECK(stats.mana == doctest::Approx(stats.max_mana));
  CHECK(stats.barrier == doctest::Approx(stats.max_barrier));
  CHECK(registry.get<PlayerStats>(player).deathCount == 1);
}
