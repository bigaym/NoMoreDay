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

void UICharacter::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize, float alpha) {
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();
    
    UIRenderer::DrawTextScaled(font, label, x, y, fontSize, width * 0.6f, theme.textSecondary, alpha);
    
    // Calculate width to align right
    float valWidth = IsFontValid(font) ? MeasureTextEx(font, value, fontSize, 1.0f).x : (float)MeasureText(value, (int)fontSize);
    
    UIRenderer::DrawTextUI(font, value, x + width - valWidth, y, fontSize, theme.textPrimary, alpha);
    y += fontSize + 8.0f; // Increased spacing
}

// 辅助函数：绘制带上限检查的属性行
static void DrawCappedStatRow(const char* label, float currentVal, float cap, bool isPercent, float x, float& y, float width, float fontSize, float alpha) {
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();
    
    UIRenderer::DrawTextScaled(font, label, x, y, fontSize, width * 0.6f, theme.textSecondary, alpha);
    
    bool isOverCap = (currentVal > cap);
    float displayVal = isOverCap ? cap : currentVal;
    
    char buffer[64];
    if (isPercent) {
        if (isOverCap) {
            sprintf(buffer, "%.0f%% (%.0f%%)", displayVal * 100.0f, currentVal * 100.0f);
        } else {
            sprintf(buffer, "%.1f%%", currentVal * 100.0f);
        }
    } else {
        if (isOverCap) {
            sprintf(buffer, "%.0f (%.0f)", displayVal, currentVal);
        } else {
            sprintf(buffer, "%.0f", currentVal);
        }
    }

    float valWidth = IsFontValid(font) ? MeasureTextEx(font, buffer, fontSize, 1.0f).x : (float)MeasureText(buffer, (int)fontSize);
    UIRenderer::DrawTextUI(font, buffer, x + width - valWidth, y, fontSize, isOverCap ? theme.success : theme.textPrimary, alpha);
    y += fontSize + 8.0f;
}

