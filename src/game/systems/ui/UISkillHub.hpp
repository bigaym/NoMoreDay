#pragma once
#include <entt/entt.hpp>
#include "game/data/BladeMasteryData.hpp"
#include "raylib.h"

namespace NoMoreDay {

class UISkillHub {
public:
    static void Draw(entt::registry& registry, entt::entity player);
    static bool TrySelectMastery(entt::registry& registry, entt::entity player,
                                 BladeMasteryId masteryId);
};

} // namespace NoMoreDay
