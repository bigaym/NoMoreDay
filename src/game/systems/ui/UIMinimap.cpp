#include "game/systems/ui/UIMinimap.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/AIComponent.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
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
    const float mapSize = 180.0f; // Slightly larger
    const float margin = 30.0f;
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
    const int viewRadius = 30; // Increased view radius
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
    if (minimapRefreshTimer >= 0.166f || s_minimapDirty) {
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
            DrawCircle((int)(logicX * scale), (int)(logicY * scale), 2.5f * scale, theme.danger);
        }
    }
    
    // Player Center (Marker)
    float centerX = x + mapSize / 2.0f;
    float centerY = y + mapSize / 2.0f;
    DrawCircle((int)(centerX * scale), (int)(centerY * scale), 4.0f * scale, theme.success);
    DrawCircleLines((int)(centerX * scale), (int)(centerY * scale), 4.0f * scale, WHITE);

    // North Indicator
    UIRenderer::DrawTextUI(font, "N", x + mapSize / 2.0f - 5.0f, y - 20.0f, 18, theme.textSecondary, 1.0f);
    
    // Decorative Frame
    DrawRectLinesScaled(x, y, mapSize, mapSize, 2.0f, theme.panelBorderHighlight);
    
    // Coordinates or Zone Name
    const char* zoneName = "地下城 - 1层";
    float tw = IsFontValid(font) ? MeasureTextEx(font, zoneName, 18, 1.0f).x : (float)MeasureText(zoneName, 18);
    UIRenderer::DrawTextUI(font, zoneName, x + mapSize - tw, y + mapSize + 10.0f, 18, theme.textHighlight, 1.0f);
}
