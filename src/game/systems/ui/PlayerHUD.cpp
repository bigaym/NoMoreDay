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

namespace {

const char *ResolveSummonDisplayName(const SummonComponent &summon) {
    switch (summon.archetype_id) {
    case SummonArchetype::SpiritSword:
        return "飞剑";
    case SummonArchetype::ShadowEcho:
        return "Shadow Echo";
    default:
        break;
    }

    if (summon.skill_id == 3) {
        return "飞剑";
    }
    return "Summon";
}

} // namespace

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

    // --- Hybrid Barrier Overlay (Cyan shield on top of HP bar) ---
    // Barrier colors
    constexpr Color BARRIER_COLOR = { 102, 217, 232, 200 };       // #66D9E8 (明亮青色)
    constexpr Color BARRIER_GLOW = { 64, 160, 255, 255 };         // #40A0FF (溢出发光)
    constexpr Color BARRIER_BG = { 26, 58, 74, 180 };             // #1A3A4A (深青色背景)
    
    bool hasBarrier = stats.barrier > 0.0f || stats.max_barrier > 0.0f;
    float barrierDisplayValue = stats.barrier;
    
    if (hasBarrier && barrierDisplayValue > 0.0f) {
        // Calculate barrier percentage relative to max_health (for visual overlay)
        // This makes barrier visually overlay the health bar proportionally
        float barrierPct = std::clamp(barrierDisplayValue / stats.max_health, 0.0f, 1.0f);
        
        // Draw barrier bar overlaying on top of HP bar (from left edge)
        float barrierWidth = barWidth * barrierPct;
        Rectangle barrierRect = { hpLeftX * scale, barTopY * scale, barrierWidth * scale, barHeight * scale };
        DrawRectangleRec(barrierRect, BARRIER_COLOR);
        
        // If barrier exceeds max_barrier (Ward mode overflow), add pulsing glow effect
        if (stats.barrier > stats.max_barrier && stats.max_barrier > 0.0f) {
            // Simple pulse animation based on time
            float pulse = (std::sin(static_cast<float>(GetTime()) * 4.0f) + 1.0f) * 0.5f; // 0-1 oscillation
            float glowAlpha = 0.3f + pulse * 0.4f; // 0.3-0.7 range
            DrawRectangleLinesEx(hpBg, 3.0f * scale, Fade(BARRIER_GLOW, glowAlpha));
        }
    }
    
    // HP + Barrier Text
    std::string hpText = std::to_string((int)stats.health) + " / " + std::to_string((int)stats.max_health);
    if (hasBarrier && barrierDisplayValue > 0.0f) {
        hpText += " (+" + std::to_string((int)barrierDisplayValue) + ")";
    }
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

    // --- 3. Blade Resource (Visual Widget) ---
    if (const auto* bladeResource = registry.try_get<BladeResourceComponent>(player)) {
        const char* label =
            (bladeResource->kind == BladeResourceKind::SwordFlow) ? "Sword Flow" : "Sword Intent";
        NoMoreDay::systems::ui::SwordIntentWidget::Draw(
            bladeResource->current, bladeResource->max, label);
    } else if (intent) {
        NoMoreDay::systems::ui::SwordIntentWidget::Draw(
            intent->stacks, intent->max_stacks, "Sword Intent");
    }

    // --- 4. Summon Status (Top Left) ---
    auto summonView = registry.view<SummonComponent>();
    std::map<uint32_t, std::pair<float, int>> summonGroups; // key -> {maxLifeRatio, count}
    std::map<uint32_t, std::string> summonNames;
    std::map<uint32_t, uint32_t> summonIcons;

    for (auto entity : summonView) {
        const auto& summon = summonView.get<SummonComponent>(entity);
        if (summon.owner == player) {
            uint32_t key = (summon.skill_id != 0) ? summon.skill_id : summon.archetype_id;
            float ratio = (summon.max_lifetime > 0) ? (summon.lifetime / summon.max_lifetime) : 0.0f;
            
            auto& group = summonGroups[key];
            group.first = std::max(group.first, ratio); // Show largest remaining duration
            group.second++;
            
            summonNames[key] = ResolveSummonDisplayName(summon);
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
