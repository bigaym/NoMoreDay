#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/AstrolabeUIComponent.hpp"

namespace NoMoreDay {

class UIAstrolabe {
public:
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry);

    // Helper to toggle UI
    static void Toggle(entt::registry& registry, entt::entity player) {
        auto* ui = registry.try_get<AstrolabeUIComponent>(player);
        if (!ui) {
            registry.emplace<AstrolabeUIComponent>(player);
            ui = registry.try_get<AstrolabeUIComponent>(player);
            // Initialize view
            ui->zoom = 1.0f;
            ui->offset = { 0.0f, 0.0f };
        }
        
        ui->isOpen = !ui->isOpen;
    }

    static bool IsVisible(entt::registry& registry, entt::entity player) {
        auto* ui = registry.try_get<AstrolabeUIComponent>(player);
        return ui && ui->isOpen;
    }
};

}
