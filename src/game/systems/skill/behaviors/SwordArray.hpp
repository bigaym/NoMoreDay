#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/SkillDefs.hpp"
#include "SkillBehaviorBase.hpp"

namespace NoMoreDay::systems {
    class SpatialHashGrid;
}

namespace NoMoreDay::skills {

struct SwordArray : public SkillBehaviorBase<SwordArray> {
    static constexpr uint32_t kSkillId = 6;
    static void Update(entt::registry& registry, entt::entity entity, SwordArrayComponent& array, float dt, const systems::SpatialHashGrid& grid);
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec);
};

} // namespace NoMoreDay::skills
