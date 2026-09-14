#include "game/systems/skill/PersistentFieldSystem.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"

#include <vector>

namespace NoMoreDay::skills {

void PersistentFieldSystem::Update(entt::registry &registry,
                                   systems::SpatialHashGrid &grid, float dt) {
  auto heavenly_view = registry.view<HeavenlySwordFieldComponent, Position>();
  for (const entt::entity entity : heavenly_view) {
    auto &field = heavenly_view.get<HeavenlySwordFieldComponent>(entity);
    HeavenlySwordDescent::UpdateField(registry, entity, field, dt, grid);
  }

  auto blood_view = registry.view<BloodSeaFieldComponent, Position>();
  for (const entt::entity entity : blood_view) {
    auto &field = blood_view.get<BloodSeaFieldComponent>(entity);
    BloodSea::UpdateField(registry, entity, field, dt, grid);
  }
}

void DestroyOwnedPersistentFields(entt::registry &registry,
                                  const entt::entity owner,
                                  const uint32_t skill_id) {
  std::vector<entt::entity> to_destroy;
  const auto view = registry.view<PersistentFieldTag, SkillComponent>();
  for (const entt::entity entity : view) {
    const auto &skill = view.get<SkillComponent>(entity);
    if (skill.owner == owner && (skill_id == 0 || skill.skill_id == skill_id)) {
      to_destroy.push_back(entity);
    }
  }
  for (const entt::entity entity : to_destroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

} // namespace NoMoreDay::skills
