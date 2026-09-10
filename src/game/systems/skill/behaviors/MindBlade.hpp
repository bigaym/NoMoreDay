#pragma once
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"

namespace NoMoreDay::skills {

struct MindBlade : SkillBehaviorBase<MindBlade> {
  static constexpr uint32_t kSkillId = 7;

  // Core Interface
  static void OnCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
};

} // namespace NoMoreDay::skills
