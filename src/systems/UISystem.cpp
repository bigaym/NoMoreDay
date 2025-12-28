#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../components/AIComponent.hpp"
#include "../core/LevelManager.hpp"
#include "../systems/FogOfWarSystem.hpp"
// 假设 LevelManager.hpp 包含了 MapSystem 的定义，如果报错则需要显式包含 MapSystem.hpp
#include "raylib.h"
#include "../tools/Logger.hpp"
#include <format>
#include <algorithm>
#include <vector>

using namespace NoMoreDay;

bool UISystem::m_showCharacterPanel = false;
bool UISystem::m_showInventory = false;
Font UISystem::m_font = { 0 };

// --- 小地图专用静态资源 (无需修改头文件) ---
static Texture2D s_minimapTexture = { 0 };
static int s_minimapW = 0;
static int s_minimapH = 0;
static std::vector<Color> s_minimapPixels;
static bool s_debugRevealMap = false; // 调试：强制显示全图

void UISystem::Initialize() {
    // 尝试加载系统宋体 (Windows)
    // 优先尝试宋体，其次尝试微软雅黑
    const char* fontPaths[] = {
        "assets/fonts/simsun.ttc", // 优先检查项目资源目录
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf"
    };

    // 生成码点列表 (Codepoints)
    // 包含: ASCII + CJK标点 + CJK统一汉字 (常用中文范围)
    std::vector<int> codepoints;
    // ASCII (32-126)
    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
    // CJK Symbols and Punctuation (0x3000-0x303F)
    for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i);
    // CJK Unified Ideographs (0x4E00-0x9FFF) - 约2万字
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i);

    bool loaded = false;
    for (const char* path : fontPaths) {
        if (FileExists(path)) {
            LOG_INFO("Attempting to load font: {}", path);
            m_font = LoadFontEx(path, 24, codepoints.data(), (int)codepoints.size());
            
            if (m_font.texture.id != 0) {
                LOG_INFO("Font loaded successfully from {}. Texture ID: {}", path, m_font.texture.id);
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
                loaded = true;
                break;
            } else {
                LOG_ERROR("Failed to load font texture from {}", path);
            }
        }
    }

    if (!loaded) {
        LOG_WARN("No suitable Chinese font found. Falling back to default font.");
        m_font = GetFontDefault();
    }
}

void UISystem::Shutdown() {
    if (IsFontReady(m_font)) {
        UnloadFont(m_font);
    }
    // 清理小地图纹理
    if (s_minimapTexture.id != 0) {
        UnloadTexture(s_minimapTexture);
        s_minimapTexture.id = 0;
    }
}

void UISystem::DrawTextUI(const char* text, float x, float y, float fontSize, Color color) {
    if (IsFontReady(m_font)) {
        // 使用自定义字体绘制，间距设为 1.0f
        DrawTextEx(m_font, text, { x, y }, fontSize, 1.0f, color);
    } else {
        // 回退到默认绘制
        DrawText(text, (int)x, (int)y, (int)fontSize, color);
    }
}

void UISystem::Update(entt::registry& registry) {
    // 切换角色面板显示
    if (IsKeyPressed(KEY_C)) {
        m_showCharacterPanel = !m_showCharacterPanel;
    }
    // 切换背包显示
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
        m_showInventory = !m_showInventory;
    }
    // 调试：切换小地图全开
    if (IsKeyPressed(KEY_F1)) {
        s_debugRevealMap = !s_debugRevealMap;
        LOG_INFO("Minimap Debug Reveal: {}", s_debugRevealMap ? "ON" : "OFF");
    }
}

void UISystem::Draw(entt::registry& registry, const LevelManager& levelManager) {
    if (m_showInventory) {
        DrawInventory(registry);
    }

    DrawMinimap(registry, levelManager);

    // 角色面板 (仅当开启且存在玩家时绘制)
    if (!m_showCharacterPanel) return;

    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
        DrawCharacterPanel(registry, view.front());
    }
}

