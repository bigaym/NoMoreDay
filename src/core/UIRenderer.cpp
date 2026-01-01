#include "UIRenderer.hpp"
#include "../systems/InventorySystem.hpp" // For context menu actions
#include "AssetLoadingSystem.hpp"
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

    const char* UIRenderer::GetItemCategoryString(const ItemComponent& item) {
        if (item.type == ItemType::Bag) return "背包";
        if (item.type == ItemType::Consumable) return "消耗品";
        if (item.type == ItemType::Material) return "材料";
        
        if (item.type == ItemType::Weapon) {
            if (item.isTwoHanded) return "双手武器";
            return "单手武器";
        }
        
        if (item.type == ItemType::Armor || item.type == ItemType::Shield) {
            switch (item.slot) {
                case EquipmentSlot::Head: return "头盔";
                case EquipmentSlot::Shoulder: return "护肩";
                case EquipmentSlot::Chest: return "胸甲";
                case EquipmentSlot::Hands: return "手套";
                case EquipmentSlot::Legs: return "护腿";
                case EquipmentSlot::Feet: return "鞋子";
                case EquipmentSlot::Neck: return "项链";
                case EquipmentSlot::Ring: return "戒指";
                // case EquipmentSlot::Ring1:
                // case EquipmentSlot::Ring2: return "戒指";
                case EquipmentSlot::OffHand: return "副手";
                default: return "装备";
            }
        }
        return "物品";
    }

    void UIRenderer::DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked, float alpha) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        Rectangle rec = { sx, sy, sSize, sSize };
        
        auto ApplyAlpha = [&](Color c, float a) -> Color {
            return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
        };

        // Background
        Color bg = highlighted ? ApplyAlpha(s_theme.panelBorderHighlight, 0.2f) : s_theme.slotBackground;
        if (isLocked) bg = ApplyAlpha(BLACK, 0.8f); // Locked slots darker
        
        DrawRectangleRec(rec, ApplyAlpha(bg, alpha));
        
        // 1. Sunken Bevel Effect
        
        // Inner Top Shadow
        DrawLineEx({sx, sy}, {sx + sSize, sy}, 2.0f * s_uiScale, ApplyAlpha(BLACK, 0.5f * alpha));
        // Inner Left Shadow
        DrawLineEx({sx, sy}, {sx, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(BLACK, 0.5f * alpha));
        
        // Inner Bottom Highlight (Subtle)
        DrawLineEx({sx, sy + sSize}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale, ApplyAlpha(WHITE, 0.1f * alpha));
        // Inner Right Highlight (Subtle)
        DrawLineEx({sx + sSize, sy}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale, ApplyAlpha(WHITE, 0.1f * alpha));

        // 2. Corner Accents (Decorative)
        float cornerLen = 6.0f * s_uiScale;
        Color cornerColor = highlighted ? s_theme.panelBorderHighlight : ApplyAlpha(s_theme.panelBorder, 0.6f);
        
        // Top-Left
        DrawLineEx({sx, sy}, {sx + cornerLen, sy}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        DrawLineEx({sx, sy}, {sx, sy + cornerLen}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        // Top-Right
        DrawLineEx({sx + sSize - cornerLen, sy}, {sx + sSize, sy}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        DrawLineEx({sx + sSize, sy}, {sx + sSize, sy + cornerLen}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        // Bottom-Left
        DrawLineEx({sx, sy + sSize}, {sx + cornerLen, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        DrawLineEx({sx, sy + sSize - cornerLen}, {sx, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha)); 
        
        // Bottom-Right
        DrawLineEx({sx + sSize - cornerLen, sy + sSize}, {sx + sSize, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));
        DrawLineEx({sx + sSize, sy + sSize - cornerLen}, {sx + sSize, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(cornerColor, alpha));

        // Border (Standard)
        Color border = highlighted ? s_theme.panelBorderHighlight : s_theme.panelBorder;
        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, ApplyAlpha(border, alpha));
        
        if (item != entt::null && registry.valid(item)) {
            auto* itemComp = registry.try_get<ItemComponent>(item);
            auto* sprite = registry.try_get<SpriteComponent>(item);

            if (itemComp) {
                Color rarityColor = GetRarityColor(itemComp->rarity);
                // Rarity Border
                DrawRectangleLinesEx(rec, 2.0f * s_uiScale, ApplyAlpha(rarityColor, alpha));

                bool textureDrawn = false;

                // 1. Try textureId from ItemComponent (Preferred)
                if (itemComp->textureId != 0) {
                    Texture2D tex = AssetLoadingSystem::GetTexture(itemComp->textureId);
                    if (tex.id > 0) {
                        Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
                        float pad = 4.0f * s_uiScale;
                        Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
                        DrawTexturePro(tex, source, dest, {0, 0}, 0.0f, ApplyAlpha(WHITE, alpha));
                        textureDrawn = true;
                    }
                }

                // 2. Try SpriteComponent (Legacy/World Entities)
                if (!textureDrawn && sprite && sprite->texture.id > 0) {
                    Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                    float pad = 4.0f * s_uiScale;
                    Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
                    DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, ApplyAlpha(WHITE, alpha));
                    textureDrawn = true;
                } 
                
                // 3. Fallback to Text
                if (!textureDrawn) {
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
            DrawLineEx({sx + sSize * 0.3f, sy + sSize * 0.3f}, {sx + sSize * 0.7f, sy + sSize * 0.7f}, 1.0f * s_uiScale, ApplyAlpha(s_theme.panelBorder, 0.5f * alpha));
            DrawLineEx({sx + sSize * 0.7f, sy + sSize * 0.3f}, {sx + sSize * 0.3f, sy + sSize * 0.7f}, 1.0f * s_uiScale, ApplyAlpha(s_theme.panelBorder, 0.5f * alpha));
        }
        // Inner shadow
        DrawRectangleLinesEx({sx+1.0f*s_uiScale, sy+1.0f*s_uiScale, sSize-2.0f*s_uiScale, sSize-2.0f*s_uiScale}, 1.0f * s_uiScale, ApplyAlpha(BLACK, 0.3f * alpha));
    }

    void UIRenderer::DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        if (!itemComp) return;

        struct TooltipLine {
            std::string text;
            Color color;
            bool isSeparator = false;
        };

        std::vector<TooltipLine> lines;
        lines.push_back({ GetItemCategoryString(*itemComp), s_theme.textSecondary });
        
        if (itemComp->attack > 0) lines.push_back({ TextFormat("攻击力: %.0f", itemComp->attack), s_theme.textPrimary });
        if (itemComp->defense > 0) lines.push_back({ TextFormat("护甲: %.0f", itemComp->defense), s_theme.textPrimary });
        if (itemComp->bagCapacity > 0) lines.push_back({ TextFormat("容量: %d 格", itemComp->bagCapacity), s_theme.textPrimary });
        
        // Implicits (Base Stats) - Hide [Tx] text
        for (const auto& aff : itemComp->implicits) {
            lines.push_back({ GetAffixDescription(aff, false), GetAffixTierColor(aff.tier) });
        }

        if ((itemComp->attack > 0 || itemComp->defense > 0 || !itemComp->implicits.empty()) && !itemComp->affixes.empty()) {
            lines.push_back({ "---", WHITE, true });
        }

        // Affixes (Random Mods) - Show [Tx] text
        for (const auto& aff : itemComp->affixes) {
            lines.push_back({ GetAffixDescription(aff, true), GetAffixTierColor(aff.tier) });
        }

        if (!itemComp->description.empty()) {
            lines.push_back({ " ", WHITE }); // Spacer
            lines.push_back({ itemComp->description, s_theme.textPrimary });
        }
        
        float fontSize = 18.0f;
        float titleSize = 22.0f;
        float padding = 10.0f;
        float lineHeight = fontSize + 4.0f;
        
        float maxW = 0.0f;
        Vector2 titleDim = IsFontValid(font) ? MeasureTextEx(font, itemComp->name.c_str(), titleSize, 1.0f) : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize), titleSize};
        maxW = std::max(maxW, titleDim.x);
        for (const auto& line : lines) {
            if (line.isSeparator || line.text == " ") continue;
            float w = IsFontValid(font) ? MeasureTextEx(font, line.text.c_str(), fontSize, 1.0f).x : (float)MeasureText(line.text.c_str(), (int)fontSize);
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
            if (line.isSeparator) {
                DrawLineEx({x + padding*s_uiScale, curSY + sLineHeight/2}, {x + sW - padding*s_uiScale, curSY + sLineHeight/2}, 1.0f*s_uiScale, Fade(s_theme.panelBorder, alpha));
            } else if (line.text != " ") {
                DrawTextScreen(line.text.c_str(), x + padding*s_uiScale, curSY, fontSize, line.color);
            }
            curSY += sLineHeight;
        }
    }

    void UIRenderer::DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha) {
        if (!uiContext.showContextMenu || !registry.valid(uiContext.contextMenuItem)) {
            uiContext.showContextMenu = false;
            return;
        }

        auto* itemComp = registry.try_get<ItemComponent>(uiContext.contextMenuItem);
        if (!itemComp) {
            uiContext.showContextMenu = false;
            return;
        }

        float w = 180.0f; // Wider
        float btnH = 36.0f; // Taller buttons
        int btnCount = 0;

        bool showEquip = false;
        bool showUse = false;
        if (uiContext.isContextFromInventory) {
            if (itemComp->type == ItemType::Weapon || itemComp->type == ItemType::Armor || 
                itemComp->type == ItemType::Shield || itemComp->type == ItemType::Bag) {
                showEquip = true;
            } else if (itemComp->type == ItemType::Consumable) {
                showUse = true;
            }
        }

        bool showUnequip = !uiContext.isContextFromInventory && uiContext.contextSourceEquipmentSlot != EquipmentSlot::None;
        bool showDrop = true;

        if (showEquip) btnCount++;
        if (showUse) btnCount++;
        if (showUnequip) btnCount++;
        if (showDrop) btnCount++;
        btnCount++; // Cancel

        float h = btnCount * btnH + 20.0f; // Extra padding
        
        float sx = uiContext.contextMenuPos.x;
        float sy = uiContext.contextMenuPos.y;
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sBtnH = btnH * s_uiScale;

        // Ensure it stays within screen bounds
        if (sx + sw > GetScreenWidth()) sx -= sw;
        if (sy + sh > GetScreenHeight()) sy -= sh;

        // Background & Border
        DrawRectangle(sx, sy, sw, sh, Fade(s_theme.panelBackground, 0.98f * alpha));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale, Fade(s_theme.panelBorder, alpha));
        // Accent line
        DrawLineEx({sx, sy}, {sx + sw, sy}, 2.0f * s_uiScale, Fade(s_theme.panelBorderHighlight, alpha));

        float curSY = sy + 10.0f * s_uiScale;

        auto DrawMenuBtn = [&](const char* text, Color textColor = WHITE) -> bool {
            Rectangle r = {sx + 5.0f * s_uiScale, curSY, sw - 10.0f * s_uiScale, sBtnH};
            bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
            bool pressed = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            
            if (hovered) {
                Color bg = pressed ? s_theme.buttonPress : s_theme.buttonHover;
                DrawRectangleRec(r, Fade(bg, 0.5f * alpha));
                DrawRectangleLinesEx(r, 1.0f * s_uiScale, Fade(s_theme.panelBorder, 0.5f * alpha));
            }
            
            float sSize = 18.0f * s_uiScale;
            if (IsFontValid(font)) {
                Vector2 textSize = MeasureTextEx(font, text, sSize, 1.0f);
                DrawTextEx(font, text, { sx + (sw - textSize.x) / 2.0f, curSY + (sBtnH - textSize.y) / 2.0f }, sSize, 1.0f * s_uiScale, Fade(hovered ? s_theme.textHighlight : textColor, alpha));
            } else {
                int textW = MeasureText(text, (int)sSize);
                DrawText(text, (int)(sx + (sw - textW) / 2.0f), (int)(curSY + (sBtnH - sSize) / 2.0f), (int)sSize, Fade(hovered ? s_theme.textHighlight : textColor, alpha));
            }
            
            curSY += sBtnH;
            return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
        };

        if (showEquip) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("装备")) {
                entt::entity player = view.front();
                if (itemComp->type == ItemType::Bag) {
                    auto* inv = registry.try_get<InventoryComponent>(player);
                    if (inv) {
                        int emptySlot = -1;
                        for (int i = 0; i < InventoryComponent::MAX_BAG_SLOTS; ++i) {
                            if (!registry.valid(inv->bag_slots[i])) { emptySlot = i; break; }
                        }
                        if (emptySlot != -1) {
                            InventorySystem::equipBag(registry, player, uiContext.contextMenuItem, emptySlot);
                        } else {
                            // No empty slot, swap with first? Or show message.
                            InventorySystem::equipBag(registry, player, uiContext.contextMenuItem, 0);
                        }
                    }
                } else {
                    InventorySystem::equipItem(registry, player, uiContext.contextMenuItem);
                }
                uiContext.showContextMenu = false;
            }
        }
        if (showUse) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("使用")) {
                InventorySystem::useItem(registry, view.front(), uiContext.contextMenuItem);
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
            if (view.begin() != view.end() && DrawMenuBtn("丢弃", s_theme.danger)) {
                InventorySystem::dropItem(registry, view.front(), uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        
        // Separator before cancel if there were buttons
        if (btnCount > 1) {
             DrawLineEx({sx + 10*s_uiScale, curSY + 2*s_uiScale}, {sx + sw - 10*s_uiScale, curSY + 2*s_uiScale}, 1.0f*s_uiScale, Fade(s_theme.panelBorder, 0.3f*alpha));
             curSY += 5*s_uiScale;
        }

        if (DrawMenuBtn("取消", s_theme.textSecondary)) {
            uiContext.showContextMenu = false;
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
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
