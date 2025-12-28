#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class StatsSystem {
public:
    // @brief 根据主要属性和修饰符重新计算特定实体的所有战斗属性。
    //
    static void Recalculate(entt::registry& registry, entt::entity entity);

    // @brief 系统更新：为所有带有 StatsDirty 标签的实体重新计算属性。
    //
    static void update(entt::registry& registry);
};

} // namespace NoMoreDay