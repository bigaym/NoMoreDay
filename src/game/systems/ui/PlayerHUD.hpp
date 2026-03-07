#pragma once
#include "game/components/SkillDefs.hpp"

#include <string>
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay::systems {

class PlayerHUD {
public:
    static const char* ResolveBladeResourceLabel(
        const BladeResourceComponent& bladeResource);
    static std::string ResolveBladeResourceDetailText(
        const BladeMasteryComponent& mastery,
        const BladeResourceComponent& bladeResource);
    static std::string ResolveBladeResourceFeedbackText(
        const BladeMasteryComponent& mastery,
        const BladeResourceComponent& bladeResource);
    static std::string ResolveBladeResourceRuntimeDetailText(
        const entt::registry& registry, entt::entity player,
        const BladeMasteryComponent& mastery,
        const BladeResourceComponent& bladeResource,
        const CombatStats& stats);
    static std::string ResolveBladeResourceRuntimeFeedbackText(
        const entt::registry& registry, entt::entity player,
        const BladeMasteryComponent& mastery,
        const BladeResourceComponent& bladeResource,
        const CombatStats& stats);
    static std::string ResolveSwordFlowFeedbackText(
        const BladeResourceComponent& bladeResource);
    static void Draw(entt::registry& registry);
};

} // namespace NoMoreDay::systems
