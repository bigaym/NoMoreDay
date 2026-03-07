#pragma once
#include "game/components/SkillDefs.hpp"

#include <string>
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay::systems {

class PlayerHUD {
public:
    static std::string ResolveSwordFlowFeedbackText(
        const BladeResourceComponent& bladeResource);
    static void Draw(entt::registry& registry);
};

} // namespace NoMoreDay::systems
