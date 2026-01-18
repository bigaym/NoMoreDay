#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/AIComponent.hpp" // For EnemyTag
#include "game/components/Buff.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "raymath.h"
#include "game/systems/ui/UISystem.hpp"
#include "engine/render/UIRenderer.hpp"

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
        
        // Champion/Elite/Boss/Nemesis Always visible
        bool isRare = false;
        if (auto* rarityComp = registry.try_get<EnemyRarityComponent>(entity)) {
            if (rarityComp->rarity > EnemyRarityComponent::NORMAL) {
                isRare = true;
            }
        }

        // Only show if tracking player OR damaged OR Rare
        // Normal monsters still hidden until engaged/damaged to reduce clutter
        if (!isTracking && !isDamaged && !isRare) continue;

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

        // --- Barrier Overlay (Cyan Shield) ---
        if (auto* stats = registry.try_get<CombatStats>(entity)) {
            if (stats->barrier > 0.0f) {
                constexpr Color BARRIER_COLOR = { 102, 217, 232, 200 }; // Cyan
                float barrierPct = std::clamp(stats->barrier / hp.max, 0.0f, 1.0f);
                Rectangle barrierRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth * barrierPct, barHeight };
                DrawRectangleRec(barrierRect, BARRIER_COLOR);
            }
        }
        
        // Border (Darker, more defined)
        DrawRectangleLinesEx(bgRect, 1.0f, { 10, 10, 10, 255 });

        // --- NAME DISPLAY ---
        Font font = UISystem::GetFont();
        float nameFontSize = 12.0f; // Reduced by 2 points
        
        std::string name = "未知怪物";
        Color nameColor = WHITE;

        if (auto* enemyState = registry.try_get<EnemyStateComponent>(entity)) {
            name = kRaceData[static_cast<size_t>(enemyState->raceType)].name;
        }

        if (auto* rarityComp = registry.try_get<EnemyRarityComponent>(entity)) {
             switch (rarityComp->rarity) {
                 case EnemyRarityComponent::CHAMPION: 
                    name = "冠军: " + name;
                    nameColor = SKYBLUE; break;
                 case EnemyRarityComponent::ELITE: 
                    name = "精英: " + name;
                    nameColor = GOLD; break;
                 case EnemyRarityComponent::BOSS: 
                    name = "首领: " + name;
                    nameColor = ORANGE; break;
                 case EnemyRarityComponent::NEMESIS: 
                    name = "宿敌: " + name;
                    nameColor = RED; break;
                 default: break;
             }
        }

        Vector2 nameSize = MeasureTextEx(font, name.c_str(), nameFontSize, 0.0f);
        UIRenderer::DrawTextUI(font, name.data(), worldPos.x - nameSize.x / 2.0f, yOffset + worldPos.y - nameSize.y - 4.0f, nameFontSize, nameColor);

        // --- BUFF / DEBUFF ICONS ---
// --- BUFF / DEBUFF ICONS ---
        // Hidden to prevent visual glitches (green squares overlapping text) until real icons are implemented
        /*
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
        */

        // --- AFFIX LABELS (Below health bar) ---
        if (auto* affixComp = registry.try_get<NoMoreDay::MonsterAffixComponent>(entity)) {
            if (!affixComp->affixes.empty()) {
                float labelYOffset = yOffset + barHeight + 2.0f;
                float labelSpacing = 2.0f;
                float labelHeight = 8.0f;
                
                // Calculate total width for centering
                std::vector<std::pair<std::string_view, Color>> labels;
                for (auto affixType : affixComp->affixes) {
                    const auto& def = NoMoreDay::MonsterAffixRegistry::GetAffixDef(affixType);
                    Color labelColor = { def.tintR, def.tintG, def.tintB, 255 };
                    labels.emplace_back(def.name, labelColor);
                }
                
                float totalLabelWidth = 0.0f;
                Font font = UISystem::GetFont();
                float fontSize = 10.0f; // Reduced by 2 points
                float textSpacing = 0.0f;

                for (const auto& [name, _] : labels) {
                    Vector2 size = MeasureTextEx(font, name.data(), fontSize, textSpacing);
                    totalLabelWidth += size.x + labelSpacing;
                }
                totalLabelWidth -= labelSpacing; // Remove last spacing
                
                float startX = worldPos.x - totalLabelWidth / 2.0f;
                float curX = startX;
                
                for (const auto& [name, color] : labels) {
                    Vector2 size = MeasureTextEx(font, name.data(), fontSize, textSpacing);
                    UIRenderer::DrawTextUI(font, name.data(), curX, worldPos.y + labelYOffset, fontSize, color);
                    curX += size.x + labelSpacing;
                }
            }
        }
    }
}

} // namespace NoMoreDay::systems
