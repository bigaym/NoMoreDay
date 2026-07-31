#pragma once
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"

namespace NoMoreDay::skills {

struct MindBlade : SkillBehaviorBase<MindBlade> {
  static constexpr uint32_t kSkillId = 7;

  // Core Interface
  static void OnCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);

  // Update Loop (Called by SkillSystem)
  // Returns true if the entity is still alive, false if it should be destroyed.
  static bool Update(entt::registry &registry, entt::entity entity,
                     MindBladeAI &ai, MindBladeComponent &comp, float dt,
                     systems::SpatialHashGrid &grid);
};

} // namespace NoMoreDay::skills
