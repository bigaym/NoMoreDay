#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "game/foundation/components/ItemComponent.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

class LevelManager; 

class UISystem {
public:
    // --- Lifecycle ---
    static void Initialize(ResourceManager& resourceManager);
    static void Shutdown();

    // --- Resource Access ---
    // U8 收尾: 字体为 UISystem 私有 static（渲染资源类常量，非可变 UI 状态），
    // 由 GameplayRenderAdapter::SetFont 注入组合根；GetRarityColor 委托
    // NoMoreDayGameUiShared（无状态纯函数）。
    static Font GetFont() { return s_globalFont; }
    static Font GetEmojiFont() { return s_emojiFont; }
    // 当前 UI 缩放因子（UIRenderer 每帧由 GameUiHost::Draw 经 SetScale 维护）。
    static float GetScaleFactor();
    static Color GetRarityColor(NoMoreDay::Rarity rarity) { return NoMoreDay::UiShared::GetRarityColor(rarity); }

    // --- Drawing Helpers (Delegated to UIRenderer) ---
    static void DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false, float alpha = 1.0f);
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color, float alpha = 1.0f);
    static void DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha = 1.0f);

    // --- Helpers ---
    static entt::entity GetPlayerEntity(entt::registry& registry);
    static Vector2 GetMousePositionLogic();

private:
    static Font s_globalFont;
    static Font s_emojiFont;
};
