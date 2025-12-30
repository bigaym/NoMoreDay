#include "UIRenderer.hpp"
#include "../systems/InventorySystem.hpp" // For context menu actions
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath> // For std::max

namespace NoMoreDay {

    static float s_uiScale = 1.0f;

    void UIRenderer::SetScale(float scale) {
        s_uiScale = scale;
    }

    float UIRenderer::GetScale() {
        return s_uiScale;
    }

    void UIRenderer::DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color) {
        float scaledSize = fontSize * s_uiScale;
        Vector2 pos = { x * s_uiScale, y * s_uiScale };

        if (IsFontValid(font)) {
            DrawTextEx(font, text, pos, scaledSize, 1.0f * s_uiScale, color);
        } else {
            DrawText(text, (int)pos.x, (int)pos.y, (int)scaledSize, color);
        }
    }

    void UIRenderer::DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color) {
        if (!text || text[0] == '\0') return;
        
        // Measure in Logic Space first to determine scaling factor relative to maxWidth
        float logicWidth = IsFontValid(font) ? MeasureTextEx(font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);
        
        // If Logic Width exceeds Max Width (Logic), we scale down the FONT SIZE
        float finalFontSize = fontSize;
        float yOffset = 0.0f;

        if (logicWidth > maxWidth && maxWidth > 0) {
            float scale = maxWidth / logicWidth;
            finalFontSize = fontSize * scale;
            yOffset = (fontSize - finalFontSize) * 0.5f;
        }

        // Now Apply Global Scale to everything
        DrawTextUI(font, text, x, y + yOffset, finalFontSize, color);
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

    void UIRenderer::DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        Rectangle rec = { sx, sy, sSize, sSize };
        DrawRectangleRec(rec, highlighted ? Fade(YELLOW, 0.2f) : (isLocked ? Fade(BLACK, 0.8f) : Fade(BLACK, 0.5f)));
        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, highlighted ? GOLD : GRAY);
        
        if (item != entt::null && registry.valid(item)) {
            auto* itemComp = registry.try_get<ItemComponent>(item);
            auto* sprite = registry.try_get<SpriteComponent>(item);

            if (itemComp) {
                Color rarityColor = GetRarityColor(itemComp->rarity);
                DrawRectangleLinesEx(rec, 2.0f * s_uiScale, rarityColor);

                if (sprite && sprite->texture.id > 0) {
                    Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                    // Padding 4px in logic space -> 4 * scale
                    float pad = 4.0f * s_uiScale;
                    Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
                    DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
                } else {
                    const char* shortName = GetShortItemTypeName(*itemComp);
                    float fontSize = 16.0f; 
                    // Centering calculation needs to happen in Logic Space or Screen Space?
                    // Let's do Screen Space for precision.
                    float scaledFontSize = fontSize * s_uiScale;
                    Vector2 textSize = IsFontValid(font) ? MeasureTextEx(font, shortName, scaledFontSize, 1.0f * s_uiScale) : Vector2{(float)MeasureText(shortName, (int)scaledFontSize), scaledFontSize};
                    
                    DrawTextUI(font, shortName, x + (size - textSize.x/s_uiScale) / 2.0f, y + (size - textSize.y/s_uiScale) / 2.0f, fontSize, rarityColor);
                }

                if (itemComp->quantity > 1) {
                    DrawTextUI(font, std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, WHITE);
                }
            }
        } else if (defaultLabel) {
             // Optional
        }
        
        if (isLocked) {
            DrawLineEx({sx + sSize * 0.3f, sy + sSize * 0.3f}, {sx + sSize * 0.7f, sy + sSize * 0.7f}, 1.0f * s_uiScale, Fade(GRAY, 0.5f));
            DrawLineEx({sx + sSize * 0.7f, sy + sSize * 0.3f}, {sx + sSize * 0.3f, sy + sSize * 0.7f}, 1.0f * s_uiScale, Fade(GRAY, 0.5f));
        }
        DrawRectangleLinesEx({sx+1.0f*s_uiScale, sy+1.0f*s_uiScale, sSize-2.0f*s_uiScale, sSize-2.0f*s_uiScale}, 1.0f * s_uiScale, Fade(BLACK, 0.3f));
    }

    void UIRenderer::DrawTooltip(const Font& font, entt::registry& registry, entt::entity item) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        if (!itemComp) return;

        // 1. Prepare Data
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
        
        // 2. Calculate Dimensions (Logic Space)
        float fontSize = 18.0f;
        float titleSize = 22.0f;
        float padding = 10.0f;
        float lineHeight = fontSize + 4.0f;
        
        float maxW = 0.0f;
        
        // Measure Logic Widths (using unscaled font size for logic calc)
        // Raylib MeasureTextEx needs Font Size.
        // We can measure with Scaled Size and divide by Scale, OR measure with Base Size.
        // MeasureTextEx scales internally if we pass font.baseSize.
        // Best to use MeasureTextEx(font, text, fontSize, 1.0f) * s_uiScale? No.
        // We want logic width.
        // MeasureTextEx returns pixel width for that fontSize.
        
        Vector2 titleDim = IsFontValid(font) ? MeasureTextEx(font, itemComp->name.c_str(), titleSize, 1.0f) : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize), titleSize};
        maxW = std::max(maxW, titleDim.x);
        
        for (const auto& line : lines) {
            if (line == "---" || line == " ") continue;
            float w = IsFontValid(font) ? MeasureTextEx(font, line.c_str(), fontSize, 1.0f).x : (float)MeasureText(line.c_str(), (int)fontSize);
            maxW = std::max(maxW, w);
        }
        
        float w = maxW + padding * 2;
        float h = padding * 2 + titleSize + 5.0f + lines.size() * lineHeight;

        // Mouse Pos is Screen Space. Convert to Logic Space?
        // Actually, Tooltip follows Mouse. Mouse is Screen Space.
        // If we draw Tooltip in Screen Space, we don't scale X/Y relative to 0,0.
        // BUT, the content of tooltip (text, box size) SHOULD be scaled.
        // So:
        // x, y (Origin) = Mouse Pos (Screen Pixels).
        // w, h (Size) = Logic Size * Scale.
        
        Vector2 m = GetMousePosition(); // Screen Space
        // We want to offset tooltip from mouse.
        float x = m.x + 15 * s_uiScale;
        float y = m.y + 15 * s_uiScale;
        
        float sW = w * s_uiScale;
        float sH = h * s_uiScale;

        if (x + sW > GetScreenWidth()) x -= (sW + 20 * s_uiScale);
        if (y + sH > GetScreenHeight()) y -= (sH + 20 * s_uiScale);

        // Draw Rectangle (Direct Screen Space coordinates)
        DrawRectangle((int)x, (int)y, (int)sW, (int)sH, Fade(BLACK, 0.9f));
        DrawRectangleLinesEx({x, y, sW, sH}, 1.0f * s_uiScale, GetRarityColor(itemComp->rarity));
        
        // Draw Text (We need a version of DrawTextUI that takes Screen coordinates but scales Size)
        // Or just use DrawTextUI and pass (x/scale, y/scale)? No, that's weird.
        // Let's use internal drawing for Tooltip since it's "Screen Space Overlay".
        
        auto DrawTextScreen = [&](const char* t, float sx, float sy, float size, Color c) {
            float sSize = size * s_uiScale;
             if (IsFontValid(font)) {
                DrawTextEx(font, t, { sx, sy }, sSize, 1.0f * s_uiScale, c);
            } else {
                DrawText(t, (int)sx, (int)sy, (int)sSize, c);
            }
        };

        DrawTextScreen(itemComp->name.c_str(), x + padding * s_uiScale, y + padding * s_uiScale, titleSize, GetRarityColor(itemComp->rarity));
        
        float curSY = y + (padding + titleSize + 5.0f) * s_uiScale;
        float sLineHeight = lineHeight * s_uiScale;

        for (const auto& line : lines) {
            if (line == "---") {
                DrawLineEx({x + padding*s_uiScale, curSY + sLineHeight/2}, {x + sW - padding*s_uiScale, curSY + sLineHeight/2}, 1.0f*s_uiScale, GRAY);
            } else if (line != " ") {
                Color c = WHITE;
                if (line.find("+") == 0) c = GREEN; 
                DrawTextScreen(line.c_str(), x + padding*s_uiScale, curSY, fontSize, c);
            }
            curSY += sLineHeight;
        }
    }

    void UIRenderer::DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry) {
        if (!uiContext.showContextMenu || !registry.valid(uiContext.contextMenuItem)) {
            uiContext.showContextMenu = false;
            return;
        }

        // Logic Sizes
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
        
        // Context Menu Pos is likely SCREEN coordinates (from Mouse Click).
        // Check UISystem::OpenContextMenu. It calls GetMousePosition().
        // So contextMenuPos is Screen Space.
        
        float sx = uiContext.contextMenuPos.x;
        float sy = uiContext.contextMenuPos.y;
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sBtnH = btnH * s_uiScale;

        DrawRectangle(sx, sy, sw, sh, Fade(BLACK, 0.95f));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f*s_uiScale, GOLD);

        float curSY = sy + 5 * s_uiScale;

        auto DrawMenuBtn = [&](const char* text) -> bool {
            Rectangle r = {sx + 5*s_uiScale, curSY, sw - 10*s_uiScale, sBtnH - 2*s_uiScale};
            bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
            if (hovered) DrawRectangleRec(r, Fade(GOLD, 0.3f));
            
            float sSize = 18 * s_uiScale;
             if (IsFontValid(font)) {
                DrawTextEx(font, text, { sx + 15*s_uiScale, curSY + 5*s_uiScale }, sSize, 1.0f * s_uiScale, hovered ? WHITE : LIGHTGRAY);
            } else {
                DrawText(text, (int)(sx + 15*s_uiScale), (int)(curSY + 5*s_uiScale), (int)sSize, hovered ? WHITE : LIGHTGRAY);
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

    void UIRenderer::DrawMessageBox(const Font& font, UIContext& uiContext) {
        if (!uiContext.showMessageBox) return;
        
        const char* text = uiContext.messageBoxText;
        float fontSize = 20;
        int textW = MeasureText(text, (int)fontSize); 
        if (IsFontValid(font)) {
            textW = (int)MeasureTextEx(font, text, fontSize, 1.0f).x;
        }
        
        float w = textW + 60.0f;
        float h = 50.0f;
        
        // Centered on Screen
        // We can do this in Screen Space directly.
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sx = (screenW - sw) / 2.0f;
        float sy = (screenH - sh) / 2.0f;
        
        DrawRectangle((int)sx, (int)sy, (int)sw, (int)sh, Fade(BLACK, 0.9f));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale, RED);
        
        float sSize = fontSize * s_uiScale;
        if (IsFontValid(font)) {
            DrawTextEx(font, text, { sx + 30*s_uiScale, sy + 15*s_uiScale }, sSize, 1.0f * s_uiScale, WHITE);
        } else {
             DrawText(text, (int)(sx + 30*s_uiScale), (int)(sy + 15*s_uiScale), (int)sSize, WHITE);
        }
    }

}