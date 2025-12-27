#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/PlayerState.hpp"
#include "../components/EnemyComponent.hpp"
#include "../components/AIComponent.hpp"
#include "../systems/MapSystem.hpp"
#include "raylib.h"
#include <string>
#include <algorithm>
#include <cmath>

void UISystem::render(const entt::registry &registry, const MapSystem *mapSystem)
{
    // 获取玩家实体 (假设单人游戏)
    auto view = registry.view<const PlayerTag, const HealthComponent, const WeaponComponent, const PlayerStats, const Position, const DashComponent>();

    for (auto entity : view)
    {
        const auto &health = view.get<HealthComponent>(entity);
        const auto &weapon = view.get<WeaponComponent>(entity);
        const auto &stats = view.get<PlayerStats>(entity);
        const auto &dash = view.get<DashComponent>(entity);

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // --- 1. 玩家血条 (左上角) ---
        float hpPercent = health.current / health.max;
        float barWidth = 200.0f;
        float barHeight = 20.0f;
        float startX = 20.0f;
        float startY = 20.0f;

        // 背景
        DrawRectangle(startX, startY, barWidth, barHeight, Fade(DARKGRAY, 0.8f));
        // 血量
        DrawRectangle(startX, startY, barWidth * hpPercent, barHeight, RED);
        // 边框
        DrawRectangleLines(startX, startY, barWidth, barHeight, WHITE);
        // 文字
        std::string hpText = std::to_string((int)health.current) + " / " + std::to_string((int)health.max);
        DrawText(hpText.c_str(), startX + 10, startY + 2, 20, WHITE);

        // --- 2. 击杀数 (右上角) ---
        std::string killText = "Kills: " + std::to_string(stats.killCount);
        int textWidth = MeasureText(killText.c_str(), 30);
        DrawText(killText.c_str(), screenWidth - textWidth - 20, 20, 30, GOLD);

        // --- 3. 武器冷却 (底部中间) ---
        float cdBoxSize = 60.0f;
        float cdX = screenWidth / 2.0f - cdBoxSize / 2.0f;
        float cdY = screenHeight - cdBoxSize - 20.0f;

        // 背景框
        DrawRectangle(cdX, cdY, cdBoxSize, cdBoxSize, Fade(BLACK, 0.5f));
        DrawRectangleLines(cdX, cdY, cdBoxSize, cdBoxSize, WHITE);

        // 冷却遮罩
        if (weapon.cooldownTimer > 0.0f)
        {
            float cdPercent = weapon.cooldownTimer / weapon.cooldown;
            float overlayHeight = cdBoxSize * cdPercent;

            // 绘制半透明遮罩表示冷却中
            DrawRectangle(cdX, cdY + (cdBoxSize - overlayHeight), cdBoxSize, overlayHeight, Fade(RED, 0.5f));

            // 显示剩余时间
            std::string cdText = std::to_string(weapon.cooldownTimer).substr(0, 3);
            DrawText(cdText.c_str(), cdX + 10, cdY + 20, 20, WHITE);
        }
        else
        {
            // 就绪状态
            DrawText("RDY", cdX + 12, cdY + 20, 20, GREEN);
        }

        DrawText("Weapon", cdX + 5, cdY - 20, 10, LIGHTGRAY);

        // --- 4. 冲刺技能 (武器右侧) ---
        float dashBoxSize = 60.0f;
        float dashX = cdX + cdBoxSize + 20.0f; // 放在武器栏右边
        float dashY = cdY;

        // 背景
        DrawRectangle(dashX, dashY, dashBoxSize, dashBoxSize, Fade(BLACK, 0.5f));
        // 闪烁反馈
        if (dash.uiFlash) {
            DrawRectangle(dashX, dashY, dashBoxSize, dashBoxSize, Fade(WHITE, 0.6f));
        }
        DrawRectangleLines(dashX, dashY, dashBoxSize, dashBoxSize, WHITE);

        // 充能点数 (圆点)
        for (int i = 0; i < dash.maxCharges; ++i) {
            Color color = (i < dash.charges) ? SKYBLUE : Fade(DARKGRAY, 0.5f);
            DrawCircle(dashX + 20 + i * 20, dashY + dashBoxSize - 15, 6, color);
        }

        // 冷却遮罩 (只在未满时显示)
        if (dash.charges < dash.maxCharges) {
            float pct = dash.cooldownTimer / dash.cooldownDuration;
            float h = dashBoxSize * pct;
            DrawRectangle(dashX, dashY + dashBoxSize - h, dashBoxSize, h, Fade(BLACK, 0.6f));
        }

        DrawText("Dash", dashX + 15, dashY - 20, 10, LIGHTGRAY);
        DrawText("SPACE", dashX + 12, dashY + 20, 10, DARKGRAY);

        // --- 5. 小地图 (右下角) ---
        float mapSize = 150.0f;
        float mapX = screenWidth - mapSize - 20.0f;
        float mapY = screenHeight - mapSize - 20.0f;
        Vector2 mapCenter = { mapX + mapSize/2.0f, mapY + mapSize/2.0f };

        // 背景
        DrawRectangle(mapX, mapY, mapSize, mapSize, Fade(BLACK, 0.7f));
        DrawRectangleLines(mapX, mapY, mapSize, mapSize, DARKGRAY);

        // 启用剪裁，防止绘制出框
        BeginScissorMode((int)mapX, (int)mapY, (int)mapSize, (int)mapSize);

        // 玩家位置
        const auto &pPos = view.get<Position>(entity);
        
        // 小地图缩放: 1 世界单位 = ? 小地图像素
        // 瓦片是 10x10. 假设我们在小地图上每个瓦片显示为 4x4 像素.
        // 缩放比例 = 4.0f / 10.0f = 0.4f.
        float minimapScale = 0.4f; 

        // 绘制墙壁 (如果提供了 MapSystem)
        if (mapSystem)
        {
            int mapW = mapSystem->getWidth();
            int mapH = mapSystem->getHeight();
            
            // 计算可见范围 (优化性能，只遍历小地图覆盖的区域)
            // 屏幕半宽 / 缩放 = 世界半宽
            float viewHalfW = (mapSize / 2.0f) / minimapScale;
            
            // 转换为瓦片坐标范围
            int minTileX = static_cast<int>((pPos.x - viewHalfW) / 10.0f) - 1;
            int maxTileX = static_cast<int>((pPos.x + viewHalfW) / 10.0f) + 1;
            int minTileY = static_cast<int>((pPos.y - viewHalfW) / 10.0f) - 1;
            int maxTileY = static_cast<int>((pPos.y + viewHalfW) / 10.0f) + 1;
            
            // 边界限制
            minTileX = std::max(0, minTileX);
            maxTileX = std::min(mapW, maxTileX);
            minTileY = std::max(0, minTileY);
            maxTileY = std::min(mapH, maxTileY);

            for (int y = minTileY; y < maxTileY; ++y)
            {
                for (int x = minTileX; x < maxTileX; ++x)
                {
                    if (mapSystem->isExplored(x, y) && mapSystem->getTileType(x, y) == Tile::Type::WALL)
                    {
                        // 计算相对于玩家的偏移
                        float worldX = x * 10.0f;
                        float worldY = y * 10.0f;
                        
                        float diffX = worldX - pPos.x;
                        float diffY = worldY - pPos.y;
                        
                        float drawX = mapCenter.x + diffX * minimapScale;
                        float drawY = mapCenter.y + diffY * minimapScale;
                        
                        float size = 10.0f * minimapScale;
                        DrawRectangle(drawX, drawY, size, size, LIGHTGRAY);
                    }
                }
            }
        }

        // 绘制敌人 (红点)
        auto enemyView = registry.view<const EnemyTag, const Position>();
        for (auto [e, pos] : enemyView.each())
        {
            float diffX = pos.x - pPos.x;
            float diffY = pos.y - pPos.y;
            
            float drawX = mapCenter.x + diffX * minimapScale;
            float drawY = mapCenter.y + diffY * minimapScale;
            
            // 简单的距离检查，避免画太远
            if (drawX >= mapX && drawX <= mapX + mapSize && 
                drawY >= mapY && drawY <= mapY + mapSize) {
                DrawRectangle(drawX - 1, drawY - 1, 3, 3, RED);
            }
        }

        // 绘制玩家 (始终在中心)
        DrawRectangle(mapCenter.x - 2, mapCenter.y - 2, 4, 4, GREEN);
        
        EndScissorMode();

        // --- 4. 调试信息 (左下角) ---
        /*
        DrawText(TextFormat("Ents: %d", registry.alive()), 20, screenHeight - 40, 20, GREEN);
        */

        // 由于是单人游戏，找到一个就退出
        break;
    }
}