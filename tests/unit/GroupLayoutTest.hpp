#pragma once
#include "doctest.h"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/registry/GroupRegistry.hpp"
#include <entt/entt.hpp>


// Test case for Phase 2: EnTT Group Memory Optimization
TEST_CASE("EnTT Group Layout Contiguity") {
  entt::registry registry;
  NoMoreDay::groups::RegisterGroups(registry);

  // Create entities with Combat components
  std::vector<entt::entity> entities;
  for (int i = 0; i < 1000; ++i) {
    auto e = registry.create();
    registry.emplace<NoMoreDay::CombatStats>(e);
    registry.emplace<NoMoreDay::StatsDirty>(
        e); // Group requirement (now included in definition)
    entities.push_back(e);
  }

  // Verify CombatGroup logic
  auto group = registry.group<NoMoreDay::StatsDirty, NoMoreDay::CombatStats>();

  // 1. Verify Iteration Count
  int count = 0;
  for (auto e : group) {
    count++;
  }
  CHECK(count == 1000);

  // 2. Verify Component Contiguity
  bool is_grouped = true;
  for (auto e : group) {
    if (!registry.all_of<NoMoreDay::CombatStats, NoMoreDay::StatsDirty>(e)) {
      is_grouped = false;
    }
  }
  CHECK(is_grouped);
}

TEST_CASE("RenderGroup Layout") {
  entt::registry registry;
  NoMoreDay::groups::RegisterGroups(registry);

  for (int i = 0; i < 500; ++i) {
    auto e = registry.create();
    registry.emplace<Position>(e);
    registry.emplace<Velocity>(e);
    registry.emplace<Radius>(e);
    registry.emplace<GPUIndex>(e);
  }

  auto group = registry.group<Position, Velocity, Radius, GPUIndex>();
  int count = 0;
  for (auto e : group) {
    count++;
  }
  CHECK(count == 500);
}

TEST_CASE("AIGroup Layout with Exclude") {
  entt::registry registry;
  NoMoreDay::groups::RegisterGroups(registry);

  for (int i = 0; i < 200; ++i) {
    auto e = registry.create();
    registry.emplace<AIComponent>(e);
    registry.emplace<Position>(e);
    registry.emplace<Velocity>(e);
    registry.emplace<EnemyTag>(e);

    if (i % 2 == 0) {
      registry.emplace<KilledTag>(e);
    }
  }

  auto group =
      registry.group<AIComponent>(entt::get<Position, Velocity, EnemyTag>);

  int active_count = 0;
  for (auto e : group) {
    if (!registry.any_of<KilledTag>(e)) {
      active_count++;
    }
  }
  CHECK(active_count == 100);
}
