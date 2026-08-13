#include "TestCommon.hpp"
#include "doctest.h"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/ai/EnemyAIBehaviors.hpp"
#include "game/systems/world/TilemapCollisionSystem.hpp"

namespace NoMoreDay {

using namespace AI;

class MockMapSystem : public MapSystem {
public:
  bool walkable = true;

  bool isWalkable(int x, int y) const override { return walkable; }
  bool raycast(const Position &start, const Position &end) const override {
    return walkable;
  }
};

TEST_CASE("[Functional] AssassinAI - Backstab Direction and Safety") {
  TestSetupScope scope;
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto &playerPos = registry.emplace<Position>(player, 100.0f, 100.0f);
  auto &playerVel =
      registry.emplace<Velocity>(player, 0.0f, 10.0f); // Moving South

  auto assassin = registry.create();
  auto &assPos = registry.emplace<Position>(assassin, 100.0f, 150.0f);
  registry.emplace<AIComponent>(assassin);
  registry.emplace<ActiveEffectsComponent>(assassin);

  MockMapSystem mapSystem;

  SUBCASE("Successful backstab behind player") {
    ExecuteBackstab(registry, assassin, mapSystem, playerPos, 2.5f);
    CHECK(assPos.y < playerPos.y);

    auto &effects = registry.get<ActiveEffectsComponent>(assassin);
    bool found = false;
    for (const auto &buff : effects.effects) {
      if (buff.id == BuffIdToString(BuffId::AssassinBackstabBoost)) {
        found = true;
        CHECK(buff.modifiers[0].value == doctest::Approx(2.5f));
      }
    }
    CHECK(found);
  }

  SUBCASE("Prevent backstab if wall is in the way") {
    mapSystem.walkable = false;
    bool success =
        ExecuteBackstab(registry, assassin, mapSystem, playerPos, 2.5f);
    CHECK_FALSE(success);
  }
}

} // namespace NoMoreDay
