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
    static void DrawVowDialog(entt::registry& registry, entt::entity player, const ProfessionStar& star);
    static void EnsureLoaded();

    static AstrolabeView s_view;
    static bool s_loaded;
    static bool s_visible;
    static float s_alpha;

    // Failure message state
    static std::string s_failMessage;
    static float s_failMessageTimer;

    // Vow confirmation state
    static ProfessionID s_pendingVowProfession;
    static float s_vowHoldProgress;
    static constexpr float VOW_HOLD_DURATION = 2.0f;
    static bool s_showVowDialog;
};

} // namespace NoMoreDay
