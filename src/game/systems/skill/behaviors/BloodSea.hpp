#pragma once

#include "SkillBehaviorBase.hpp"

#include "engine/physics/SpatialGrid.hpp"

namespace NoMoreDay {
struct CombatEvent;
}

namespace NoMoreDay::skills {

struct BloodSea : SkillBehaviorBase<BloodSea> {
  static constexpr uint32_t kSkillId = 12;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          BloodSeaFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
};

void RegisterBloodSea();

} // namespace NoMoreDay::skills
