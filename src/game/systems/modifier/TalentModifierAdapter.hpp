#pragma once

#include "game/components/Progression.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay {

class TalentModifierAdapter {
public:
  [[nodiscard]] static std::vector<uint32_t>
  CollectActiveNodeIds(const AstrolabeComponent &astrolabeComp);

  [[nodiscard]] static float
  EvaluateFlatHealthBonus(std::span<const uint32_t> nodeIds);

  [[nodiscard]] static float
  ApplyFlatHealthBonus(float baseHealth, std::span<const uint32_t> nodeIds);
};

} // namespace NoMoreDay
