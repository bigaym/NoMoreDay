#pragma once

#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"
#include "UIContext.hpp"

namespace NoMoreDay {

    class UIRenderer {
    public:
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

        // Complex Elements (Stateless, data passed in)
        static void DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha = 1.0f);
        static void DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha = 1.0f);
        static void DrawMessageBox(const Font& font, UIContext& uiContext, float alpha = 1.0f);

    private:
        static std::vector<std::string> GetTooltipLines(const ItemComponent& item);
    };

}
