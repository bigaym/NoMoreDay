#pragma once
#include <entt/entity/fwd.hpp>
#include <cstdint>

namespace NoMoreDay {

class AstrolabeSystem {
public:
    static bool can_activate(entt::registry& registry, entt::entity entity, uint32_t node_id);
    static bool activate_node(entt::registry& registry, entt::entity entity, uint32_t node_id);
    static bool deactivate_node(entt::registry& registry, entt::entity entity, uint32_t node_id);
};

} // namespace NoMoreDay
