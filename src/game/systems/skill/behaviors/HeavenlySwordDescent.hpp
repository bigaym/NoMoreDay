#pragma once

#include "SkillBehaviorBase.hpp"

#include "game/systems/physics/SpatialGrid.hpp"

namespace NoMoreDay {
struct CombatEvent;
}

namespace NoMoreDay::skills {

struct HeavenlySwordDescent : SkillBehaviorBase<HeavenlySwordDescent> {
  static constexpr uint32_t kSkillId = 11;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          HeavenlySwordFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
};

void RegisterHeavenlySwordDescent();

} // namespace NoMoreDay::skills
