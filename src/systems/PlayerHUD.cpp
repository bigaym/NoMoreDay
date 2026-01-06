#include "PlayerHUD.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/SkillSystem.hpp"
#include "../core/UIRenderer.hpp"
#include <string>
#include <cmath>

namespace NoMoreDay::systems {

void PlayerHUD::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag, CombatStats, SwordIntentComponent>();
    if (view.begin() == view.end()) return;

    entt::entity player = view.front();
    const auto& stats = view.get<CombatStats>(player);
    const auto& intent = view.get<SwordIntentComponent>(player);
    
    float scale = UISystem::State.scaleFactor;
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();

    // Bar Metrics
    float barWidth = 300.0f * scale;
    float barHeight = 24.0f * scale;
    float padding = 10.0f * scale;
    
    float startX = (screenW - (barWidth * 2 + padding)) / 2.0f;
    float startY = screenH - barHeight - 20.0f * scale;

    // --- 1. Health Bar (Left) ---
    float hpPct = std::clamp(stats.health / stats.max_health, 0.0f, 1.0f);
    Rectangle hpBg = { startX, startY, barWidth, barHeight };
    DrawRectangleRec(hpBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ startX, startY, barWidth * hpPct, barHeight }, MAROON);
    DrawRectangleLinesEx(hpBg, 2.0f * scale, DARKGRAY);
    
    // HP Text
    std::string hpText = std::to_string((int)stats.health) + " / " + std::to_string((int)stats.max_health);
    UISystem::DrawTextUI(hpText.c_str(), startX + 10.0f * scale, startY + 2.0f * scale, 18.0f, WHITE, 1.0f);

    // --- 2. Mana Bar (Right) ---
    float manaPct = std::clamp(stats.mana / stats.max_mana, 0.0f, 1.0f);
    float manaX = startX + barWidth + padding;
    Rectangle manaBg = { manaX, startY, barWidth, barHeight };
    DrawRectangleRec(manaBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ manaX, startY, barWidth * manaPct, barHeight }, DARKBLUE);
    DrawRectangleLinesEx(manaBg, 2.0f * scale, DARKGRAY);
    
    // Mana Text
    std::string manaText = std::to_string((int)stats.mana) + " / " + std::to_string((int)stats.max_mana);
    UISystem::DrawTextUI(manaText.c_str(), manaX + 10.0f * scale, startY + 2.0f * scale, 18.0f, WHITE, 1.0f);

    // --- 3. Sword Intent (Above HP Bar) ---
    float intentW = barWidth;
    float intentH = 8.0f * scale;
    float intentY = startY - intentH - 4.0f * scale;
    float intentPct = (float)intent.stacks / intent.max_stacks;
    
    Rectangle intentBg = { startX, intentY, intentW, intentH };
    DrawRectangleRec(intentBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ startX, intentY, intentW * intentPct, intentH }, GOLD);
    DrawRectangleLinesEx(intentBg, 1.0f * scale, DARKGRAY);

    if (intent.stacks > 0) {
        std::string stackText = "剑意: " + std::to_string(intent.stacks);
        UISystem::DrawTextUI(stackText.c_str(), startX, intentY - 18.0f * scale, 16.0f, GOLD, 1.0f);
    }
}

} // namespace NoMoreDay::systems
