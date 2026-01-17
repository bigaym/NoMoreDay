#include "game/systems/ui/PlayerHUD.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UICommon.hpp"
#include "game/systems/ui/SwordIntentWidget.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include <string>
#include <cmath>
#include <map>

namespace NoMoreDay::systems {

void PlayerHUD::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag, CombatStats>();
    if (view.begin() == view.end()) return;

    entt::entity player = view.front();
    const auto& stats = view.get<CombatStats>(player);
    const auto* intent = registry.try_get<SwordIntentComponent>(player);
    
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

    // --- 3. Sword Intent (Visual Widget) ---
    if (intent) {
        NoMoreDay::systems::ui::SwordIntentWidget::Draw(intent->stacks, intent->max_stacks);
    }

    // --- 4. Summon Status (Top Left) ---
    auto summonView = registry.view<SummonComponent>();
    std::map<uint32_t, std::pair<float, int>> summonGroups; // key -> {maxLifeRatio, count}
    std::map<uint32_t, std::string> summonNames;
    std::map<uint32_t, uint32_t> summonIcons;

    for (auto entity : summonView) {
        const auto& summon = summonView.get<SummonComponent>(entity);
        if (summon.owner == player) {
            uint32_t key = (summon.skill_id != 0) ? summon.skill_id : entt::hashed_string{summon.name.c_str()}.value();
            float ratio = (summon.max_lifetime > 0) ? (summon.lifetime / summon.max_lifetime) : 0.0f;
            
            auto& group = summonGroups[key];
            group.first = std::max(group.first, ratio); // Show largest remaining duration
            group.second++;
            
            summonNames[key] = summon.name;
            summonIcons[key] = summon.icon_id;
        }
    }

    float startY = 40.0f;
    for (auto const& [key, data] : summonGroups) {
        Texture2D icon = {0};
        if (summonIcons[key] != 0) {
            icon = AssetLoadingSystem::GetTexture(summonIcons[key]);
        }
        
        std::string displayName = summonNames[key];
        if (data.second > 1) {
            displayName += " x" + std::to_string(data.second);
        }
        
        UIRenderer::DrawSummonIcon(UISystem::State.globalFont, 10.0f, startY, 150.0f, 40.0f, icon, data.first, displayName.c_str(), 1.0f);
        startY += 45.0f;
    }
}

} // namespace NoMoreDay::systems
