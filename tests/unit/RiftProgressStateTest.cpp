#include "doctest.h"

#include "game/foundation/components/WorldState.hpp"
#include "game/foundation/data/BiomeTypes.hpp"

using namespace NoMoreDay;

TEST_CASE("[Rift] In-progress state detection") {
  ActiveDimensionalState state;
  state.isActive = true;
  state.isCompleted = false;
  CHECK(HasInProgressRift(state));

  state.isCompleted = true;
  CHECK_FALSE(HasInProgressRift(state));

  state.isActive = false;
  CHECK_FALSE(HasInProgressRift(state));
}

TEST_CASE("[Rift] Clear instance data resets runtime fields") {
  ActiveDimensionalState state;
  state.isActive = true;
  state.isCompleted = false;
  state.seed = 12345;
  state.biome = BiomeID::Cave;
  state.currentDepth = 3;
  state.maxDepth = 5;
  state.killCounter = 42;
  state.lastExitPosition = {100.0f, 200.0f};
  state.difficultyScore = 77;
  state.calculatedRarity = 1.2f;
  state.calculatedQuantity = 0.8f;
  state.selectedBaseLevel = 55;
  state.gridSnapshots[0].hasFragment = true;

  ClearRiftInstanceData(state);

  CHECK_FALSE(state.isActive);
  CHECK(state.isCompleted);
  CHECK(state.seed == 0);
  CHECK(state.biome == BiomeID::None);
  CHECK(state.currentDepth == 1);
  CHECK(state.maxDepth == 3);
  CHECK(state.killCounter == 0);
  CHECK(state.lastExitPosition.x == doctest::Approx(0.0f));
  CHECK(state.lastExitPosition.y == doctest::Approx(0.0f));
  CHECK(state.difficultyScore == 0);
  CHECK(state.calculatedRarity == doctest::Approx(0.0f));
  CHECK(state.calculatedQuantity == doctest::Approx(0.0f));
  CHECK(state.selectedBaseLevel == 55);
  CHECK_FALSE(state.gridSnapshots[0].hasFragment);
}

TEST_CASE("[Rift] Serialization contains lastExitPosition") {
  ActiveDimensionalState state;
  state.isActive = true;
  state.biome = BiomeID::Cave;
  state.currentDepth = 2;
  state.lastExitPosition = {1420.5f, 880.0f};

  nlohmann::json j = state;
  CHECK(j.contains("lastExitPosition"));
  CHECK(j["lastExitPosition"]["x"].get<float>() == doctest::Approx(1420.5f));
  CHECK(j["lastExitPosition"]["y"].get<float>() == doctest::Approx(880.0f));

  const auto roundTrip = j.get<ActiveDimensionalState>();
  CHECK(roundTrip.lastExitPosition.x == doctest::Approx(1420.5f));
  CHECK(roundTrip.lastExitPosition.y == doctest::Approx(880.0f));
}
