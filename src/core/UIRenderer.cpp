#include "UIRenderer.hpp"
#include "../systems/InventorySystem.hpp" // For context menu actions
#include <algorithm>
#include <string>
#include <cstdio>

namespace NoMoreDay {

    void UIRenderer::DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color) {
        if (IsFontValid(font)) {
            DrawTextEx(font, text, { x, y }, fontSize, 1.0f, color);
        } else {
            DrawText(text, (int)x, (int)y, (int)fontSize, color);
        }
    }

    void UIRenderer::DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color) {
        if (!text || text[0] == '\0') return;
        float currentWidth = IsFontValid(font) ? MeasureTextEx(font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);

        if (currentWidth > maxWidth && maxWidth > 0) {
            float scale = maxWidth / currentWidth;
            float scaledFontSize = fontSize * scale;
            float yOffset = (fontSize - scaledFontSize) * 0.5f;
            if (IsFontValid(font)) DrawTextEx(font, text, { x, y + yOffset }, scaledFontSize, 1.0f, color);
            else DrawText(text, (int)x, (int)(y + yOffset), (int)scaledFontSize, color);
        } else {
            DrawTextUI(font, text, x, y, fontSize, color);
        }
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
        Rectangle rec = { x, y, size, size };
        DrawRectangleRec(rec, highlighted ? Fade(YELLOW, 0.2f) : (isLocked ? Fade(BLACK, 0.8f) : Fade(BLACK, 0.5f)));
        DrawRectangleLinesEx(rec, 1.0f, highlighted ? GOLD : GRAY);
        
        if (item != entt::null && registry.valid(item)) {
            auto* itemComp = registry.try_get<ItemComponent>(item);
            auto* sprite = registry.try_get<SpriteComponent>(item);

            if (itemComp) {
                Color rarityColor = GetRarityColor(itemComp->rarity);
                DrawRectangleLinesEx(rec, 2.0f, rarityColor);

                if (sprite && sprite->texture.id > 0) {
                    Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                    Rectangle dest = {x + 4, y + 4, size - 8, size - 8};
                    DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
                } else {
                    const char* shortName = GetShortItemTypeName(*itemComp);
                    float fontSize = 16.0f;
                    Vector2 textSize = IsFontValid(font) ? MeasureTextEx(font, shortName, fontSize, 1.0f) : Vector2{(float)MeasureText(shortName, (int)fontSize), fontSize};
                    DrawTextUI(font, shortName, x + (size - textSize.x) / 2.0f, y + (size - textSize.y) / 2.0f, fontSize, rarityColor);
                }

                if (itemComp->quantity > 1) {
                    DrawTextUI(font, std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, WHITE);
                }
            }
        } else if (defaultLabel) {
             // Optional: Draw default label if slot is empty (e.g. "Head")
             // Not implementing for now to match original exactly, but added parameter for future.
        }
        
        if (isLocked) {
            DrawLine(x + size * 0.3f, y + size * 0.3f, x + size * 0.7f, y + size * 0.7f, Fade(GRAY, 0.5f));
            DrawLine(x + size * 0.7f, y + size * 0.3f, x + size * 0.3f, y + size * 0.7f, Fade(GRAY, 0.5f));
        }
        DrawRectangleLinesEx({x+1, y+1, size-2, size-2}, 1.0f, Fade(BLACK, 0.3f));
    }

    void UIRenderer::DrawTooltip(const Font& font, entt::registry& registry, entt::entity item) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        if (!itemComp) return;

        // 1. Prepare Data
        std::vector<std::string> lines;
        
        // Basic Stats
        if (itemComp->attack > 0) lines.push_back(TextFormat("攻击力: %.0f", itemComp->attack));
        if (itemComp->defense > 0) lines.push_back(TextFormat("护甲: %.0f", itemComp->defense));
        if (itemComp->bagCapacity > 0) lines.push_back(TextFormat("容量: %d 格", itemComp->bagCapacity));
        
        // Implicits
        for (const auto& aff : itemComp->implicits) {
            lines.push_back(GetAffixDescription(aff));
        }
        
        // Separator
        if ((!lines.empty()) && !itemComp->affixes.empty()) {
            lines.push_back("---");
        }

        // Explicit Affixes
        for (const auto& aff : itemComp->affixes) {
            lines.push_back(GetAffixDescription(aff));
        }

        // Description
        if (!itemComp->description.empty()) {
            if (!lines.empty()) lines.push_back(" "); 
            lines.push_back(itemComp->description);
        }
        
        // 2. Calculate Dimensions
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
        float x = m.x + 15;
        float y = m.y + 15;
        
        if (x + w > GetScreenWidth()) x -= (w + 20);
        if (y + h > GetScreenHeight()) y -= (h + 20);

        // 3. Draw
        DrawRectangle(x, y, w, h, Fade(BLACK, 0.9f));
        DrawRectangleLines(x, y, w, h, GetRarityColor(itemComp->rarity));
        
        DrawTextUI(font, itemComp->name.c_str(), x + padding, y + padding, titleSize, GetRarityColor(itemComp->rarity));
        
        float curY = y + padding + titleSize + 5.0f;
        for (const auto& line : lines) {
            if (line == "---") {
                DrawLine(x + padding, curY + lineHeight/2, x + w - padding, curY + lineHeight/2, GRAY);
            } else if (line != " ") {
                Color c = WHITE;
                if (line.find("+") == 0) c = GREEN; 
                DrawTextUI(font, line.c_str(), x + padding, curY, fontSize, c);
            }
            curY += lineHeight;
        }
    }

    void UIRenderer::DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry) {
        if (!uiContext.showContextMenu || !registry.valid(uiContext.contextMenuItem)) {
            uiContext.showContextMenu = false;
            return;
        }

        auto view = registry.view<PlayerTag>();
        if (view.begin() == view.end()) return;
        entt::entity player = view.front();

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
        float x = uiContext.contextMenuPos.x;
        float y = uiContext.contextMenuPos.y;

        DrawRectangle(x, y, w, h, Fade(BLACK, 0.95f));
        DrawRectangleLines(x, y, w, h, GOLD);

        float curY = y + 5;

        auto DrawMenuBtn = [&](const char* text) -> bool {
            Rectangle r = {x + 5, curY, w - 10, btnH - 2};
            bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
            if (hovered) DrawRectangleRec(r, Fade(GOLD, 0.3f));
            DrawTextUI(font, text, x + 15, curY + 5, 18, hovered ? WHITE : LIGHTGRAY);
            curY += btnH;
            return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        };

        if (showEquip) {
            if (DrawMenuBtn("装备 / 使用")) {
                InventorySystem::equipItem(registry, player, uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        if (showUnequip) {
            if (DrawMenuBtn("卸下")) {
                if (!InventorySystem::unequipItem(registry, player, uiContext.contextSourceEquipmentSlot)) {
                    uiContext.showMessageBox = true;
                    snprintf(uiContext.messageBoxText, 64, "背包已满！无法卸下装备。");
                    uiContext.messageBoxTimer = 2.0f;
                }
                uiContext.showContextMenu = false;
            }
        }
        if (showDrop) {
            if (DrawMenuBtn("丢弃")) {
                InventorySystem::dropItem(registry, player, uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        if (DrawMenuBtn("取消")) {
            uiContext.showContextMenu = false;
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (!CheckCollisionPointRec(GetMousePosition(), {x, y, w, h})) {
                uiContext.showContextMenu = false;
            }
        }
    }

    void UIRenderer::DrawMessageBox(const Font& font, UIContext& uiContext) {
        if (!uiContext.showMessageBox) return;
        
        const char* text = uiContext.messageBoxText;
        float fontSize = 20;
        int textW = MeasureText(text, (int)fontSize); // Fallback measurement for box width estimation
        if (IsFontValid(font)) {
            textW = (int)MeasureTextEx(font, text, fontSize, 1.0f).x;
        }
        
        float w = textW + 60.0f;
        float h = 50.0f;
        float x = (GetScreenWidth() - w) / 2.0f;
        float y = (GetScreenHeight() - h) / 2.0f;
        
        DrawRectangle((int)x, (int)y, (int)w, (int)h, Fade(BLACK, 0.9f));
        DrawRectangleLines((int)x, (int)y, (int)w, (int)h, RED);
        DrawTextUI(font, text, x + 30, y + 15, fontSize, WHITE);
    }

}
