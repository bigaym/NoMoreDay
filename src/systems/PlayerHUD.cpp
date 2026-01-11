#include "PlayerHUD.hpp"
#include "UISystem.hpp"
#include "UICommon.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/SkillSystem.hpp"
#include "../core/UIRenderer.hpp"
#include "../core/AssetLoadingSystem.hpp"
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

    // --- Logic Metrics (2K Reference) ---
    // Hotbar Metrics (Sync with UISystem.cpp)
    float slotSize = 54.0f;
    float hotbarPadding = 8.0f;
    float hotbarW = (slotSize * 5) + (hotbarPadding * 4); // 302
    float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f; // 1129
    float hotbarRight = hotbarLeft + hotbarW;           // 1431
    
    // Bar Metrics
    float barWidth = 450.0f;
    float barHeight = 28.0f;
    float margin = 50.0f; // Spacing between bars and hotbar
    
    // Y Alignment (Near bottom)
    float barBottomY = UI_REF_HEIGHT - 30.0f;
    float barTopY = barBottomY - barHeight;

    // Draw FPS in top-left
    int fps = GetFPS();
    Color fpsColor = GREEN;
    if (fps < 30) fpsColor = RED;
    else if (fps < 60) fpsColor = YELLOW;
    UISystem::DrawTextUI(TextFormat("FPS: %d", fps), 10.0f, 10.0f, 20.0f, fpsColor, 1.0f);

    // HP Bar background

    float hpRightX = hotbarLeft - margin;
    float hpLeftX = hpRightX - barWidth;
    float hpPct = std::clamp(stats.health / stats.max_health, 0.0f, 1.0f);
    
    Rectangle hpBg = { hpLeftX * scale, barTopY * scale, barWidth * scale, barHeight * scale };
    DrawRectangleRec(hpBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ hpLeftX * scale, barTopY * scale, (barWidth * hpPct) * scale, barHeight * scale }, MAROON);
    DrawRectangleLinesEx(hpBg, 2.0f * scale, DARKGRAY);
    
    // HP Text
    std::string hpText = std::to_string((int)stats.health) + " / " + std::to_string((int)stats.max_health);
    UISystem::DrawTextUI(hpText.c_str(), hpLeftX + 10.0f, barTopY + 4.0f, 18.0f, WHITE, 1.0f);

    // --- 2. Mana Bar (Right of Hotbar) ---
    float manaLeftX = hotbarRight + margin;
    float manaPct = std::clamp(stats.mana / stats.max_mana, 0.0f, 1.0f);
    
    Rectangle manaBg = { manaLeftX * scale, barTopY * scale, barWidth * scale, barHeight * scale };
    DrawRectangleRec(manaBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ manaLeftX * scale, barTopY * scale, (barWidth * manaPct) * scale, barHeight * scale }, DARKBLUE);
    DrawRectangleLinesEx(manaBg, 2.0f * scale, DARKGRAY);
    
    // Mana Text
    std::string manaText = std::to_string((int)stats.mana) + " / " + std::to_string((int)stats.max_mana);
    float textW = 120.0f; // Approximate
    UISystem::DrawTextUI(manaText.c_str(), manaLeftX + barWidth - textW, barTopY + 4.0f, 18.0f, WHITE, 1.0f);

    // --- 3. Sword Intent (Above HP Bar) ---
    float intentH = 10.0f;
    float intentTopY = barTopY - intentH - 6.0f;
    float intentPct = (float)intent.stacks / intent.max_stacks;
    
    Rectangle intentBg = { hpLeftX * scale, intentTopY * scale, barWidth * scale, intentH * scale };
    DrawRectangleRec(intentBg, Fade(BLACK, 0.6f));
    DrawRectangleRec({ hpLeftX * scale, intentTopY * scale, (barWidth * intentPct) * scale, intentH * scale }, GOLD);
    DrawRectangleLinesEx(intentBg, 1.0f * scale, DARKGRAY);

    if (intent.stacks > 0) {
        std::string stackText = "剑意: " + std::to_string(intent.stacks);
        UISystem::DrawTextUI(stackText.c_str(), hpLeftX, intentTopY - 22.0f, 20.0f, GOLD, 1.0f);
    }

    // --- 4. Summon Status (Top Left) ---
    auto summonView = registry.view<SummonComponent>();
    for (auto entity : summonView) {
        const auto& summon = summonView.get<SummonComponent>(entity);
        if (summon.owner == player) {
            float healthPct = summon.lifetime / summon.max_lifetime; // Use lifetime as health for now
            Texture2D icon = {0};
            if (summon.icon_id != 0) {
                 icon = AssetLoadingSystem::GetTexture(summon.icon_id);
            }
            
            UIRenderer::DrawSummonIcon(UISystem::State.globalFont, 10.0f, 40.0f, 150.0f, 40.0f, icon, healthPct, summon.name.c_str(), 1.0f);
            break; // Show only one
        }
    }
}

} // namespace NoMoreDay::systems
