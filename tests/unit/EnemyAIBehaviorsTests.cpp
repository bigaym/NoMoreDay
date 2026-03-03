#include "TestCommon.hpp"

#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/systems/ai/EnemyAIBehaviors.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] EnemyAIBehaviors - FindNearestRanger filters by archetype and radius") {
  TestSetupScope scope;
  entt::registry registry;

  auto source = registry.create();
  registry.emplace<EnemyTag>(source);
  const Position sourcePos = registry.emplace<Position>(source, 0.0f, 0.0f);
  registry.emplace<EnemyStateComponent>(source, EnemyRace::UNDEAD,
                                        EnemyArchetype::TANK);

  auto rangerNear = registry.create();
  registry.emplace<EnemyTag>(rangerNear);
  registry.emplace<Position>(rangerNear, 30.0f, 0.0f);
  registry.emplace<EnemyStateComponent>(rangerNear, EnemyRace::UNDEAD,
                                        EnemyArchetype::RANGER);

  auto rangerFar = registry.create();
  registry.emplace<EnemyTag>(rangerFar);
  registry.emplace<Position>(rangerFar, 300.0f, 0.0f);
  registry.emplace<EnemyStateComponent>(rangerFar, EnemyRace::UNDEAD,
                                        EnemyArchetype::RANGER);

  auto nonRangerNear = registry.create();
  registry.emplace<EnemyTag>(nonRangerNear);
  registry.emplace<Position>(nonRangerNear, 10.0f, 0.0f);
  registry.emplace<EnemyStateComponent>(nonRangerNear, EnemyRace::UNDEAD,
                                        EnemyArchetype::FODDER);

  CHECK(AI::FindNearestRanger(registry, sourcePos, 100.0f, source) == rangerNear);
  CHECK(AI::FindNearestRanger(registry, sourcePos, 20.0f, source) ==
        entt::entity{entt::null});
}

TEST_CASE("[Unit] EnemyAIBehaviors - ApplyBuffToNearbyAllies excludes source and out-of-range") {
  TestSetupScope scope;
  entt::registry registry;

  auto source = registry.create();
  const Position sourcePos = registry.emplace<Position>(source, 0.0f, 0.0f);
  registry.emplace<EnemyTag>(source);
  registry.emplace<ActiveEffectsComponent>(source);

  auto allyNear = registry.create();
  registry.emplace<Position>(allyNear, 40.0f, 0.0f);
  registry.emplace<EnemyTag>(allyNear);
  registry.emplace<ActiveEffectsComponent>(allyNear);

  auto allyFar = registry.create();
  registry.emplace<Position>(allyFar, 250.0f, 0.0f);
  registry.emplace<EnemyTag>(allyFar);
  registry.emplace<ActiveEffectsComponent>(allyFar);

  AI::ApplyBuffToNearbyAllies(registry, source, sourcePos, 100.0f);

  const auto &sourceEffects = registry.get<ActiveEffectsComponent>(source);
  CHECK(sourceEffects.effects.empty());

  const auto &nearEffects = registry.get<ActiveEffectsComponent>(allyNear);
  REQUIRE(nearEffects.effects.size() == 1);
  CHECK(nearEffects.effects.front().id == "support_shield");
  CHECK(nearEffects.effects.front().type == BuffType::Shield);
  REQUIRE(nearEffects.effects.front().modifiers.size() == 1);
  CHECK(nearEffects.effects.front().modifiers.front().type == StatType::Armor);

  const auto &farEffects = registry.get<ActiveEffectsComponent>(allyFar);
  CHECK(farEffects.effects.empty());
}

} // namespace NoMoreDay