void UISystem::DrawCharacterPanel(entt::registry& registry, entt::entity player) {
    // --- 1. 面板背景 ---
    const float panelW = 420.0f;
    const float panelH = 580.0f;
    const float margin = 20.0f;
    
    // 锚定左下角 (Bottom-Left Anchor)
    const float panelX = margin;
    const float panelY = (float)GetScreenHeight() - panelH - margin;
    const float padding = 20.0f;

    // 半透明黑色背景 + 边框
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.85f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0f, GOLD);

    // 标题
    DrawTextUI("角色属性", panelX + padding, panelY + padding, 30, WHITE);
    DrawTextUI("按 'C' 关闭", panelX + panelW - 100, panelY + padding + 10, 18, LIGHTGRAY);

    float currentY = panelY + 70.0f;

    // --- 2. 角色概览 (头像 & 等级) ---
    // 尝试获取 Sprite 和 PlayerStats
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    const auto* pStats = registry.try_get<PlayerStats>(player);

    // 绘制头像 (缩略图)
    float avatarSize = 80.0f;
    DrawRectangleLines(panelX + padding, currentY, avatarSize, avatarSize, LIGHTGRAY);
    if (sprite && sprite->texture.id > 0) {
        // 简单缩放绘制纹理到头像框
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {panelX + padding, currentY, avatarSize, avatarSize};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
    } else {
        DrawTextUI("?", panelX + padding + 30, currentY + 20, 40, GRAY);
    }

    // 绘制等级信息
    float infoX = panelX + padding + avatarSize + 20.0f;
    if (pStats) {
        DrawTextUI(std::format("等级 {}", pStats->level).c_str(), infoX, currentY + 10, 24, GOLD);
        DrawTextUI(std::format("经验: {:.0f}", pStats->experience).c_str(), infoX, currentY + 40, 16, LIGHTGRAY);
    } else {
        DrawTextUI("等级 ??", infoX, currentY + 10, 24, GRAY);
    }

    currentY += avatarSize + 30.0f;

    // 获取属性组件
    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);

    if (!primStats || !combatStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    DrawTextUI("基础属性", panelX + padding, currentY, 20, YELLOW);
    currentY += 25.0f;
    
    float col1X = panelX + padding;
    float col2X = panelX + panelW / 2.0f + padding;

    // 使用 18.0f 字体大小 (原为默认 20.0f)
    DrawStatRow("力量", std::format("{:.0f}", primStats->strength).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("敏捷", std::format("{:.0f}", primStats->dexterity).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("智力", std::format("{:.0f}", primStats->intelligence).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("体能", std::format("{:.0f}", primStats->vitality).c_str(), col1X, currentY, 150, 18.0f);

    currentY += 20.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 20.0f;

    // --- 4. 战斗属性 (Combat Stats) ---
    DrawTextUI("战斗属性", panelX + padding, currentY, 20, RED);
    currentY += 25.0f;

    // 重置 Y 坐标用于双列布局
    float combatStartY = currentY;
    
    // 左列：进攻
    float leftY = combatStartY;
    DrawStatRow("伤害", std::format("{:.0f}-{:.0f}", combatStats->min_weapon_damage, combatStats->max_weapon_damage).c_str(), col1X, leftY, 150);
    DrawStatRow("攻速", std::format("{:.2f}", combatStats->attack_speed).c_str(), col1X, leftY, 150);
    DrawStatRow("暴击率", std::format("{:.1f}%", combatStats->crit_chance * 100.0f).c_str(), col1X, leftY, 150);
    DrawStatRow("暴击伤害", std::format("{:.0f}%", combatStats->crit_damage * 100.0f).c_str(), col1X, leftY, 150);
    // 显示综合冷却效率 (这里简单显示回复速度，或者你可以计算最终系数)
    DrawStatRow("冷却回复", std::format("{:.0f}%", combatStats->cooldown_recovery_speed * 100.0f).c_str(), col1X, leftY, 150);

    // 右列：防御 & 状态
    float rightY = combatStartY;
    DrawStatRow("生命值", std::format("{:.0f}/{:.0f}", combatStats->health, combatStats->max_health).c_str(), col2X, rightY, 150);
    DrawStatRow("护甲", std::format("{:.0f}", combatStats->armor).c_str(), col2X, rightY, 150);
    DrawStatRow("闪避", std::format("{:.1f}%", combatStats->dodge_chance * 100.0f).c_str(), col2X, rightY, 150);
    DrawStatRow("移动速度", std::format("{:.0f}", combatStats->move_speed).c_str(), col2X, rightY, 150);
    DrawStatRow("生命回复", std::format("{:.1f}/s", combatStats->health_regen).c_str(), col2X, rightY, 150);

    currentY = std::max(leftY, rightY) + 20.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 20.0f;

    // --- 5. 抗性 (Resistances) ---
    DrawTextUI("抗性", panelX + padding, currentY, 20, SKYBLUE);
    currentY += 25.0f;

    struct ResInfo { const char* name; Color color; int index; };
    ResInfo resList[] = {
        {"火焰", ORANGE, (int)DamageType::Fire},
        {"冰霜", SKYBLUE, (int)DamageType::Cold},
        {"闪电", YELLOW, (int)DamageType::Lightning},
        {"毒素", LIME, (int)DamageType::Poison},
        {"暗影", PURPLE, (int)DamageType::Shadow},
        {"物理", BEIGE, (int)DamageType::Physical}
    };

    float resX = panelX + padding;
    for (const auto& res : resList) {
        float val = combatStats->resistances[res.index];
        
        // 绘制抗性条背景
        DrawRectangle(resX, currentY, 60, 10, Fade(BLACK, 0.5f));
        // 绘制抗性值条
        float barWidth = std::clamp(val, 0.0f, 1.0f) * 60.0f;
        DrawRectangle(resX, currentY, barWidth, 10, res.color);
        
        // 文字
        DrawTextUI(res.name, resX, currentY + 12, 10, LIGHTGRAY);
        DrawTextUI(std::format("{:.0f}%", val * 100.0f).c_str(), resX, currentY + 24, 10, WHITE);

        resX += 70.0f; // 间距
    }
}

void UISystem::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize) {
    DrawTextUI(label, x, y, fontSize, LIGHTGRAY);
    // 右对齐数值
    float textWidth = 0.0f;
    if (IsFontReady(m_font)) {
        textWidth = MeasureTextEx(m_font, value, fontSize, 1.0f).x;
    } else {
        textWidth = (float)MeasureText(value, (int)fontSize);
    }
    DrawTextUI(value, x + width - textWidth, y, fontSize, WHITE);
    y += fontSize + 5.0f;
}

void UISystem::DrawInventory(entt::registry& registry) {
    const float panelW = 350.0f;
    const float panelH = 500.0f;
    const float margin = 20.0f;
    
    // 锚定右侧居中 (Right-Center Anchor)
    const float panelX = (float)GetScreenWidth() - panelW - margin;
    const float panelY = ((float)GetScreenHeight() - panelH) / 2.0f;
    const float padding = 20.0f;

    // 背景
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.85f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0f, GOLD);

    // 标题
    DrawTextUI("背包", panelX + padding, panelY + padding, 24, WHITE);
    DrawTextUI("按 'I' 关闭", panelX + panelW - 100, panelY + padding + 5, 18, LIGHTGRAY);

    // 绘制网格 (示例：5列 x 8行)
    const int cols = 5;
    const int rows = 8;
    const float cellSize = 50.0f;
    const float gap = 10.0f;
    
    float startX = panelX + (panelW - (cols * cellSize + (cols - 1) * gap)) / 2.0f;
    float startY = panelY + 60.0f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = startX + c * (cellSize + gap);
            float y = startY + r * (cellSize + gap);
            DrawRectangleLines(x, y, cellSize, cellSize, Fade(LIGHTGRAY, 0.3f));
        }
    }
}

