#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

class LevelManager; // 前置声明，避免循环依赖

class UISystem {
public:
    static void Initialize();
    static void Shutdown();
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry, const LevelManager& levelManager);

private:
    static bool m_showCharacterPanel;
    static bool m_showInventory;
    static Font m_font;

    static void DrawCharacterPanel(entt::registry& registry, entt::entity player);
    static void DrawInventory(entt::registry& registry);
    static void DrawMinimap(entt::registry& registry, const LevelManager& levelManager);
    // 辅助函数：绘制属性行
    static void DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize = 20.0f);
    
    // 内部辅助：使用自定义字体绘制文本
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color);
};