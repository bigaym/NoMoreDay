#pragma once
#include <entt/entity/registry.hpp>

namespace NoMoreDay {

class XPAwardingSystem {
public:
    /**
     * @brief Processes killed entities and awards experience to players.
     * @param registry The EnTT registry.
     */
    static void update(entt::registry& registry);
};

} // namespace NoMoreDay
