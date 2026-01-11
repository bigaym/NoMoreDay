#include "MonsterHealthBarSystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/AIComponent.hpp" // For EnemyTag
#include "../components/Buff.hpp"
#include "raymath.h"

namespace NoMoreDay::systems {

void MonsterHealthBarSystem::Render(entt::registry& registry, const Camera2D& camera) {
    // We only care about enemies that have health and position
    // Exclude killed entities to avoid showing bars for dead monsters during their cleanup/animation phase
    auto view = registry.view<EnemyTag, Position, HealthComponent>(entt::exclude<KilledTag>);

    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        const auto& hp = view.get<HealthComponent>(entity);

        // Skip dead monsters
        if (hp.current <= 0) continue;

        // --- Visibility Logic ---
        // Show health bar when:
        // 1. Monster is tracking/chasing the player (CHASE or ATTACK state)
        // 2. Monster has taken damage
        
        bool isTracking = false;
        if (auto* ai = registry.try_get<AIComponent>(entity)) {
            // Show when monster has detected and is actively pursuing the player
            if (ai->aiType == AIType::CHASE || ai->aiType == AIType::ATTACK) {
                isTracking = true;
            }
        }
        
        bool isDamaged = hp.current < hp.max - 0.1f;
        
        // Only show if tracking player OR damaged
        if (!isTracking && !isDamaged) continue;

        float hpPercent = hp.current / hp.max;
        hpPercent = std::clamp(hpPercent, 0.0f, 1.0f);

        // Health bar dimensions
        float barWidth = 44.0f;
        float barHeight = 5.0f;
        float yOffset = -20.0f; // Above the entity

        Vector2 worldPos = { pos.x, pos.y };
        
        // Background (Dark Metallic Gray)
        Rectangle bgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth, barHeight };
        DrawRectangleRec(bgRect, { 20, 20, 20, 220 });

        // Foreground (ARPG Red Gradient-like)
        // Base color is a deep red
        Color barColor = { 200, 30, 30, 255 };
        if (hpPercent < 0.25f) barColor = { 255, 40, 40, 255 }; // Bright red when critical
        
        Rectangle fgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth * hpPercent, barHeight };
        
        // Draw health with a slight shadow/gradient effect
        DrawRectangleRec(fgRect, barColor);
        // Top highlight line for the health bar
        DrawRectangle(fgRect.x, fgRect.y, fgRect.width, 1, { 255, 255, 255, 80 });
        
        // Border (Darker, more defined)
        DrawRectangleLinesEx(bgRect, 1.0f, { 10, 10, 10, 255 });

        // --- BUFF / DEBUFF ICONS ---
        if (auto* activeEffects = registry.try_get<ActiveEffectsComponent>(entity)) {
            float iconSize = 10.0f;
            float iconSpacing = 2.0f;
            float iconsYOffset = yOffset - iconSize - 3.0f;
            
            if (!activeEffects->effects.empty()) {
                float totalWidth = (activeEffects->effects.size() * iconSize) + ((activeEffects->effects.size() - 1) * iconSpacing);
                float startX = worldPos.x - totalWidth / 2.0f;

                for (size_t i = 0; i < activeEffects->effects.size(); ++i) {
                    const auto& effect = activeEffects->effects[i];
                    Color iconColor = effect.is_debuff ? Color{ 180, 40, 40, 255 } : Color{ 40, 180, 40, 255 };
                    
                    Rectangle iconRect = { startX + i * (iconSize + iconSpacing), worldPos.y + iconsYOffset, iconSize, iconSize };
                    DrawRectangleRec(iconRect, iconColor);
                    DrawRectangleLinesEx(iconRect, 1.0f, { 10, 10, 10, 255 });
                }
            }
        }
    }
}

} // namespace NoMoreDay::systems
