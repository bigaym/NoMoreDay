#pragma once

#include <entt/entt.hpp>
#include "raylib.h"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/application/ui/OverlayController.hpp"
#include "engine/render/GPUData.hpp"

namespace NoMoreDay {

    struct UITheme {
        Color panelBackground = components::Colors::UI_BACKGROUND;
        Color panelBorder = components::Colors::UI_BORDER;
        Color panelBorderHighlight = components::Colors::UI_BORDER_HIGHLIGHT;
        Color slotBackground = components::Colors::UI_SLOT_BG;
        
        Color textPrimary = components::Colors::TEXT_PRIMARY;
        Color textSecondary = components::Colors::TEXT_SECONDARY;
        Color textHighlight = components::Colors::TEXT_HIGHLIGHT;
        
        Color buttonNormal = components::Colors::BUTTON_NORMAL;
        Color buttonHover = components::Colors::BUTTON_HOVER;
        Color buttonPress = components::Colors::BUTTON_PRESS;
        
        Color danger = components::Colors::STATUS_DANGER;
        Color success = components::Colors::STATUS_SUCCESS;
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
        
        // Generic Button
        static void DrawButton(const Font& font, Texture2D texture, Rectangle bounds, const char* text, float fontSize, Color textColor, Color textureTint, bool isHovered, bool isPressed, float alpha = 1.0f);

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
        // U8 收尾: the overlays are instance-owned by OverlayController; the
        // renderer draws from the controller's state and routes menu actions
        // (close / open quantity popup / show message box) back through it.
        static void DrawContextMenu(const Font& font, NoMoreDay::ui::OverlayController& overlay, entt::registry& registry, float alpha = 1.0f);
        static void DrawMessageBox(const Font& font, const char* text, float alpha = 1.0f);

        struct TooltipLine {
            std::string text;
            Color color;
            bool isSeparator = false;
        };

        // Logic Helpers (for testing)
        static std::vector<TooltipLine> GetSkillTooltipLines(entt::registry& registry, uint32_t skillId);

    private:
        static std::vector<TooltipLine> GetTooltipLines(const ItemComponent& item);
        static inline float s_uiScale = 1.0f;
        static inline UITheme s_theme;
        // U8 收尾: skill-tooltip position lock (was State.tooltipPos /
        // State.tooltipInitialized). Renderer-local draw state, same category
        // as s_uiScale; TooltipController passes forceDraw = !locked to reset
        // the lock when the hover target changes.
        static inline Vector2 s_skillTooltipPos = {0.0f, 0.0f};
        static inline bool s_skillTooltipInitialized = false;
    };

}
