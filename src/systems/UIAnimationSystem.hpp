#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay {

    class UIAnimationSystem {
    public:
        static void Update(entt::registry& registry, float dt);
        
        // Easing helpers
        static float Ease(float t, float b, float c, float d, int type);
    };

}
