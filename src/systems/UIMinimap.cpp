#include "UIMinimap.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/AIComponent.hpp"
#include "../core/LevelManager.hpp"
#include "../core/UIRenderer.hpp"
#include "FogOfWarSystem.hpp"
#include "raylib.h"
#include "../tools/Logger.hpp"
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

    // Layout in Logic Space
    const float mapSize = 150.0f;
    const float margin = 20.0f;
    // Anchor Top-Right relative to UI_REF_WIDTH
    const float x = UI_REF_WIDTH - mapSize - margin;
    const float y = margin;

    // Helpers
    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), c);
    };
    auto DrawRectLinesScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangleLinesEx({x*scale, y*scale, w*scale, h*scale}, 1.0f*scale, c);
    };

    DrawRectScaled(x - 2, y - 2, mapSize + 4, mapSize + 4, DARKGRAY);
    DrawRectScaled(x, y, mapSize, mapSize, BLACK);

    int gridW = fog.getWidth();
    int gridH = fog.getHeight();
    if (gridW == 0 || gridH == 0) return;

    auto view = registry.view<PlayerTag, Position>();
    if (view.begin() == view.end()) return;
    
    entt::entity playerEntity = view.front();
    const auto& playerPos = view.get<Position>(playerEntity);
    
    int playerGx = static_cast<int>(playerPos.x / FogOfWarSystem::TILE_SIZE);
    int playerGy = static_cast<int>(playerPos.y / FogOfWarSystem::TILE_SIZE);
    const int viewRadius = 25; 
    float minimapScale = mapSize / (float)(viewRadius * 2); // logic scale of map content

    // 初始化纹理
    if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
        if (s_minimapTexture.id != 0) UnloadTexture(s_minimapTexture);
        s_minimapW = gridW;
        s_minimapH = gridH;
        s_minimapPixels.resize(gridW * gridH);
        Image img = GenImageColor(gridW, gridH, BLACK);
        s_minimapTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT);
        s_minimapDirty = true;
    }

    // 更新纹理 (每10帧或Dirty)
    static int frameCounter = 0;
    if (frameCounter++ % 10 == 0 || s_minimapDirty) {
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
                        c = isVisible ? Color{180, 180, 180, 255} : Color{80, 80, 80, 255};
                    } else {
                        c = isVisible ? Color{100, 100, 100, 255} : Color{40, 40, 40, 255};
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

    // Draw Texture Scaled
    Rectangle sourceRec = { (float)playerGx - viewRadius, (float)playerGy - viewRadius, (float)viewRadius * 2, (float)viewRadius * 2 };
    Rectangle destRec = { x * scale, y * scale, mapSize * scale, mapSize * scale };
    DrawTexturePro(s_minimapTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);

    // 绘制怪物
    auto enemyView = registry.view<EnemyTag, Position>();
    for (auto entity : enemyView) {
        const auto& enemyPos = enemyView.get<Position>(entity);
        float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
        float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;
        if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
            // Logic Position relative to minimap origin
            float logicX = x + (dx + viewRadius) * minimapScale + minimapScale * 0.5f;
            float logicY = y + (dy + viewRadius) * minimapScale + minimapScale * 0.5f;
            DrawCircle((int)(logicX * scale), (int)(logicY * scale), 2.0f * scale, RED);
        }
    }
    
    // Player Center
    DrawCircle((int)((x + mapSize / 2.0f) * scale), (int)((y + mapSize / 2.0f) * scale), 3.0f * scale, GREEN);
    DrawRectLinesScaled(x, y, mapSize, mapSize, GOLD);
}