#include "game/application/ui/MonsterHealthBarSystem.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/EliteModifierComponents.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Buff.hpp"
#include "engine/render/CoordSystem.hpp"
#include <cfloat>

namespace NoMoreDay::systems {

struct HealthBarDrawCmd {
    Rectangle bgRect;
    Rectangle fgRect;
    Color fgColor;
    bool isRare;
};

// Initialize static member
entt::entity MonsterHealthBarSystem::s_hoveredEntity = entt::null;

void MonsterHealthBarSystem::RenderUI(entt::registry& registry) {
    if (registry.valid(s_hoveredEntity)) {
        DrawTargetWidget(registry, s_hoveredEntity);
    }
}

void MonsterHealthBarSystem::Render(entt::registry& registry, const Camera2D& camera) {
    // --- 1. Setup & Mouse Detection ---
    const NoMoreDay::render::coord::Camera2DTransform cam =
        NoMoreDay::render::coord::Camera2DTransform::From(camera);
    Vector2 mousePosScreen = GetMousePosition();
    Vector2 mousePosWorld = NoMoreDay::render::coord::ScenePixelToWorld(cam, mousePosScreen);
    
    // Viewport Culling Bounds
    Vector2 screenMin = NoMoreDay::render::coord::ScenePixelToWorld(cam, { 0, 0 });
    Vector2 screenMax = NoMoreDay::render::coord::ScenePixelToWorld(
        cam, { (float)GetScreenWidth(), (float)GetScreenHeight() });
    float padding = 100.0f;
    Rectangle viewBounds = { 
        screenMin.x - padding, screenMin.y - padding, 
        (screenMax.x - screenMin.x) + padding * 2.0f, (screenMax.y - screenMin.y) + padding * 2.0f 
    };

    auto view = registry.view<EnemyTag, Position, HealthComponent>(entt::exclude<KilledTag>);
    
    std::vector<HealthBarDrawCmd> batch;
    batch.reserve(200);

    // Reset hovered entity for this frame
    s_hoveredEntity = entt::null;
    float closestDistSq = FLT_MAX;
    const float HOVER_RADIUS_SQ = 40.0f * 40.0f; // Mouse pick radius
    
    // --- 2. Iterate Entities ---
    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        
        // Culling
        if (pos.x < viewBounds.x || pos.x > viewBounds.x + viewBounds.width ||
            pos.y < viewBounds.y || pos.y > viewBounds.y + viewBounds.height) {
            continue;
        }

        const auto& hp = view.get<HealthComponent>(entity);
        if (hp.current <= 0) continue;

        // Mouse Hover Check (Find closest under cursor)
        float dx = pos.x - mousePosWorld.x;
        float dy = pos.y - mousePosWorld.y;
        float distSq = dx*dx + dy*dy;
        
        // Use Entity Radius if available, else default
        float pickRadiusSq = HOVER_RADIUS_SQ;
        if (registry.all_of<Radius>(entity)) {
            float r = registry.get<Radius>(entity).value + 10.0f; // slightly larger for easier picking
            pickRadiusSq = r * r;
        }

        if (distSq < pickRadiusSq && distSq < closestDistSq) {
            closestDistSq = distSq;
            s_hoveredEntity = entity;
        }

        // --- Overhead Bar Logic ---
        bool isDamaged = hp.current < hp.max - 0.1f;
        
        // Optimization: Only draw overhead bar if DAMAGED
        if (!isDamaged) continue;

        float hpPercent = std::clamp(hp.current / hp.max, 0.0f, 1.0f);
        
        // Simple Overhead Bar
        float barWidth = 40.0f;
        float barHeight = 4.0f;
        float yOffset = -25.0f;
        
        HealthBarDrawCmd cmd;
        cmd.bgRect = { pos.x - barWidth / 2.0f, pos.y + yOffset, barWidth, barHeight };
        cmd.fgRect = { cmd.bgRect.x, cmd.bgRect.y, barWidth * hpPercent, barHeight };
        
        // Color coding by HP state
        if (hpPercent < 0.25f) cmd.fgColor = { 255, 40, 40, 255 }; // Vital
        else cmd.fgColor = { 200, 30, 30, 255 }; // Normal Red

        // Tint for Rarity (Subtle)
        if (auto* r = registry.try_get<EnemyRarityComponent>(entity)) {
            if (r->rarity > EnemyRarityComponent::NORMAL) {
                cmd.isRare = true;
                cmd.fgColor = { 255, 180, 0, 255 }; // Gold-ish for damaged elites
            } else {
                cmd.isRare = false;
            }
        } else {
            cmd.isRare = false;
        }

        batch.push_back(cmd);
    }

