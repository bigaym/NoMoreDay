#include "TestCommon.hpp"

#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/systems/ai/AISystem.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/systems/world/MapSystem.hpp"

namespace NoMoreDay {

namespace {

constexpr float kDt = 1.0f / 60.0f;

NoMoreDay::systems::SpatialHashGrid MakeGrid() {
  return NoMoreDay::systems::SpatialHashGrid(256, 256, 32.0f);
}

} // namespace

TEST_CASE("[Unit] AISystem - awakened dormant enemies stay active on the next AI tick") {
  TestSetupScope scope;
  entt::registry registry;
  MapSystem mapSystem;
  mapSystem.generateTownMap(256, 256);
  auto grid = MakeGrid();
  EnemySpawnSystem spawnSystem;

  const Position playerPos = {5000.0f, 5000.0f};

  const entt::entity enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 0.0f, 0.0f);
  registry.emplace<PrevPosition>(enemy, 0.0f, 0.0f);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
  registry.emplace<AIComponent>(enemy, AIType::IDLE, 800.0f, 30.0f, 80.0f, 0.0f);
  registry.emplace<DormantTag>(enemy);

  for (int i = 0; i < NoMoreDay::Constants::Enemy::DORMANT_CHECK_INTERVAL_FRAMES; ++i) {
    spawnSystem.updateDormantEntities(registry, playerPos, 2000, 2000);
  }

  REQUIRE_FALSE(registry.any_of<DormantTag>(enemy));

  const auto posView = registry.view<Position>();
  grid.rebuild(posView, registry);
  AISystem::update(registry, grid, mapSystem, playerPos, kDt);

  CHECK_FALSE(registry.any_of<DormantTag>(enemy));
}

TEST_CASE("[Unit] AISystem - enemies aggro within detection range even when activation range is smaller") {
  TestSetupScope scope;
  entt::registry registry;
  MapSystem mapSystem;
  mapSystem.generateTownMap(256, 256);
  auto grid = MakeGrid();

  const Position playerPos = {0.0f, 0.0f};
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, playerPos.x, playerPos.y);

  const entt::entity enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 700.0f, 0.0f);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);

  auto &ai = registry.emplace<AIComponent>(enemy, AIType::IDLE, 800.0f, 30.0f, 80.0f, 0.0f);
  ai.lastDecisionTime = 1.0f;
  ai.updateAccumulator = 1.0f;

  auto &state = registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                                      EnemyArchetype::FODDER);
  state.activationRange = 500.0f;
  state.detectionRange = 800.0f;

  const auto posView = registry.view<Position>();
  grid.rebuild(posView, registry);
  AISystem::update(registry, grid, mapSystem, playerPos, kDt);

  CHECK(ai.target == player);
  CHECK(ai.aiType == AIType::CHASE);
}

} // namespace NoMoreDay
