#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class XPAwardingSystem {
public:
    /*
     * @brief 处理被击杀的实体并向玩家奖励经验。
     * @param registry EnTT 注册表。
     */
    static void update(entt::registry& registry);
};

} // namespace NoMoreDay
