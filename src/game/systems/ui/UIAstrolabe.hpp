#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include "game/data/TalentData.hpp"
#include "game/systems/ui/AstrolabeRenderer.hpp"

namespace NoMoreDay {

class UIAstrolabe {
public:
    static void Initialize();
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry);
    
    static void Toggle(entt::registry& registry, entt::entity player);
    static bool IsVisible(entt::registry& registry, entt::entity player);

    static void Show();
    static void Hide();
    static void ResetView();

private:
    static void DrawInternal(entt::registry& registry, entt::entity player);
    static void EnsureLoaded();

    // static AstrolabeMap s_map; // Removed: Using AstrolabeRegistry
    static AstrolabeView s_view;
    static bool s_loaded;
    static bool s_visible;
    static float s_alpha;
};

} // namespace NoMoreDay
