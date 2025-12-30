#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class DropSystem {
public:
    /**
     * @brief 处理被击杀的实体并根据其 DropTableComponent 生成掉落物。
     */
    static void update(entt::registry& registry, int areaLevel = 1);

    /**
     * @brief 计算特定实体掉落物的核心逻辑。
     * 可独立调用用于测试或特殊事件。
     */
    static void GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim, int areaLevel = 0);
};

} // namespace NoMoreDay
