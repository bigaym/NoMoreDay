#include "MonsterHealthBarSystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/AIComponent.hpp" // For EnemyTag
#include "../components/Buff.hpp"
#include "raymath.h"

namespace NoMoreDay::systems {

void MonsterHealthBarSystem::Render(entt::registry& registry, const Camera2D& camera) {
    // We only care about enemies that have health and position
    auto view = registry.view<EnemyTag, Position, CombatStats>();

    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        const auto& stats = view.get<CombatStats>(entity);

        // Don't show full health bars (optional UX choice)
        if (stats.health >= stats.max_health) continue;
        if (stats.health <= 0) continue;

        float hpPercent = stats.health / stats.max_health;
        if (hpPercent < 0) hpPercent = 0;
        if (hpPercent > 1) hpPercent = 1;

        // Health bar dimensions
        float barWidth = 40.0f;
        float barHeight = 4.0f;
        float yOffset = -15.0f; // Above the entity

        Vector2 worldPos = { pos.x, pos.y };
        
        // Background (Gray/Black)
        Rectangle bgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth, barHeight };
        DrawRectangleRec(bgRect, { 40, 40, 40, 200 });

        // Foreground (Green/Red based on health)
        Color barColor = GREEN;
        if (hpPercent < 0.25f) barColor = RED;
        else if (hpPercent < 0.5f) barColor = ORANGE;

        Rectangle fgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth * hpPercent, barHeight };
        DrawRectangleRec(fgRect, barColor);
        
        // Border
        DrawRectangleLinesEx(bgRect, 1.0f, { 20, 20, 20, 255 });

        // --- BUFF / DEBUFF ICONS ---
        if (auto* activeEffects = registry.try_get<ActiveEffectsComponent>(entity)) {
            float iconSize = 8.0f;
            float iconSpacing = 2.0f;
            float iconsYOffset = yOffset - iconSize - 2.0f;
            float totalWidth = (activeEffects->effects.size() * iconSize) + ((activeEffects->effects.size() - 1) * iconSpacing);
            float startX = worldPos.x - totalWidth / 2.0f;

            for (size_t i = 0; i < activeEffects->effects.size(); ++i) {
                const auto& effect = activeEffects->effects[i];
                Color iconColor = effect.is_debuff ? RED : GREEN;
                
                Rectangle iconRect = { startX + i * (iconSize + iconSpacing), worldPos.y + iconsYOffset, iconSize, iconSize };
                DrawRectangleRec(iconRect, iconColor);
                DrawRectangleLinesEx(iconRect, 0.5f, { 20, 20, 20, 255 });
            }
        }
    }
}

} // namespace NoMoreDay::systems
