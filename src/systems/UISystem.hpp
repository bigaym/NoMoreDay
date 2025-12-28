#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"

class LevelManager; // 前置声明，避免循环依赖

class UISystem {
public:
    static void Initialize();
    static void Shutdown();
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry, const LevelManager& levelManager);
    static Font GetFont() { return m_font; }
    static Color GetRarityColor(NoMoreDay::Rarity rarity);

private:
    static bool m_showCharacterPanel;
    static bool m_showInventory;
    static Font m_font;
    
    // 拖拽状态
    static entt::entity m_draggedItem;
    static bool m_isDraggingFromInventory;
    static int m_dragSourceInventoryIndex;
    static NoMoreDay::EquipmentSlot m_dragSourceEquipmentSlot;

    static entt::entity m_hoveredItem;

    static void DrawCharacterPanel(entt::registry& registry, entt::entity player);
    static void DrawInventoryAndEquipment(entt::registry& registry);
    static void DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false);
    static void DrawMinimap(entt::registry& registry, const LevelManager& levelManager);
    // 辅助函数：绘制属性行
    static void DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize = 20.0f);
    static void DrawTooltip(entt::registry& registry, entt::entity item);
    
    // 内部辅助：使用自定义字体绘制文本
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color);
    static const char* GetShortItemTypeName(const NoMoreDay::ItemComponent& item);
};