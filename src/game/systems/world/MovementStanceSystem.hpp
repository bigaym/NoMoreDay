#pragma once
#include "entt/entt.hpp"

namespace NoMoreDay {

class MovementStanceSystem {
public:
    static void Update(entt::registry& registry, float dt);
    
    /**
     * @brief Call this when an entity takes damage to interrupt stances.
     */
    static void OnTakeDamage(entt::registry& registry, entt::entity entity);
};

}
