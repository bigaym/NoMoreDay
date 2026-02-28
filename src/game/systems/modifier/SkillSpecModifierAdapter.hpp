#pragma once

#include "game/components/SkillDefs.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay {

class SkillSpecModifierAdapter {
public:
  [[nodiscard]] static std::vector<uint32_t>
  CollectAllocatedNodeIds(const SpecializedSkill &activeSkillSlot);

  [[nodiscard]] static float
  EvaluateDamageMultiplier(uint32_t skillId, Tag skillTags,
                           std::span<const uint32_t> nodeIds);

  [[nodiscard]] static float ApplyHeavyMomentum(float baseline, uint32_t skillId,
                                                Tag skillTags,
                                                std::span<const uint32_t> nodeIds);

  static void ApplyHeavyMomentumToDamageMultipliers(
      std::array<float, 6> &damageMultipliers,
      uint32_t skillId,
      Tag skillTags,
      std::span<const uint32_t> nodeIds);
};

} // namespace NoMoreDay