void UICharacter::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag>();
    if (view.begin() == view.end()) return;
    entt::entity player = view.front();

    // Scaling Factor
    float scale = UIRenderer::GetScale();
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();

    // Logic Mouse Position
    Vector2 mousePos = UISystem::GetMousePositionLogic();
    float alpha = UISystem::State.characterPanelAlpha;

    // --- 1. 面板背景 (Logic Coords) ---
    const float panelW = 450.0f; // Widened slightly
    const float panelH = 780.0f; // Heightened for better spacing
    const float margin = 40.0f;  // Increased margin from screen edge
    
    // 锚定左侧居中 (Center Left Anchor)
    const float panelX = margin;
    const float panelY = (UI_REF_HEIGHT - panelH) / 2.0f;
    const float padding = 25.0f;

    // Helpers for scaled drawing
    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), Fade(c, alpha));
    };
    auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
        DrawRectangleLinesEx({rec.x*scale, rec.y*scale, rec.width*scale, rec.height*scale}, thick*scale, Fade(c, alpha));
    };

    // Background Panel
    DrawRectScaled(panelX, panelY, panelW, panelH, theme.panelBackground);
    DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 1.0f, theme.panelBorder);
    // Header Line
    DrawLineEx({panelX*scale, (panelY + 60)*scale}, {(panelX + panelW)*scale, (panelY + 60)*scale}, 1.0f*scale, Fade(theme.panelBorder, alpha));

    // 标题
    UIRenderer::DrawTextUI(font, "角色属性", panelX + padding, panelY + 18, 28, theme.textHighlight, alpha);
    UIRenderer::DrawTextUI(font, "按 'C' 关闭", panelX + panelW - 120, panelY + 25, 18, theme.textSecondary, alpha);

    float currentY = panelY + 80.0f;

    // --- 2. 角色概览 (头像 & 等级) ---
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    auto* pStats = registry.try_get<PlayerStats>(player);

    float avatarSize = 90.0f;
    Rectangle avatarRect = {panelX + padding, currentY, avatarSize, avatarSize};
    DrawRectScaled(avatarRect.x, avatarRect.y, avatarRect.width, avatarRect.height, theme.slotBackground);
    DrawRectLinesScaled(avatarRect, 1.0f, theme.panelBorder);
    
    if (sprite && sprite->texture.id > 0) {
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {avatarRect.x*scale, avatarRect.y*scale, avatarSize*scale, avatarSize*scale};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, Fade(WHITE, alpha));
    } else {
        UIRenderer::DrawTextUI(font, "?", avatarRect.x + 35, avatarRect.y + 25, 40, theme.textSecondary, alpha);
    }

    float infoX = panelX + padding + avatarSize + 25.0f;
    if (pStats) {
        bool isMaxLevel = (pStats->level >= 100);
        UIRenderer::DrawTextUI(font, TextFormat("等级 %d", pStats->level), infoX, currentY + 10, 26, isMaxLevel ? theme.textHighlight : theme.textPrimary, alpha);
        
        // XP Bar Background
        DrawRectScaled(infoX, currentY + 50, 200, 12, theme.slotBackground);
        
        // XP Bar Fill
        float xpRatio = 0.0f;
        if (isMaxLevel) {
            xpRatio = 1.0f;
        } else if (pStats->required_xp > 0) {
            xpRatio = (float)pStats->current_xp / pStats->required_xp;
        }
        if (xpRatio > 1.0f) xpRatio = 1.0f;
        
        DrawRectScaled(infoX, currentY + 50, 200 * xpRatio, 12, isMaxLevel ? theme.textHighlight : theme.success);
        
        if (isMaxLevel) {
            UIRenderer::DrawTextUI(font, "MAX LEVEL", infoX, currentY + 65, 16, theme.textHighlight, alpha);
        } else {
            UIRenderer::DrawTextUI(font, TextFormat("XP: %.0f / %.0f", pStats->current_xp, pStats->required_xp), infoX, currentY + 65, 16, theme.textSecondary, alpha);
        }
    } else {
        UIRenderer::DrawTextUI(font, "等级 ??", infoX, currentY + 10, 26, theme.textSecondary, alpha);
    }

    currentY += avatarSize + 30.0f;

    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);
    auto& attrUI = registry.get_or_emplace<AttributeUIComponent>(player);

    if (!primStats || !combatStats || !pStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    UIRenderer::DrawTextUI(font, "基础属性", panelX + padding, currentY, 22, theme.textHighlight, alpha);
    
    int totalTemp = attrUI.tempStr + attrUI.tempDex + attrUI.tempInt + attrUI.tempVit;
    int remainingPoints = pStats->available_attribute_points - totalTemp;
    
    const char* pointsText = TextFormat("可用点数: %d", remainingPoints);
    UIRenderer::DrawTextUI(font, pointsText, panelX + panelW - padding - 140, currentY + 4, 18, remainingPoints > 0 ? theme.success : theme.textSecondary, alpha);

    currentY += 35.0f;
    float col1X = panelX + padding;

    auto DrawBtn = [&](float bx, float by, const char* txt) -> bool {
        float size = 22.0f;
        Rectangle r = {bx, by, size, size};
        bool hovered = CheckCollisionPointRec(mousePos, r);
        bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        
        Color bg = hovered ? theme.buttonHover : theme.buttonNormal;
        if (clicked) bg = theme.buttonPress;

        DrawRectScaled(bx, by, size, size, bg);
        DrawRectLinesScaled(r, 1.0f, theme.panelBorder);
        
        float tw = IsFontValid(font) ? MeasureTextEx(font, txt, 18, 1.0f).x : (float)MeasureText(txt, 18);
        UIRenderer::DrawTextUI(font, txt, bx + (size-tw)/2, by + 2, 18, theme.textPrimary, alpha);
        return clicked;
    };

    auto DrawAttrRow = [&](const char* label, float baseVal, int& tempVal, float& y) {
        float rowH = 28.0f;
        UIRenderer::DrawTextUI(font, label, col1X, y + 4, 20, theme.textSecondary, alpha);
        
        float finalVal = baseVal + tempVal;
        const char* valStr = (tempVal > 0) ? TextFormat("%.0f (+%d)", finalVal, tempVal) : TextFormat("%.0f", finalVal);
        
        UIRenderer::DrawTextUI(font, valStr, col1X + 100, y + 4, 20, (tempVal > 0) ? theme.success : theme.textPrimary, alpha);
        
        float btnX = col1X + 260.0f;
        if (tempVal > 0 && DrawBtn(btnX, y, "-")) tempVal--;
        if (remainingPoints > 0 && DrawBtn(btnX + 30, y, "+")) tempVal++;
        y += rowH + 8.0f;
    };

    DrawAttrRow("力量", primStats->strength, attrUI.tempStr, currentY);
    DrawAttrRow("敏捷", primStats->dexterity, attrUI.tempDex, currentY);
    DrawAttrRow("智力", primStats->intelligence, attrUI.tempInt, currentY);
    DrawAttrRow("体能", primStats->vitality, attrUI.tempVit, currentY);

    currentY += 20.0f;
    DrawLineEx({(panelX + padding)*scale, currentY*scale}, {(panelX + panelW - padding)*scale, currentY*scale}, 1.0f*scale, Fade(theme.panelBorder, alpha));
    currentY += 15.0f;

    // --- 4. Tabs ---
    const char* tabNames[] = { "攻击", "防御", "召唤", "其他" };
    float tabW = (panelW - padding * 2) / 4;
    for (int i = 0; i < 4; ++i) {
        float tx = panelX + padding + i * tabW;
        Rectangle tabRect = { tx, currentY, tabW, 32 };
        bool isSelected = (s_activeCharTab == i);
        bool isHovered = CheckCollisionPointRec(mousePos, tabRect);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHovered) { s_activeCharTab = i; s_charPanelScroll = 0.0f; }
        
        Color bg = isSelected ? theme.panelBorderHighlight : (isHovered ? theme.buttonHover : theme.buttonNormal);
        DrawRectScaled(tabRect.x, tabRect.y, tabRect.width, tabRect.height, bg);
        DrawRectLinesScaled(tabRect, 1.0f, theme.panelBorder);
        
        float tw = IsFontValid(font) ? MeasureTextEx(font, tabNames[i], 18, 1.0f).x : (float)MeasureText(tabNames[i], 18);
        UIRenderer::DrawTextUI(font, tabNames[i], tx + (tabW-tw)/2, currentY + 8, 18, isSelected ? theme.panelBackground : theme.textSecondary, alpha);
    }
    currentY += 40.0f;

    // --- 5. Content ---
    float contentH = panelY + panelH - currentY - padding - (totalTemp > 0 ? 50.0f : 0.0f);
    Rectangle viewRect = { panelX + padding, currentY, panelW - padding * 2, contentH };
    if (CheckCollisionPointRec(mousePos, viewRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) s_charPanelScroll += wheel * 30.0f;
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
    float rowW = (viewRect.width - 20.0f); // Adjusted width

    if (s_activeCharTab == 0) {
        UIRenderer::DrawTextUI(font, "攻击基础", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        float flatPhys = combatStats->flat_damage[(int)DamageType::Physical];
        float physMult = combatStats->damage_multipliers[(int)DamageType::Physical];
        DrawStatRow("物理伤害", TextFormat("%.0f - %.0f", (combatStats->min_weapon_damage + flatPhys) * physMult, (combatStats->max_weapon_damage + flatPhys) * physMult), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("攻击速度", TextFormat("%.2f", combatStats->attack_speed), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("命中率", TextFormat("%.0f%%", combatStats->accuracy * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("暴击几率", TextFormat("%.1f%%", combatStats->crit_chance * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("暴击伤害", TextFormat("%.0f%%", combatStats->crit_damage * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("护甲穿透", TextFormat("%.0f", combatStats->armor_pen), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("施法速度", TextFormat("%.2f", combatStats->cast_speed), rowX, y, rowW, 20.0f, alpha);

        y += 15.0f; UIRenderer::DrawTextUI(font, "附加点伤", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        auto DrawFlat = [&](DamageType t, const char* n) { 
            if (combatStats->flat_damage[(int)t] > 0)
                DrawStatRow(n, TextFormat("+%.0f", combatStats->flat_damage[(int)t]), rowX, y, rowW, 20.0f, alpha); 
        };
        DrawFlat(DamageType::Fire, "附加火焰"); DrawFlat(DamageType::Cold, "附加冰霜"); DrawFlat(DamageType::Lightning, "附加闪电");
        DrawFlat(DamageType::Poison, "附加毒素"); DrawFlat(DamageType::Shadow, "附加暗影");

        y += 15.0f; UIRenderer::DrawTextUI(font, "伤害增益", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        auto DrawMult = [&](DamageType t, const char* n) { 
            DrawStatRow(n, TextFormat("+%.0f%%", (combatStats->damage_multipliers[(int)t] - 1.0f) * 100.0f), rowX, y, rowW, 20.0f, alpha); 
        };
        DrawMult(DamageType::Physical, "物理加成"); DrawMult(DamageType::Fire, "火焰加成"); DrawMult(DamageType::Cold, "冰霜加成");
        DrawMult(DamageType::Lightning, "闪电加成"); DrawMult(DamageType::Poison, "毒素加成"); DrawMult(DamageType::Shadow, "暗影加成");

    } else if (s_activeCharTab == 1) {
        UIRenderer::DrawTextUI(font, "防御基础", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        DrawStatRow("生命值", TextFormat("%.0f / %.0f", combatStats->health, combatStats->max_health), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("法力值", TextFormat("%.0f / %.0f", combatStats->mana, combatStats->max_mana), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("护甲", TextFormat("%.0f", combatStats->armor), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("闪避几率", TextFormat("%.1f%%", combatStats->dodge_chance * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("格挡几率", TextFormat("%.1f%%", combatStats->block_chance * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("格挡减免", TextFormat("%.0f", combatStats->block_amount), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("全局减伤", TextFormat("%.1f%%", combatStats->damage_reduction * 100.0f), rowX, y, rowW, 20.0f, alpha);

        y += 15.0f; UIRenderer::DrawTextUI(font, "元素抗性", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        auto DrawRes = [&](DamageType t, const char* n) { 
            DrawStatRow(n, TextFormat("%.0f%%", combatStats->resistances[(int)t] * 100.0f), rowX, y, rowW, 20.0f, alpha); 
        };
        DrawRes(DamageType::Physical, "物理抗性"); DrawRes(DamageType::Fire, "火焰抗性"); DrawRes(DamageType::Cold, "冰霜抗性");
        DrawRes(DamageType::Lightning, "闪电抗性"); DrawRes(DamageType::Poison, "毒素抗性"); DrawRes(DamageType::Shadow, "暗影抗性");

    } else if (s_activeCharTab == 3) {
        UIRenderer::DrawTextUI(font, "回复与辅助", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        DrawStatRow("生命回复", TextFormat("%.1f /秒", combatStats->health_regen), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("法力回复", TextFormat("%.1f /秒", combatStats->mana_regen), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("生命吸取", TextFormat("%.1f%%", combatStats->life_steal * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("击回生命", TextFormat("%.0f", combatStats->life_on_hit), rowX, y, rowW, 20.0f, alpha);
        
        y += 15.0f; UIRenderer::DrawTextUI(font, "综合属性", rowX, y, 20, theme.textHighlight, alpha); y += 30.0f;
        DrawStatRow("移动速度", TextFormat("%.0f", combatStats->move_speed), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("魔法寻宝", TextFormat("%.0f%%", combatStats->magic_find * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("冷却缩减", TextFormat("%.0f%%", combatStats->cooldown_reduction * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("拾取范围", TextFormat("%.0f", combatStats->pickup_range), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("金币加成", TextFormat("%.0f%%", combatStats->gold_bonus * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("经验加成", TextFormat("%.0f%%", combatStats->experience_gain_mult * 100.0f), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("荆棘伤害", TextFormat("%.0f", combatStats->thorns), rowX, y, rowW, 20.0f, alpha);
        DrawStatRow("技能范围", TextFormat("%.0f%%", combatStats->area_scale * 100.0f), rowX, y, rowW, 20.0f, alpha);
    }

    s_lastContentHeight = y - startY;
    EndScissorMode();

    if (totalTemp > 0) {
        float btnY = panelY + panelH - 50.0f;
        Rectangle confirmRect = {panelX + panelW - padding - 120, btnY, 120, 36};
        bool hovered = CheckCollisionPointRec(mousePos, confirmRect);
        
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) attrUI.showConfirmPopup = true;
        
        DrawRectScaled(confirmRect.x, confirmRect.y, confirmRect.width, confirmRect.height, hovered ? theme.success : Fade(theme.success, 0.8f));
        DrawRectLinesScaled(confirmRect, 1.0f, theme.panelBorder);
        
        float tw = IsFontValid(font) ? MeasureTextEx(font, "确认加点", 20, 1.0f).x : (float)MeasureText("确认加点", 20);
        UIRenderer::DrawTextUI(font, "确认加点", confirmRect.x + (confirmRect.width-tw)/2, confirmRect.y + 8, 20, theme.textPrimary, alpha);
    }
}
