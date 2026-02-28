#pragma once

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>

namespace NoMoreDay {

struct ModifierDelta {
  void AddFlat(uint32_t statType, float value);
  void AddPercentAdd(uint32_t statType, float value);
  void AddPercentMult(uint32_t statType, float value);
  void AddSkillLevel(uint32_t skillId, float value);

  [[nodiscard]] float GetSkillLevelBonus(uint32_t skillId) const;

  std::unordered_map<uint32_t, float> flat;
  std::unordered_map<uint32_t, float> percent_add;
  std::unordered_map<uint32_t, float> percent_mult;
  std::unordered_map<uint32_t, float> skill_levels;
};

class ModifierEvaluator {
public:
  [[nodiscard]] static ModifierDelta
  Evaluate(std::span<const ModifierRecord> records, const ModifierEvalContext &ctx);

  [[nodiscard]] static ModifierDelta
  Evaluate(const ModifierRuntimeRegistry &registry,
           std::span<const uint32_t> recordIds,
           const ModifierEvalContext &ctx);

  [[nodiscard]] static float ApplyStat(float baseValue, uint32_t statType,
                                       const ModifierDelta &delta);
};

} // namespace NoMoreDay
