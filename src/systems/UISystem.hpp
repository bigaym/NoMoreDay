#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"
#include "../core/ResourceManager.hpp"
#include "UICommon.hpp"

class LevelManager; // 前置声明

class UISystem {
public:
    // --- 公共状态 (供子系统访问) ---
    static NoMoreDay::UIState_t State;

    // --- 生命周期与主循环 ---
    static void Initialize(ResourceManager& resourceManager);
    static void Shutdown();
    static void Update(entt::registry& registry, const LevelManager& levelManager);
    static void Draw(entt::registry& registry, const LevelManager& levelManager, const Camera2D& camera);
    static void Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames = 100);
    
    // --- 资源访问 ---
    static Font GetFont() { return m_font; }
    static Color GetRarityColor(NoMoreDay::Rarity rarity);

    // --- 绘图辅助函数 (供子系统使用) ---
    // 绘制标准物品槽位
    static void DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false);
    // 使用自定义字体绘制文本
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color);
    // 自动缩放文本以适应宽度
    static void DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color);
    
    // --- 交互辅助 ---
    // 打开右键上下文菜单
    static void OpenContextMenu(entt::entity item, bool fromInv, int invIdx, NoMoreDay::EquipmentSlot slot);

private:
    static Font m_font;
    
    // --- 内部状态 (Tooltip / Menu / Popup) ---
    static std::vector<const char*> s_tooltipLines;
    static int s_bufferPoolIndex;
    static char s_textBufferPool[16][128];
    
    // 上下文菜单状态
    static entt::entity m_contextMenuItem;
    static Vector2 m_contextMenuPos;
    static bool m_isContextFromInventory;
    static int m_contextSourceInventoryIndex;
    static NoMoreDay::EquipmentSlot m_contextSourceEquipmentSlot;

    // 数量选择弹窗状态
    static bool m_showQuantityPopup;
    static entt::entity m_quantityTargetItem;
    static int m_quantityActionType; // 0: 丢弃, 1: 销毁
    static int m_quantityVal;
    static int m_quantityMax;
    static char m_quantityInputBuf[16];

    // --- 内部私有函数 ---
    static void DrawTooltip(entt::registry& registry, entt::entity item);
    static void DrawContextMenu(entt::registry& registry);
    static void DrawQuantityPopup(entt::registry& registry);
    static void DrawMessageBox();
    static const char* GetShortItemTypeName(const NoMoreDay::ItemComponent& item);
};
