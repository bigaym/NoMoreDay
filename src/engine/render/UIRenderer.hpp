#pragma once

#include <entt/entt.hpp>
#include "raylib.h"
#include "game/components/ItemComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/ui/UIContext.hpp"

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

        // Helpers
        static Color GetRarityColor(Rarity rarity);
        static const char* GetShortItemTypeName(const ItemComponent& item);
        static const char* GetItemCategoryString(const ItemComponent& item);

        // Text Helpers
        static void DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color, float alpha = 1.0f);
        static void DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha = 1.0f);
        
        // Item/Slot Helpers
        static void DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false, float alpha = 1.0f, EquipmentSlot slotHint = EquipmentSlot::None);
        static void DrawSkillSlot(const Font& font, float x, float y, float size, 
                                 Texture2D icon, const char* keyLabel, 
                                 float cooldownRatio, float remainingCooldown, float manaCost, 
                                  int charges, int maxCharges,
                                  bool hasEnoughMana, bool isHighlighted, bool isPressed = false, float alpha = 1.0f);

        static void DrawBuffIcon(const Font& font, float x, float y, float size,
                                 Texture2D icon, const char* text, float durationRatio, int stacks,
                                 bool isDebuff, float alpha = 1.0f);

        static void DrawSummonIcon(const Font& font, float x, float y, float width, float height,
                                  Texture2D icon, float healthPct, const char* name, float alpha = 1.0f);

        static void DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha = 1.0f);
        static void DrawSkillTooltip(const Font& font, entt::registry& registry, uint32_t skillId, float alpha = 1.0f, bool forceDraw = false);
        static void DrawBuffTooltip(const Font& font, const BuffEffect& effect, float alpha = 1.0f);
        static void DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha = 1.0f);
        static void DrawMessageBox(const Font& font, UIContext& uiContext, float alpha = 1.0f);

        struct TooltipLine {
            std::string text;
            Color color;
            bool isSeparator = false;
        };

        // Logic Helpers (for testing)
        static std::vector<TooltipLine> GetSkillTooltipLines(entt::registry& registry, uint32_t skillId);

    private:
        static std::vector<TooltipLine> GetTooltipLines(const ItemComponent& item);
    };

}
