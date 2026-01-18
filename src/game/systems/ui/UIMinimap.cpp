#include "game/systems/ui/UIMinimap.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/data/AffixMapping.hpp" // Added by user instruction
#include "raylib.h"
#include "core/logging/Logger.hpp"
#include <vector>

using namespace NoMoreDay;

static Texture2D s_minimapTexture = { 0 };
static int s_minimapW = 0;
static int s_minimapH = 0;
static std::vector<Color> s_minimapPixels;
static bool s_debugRevealMap = false;
static bool s_minimapDirty = true;

void UIMinimap::Cleanup() {
    if (s_minimapTexture.id != 0) {
        UnloadTexture(s_minimapTexture);
        s_minimapTexture.id = 0;
    }
}

void UIMinimap::ToggleDebugReveal() {
    s_debugRevealMap = !s_debugRevealMap;
    s_minimapDirty = true;
    LOG_INFO("Minimap Debug Reveal: {}", s_debugRevealMap ? "ON" : "OFF");
}

void UIMinimap::Draw(entt::registry& registry, const LevelManager& levelManager) {
    const auto& map = levelManager.getMapSystem();
    const auto& fog = levelManager.getFogSystem();

    // Scale
    float scale = UIRenderer::GetScale();
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();

    // Layout in Logic Space
    const float mapSize = Constants::MAP_SIZE; // Slightly larger
    const float margin = Constants::MARGIN;
    // Anchor Top-Right relative to UI_REF_WIDTH
    const float x = UI_REF_WIDTH - mapSize - margin;
    const float y = margin;

    // Helpers
    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), c);
    };
    auto DrawRectLinesScaled = [&](float x, float y, float w, float h, float thick, Color c) {
        DrawRectangleLinesEx({x*scale, y*scale, w*scale, h*scale}, thick*scale, c);
    };

    // Background Shadow/Border
    DrawRectScaled(x - 4, y - 4, mapSize + 8, mapSize + 8, theme.panelBackground);
    DrawRectLinesScaled(x - 4, y - 4, mapSize + 8, mapSize + 8, 1.0f, theme.panelBorder);

    int gridW = fog.getWidth();
    int gridH = fog.getHeight();
    if (gridW == 0 || gridH == 0) return;

    auto view = registry.view<PlayerTag, Position>();
    if (view.begin() == view.end()) return;
    
    entt::entity playerEntity = view.front();
    const auto& playerPos = view.get<Position>(playerEntity);
    
    int playerGx = static_cast<int>(playerPos.x / FogOfWarSystem::TILE_SIZE);
    int playerGy = static_cast<int>(playerPos.y / FogOfWarSystem::TILE_SIZE);
    const int viewRadius = Constants::VIEW_RADIUS; // Increased view radius
    float minimapScale = mapSize / (float)(viewRadius * 2); // logic scale of map content

    // 初始化纹理 (Initialize Texture)
    if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
        if (s_minimapTexture.id != 0) UnloadTexture(s_minimapTexture);
        s_minimapW = gridW;
        s_minimapH = gridH;
        s_minimapPixels.assign(gridW * gridH, BLACK);
        Image img = GenImageColor(gridW, gridH, BLACK);
        s_minimapTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT);
        s_minimapDirty = true;
    }

    // 更新纹理 (Update Texture) - Time-based refresh (~6 Hz, ~10 frames at 60 FPS)
    static float minimapRefreshTimer = 0.0f;
    minimapRefreshTimer += GetFrameTime();
    if (minimapRefreshTimer >= Constants::REFRESH_INTERVAL || s_minimapDirty) {
        minimapRefreshTimer = 0.0f;
        bool changed = false;
        for (int gy = 0; gy < gridH; ++gy) {
            for (int gx = 0; gx < gridW; ++gx) {
                int index = gy * gridW + gx;
                Color oldC = s_minimapPixels[index];
                Color c = BLACK;

                bool isExplored = fog.isExplored(gx, gy);
                if (isExplored || s_debugRevealMap) {
                    bool isVisible = s_debugRevealMap ? true : fog.isVisible(gx, gy);
                    if (map.isWalkable(gx, gy)) {
                        // Use theme colors but darkened for minimap background
                        c = isVisible ? Color{160, 160, 160, 255} : Color{60, 60, 60, 255};
                    } else {
                        c = isVisible ? Color{80, 80, 80, 255} : Color{30, 30, 30, 255};
                    }
                }
                if (c.r != oldC.r || c.g != oldC.g || c.b != oldC.b) {
                    s_minimapPixels[index] = c;
                    changed = true;
                }
            }
        }
        if (changed) UpdateTexture(s_minimapTexture, s_minimapPixels.data());
        s_minimapDirty = false;
    }

    // Draw Map Texture
    Rectangle sourceRec = { (float)playerGx - viewRadius, (float)playerGy - viewRadius, (float)viewRadius * 2, (float)viewRadius * 2 };
    Rectangle destRec = { x * scale, y * scale, mapSize * scale, mapSize * scale };
    DrawTexturePro(s_minimapTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);

    // Scanline / Overlay effect
    DrawRectangleGradientV((int)(x*scale), (int)(y*scale), (int)(mapSize*scale), (int)(mapSize*scale), Fade(WHITE, 0.05f), Fade(BLACK, 0.1f));

    // 绘制怪物 (Draw Enemies)
    auto enemyView = registry.view<EnemyTag, Position>();
    for (auto entity : enemyView) {
        const auto& enemyPos = enemyView.get<Position>(entity);
        float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
        float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;
        if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
            float logicX = x + (dx + viewRadius) * minimapScale;
            float logicY = y + (dy + viewRadius) * minimapScale;
            DrawCircle((int)(logicX * scale), (int)(logicY * scale), Constants::ENEMY_MARKER_SIZE * scale, theme.danger);
        }
    }
    
    // Player Center (Marker)
    float centerX = x + mapSize / 2.0f;
    float centerY = y + mapSize / 2.0f;
    DrawCircle((int)(centerX * scale), (int)(centerY * scale), Constants::PLAYER_MARKER_SIZE * scale, theme.success);
    DrawCircleLines((int)(centerX * scale), (int)(centerY * scale), Constants::PLAYER_MARKER_SIZE * scale, WHITE);

    // North Indicator
    UIRenderer::DrawTextUI(font, "N", x + mapSize / 2.0f - 5.0f, y - 20.0f, 18, theme.textSecondary, 1.0f);
    
    // Decorative Frame
    DrawRectLinesScaled(x, y, mapSize, mapSize, 2.0f, theme.panelBorderHighlight);
    
    // Coordinates or Zone Name
    const char* zoneName = levelManager.getCurrentBiome() == "town" ? "宁静村落" : "地下城 - 1层";
    if (levelManager.getCurrentBiome() != "town") {
        static char zoneBuf[64];
        // Unify naming to '异界' (Otherworld) for all combat zones per user request
        snprintf(zoneBuf, sizeof(zoneBuf), "异界 - %d层", levelManager.getCurrentLevel());
        zoneName = zoneBuf;
    }

    float tw = IsFontValid(font) ? MeasureTextEx(font, zoneName, 18, 1.0f).x : (float)MeasureText(zoneName, 18);
    UIRenderer::DrawTextUI(font, zoneName, x + mapSize - tw, y + mapSize + 10.0f, 18, theme.textHighlight, 1.0f);

    // 1. Kill Count (Below Minimap Level Name)
    auto* pStats = registry.try_get<PlayerStats>(playerEntity);
    if (pStats && levelManager.getCurrentBiome() != "town") {
        char killBuf[64];
        Color killColor = (pStats->current_map_kills >= 100) ? theme.success : theme.textSecondary;
        
        if (pStats->current_map_kills < 100) {
            snprintf(killBuf, sizeof(killBuf), "击杀: %u / 100", pStats->current_map_kills);
        } else {
            snprintf(killBuf, sizeof(killBuf), "击杀: %u (出口已标位)", pStats->current_map_kills);
        }
        
        float killTw = IsFontValid(font) ? MeasureTextEx(font, killBuf, 16, 1.0f).x : (float)MeasureText(killBuf, 16);
        UIRenderer::DrawTextUI(font, killBuf, x + mapSize - killTw, y + mapSize + 35.0f, 16, killColor, 1.0f);

        // 2. Navigation Arrow (Points to NextLevel Portal)
        if (pStats->current_map_kills >= 100) {
            entt::entity exitPortal = entt::null;
            auto portalView = registry.view<PortalComponent, Position>();
            for (auto e : portalView) {
                if (portalView.get<PortalComponent>(e).type == PortalType::NextLevel) {
                    exitPortal = e;
                    break;
                }
            }

            if (exitPortal != entt::null) {
                const auto& portalPos = portalView.get<Position>(exitPortal);
                float dx = portalPos.x - playerPos.x;
                float dy = portalPos.y - playerPos.y;
                float angle = atan2f(dy, dx);
                
                // Draw rotating arrow next to text
                float arrowX = x + mapSize - killTw - 20.0f;
                float arrowY = y + mapSize + 43.0f;
                
                // Draw simple arrow head pointing towards portal
                Vector2 center = { arrowX * scale, arrowY * scale };
                float arrowSize = 10.0f * scale;
                Vector2 v1 = { center.x + cosf(angle) * arrowSize, center.y + sinf(angle) * arrowSize };
                Vector2 v2 = { center.x + cosf(angle + 2.4f) * arrowSize * 0.6f, center.y + sinf(angle + 2.4f) * arrowSize * 0.6f };
                Vector2 v3 = { center.x + cosf(angle - 2.4f) * arrowSize * 0.6f, center.y + sinf(angle - 2.4f) * arrowSize * 0.6f };
                
                DrawTriangle(v1, v2, v3, theme.success);
                // Glow effect
                DrawCircleGradient((int)center.x, (int)center.y, arrowSize * 1.5f, Fade(theme.success, 0.3f), BLANK);
            }
        }

        // 3. Map Affixes (Bonuses/Modifiers)
        if (levelManager.isMosaicLevel()) {
            const auto& resonance = levelManager.getCurrentResonance();
            float bonusY = y + mapSize + 65.0f;
            
            auto drawBonus = [&](const char* label, float value, bool isPositive) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s: %+.0f%%", label, (value - 1.0f) * 100.0f);
                Color c = isPositive ? components::Colors::MAP_AFFIX_POSITIVE : components::Colors::MAP_AFFIX_NEGATIVE;
                float tw = MeasureTextEx(font, buf, 14, 1.0f).x;
                UIRenderer::DrawTextUI(font, buf, x + mapSize - tw, bonusY, 14, c, 1.0f);
                bonusY += 18.0f;
            };

            if (resonance.totalEnemyDensity != 1.0f) {
                // Density buff (value > 1.0) is negative for player (more enemies)
                drawBonus("怪物密度", resonance.totalEnemyDensity, resonance.totalEnemyDensity < 1.0f); 
            }
            if (resonance.totalDropRate != 1.0f) {
                drawBonus("物品掉落", resonance.totalDropRate, resonance.totalDropRate > 1.0f);
            }
            if (resonance.totalLevelMod != 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "怪物等级: %+d", resonance.totalLevelMod);
                float tw = MeasureTextEx(font, buf, 14, 1.0f).x;
                UIRenderer::DrawTextUI(font, buf, x + mapSize - tw, bonusY, 14, components::Colors::MAP_AFFIX_NEGATIVE, 1.0f);
                bonusY += 18.0f;
            }
            
            // Dominant Element
            if (resonance.dominantElement != FragmentElement::None) {
                const char* elemName = FragmentElementzh[static_cast<size_t>(resonance.dominantElement)].data();
                float tw = MeasureTextEx(font, elemName, 14, 1.0f).x;
                UIRenderer::DrawTextUI(font, elemName, x + mapSize - tw, bonusY, 14, GOLD, 1.0f);
                bonusY += 18.0f;
            }
        }
    }
}