void UISystem::DrawMinimap(entt::registry& registry, const LevelManager& levelManager) {
    const auto& map = levelManager.getMapSystem();
    const auto& fog = levelManager.getFogSystem();

    // 小地图配置
    const float mapSize = 150.0f;
    const float margin = 20.0f;
    const float x = (float)GetScreenWidth() - mapSize - margin;
    const float y = margin;

    // 绘制背景边框
    DrawRectangle(x - 2, y - 2, mapSize + 4, mapSize + 4, DARKGRAY);
    DrawRectangle(x, y, mapSize, mapSize, BLACK);

    // 获取地图尺寸 (Grid Units)
    int gridW = fog.getWidth();
    int gridH = fog.getHeight();
    if (gridW == 0 || gridH == 0) return;

    // 1. 获取玩家位置 (作为小地图中心)
    auto view = registry.view<PlayerTag, Position>();
    if (view.begin() == view.end()) return;
    
    entt::entity playerEntity = view.front();
    const auto& playerPos = view.get<Position>(playerEntity);
    
    // 玩家所在的网格坐标
    int playerGx = static_cast<int>(playerPos.x / FogOfWarSystem::TILE_SIZE);
    int playerGy = static_cast<int>(playerPos.y / FogOfWarSystem::TILE_SIZE);

    // 2. 定义视野范围 (半径，单位：格)
    // 25格半径 => 50x50格的显示区域
    const int viewRadius = 25; 
    
    // 计算缩放比例：地图框大小 / (视野直径)
    float scale = mapSize / (float)(viewRadius * 2);

    // --- 3. 纹理更新与绘制 (Texture Update & Draw) ---
    
    // 初始化或重建纹理 (如果尺寸变化或未加载)
    if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
        if (s_minimapTexture.id != 0) UnloadTexture(s_minimapTexture);
        s_minimapW = gridW;
        s_minimapH = gridH;
        s_minimapPixels.resize(gridW * gridH);
        Image img = GenImageColor(gridW, gridH, BLACK);
        s_minimapTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT); // 保持像素清晰
        LOG_INFO("Minimap texture initialized: {}x{}", gridW, gridH);
    }

    // 更新像素数据
    int exploredCount = 0; // 统计已探索格子数
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            int index = gy * gridW + gx;
            Color c = BLACK;

            bool isExplored = fog.isExplored(gx, gy);
            if (isExplored) exploredCount++;

            if (isExplored || s_debugRevealMap) {
                bool isVisible = s_debugRevealMap ? true : fog.isVisible(gx, gy);
                if (map.isWalkable(gx, gy)) {
                    c = isVisible ? Color{180, 180, 180, 255} : Color{80, 80, 80, 255};
                } else {
                    c = isVisible ? Color{100, 100, 100, 255} : Color{40, 40, 40, 255};
                }
            }
            s_minimapPixels[index] = c;
        }
    }
    UpdateTexture(s_minimapTexture, s_minimapPixels.data());

    // 调试日志 (每 5 秒一次，帮助定位全黑问题)
    static double lastLogTime = 0;
    if (GetTime() - lastLogTime > 5.0) {
        lastLogTime = GetTime();
        bool pExplored = fog.isExplored(playerGx, playerGy);
        bool pVisible = fog.isVisible(playerGx, playerGy);
        LOG_INFO("[Minimap Debug] Player Raw: ({:.1f}, {:.1f}) -> Grid: ({}, {}), Explored: {}, TotalExplored: {}, TextureID: {}", 
                 playerPos.x, playerPos.y, playerGx, playerGy, pExplored, exploredCount, s_minimapTexture.id);
    }

    // 绘制纹理部分
    // 源区域：以玩家为中心，半径为 viewRadius 的区域
    Rectangle sourceRec = {
        (float)playerGx - viewRadius, 
        (float)playerGy - viewRadius, 
        (float)viewRadius * 2, 
        (float)viewRadius * 2
    };
    
    // 目标区域：UI 框
    Rectangle destRec = { x, y, mapSize, mapSize };
    
    // 绘制纹理 (Raylib 会自动处理 sourceRec 超出纹理边界的情况)
    DrawTexturePro(s_minimapTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);

    // 4. 绘制怪物 (红点)
    auto enemyView = registry.view<AIComponent, Position>();
    for (auto entity : enemyView) {
        const auto& enemyPos = enemyView.get<Position>(entity);
        
        // 计算相对于玩家的网格距离
        float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
        float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;

        // 仅绘制视野范围内的怪物
        if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
            float drawX = x + (dx + viewRadius) * scale;
            float drawY = y + (dy + viewRadius) * scale;
            DrawCircle((int)(drawX + scale * 0.5f), (int)(drawY + scale * 0.5f), 2.0f, RED);
        }
    }

    // 绘制玩家位置
    // 玩家永远在小地图中心
    DrawCircle((int)(x + mapSize / 2.0f), (int)(y + mapSize / 2.0f), 3.0f, GREEN);
    
    // 边框装饰
    DrawRectangleLines(x, y, mapSize, mapSize, GOLD);
}