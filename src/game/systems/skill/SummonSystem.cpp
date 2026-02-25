#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/SummonAISystem.hpp"
#include "game/systems/skill/SummonLifecycleSystem.hpp"

namespace NoMoreDay::systems {

void SummonSystem::Update(entt::registry &registry, float dt,
                          const SpatialHashGrid &grid) {
  SummonLifecycleSystem::Update(registry, dt);
  SummonAISystem::Update(registry, dt, grid);
}

} // namespace NoMoreDay::systems