    // --- 3. Render Overhead Bars (Batch) ---
    // Backgrounds
    for (const auto& cmd : batch) DrawRectangleRec(cmd.bgRect, { 20, 20, 20, 180 });
    // Foregrounds
    for (const auto& cmd : batch) DrawRectangleRec(cmd.fgRect, cmd.fgColor);
    // Borders (Optional, keep for Rares for visual hierarchy)
    for (const auto& cmd : batch) {
        if (cmd.isRare) DrawRectangleLinesEx(cmd.bgRect, 1.0f, { 0, 0, 0, 200 });
    }

    // RenderUI will handle the widget later, outside Mode2D
}

// Helper for Top Widget (Implementation local to file for now)
void MonsterHealthBarSystem::DrawTargetWidget(entt::registry& registry, entt::entity entity) {
    const auto& hp = registry.get<HealthComponent>(entity);
    float hpPercent = std::clamp(hp.current / hp.max, 0.0f, 1.0f);

    float screenW = (float)GetScreenWidth();
    float width = 400.0f;
    float height = 40.0f; // Bar height
    float x = (screenW - width) / 2.0f;
    float y = 50.0f; // Top margin

    // 1. Background Frame
    Rectangle bgRect = { x, y, width, height };
    DrawRectangleRounded(bgRect, 0.1f, 6, { 10, 10, 10, 230 });
    DrawRectangleRoundedLinesEx(bgRect, 0.1f, 6, 2.0f, { 50, 50, 50, 255 });

    // 2. Health Bar
    float margin = 4.0f;
    Rectangle barBg = { x + margin, y + margin, width - margin*2, height - margin*2 };
    DrawRectangleRec(barBg, { 30, 0, 0, 255 }); // Dark Red backing
    
    Rectangle barFg = { barBg.x, barBg.y, barBg.width * hpPercent, barBg.height };
    
    // Rarity Colors
    Color barColor = { 180, 20, 20, 255 }; // Normal Red
    Color nameColor = WHITE;

    EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
    if (auto* r = registry.try_get<EnemyRarityComponent>(entity)) {
        rarity = r->rarity;
        switch (rarity) {
            case EnemyRarityComponent::CHAMPION: barColor = SKYBLUE; nameColor = SKYBLUE; break;
            case EnemyRarityComponent::ELITE:    barColor = GOLD;    nameColor = GOLD; break;
            case EnemyRarityComponent::BOSS:     barColor = ORANGE;  nameColor = ORANGE; break;
            case EnemyRarityComponent::NEMESIS:  barColor = RED;     nameColor = RED; break;
            default: break;
        }
    }
    DrawRectangleRec(barFg, barColor);

    // 3. Name & Info
    Font font = UISystem::GetFont();
    std::string name = "Enemy";
    if (auto* s = registry.try_get<EnemyStateComponent>(entity)) {
        if(static_cast<size_t>(s->raceType) < kRaceData.size())
            name = kRaceData[static_cast<size_t>(s->raceType)].name;
    }

    // Draw Name (Above bar)
    float nameSize = 18.0f;
    Vector2 nameDim = MeasureTextEx(font, name.c_str(), nameSize, 0);
    UIRenderer::DrawTextUI(font, name.c_str(), x + (width - nameDim.x)/2.0f, y - 25.0f, nameSize, nameColor);

    // Draw HP Text (Inside bar)
    std::string hpText = TextFormat("%.0f / %.0f", hp.current, hp.max);
    Vector2 hpDim = MeasureTextEx(font, hpText.c_str(), 16.0f, 0);
    UIRenderer::DrawTextUI(font, hpText.c_str(), x + (width - hpDim.x)/2.0f, y + (height - hpDim.y)/2.0f, 16.0f, WHITE);

    // 4. Affixes (Below bar)
    if (auto* affixComp = registry.try_get<NoMoreDay::MonsterAffixComponent>(entity)) {
        if (!affixComp->affixes.empty()) {
            float labelX = x;
            float labelY = y + height + 8.0f;
            
            for (auto type : affixComp->affixes) {
                const auto& def = NoMoreDay::MonsterAffixRegistry::GetAffixDef(type);
                std::string label = "[" + std::string(def.name) + "]";
                Color c = { def.tintR, def.tintG, def.tintB, 255 };
                
                UIRenderer::DrawTextUI(font, label.c_str(), labelX, labelY, 14.0f, c);
                labelX += MeasureTextEx(font, label.c_str(), 14.0f, 0).x + 5.0f;
            }
        }
    }
}

} // namespace NoMoreDay::systems
