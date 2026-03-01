#pragma once

#include "game/components/WorldState.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

namespace NoMoreDay {

class MapModifierAdapter {
public:
  [[nodiscard]] static ModifierDelta
  EvaluateEnemyAffixDelta(const ActiveDimensionalState &state);
};

} // namespace NoMoreDay
