#include "UICharacter.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../core/UIRenderer.hpp" 
#include "raylib.h"
#include <algorithm>
#include <cmath>

using namespace NoMoreDay;

// 内部状态
static int s_activeCharTab = 0; // 0: 攻击, 1: 防御, 2: 召唤, 3: 其他
static float s_charPanelScroll = 0.0f;
static float s_lastContentHeight = 0.0f;

void UICharacter::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag>();
    if (view.begin() == view.end()) return;
    entt::entity player = view.front();

    // Scaling Factor
    float scale = UIRenderer::GetScale();

    // Logic Mouse Position
    Vector2 mousePos = UISystem::GetMousePositionLogic();
    float alpha = UISystem::State.characterPanelAlpha;

    // --- 1. 面板背景 (Logic Coords) ---
    const float panelW = 420.0f;
    const float panelH = 580.0f;
    const float margin = 20.0f;
    
    // 锚定左下角 (Bottom-Left Anchor in Logic Space)
    const float panelX = margin;
    const float panelY = UI_REF_HEIGHT - panelH - margin;
    const float padding = 20.0f;

    // Helpers for scaled drawing
    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), Fade(c, alpha));
    };
    auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
        DrawRectangleLinesEx({rec.x*scale, rec.y*scale, rec.width*scale, rec.height*scale}, thick*scale, Fade(c, alpha));
    };

    // 半透明背景 + 边框
    DrawRectScaled(panelX, panelY, panelW, panelH, DARKGRAY);
    DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 2.0f, GOLD);

    // 标题
    UISystem::DrawTextUI("角色属性", panelX + padding, panelY + padding, 30, WHITE, alpha);
    UISystem::DrawTextUI("按 'C' 关闭", panelX + panelW - 100, panelY + padding + 10, 18, LIGHTGRAY, alpha);

    float currentY = panelY + 70.0f;

    // --- 2. 角色概览 (头像 & 等级) ---
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    auto* pStats = registry.try_get<PlayerStats>(player);

    float avatarSize = 80.0f;
    DrawRectLinesScaled({panelX + padding, currentY, avatarSize, avatarSize}, 1.0f, LIGHTGRAY);
    
    if (sprite && sprite->texture.id > 0) {
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {(panelX + padding)*scale, currentY*scale, avatarSize*scale, avatarSize*scale};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, Fade(WHITE, alpha));
    } else {
        UISystem::DrawTextUI("?", panelX + padding + 30, currentY + 20, 40, GRAY, alpha);
    }

    float infoX = panelX + padding + avatarSize + 20.0f;
    if (pStats) {
        UISystem::DrawTextUI(TextFormat("等级 %d", pStats->level), infoX, currentY + 10, 24, GOLD, alpha);
        UISystem::DrawTextUI(TextFormat("经验: %.0f", pStats->current_xp), infoX, currentY + 40, 16, LIGHTGRAY, alpha);
    } else {
        UISystem::DrawTextUI("等级 ??", infoX, currentY + 10, 24, GRAY, alpha);
    }

    currentY += avatarSize + 30.0f;

    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);
    auto& attrUI = registry.get_or_emplace<AttributeUIComponent>(player);

    if (!primStats || !combatStats || !pStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    UISystem::DrawTextUI("基础属性", panelX + padding, currentY, 20, YELLOW, alpha);
    
    int totalTemp = attrUI.tempStr + attrUI.tempDex + attrUI.tempInt + attrUI.tempVit;
    int remainingPoints = pStats->available_attribute_points - totalTemp;
    
    const char* pointsText = TextFormat("可用点数: %d", remainingPoints);
    UISystem::DrawTextUI(pointsText, panelX + panelW - padding - 120, currentY + 2, 18, remainingPoints > 0 ? GREEN : LIGHTGRAY, alpha);

    currentY += 25.0f;
    float col1X = panelX + padding;

    auto DrawBtn = [&](float bx, float by, const char* txt) -> bool {
        float size = 20.0f;
        Rectangle r = {bx, by, size, size};
        bool hovered = CheckCollisionPointRec(mousePos, r);
        bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        DrawRectScaled(bx, by, size, size, hovered ? LIGHTGRAY : DARKGRAY);
        DrawRectLinesScaled(r, 1.0f, WHITE);
        float tw = (float)MeasureText(txt, 16);
        UISystem::DrawTextUI(txt, bx + (size-tw)/2, by + 2, 16, WHITE, alpha);
        return clicked;
    };

    auto DrawAttrRow = [&](const char* label, float baseVal, int& tempVal, float& y) {
        float rowH = 24.0f;
        UISystem::DrawTextUI(label, col1X, y, 18, LIGHTGRAY, alpha);
        float finalVal = baseVal + tempVal;
        const char* valStr = (tempVal > 0) ? TextFormat("%.0f (+%d)", finalVal, tempVal) : TextFormat("%.0f", finalVal);
        UISystem::DrawTextUI(valStr, col1X + 80, y, 18, (tempVal > 0) ? GREEN : WHITE, alpha);
        float btnX = col1X + 200.0f;
        if (tempVal > 0 && DrawBtn(btnX, y, "-")) tempVal--;
        if (remainingPoints > 0 && DrawBtn(btnX + 25, y, "+")) tempVal++;
        y += rowH + 5.0f;
    };

    DrawAttrRow("力量", primStats->strength, attrUI.tempStr, currentY);
    DrawAttrRow("敏捷", primStats->dexterity, attrUI.tempDex, currentY);
    DrawAttrRow("智力", primStats->intelligence, attrUI.tempInt, currentY);
    DrawAttrRow("体能", primStats->vitality, attrUI.tempVit, currentY);

    currentY += 15.0f;
    DrawLineEx({(panelX + padding)*scale, currentY*scale}, {(panelX + panelW - padding)*scale, currentY*scale}, 1.0f*scale, Fade(GRAY, alpha));
    currentY += 10.0f;

    // --- 4. Tabs ---
    const char* tabNames[] = { "攻击", "防御", "召唤", "其他" };
    float tabW = (panelW - padding * 2) / 4;
    for (int i = 0; i < 4; ++i) {
        float tx = panelX + padding + i * tabW;
        Rectangle tabRect = { tx, currentY, tabW, 30 };
        bool isSelected = (s_activeCharTab == i);
        bool isHovered = CheckCollisionPointRec(mousePos, tabRect);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHovered) { s_activeCharTab = i; s_charPanelScroll = 0.0f; }
        DrawRectScaled(tabRect.x, tabRect.y, tabRect.width, tabRect.height, isSelected ? Fade(GOLD, 0.3f) : (isHovered ? Fade(WHITE, 0.1f) : Fade(BLACK, 0.5f)));
        DrawRectLinesScaled(tabRect, 1.0f, isSelected ? GOLD : DARKGRAY);
        UISystem::DrawTextUI(tabNames[i], tx + 10, currentY + 6, 18, isSelected ? WHITE : GRAY, alpha);
    }
    currentY += 35.0f;

    // --- 5. Content ---
    float contentH = panelY + panelH - currentY - padding - (totalTemp > 0 ? 40.0f : 0.0f);
    Rectangle viewRect = { panelX + padding, currentY, panelW - padding * 2, contentH };
    if (CheckCollisionPointRec(mousePos, viewRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) s_charPanelScroll += wheel * 20.0f;
    }
    if (s_charPanelScroll > 0) s_charPanelScroll = 0;
    if (s_lastContentHeight > viewRect.height) {
        float minScroll = viewRect.height - s_lastContentHeight;
        if (s_charPanelScroll < minScroll) s_charPanelScroll = minScroll;
    } else { s_charPanelScroll = 0; }

    BeginScissorMode((int)(viewRect.x * scale), (int)(viewRect.y * scale), (int)(viewRect.width * scale), (int)(viewRect.height * scale));
    float startY = currentY + s_charPanelScroll;
    float y = startY;
    float rowX = panelX + padding + 5.0f;
    float rowW = (viewRect.width - 10.0f) * 0.7f;

    if (s_activeCharTab == 0) {
        UISystem::DrawTextUI("面板伤害", rowX, y, 18, YELLOW, alpha); y += 25.0f;
        float flatPhys = combatStats->flat_damage[(int)DamageType::Physical];
        float physMult = combatStats->damage_multipliers[(int)DamageType::Physical];
        DrawStatRow("物理伤害", TextFormat("%.0f-%.0f", (combatStats->min_weapon_damage+flatPhys)*physMult, (combatStats->max_weapon_damage+flatPhys)*physMult), rowX, y, rowW, 18.0f, alpha);
        
        y += 10.0f; UISystem::DrawTextUI("伤害加成详情", rowX, y, 18, YELLOW, alpha); y += 25.0f;
        auto DrawDmgB = [&](DamageType t, const char* n) { DrawStatRow(TextFormat("%s加成", n), TextFormat("%.0f / +%.0f%%", combatStats->flat_damage[(int)t], (combatStats->damage_multipliers[(int)t]-1.0f)*100.0f), rowX, y, rowW, 18.0f, alpha); };
        DrawDmgB(DamageType::Physical, "物理"); DrawDmgB(DamageType::Fire, "火焰"); DrawDmgB(DamageType::Cold, "冰霜");
        DrawStatRow("攻击速度", TextFormat("%.2f", combatStats->attack_speed), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("暴击几率", TextFormat("%.1f%%", combatStats->crit_chance * 100.0f), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("暴击伤害", TextFormat("%.0f%%", combatStats->crit_damage * 100.0f), rowX, y, rowW, 18.0f, alpha);
    } else if (s_activeCharTab == 1) {
        UISystem::DrawTextUI("生存", rowX, y, 18, YELLOW, alpha); y += 25.0f;
        DrawStatRow("生命值", TextFormat("%.0f / %.0f", combatStats->health, combatStats->max_health), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("护甲", TextFormat("%.0f", combatStats->armor), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("火焰抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Fire] * 100.0f), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("冰霜抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Cold] * 100.0f), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("闪电抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Lightning] * 100.0f), rowX, y, rowW, 18.0f, alpha);
    } else if (s_activeCharTab == 3) {
        UISystem::DrawTextUI("综合", rowX, y, 18, YELLOW, alpha); y += 25.0f;
        DrawStatRow("移动速度", TextFormat("%.0f", combatStats->move_speed), rowX, y, rowW, 18.0f, alpha);
        DrawStatRow("魔法寻宝", TextFormat("%.0f%%", combatStats->magic_find * 100.0f), rowX, y, rowW, 18.0f, alpha);
    }

    s_lastContentHeight = y - startY;
    EndScissorMode();

    if (totalTemp > 0) {
        float btnY = panelY + panelH - 45.0f;
        Rectangle confirmRect = {panelX + panelW - padding - 100, btnY, 100, 30};
        if (CheckCollisionPointRec(mousePos, confirmRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) attrUI.showConfirmPopup = true;
        DrawRectScaled(confirmRect.x, confirmRect.y, confirmRect.width, confirmRect.height, GREEN);
        UISystem::DrawTextUI("确认加点", confirmRect.x + 15, confirmRect.y + 6, 18, BLACK, alpha);
    }
}

void UICharacter::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize, float alpha) {
    UISystem::DrawTextScaled(label, x, y, fontSize, width * 0.6f, LIGHTGRAY, alpha);
    float textWidth = (float)MeasureText(value, (int)fontSize);
    UISystem::DrawTextUI(value, x + width - textWidth, y, fontSize, WHITE, alpha);
    y += fontSize + 5.0f;
}
