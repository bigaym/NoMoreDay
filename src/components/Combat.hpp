#pragma once
#include "Stats.hpp"
#include <cstdint>
#include <vector>
#include <entt/entity/entity.hpp>

namespace NoMoreDay {

struct DamageEvent {
    entt::entity attacker;
    entt::entity target;
    float amount;
    DamageType type;
    bool is_critical;
    bool is_hit; // False if dodged/blocked completely
};

// Component to queue damage events for processing
struct DamageQueue {
    std::vector<DamageEvent> events;
};

} // namespace NoMoreDay
