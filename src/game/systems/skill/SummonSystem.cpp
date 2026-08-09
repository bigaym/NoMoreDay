#include "game/systems/skill/SummonSystem.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/systems/skill/SummonAISystem.hpp"
#include "game/systems/skill/SummonLifecycleSystem.hpp"

namespace NoMoreDay::systems {

void SummonSystem::Update(entt::registry &registry, float dt,
                          const SpatialHashGrid &grid) {
  uint32_t activeSummonCount = 0;
  for (auto entity : registry.view<SummonComponent>()) {
    (void)entity;
    ++activeSummonCount;
  }
  CombatTelemetry::Get().RecordSummonEntityCount(activeSummonCount);

  SummonLifecycleSystem::Update(registry, dt);
  SummonAISystem::Update(registry, dt, grid);
}

} // namespace NoMoreDay::systems
