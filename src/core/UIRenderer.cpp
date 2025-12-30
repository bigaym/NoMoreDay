#include "UIRenderer.hpp"
#include "../systems/InventorySystem.hpp" // For context menu actions
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath> // For std::max

namespace NoMoreDay {

    static float s_uiScale = 1.0f;
    static UITheme s_theme;

    void UIRenderer::SetTheme(const UITheme& theme) {
        s_theme = theme;
    }

    UITheme& UIRenderer::GetTheme() {
        return s_theme;
    }

    void UIRenderer::SetScale(float scale) {
        s_uiScale = scale;
    }

    float UIRenderer::GetScale() {
        return s_uiScale;
    }

    void UIRenderer::DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color, float alpha) {
        float scaledSize = fontSize * s_uiScale;
        Vector2 pos = { x * s_uiScale, y * s_uiScale };
        Color finalColor = Fade(color, alpha);

        if (IsFontValid(font)) {
            DrawTextEx(font, text, pos, scaledSize, 1.0f * s_uiScale, finalColor);
        } else {
            DrawText(text, (int)pos.x, (int)pos.y, (int)scaledSize, finalColor);
        }
    }

    void UIRenderer::DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha) {
        if (!text || text[0] == '\0') return;
        
        float logicWidth = IsFontValid(font) ? MeasureTextEx(font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);
        
        float finalFontSize = fontSize;
        float yOffset = 0.0f;

        if (logicWidth > maxWidth && maxWidth > 0) {
            float scale = maxWidth / logicWidth;
            finalFontSize = fontSize * scale;
            yOffset = (fontSize - finalFontSize) * 0.5f;
        }

        DrawTextUI(font, text, x, y + yOffset, finalFontSize, color, alpha);
    }

    Color UIRenderer::GetRarityColor(Rarity rarity) {
        switch (rarity) {
            case Rarity::Common:    return LIGHTGRAY;
            case Rarity::Magic:     return SKYBLUE;
            case Rarity::Rare:      return YELLOW;
            case Rarity::Uncommon:  return LIME;
            case Rarity::Set:       return GREEN;
            case Rarity::Epic:      return PURPLE;
            case Rarity::Legendary: return ORANGE;
            case Rarity::Mythic:    return RED;
            default:                return WHITE;
        }
    }

    const char* UIRenderer::GetShortItemTypeName(const ItemComponent& item) {
        if (item.type == ItemType::Weapon) return "武";
        if (item.type == ItemType::Armor) return "甲";
        if (item.type == ItemType::Consumable) return "耗";
        if (item.type == ItemType::Material) return "料";
        return "物";
    }

    void UIRenderer::DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked, float alpha) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        Rectangle rec = { sx, sy, sSize, sSize };
        // Background
        Color bg = highlighted ? Fade(s_theme.panelBorderHighlight, 0.2f) : s_theme.slotBackground;
        if (isLocked) bg = Fade(BLACK, 0.8f); // Locked slots darker
        
        DrawRectangleRec(rec, Fade(bg, alpha));
        
        // Border
        Color border = highlighted ? s_theme.panelBorderHighlight : s_theme.panelBorder;
        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, Fade(border, alpha));
        
        if (item != entt::null && registry.valid(item)) {
            auto* itemComp = registry.try_get<ItemComponent>(item);
            auto* sprite = registry.try_get<SpriteComponent>(item);

            if (itemComp) {
                Color rarityColor = GetRarityColor(itemComp->rarity);
                // Rarity Border
                DrawRectangleLinesEx(rec, 2.0f * s_uiScale, Fade(rarityColor, alpha));

                if (sprite && sprite->texture.id > 0) {
                    Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                    float pad = 4.0f * s_uiScale;
                    Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
                    DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, Fade(WHITE, alpha));
                } else {
                    const char* shortName = GetShortItemTypeName(*itemComp);
                    float fontSize = 16.0f; 
                    float scaledFontSize = fontSize * s_uiScale;
                    Vector2 textSize = IsFontValid(font) ? MeasureTextEx(font, shortName, scaledFontSize, 1.0f * s_uiScale) : Vector2{(float)MeasureText(shortName, (int)scaledFontSize), scaledFontSize};
                    
                    DrawTextUI(font, shortName, x + (size - textSize.x/s_uiScale) / 2.0f, y + (size - textSize.y/s_uiScale) / 2.0f, fontSize, rarityColor, alpha);
                }

                if (itemComp->quantity > 1) {
                    DrawTextUI(font, std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, s_theme.textPrimary, alpha);
                }
            }
        } 
        
        if (isLocked) {
            DrawLineEx({sx + sSize * 0.3f, sy + sSize * 0.3f}, {sx + sSize * 0.7f, sy + sSize * 0.7f}, 1.0f * s_uiScale, Fade(s_theme.panelBorder, 0.5f * alpha));
            DrawLineEx({sx + sSize * 0.7f, sy + sSize * 0.3f}, {sx + sSize * 0.3f, sy + sSize * 0.7f}, 1.0f * s_uiScale, Fade(s_theme.panelBorder, 0.5f * alpha));
        }
        // Inner shadow
        DrawRectangleLinesEx({sx+1.0f*s_uiScale, sy+1.0f*s_uiScale, sSize-2.0f*s_uiScale, sSize-2.0f*s_uiScale}, 1.0f * s_uiScale, Fade(BLACK, 0.3f * alpha));
    }

    void UIRenderer::DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        if (!itemComp) return;

        std::vector<std::string> lines;
        if (itemComp->attack > 0) lines.push_back(TextFormat("攻击力: %.0f", itemComp->attack));
        if (itemComp->defense > 0) lines.push_back(TextFormat("护甲: %.0f", itemComp->defense));
        if (itemComp->bagCapacity > 0) lines.push_back(TextFormat("容量: %d 格", itemComp->bagCapacity));
        for (const auto& aff : itemComp->implicits) lines.push_back(GetAffixDescription(aff));
        if ((!lines.empty()) && !itemComp->affixes.empty()) lines.push_back("---");
        for (const auto& aff : itemComp->affixes) lines.push_back(GetAffixDescription(aff));
        if (!itemComp->description.empty()) {
            if (!lines.empty()) lines.push_back(" "); 
            lines.push_back(itemComp->description);
        }
        
        float fontSize = 18.0f;
        float titleSize = 22.0f;
        float padding = 10.0f;
        float lineHeight = fontSize + 4.0f;
        
        float maxW = 0.0f;
        Vector2 titleDim = IsFontValid(font) ? MeasureTextEx(font, itemComp->name.c_str(), titleSize, 1.0f) : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize), titleSize};
        maxW = std::max(maxW, titleDim.x);
        for (const auto& line : lines) {
            if (line == "---" || line == " ") continue;
            float w = IsFontValid(font) ? MeasureTextEx(font, line.c_str(), fontSize, 1.0f).x : (float)MeasureText(line.c_str(), (int)fontSize);
            maxW = std::max(maxW, w);
        }
        
        float w = maxW + padding * 2;
        float h = padding * 2 + titleSize + 5.0f + lines.size() * lineHeight;

        Vector2 m = GetMousePosition();
        float x = m.x + 15 * s_uiScale;
        float y = m.y + 15 * s_uiScale;
        float sW = w * s_uiScale;
        float sH = h * s_uiScale;

        if (x + sW > GetScreenWidth()) x -= (sW + 20 * s_uiScale);
        if (y + sH > GetScreenHeight()) y -= (sH + 20 * s_uiScale);

        DrawRectangle((int)x, (int)y, (int)sW, (int)sH, Fade(s_theme.panelBackground, 0.95f * alpha));
        DrawRectangleLinesEx({x, y, sW, sH}, 1.0f * s_uiScale, Fade(GetRarityColor(itemComp->rarity), alpha));
        
        auto DrawTextScreen = [&](const char* t, float sx, float sy, float size, Color c) {
            float sSize = size * s_uiScale;
             if (IsFontValid(font)) {
                DrawTextEx(font, t, { sx, sy }, sSize, 1.0f * s_uiScale, Fade(c, alpha));
            } else {
                DrawText(t, (int)sx, (int)sy, (int)sSize, Fade(c, alpha));
            }
        };

        DrawTextScreen(itemComp->name.c_str(), x + padding * s_uiScale, y + padding * s_uiScale, titleSize, GetRarityColor(itemComp->rarity));
        float curSY = y + (padding + titleSize + 5.0f) * s_uiScale;
        float sLineHeight = lineHeight * s_uiScale;

        for (const auto& line : lines) {
            if (line == "---") {
                DrawLineEx({x + padding*s_uiScale, curSY + sLineHeight/2}, {x + sW - padding*s_uiScale, curSY + sLineHeight/2}, 1.0f*s_uiScale, Fade(s_theme.panelBorder, alpha));
            } else if (line != " ") {
                Color c = s_theme.textPrimary;
                if (line.find("+") == 0) c = s_theme.success; 
                DrawTextScreen(line.c_str(), x + padding*s_uiScale, curSY, fontSize, c);
            }
            curSY += sLineHeight;
        }
    }

    void UIRenderer::DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha) {
        if (!uiContext.showContextMenu || !registry.valid(uiContext.contextMenuItem)) {
            uiContext.showContextMenu = false;
            return;
        }

        float w = 140; 
        float h = 0;
        float btnH = 30;
        int btnCount = 0;

        bool showEquip = uiContext.isContextFromInventory; 
        bool showUnequip = !uiContext.isContextFromInventory && uiContext.contextSourceEquipmentSlot != EquipmentSlot::None;
        bool showDrop = true;

        if (showEquip) btnCount++;
        if (showUnequip) btnCount++;
        if (showDrop) btnCount++;
        btnCount++; // Cancel

        h = btnCount * btnH + 10;
        
        float sx = uiContext.contextMenuPos.x;
        float sy = uiContext.contextMenuPos.y;
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sBtnH = btnH * s_uiScale;

        DrawRectangle(sx, sy, sw, sh, Fade(s_theme.panelBackground, 0.95f * alpha));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f*s_uiScale, Fade(s_theme.panelBorderHighlight, alpha));

        float curSY = sy + 5 * s_uiScale;

        auto DrawMenuBtn = [&](const char* text) -> bool {
            Rectangle r = {sx + 5*s_uiScale, curSY, sw - 10*s_uiScale, sBtnH - 2*s_uiScale};
            bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
            if (hovered) DrawRectangleRec(r, Fade(s_theme.buttonHover, 0.3f * alpha));
            
            float sSize = 18 * s_uiScale;
             if (IsFontValid(font)) {
                DrawTextEx(font, text, { sx + 15*s_uiScale, curSY + 5*s_uiScale }, sSize, 1.0f * s_uiScale, Fade(hovered ? s_theme.textHighlight : s_theme.textSecondary, alpha));
            } else {
                DrawText(text, (int)(sx + 15*s_uiScale), (int)(curSY + 5*s_uiScale), (int)sSize, Fade(hovered ? s_theme.textHighlight : s_theme.textSecondary, alpha));
            }
            
            curSY += sBtnH;
            return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        };

        if (showEquip) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("装备 / 使用")) {
                InventorySystem::equipItem(registry, view.front(), uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        if (showUnequip) {
             auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("卸下")) {
                if (!InventorySystem::unequipItem(registry, view.front(), uiContext.contextSourceEquipmentSlot)) {
                    uiContext.showMessageBox = true;
                    snprintf(uiContext.messageBoxText, 64, "背包已满！无法卸下装备。");
                    uiContext.messageBoxTimer = 2.0f;
                }
                uiContext.showContextMenu = false;
            }
        }
        if (showDrop) {
             auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("丢弃")) {
                InventorySystem::dropItem(registry, view.front(), uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        if (DrawMenuBtn("取消")) {
            uiContext.showContextMenu = false;
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (!CheckCollisionPointRec(GetMousePosition(), {sx, sy, sw, sh})) {
                uiContext.showContextMenu = false;
            }
        }
    }

    void UIRenderer::DrawMessageBox(const Font& font, UIContext& uiContext, float alpha) {
        if (!uiContext.showMessageBox) return;
        
        const char* text = uiContext.messageBoxText;
        float fontSize = 20;
        int textW = MeasureText(text, (int)fontSize); 
        if (IsFontValid(font)) {
            textW = (int)MeasureTextEx(font, text, fontSize, 1.0f).x;
        }
        
        float w = textW + 60.0f;
        float h = 50.0f;
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sx = (screenW - sw) / 2.0f;
        float sy = (screenH - sh) / 2.0f;
        
        DrawRectangle((int)sx, (int)sy, (int)sw, (int)sh, Fade(s_theme.panelBackground, 0.9f * alpha));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale, Fade(s_theme.danger, alpha));
        
        float sSize = fontSize * s_uiScale;
        if (IsFontValid(font)) {
            DrawTextEx(font, text, { sx + 30*s_uiScale, sy + 15*s_uiScale }, sSize, 1.0f * s_uiScale, Fade(s_theme.textPrimary, alpha));
        } else {
             DrawText(text, (int)(sx + 30*s_uiScale), (int)(sy + 15*s_uiScale), (int)sSize, Fade(s_theme.textPrimary, alpha));
        }
    }

}
