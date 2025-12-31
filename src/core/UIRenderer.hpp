#pragma once

#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"
#include "UIContext.hpp"

namespace NoMoreDay {

    struct UITheme {
        Color panelBackground = { 35, 35, 45, 180 }; // Lighter Blue-Gray, 70% opacity
        Color panelBorder = { 70, 70, 85, 255 };     // Lighter Gray
        Color panelBorderHighlight = { 200, 170, 50, 255 }; // Gold
        Color slotBackground = { 25, 25, 35, 200 };
        
        Color textPrimary = { 245, 245, 245, 255 };
        Color textSecondary = { 180, 180, 180, 255 };
        Color textHighlight = { 255, 215, 0, 255 };
        
        Color buttonNormal = { 50, 50, 65, 255 };
        Color buttonHover = { 70, 70, 95, 255 };
        Color buttonPress = { 40, 40, 55, 255 };
        
        Color danger = { 200, 50, 50, 255 };
        Color success = { 50, 200, 50, 255 };
    };

    class UIRenderer {
    public:
        // Theme
        static UITheme& GetTheme();
        static void SetTheme(const UITheme& theme);

        // Scaling
        static void SetScale(float scale);
        static float GetScale();

        // Text Helpers
        static void DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color, float alpha = 1.0f);
        static void DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha = 1.0f);
        
        // Item/Slot Helpers
        static void DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false, float alpha = 1.0f);
        static Color GetRarityColor(Rarity rarity);
        static const char* GetShortItemTypeName(const ItemComponent& item);
        static const char* GetItemCategoryString(const ItemComponent& item);

        // Complex Elements (Stateless, data passed in)
        static void DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha = 1.0f);
        static void DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha = 1.0f);
        static void DrawMessageBox(const Font& font, UIContext& uiContext, float alpha = 1.0f);

    private:
        static std::vector<std::string> GetTooltipLines(const ItemComponent& item);
    };

}
