#pragma once

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

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

  // 技能交付参数算子 (opcode 30..40)：从专精加点求值交付 delta，
  // 调用方按设计 §3.4 的顺序合成进 BakedSkillProfile；无可采集记录返回默认 delta。
  [[nodiscard]] static ModifierDelta EvaluateSkillDeliveryDeltas(
      uint32_t skillId, Tag skillTags, const SpecializedSkill &activeSkillSlot);
};

} // namespace NoMoreDay
