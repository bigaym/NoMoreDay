#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/Common.hpp"
#include <raylib.h>
#include <cmath>

namespace NoMoreDay::Utils {

    inline void ApplyKnockback(entt::registry& registry, entt::entity target, Vector2 sourcePos, float force) {
        if (!registry.valid(target) || !registry.all_of<Position, Velocity>(target)) return;
        
        // Future proofing: Check for specific tags like 'Immovable' or 'Boss' here if needed
        // if (registry.any_of<ImmovableTag>(target)) return;

        auto& tPos = registry.get<Position>(target);
        auto& tVel = registry.get<Velocity>(target);

        float dx = tPos.x - sourcePos.x;
        float dy = tPos.y - sourcePos.y;
        float lenSq = dx*dx + dy*dy;
        
        float dirX = 0.0f;
        float dirY = 0.0f;

        if (lenSq > 0.001f) {
            float len = std::sqrt(lenSq);
            dirX = dx / len;
            dirY = dy / len;
        } else {
            // If strictly overlapping, knockback right (or random) to avoid NaN
            dirX = 1.0f; 
        }

        // Apply impulse
        // Ideally, we would divide by mass here. For now, we assume uniform mass.
        tVel.vx += dirX * force;
        tVel.vy += dirY * force;
    }

}
