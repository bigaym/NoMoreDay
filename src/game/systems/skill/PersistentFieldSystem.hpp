#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace NoMoreDay::systems {
class SpatialHashGrid;
}

namespace NoMoreDay::skills {

// 持久场统一调度：聚合技能 11/12 的场更新，隔离调度层对具体场组件的依赖。
class PersistentFieldSystem {
public:
  static void Update(entt::registry &registry, systems::SpatialHashGrid &grid,
                     float dt);
};

// 销毁属于指定施法者的持久场实体（PersistentFieldTag + SkillComponent）。
// 场实体与 AreaFieldComponent 同实体，销毁实体即连带清理其上的范围场。
// skill_id == 0 为「全部销毁」哨兵。
void DestroyOwnedPersistentFields(entt::registry &registry,
                                  const entt::entity owner,
                                  const uint32_t skill_id);

} // namespace NoMoreDay::skills
