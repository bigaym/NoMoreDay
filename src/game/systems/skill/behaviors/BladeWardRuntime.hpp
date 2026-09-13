#pragma once
// 剑气护体 (ID 4) 运行时扩展：动态节点效果推进、反击剑气实体生成与命中元素结算。
// 独立轻量头文件，使伤害结算站点无需引入完整行为层依赖。
#include <cstdint>
#include <entt/entt.hpp>

namespace NoMoreDay {
struct BladeWardComponent;
namespace systems {
class SpatialHashGrid;
}
namespace skills {

// 推进剑气护体动态节点效果：410 厚积薄发、413 破釜沉舟、415 坚韧回生、
// 433 鲜血壁垒、434 无瑕之御、472 雷霆法环。
void UpdateBladeWardRuntime(entt::registry &registry,
                            systems::SpatialHashGrid &grid, entt::entity entity,
                            BladeWardComponent &ward, float dt);

// 反击触发时生成 counter_swords 道反击剑气实体（470）。
// 伤害归因仍由 ResolveSkill4Counter 统一承担，此处实体仅承担表现与射程语义，
// 因此不挂载 payload_context，避免同一次反击被重复结算。
void SpawnBladeWardCounterSwords(entt::registry &registry, entt::entity owner,
                                 uint64_t cast_id);

// 反击命中时结算 474 霜铠冰霜风暴与 476 元素曝光。
// owner 为护盾持有者，target 为被反击者。
void ApplyBladeWardCounterOnHit(entt::registry &registry, entt::entity owner,
                                entt::entity target,
                                const BladeWardComponent &ward);

} // namespace skills
} // namespace NoMoreDay
