#pragma once
#include <entt/entt.hpp>
#include "game/foundation/data/BladeMasteryData.hpp"
#include "raylib.h"

namespace NoMoreDay {

// Instance panel for the skill hub (skill specialization) UI.
//
// U7 cleanup: converted from a static class to an instance type so the panel
// keeps no static mutable state (session state lives in UISystem::State).
// The class is non-copyable; GameUiHost owns one instance and routes calls.
class UISkillHub {
public:
    UISkillHub() = default;

    UISkillHub(const UISkillHub&) = delete;
    UISkillHub& operator=(const UISkillHub&) = delete;

    void Draw(entt::registry& registry, entt::entity player);
    bool TrySelectMastery(entt::registry& registry, entt::entity player,
                          BladeMasteryId masteryId);
};

} // namespace NoMoreDay
