#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/components/Buff.hpp"

namespace NoMoreDay::systems {

void MonsterHealthBarSystem::Render(entt::registry& registry, const Camera2D& camera) {
    // 1. Calculate Viewport Bounds in World Space
    Vector2 screenMin = GetScreenToWorld2D({ 0, 0 }, camera);
    Vector2 screenMax = GetScreenToWorld2D({ (float)GetScreenWidth(), (float)GetScreenHeight() }, camera);
    
    // Add some padding to avoid bars popping in/out at edges
    float padding = 100.0f;
    Rectangle viewBounds = { 
        screenMin.x - padding, 
        screenMin.y - padding, 
        (screenMax.x - screenMin.x) + padding * 2.0f, 
        (screenMax.y - screenMin.y) + padding * 2.0f 
    };

    auto view = registry.view<EnemyTag, Position, HealthComponent>(entt::exclude<KilledTag>);
    
    // Performance optimization: limit number of health bars drawn per frame if density is insane
    int drawCount = 0;
    const int MAX_BARS_PER_FRAME = 200; 

    for (auto entity : view) {
        if (drawCount >= MAX_BARS_PER_FRAME) break;

        const auto& pos = view.get<Position>(entity);
        
        // --- 2. Viewport Culling ---
        // Fast bounds check before more complex logic
        if (pos.x < viewBounds.x || pos.x > viewBounds.x + viewBounds.width ||
            pos.y < viewBounds.y || pos.y > viewBounds.y + viewBounds.height) {
            continue;
        }

        const auto& hp = view.get<HealthComponent>(entity);

        // Skip dead monsters
        if (hp.current <= 0) continue;

        // ...
        
        bool isTracking = false;
        if (auto* ai = registry.try_get<AIComponent>(entity)) {
            if (ai->aiType == AIType::CHASE || ai->aiType == AIType::ATTACK) {
                isTracking = true;
            }
        }
        
        bool isDamaged = hp.current < hp.max - 0.1f;
        
        bool isRare = false;
        EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
        if (auto* rarityComp = registry.try_get<EnemyRarityComponent>(entity)) {
            rarity = rarityComp->rarity;
            if (rarity > EnemyRarityComponent::NORMAL) {
                isRare = true;
            }
        }

        // --- ARPG Optimization: Only show HP bar if interesting (damaged, elite, or attacking) ---
        if (!isTracking && !isDamaged && !isRare) continue;
        
        drawCount++;
        float hpPercent = hp.current / hp.max;
        hpPercent = std::clamp(hpPercent, 0.0f, 1.0f);

        // Health bar dimensions - Elites get larger bars
        float barWidth = isRare ? 54.0f : 40.0f;
        float barHeight = isRare ? 6.0f : 4.0f;
        float yOffset = -20.0f; 

        Vector2 worldPos = { pos.x, pos.y };
        
        // Background
        Rectangle bgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth, barHeight };
        DrawRectangleRec(bgRect, { 20, 20, 20, 180 });

        // Foreground
        Color barColor = { 200, 30, 30, 255 };
        if (hpPercent < 0.25f) barColor = { 255, 40, 40, 255 }; 
        if (isRare) barColor = { 255, 200, 0, 255 }; // Gold for elites
        
        Rectangle fgRect = { worldPos.x - barWidth / 2.0f, worldPos.y + yOffset, barWidth * hpPercent, barHeight };
        DrawRectangleRec(fgRect, barColor);

        // Only draw extra details for Rares+ to save CPU
        if (isRare) {
            DrawRectangle(fgRect.x, fgRect.y, fgRect.width, 1, { 255, 255, 255, 80 });
            DrawRectangleLinesEx(bgRect, 1.0f, { 10, 10, 10, 255 });
        }

        // --- NAME DISPLAY (Rares Only) ---
        if (isRare) {
            Font font = UISystem::GetFont();
            float nameFontSize = 12.0f;
            
            std::string name = "未知怪物";
            Color nameColor = WHITE;

            if (auto* enemyState = registry.try_get<EnemyStateComponent>(entity)) {
                name = kRaceData[static_cast<size_t>(enemyState->raceType)].name;
            }

            switch (rarity) {
                case EnemyRarityComponent::CHAMPION: name = "冠军: " + name; nameColor = SKYBLUE; break;
                case EnemyRarityComponent::ELITE:    name = "精英: " + name; nameColor = GOLD; break;
                case EnemyRarityComponent::BOSS:     name = "首领: " + name; nameColor = ORANGE; break;
                case EnemyRarityComponent::NEMESIS:  name = "宿敌: " + name; nameColor = RED; break;
                default: break;
            }

            Vector2 nameSize = MeasureTextEx(font, name.c_str(), nameFontSize, 0.0f);
            UIRenderer::DrawTextUI(font, name.data(), worldPos.x - nameSize.x / 2.0f, yOffset + worldPos.y - nameSize.y - 4.0f, nameFontSize, nameColor);
        }

        // --- AFFIX LABELS (Only for Rares) ---
        if (isRare) {
            if (auto* affixComp = registry.try_get<NoMoreDay::MonsterAffixComponent>(entity)) {
                if (!affixComp->affixes.empty()) {
                    float labelYOffset = yOffset + barHeight + 2.0f;
                    float labelSpacing = 2.0f;
                    
                    // Calculate total width for centering
                    std::vector<std::pair<std::string_view, Color>> labels;
                    for (auto affixType : affixComp->affixes) {
                        const auto& def = NoMoreDay::MonsterAffixRegistry::GetAffixDef(affixType);
                        Color labelColor = { def.tintR, def.tintG, def.tintB, 255 };
                        labels.emplace_back(def.name, labelColor);
                    }
                    
                    float totalLabelWidth = 0.0f;
                    Font font = UISystem::GetFont();
                    float fontSize = 10.0f; 
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
}

} // namespace NoMoreDay::systems
