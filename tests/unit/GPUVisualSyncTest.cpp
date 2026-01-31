#include "doctest.h"
#include "engine/render/GPUEntitySync.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;
using namespace NoMoreDay::render;
using namespace NoMoreDay::components;

TEST_CASE("[Unit] GPUVisualSync - Data Propagation") {
  entt::registry registry;
  GPUVisualSync visualSync;

  // Config: refresh every 5 frames
  GPUVisualSync::Config config;
  config.refreshInterval = 5;
  visualSync.Init(config);

  std::vector<GPUVisualStats> buffer;
  buffer.resize(10); // 10 slots

  auto e1 = registry.create();
  registry.emplace<GPUIndex>(e1, 0); // Slot 0
  auto &stats = registry.emplace<CombatStats>(e1);
  stats.min_weapon_damage = 100.0f;
  stats.max_weapon_damage = 100.0f;
  stats.attack_speed = 2.0f;

  registry.emplace<StatsDirty>(e1); // Force sync

  SUBCASE("Basic Stats Sync") {
    auto updated = visualSync.Execute(registry, buffer, 1, 10.0f);

    CHECK(buffer[0].weaponDamage == doctest::Approx(100.0f));
    CHECK(buffer[0].attackSpeed == doctest::Approx(2.0f));
    CHECK(updated.size() == 1);
    CHECK(updated[0] == 0);
  }

  SUBCASE("Status Effect Sync") {
    auto &effects = registry.emplace<ActiveEffectsComponent>(e1);
    BuffEffect burn;
    burn.type = NoMoreDay::BuffType::Burn;
    burn.id = "burn";
    effects.AddOrRefresh(burn);

    visualSync.Execute(registry, buffer, 1, 12.0f);

    // Check if Burn bit is set
    uint32_t expected = NoMoreDay::Constants::GPU::STATUS_BURNING;
    CHECK((buffer[0].activeStatusMask & expected) == expected);
  }

  SUBCASE("Dirty Flag Optimization") {
    // First run (dirty)
    auto updated = visualSync.Execute(registry, buffer, 1, 1.0f);
    CHECK(buffer[0].weaponDamage == doctest::Approx(100.0f));
    CHECK(updated.size() == 1);

    // Clear dirty, modify stats
    registry.remove<StatsDirty>(e1);
    stats.min_weapon_damage = 999.0f;
    stats.max_weapon_damage = 999.0f;

    // Second run (not dirty, frame 2 % 5 != 0)
    updated = visualSync.Execute(registry, buffer, 2, 2.0f);

    // Should NOT update stats (optimization)
    CHECK(buffer[0].weaponDamage == doctest::Approx(100.0f));
    CHECK(updated.empty());

    // Third run (frame 5 % 5 == 0 -> periodic refresh)
    updated = visualSync.Execute(registry, buffer, 5, 5.0f);
    CHECK(buffer[0].weaponDamage == doctest::Approx(999.0f));
    CHECK(updated.size() == 1);
  }
}
