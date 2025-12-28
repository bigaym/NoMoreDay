#include "UICharacter.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
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

    // --- 1. 面板背景 ---
    const float panelW = 420.0f;
    const float panelH = 580.0f;
    const float margin = 20.0f;
    
    // 锚定左下角 (Bottom-Left Anchor)
    const float panelX = margin;
    const float panelY = (float)GetScreenHeight() - panelH - margin;
    const float padding = 20.0f;

    // 半透明黑色背景 + 边框
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.85f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0f, GOLD);

    // 标题
    UISystem::DrawTextUI("角色属性", panelX + padding, panelY + padding, 30, WHITE);
    UISystem::DrawTextUI("按 'C' 关闭", panelX + panelW - 100, panelY + padding + 10, 18, LIGHTGRAY);

    float currentY = panelY + 70.0f;

    // --- 2. 角色概览 (头像 & 等级) ---
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    auto* pStats = registry.try_get<PlayerStats>(player);

    // 绘制头像 (缩略图)
    float avatarSize = 80.0f;
    DrawRectangleLines(panelX + padding, currentY, avatarSize, avatarSize, LIGHTGRAY);
    if (sprite && sprite->texture.id > 0) {
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {panelX + padding, currentY, avatarSize, avatarSize};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
    } else {
        UISystem::DrawTextUI("?", panelX + padding + 30, currentY + 20, 40, GRAY);
    }

    // 绘制等级信息
    float infoX = panelX + padding + avatarSize + 20.0f;
    if (pStats) {
        UISystem::DrawTextUI(TextFormat("等级 %d", pStats->level), infoX, currentY + 10, 24, GOLD);
        UISystem::DrawTextUI(TextFormat("经验: %.0f", pStats->current_xp), infoX, currentY + 40, 16, LIGHTGRAY);
    } else {
        UISystem::DrawTextUI("等级 ??", infoX, currentY + 10, 24, GRAY);
    }

    currentY += avatarSize + 30.0f;

    // 获取属性组件
    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);
    auto& attrUI = registry.get_or_emplace<AttributeUIComponent>(player);

    if (!primStats || !combatStats || !pStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    UISystem::DrawTextUI("基础属性", panelX + padding, currentY, 20, YELLOW);
    
    // 计算剩余点数
    int totalTemp = attrUI.tempStr + attrUI.tempDex + attrUI.tempInt + attrUI.tempVit;
    int remainingPoints = pStats->available_attribute_points - totalTemp;
    
    // 显示可用点数
    const char* pointsText = TextFormat("可用点数: %d", remainingPoints);
    float pointsWidth = 0; // 简化计算
    UISystem::DrawTextUI(pointsText, panelX + panelW - padding - 120, currentY + 2, 18, remainingPoints > 0 ? GREEN : LIGHTGRAY);

    currentY += 25.0f;
    float col1X = panelX + padding;

    auto DrawBtn = [&](float bx, float by, const char* txt) -> bool {
        float size = 20.0f;
        Rectangle r = {bx, by, size, size};
        bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
        bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        DrawRectangleRec(r, hovered ? LIGHTGRAY : DARKGRAY);
        DrawRectangleLinesEx(r, 1.0f, WHITE);
        float tw = MeasureText(txt, 16);
        UISystem::DrawTextUI(txt, bx + (size-tw)/2, by + 2, 16, WHITE);
        return clicked;
    };

    auto DrawAttrRow = [&](const char* label, float baseVal, int& tempVal, float& y) {
        float rowH = 24.0f;
        UISystem::DrawTextUI(label, col1X, y, 18, LIGHTGRAY);
        
        float finalVal = baseVal + tempVal;
        const char* valStr = (tempVal > 0) ? TextFormat("%.0f (+%d)", finalVal, tempVal) : TextFormat("%.0f", finalVal);
        Color valColor = (tempVal > 0) ? GREEN : WHITE;
        UISystem::DrawTextUI(valStr, col1X + 80, y, 18, valColor);

        float btnX = col1X + 200.0f;
        if (tempVal > 0) {
            if (DrawBtn(btnX, y, "-")) tempVal--;
        }
        if (remainingPoints > 0) {
            if (DrawBtn(btnX + 25, y, "+")) tempVal++;
        }
        y += rowH + 5.0f;
    };

    DrawAttrRow("力量", primStats->strength, attrUI.tempStr, currentY);
    DrawAttrRow("敏捷", primStats->dexterity, attrUI.tempDex, currentY);
    DrawAttrRow("智力", primStats->intelligence, attrUI.tempInt, currentY);
    DrawAttrRow("体能", primStats->vitality, attrUI.tempVit, currentY);

    currentY += 15.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 10.0f;

    // --- 4. 标签页 (Tabs) ---
    const char* tabNames[] = { "攻击", "防御", "召唤", "其他" };
    int tabCount = 4;
    float tabW = (panelW - padding * 2) / tabCount;
    float tabH = 30.0f;

    for (int i = 0; i < tabCount; ++i) {
        float tx = panelX + padding + i * tabW;
        Rectangle tabRect = { tx, currentY, tabW, tabH };
        bool isSelected = (s_activeCharTab == i);
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), tabRect);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHovered) {
            s_activeCharTab = i;
            s_charPanelScroll = 0.0f;
        }

        DrawRectangleRec(tabRect, isSelected ? Fade(GOLD, 0.3f) : (isHovered ? Fade(WHITE, 0.1f) : Fade(BLACK, 0.5f)));
        DrawRectangleLinesEx(tabRect, 1.0f, isSelected ? GOLD : DARKGRAY);
        UISystem::DrawTextUI(tabNames[i], tx + 10, currentY + 6, 18, isSelected ? WHITE : GRAY);
    }
    currentY += tabH + 5.0f;

    // --- 5. 可滚动内容区域 ---
    float contentH = panelY + panelH - currentY - padding - (totalTemp > 0 ? 40.0f : 0.0f);
    Rectangle viewRect = { panelX + padding, currentY, panelW - padding * 2, contentH };

    if (CheckCollisionPointRec(GetMousePosition(), viewRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) s_charPanelScroll += wheel * 20.0f;
    }
    if (s_charPanelScroll > 0) s_charPanelScroll = 0;
    if (s_lastContentHeight > viewRect.height) {
        float minScroll = viewRect.height - s_lastContentHeight;
        if (s_charPanelScroll < minScroll) s_charPanelScroll = minScroll;
    } else {
        s_charPanelScroll = 0;
    }

    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width, (int)viewRect.height);
    float startY = currentY + s_charPanelScroll;
    float y = startY;
    float rowX = panelX + padding + 5.0f;
    float rowW = (viewRect.width - 10.0f) * 0.7f; // 缩短行宽，使数值更靠近标签 (减少约一半间距)

    if (s_activeCharTab == 0) { // 攻击
        UISystem::DrawTextUI("面板伤害", rowX, y, 18, YELLOW); y += 25.0f;
        
        float physMult = combatStats->damage_multipliers[(int)DamageType::Physical];
        float flatPhys = combatStats->flat_damage[(int)DamageType::Physical];
        float dispMin = (combatStats->min_weapon_damage + flatPhys) * physMult;
        float dispMax = (combatStats->max_weapon_damage + flatPhys) * physMult;

        DrawStatRow("物理伤害", TextFormat("%.0f-%.0f", dispMin, dispMax), rowX, y, rowW);
        
        // 显示其他元素伤害
        auto DrawElemDmg = [&](DamageType type, const char* name) {
            float flat = combatStats->flat_damage[(int)type];
            if (flat > 0.0f) {
                float mult = combatStats->damage_multipliers[(int)type];
                DrawStatRow(name, TextFormat("%.0f", flat * mult), rowX, y, rowW);
            }
        };
        DrawElemDmg(DamageType::Fire, "火焰伤害");
        DrawElemDmg(DamageType::Cold, "冰霜伤害");
        DrawElemDmg(DamageType::Lightning, "闪电伤害");
        DrawElemDmg(DamageType::Poison, "毒素伤害");
        DrawElemDmg(DamageType::Shadow, "暗影伤害");

        y += 10.0f;
        UISystem::DrawTextUI("伤害加成详情", rowX, y, 18, YELLOW); y += 25.0f;

        auto DrawDmgBreakdown = [&](DamageType type, const char* name) {
            float mult = combatStats->damage_multipliers[(int)type];
            float flat = combatStats->flat_damage[(int)type];
            
            // 折叠显示: 附加点伤 / 百分比加成 (例如: 20 / +20%)
            float pct = (mult - 1.0f) * 100.0f;
            DrawStatRow(TextFormat("%s加成", name), TextFormat("%.0f / +%.0f%%", flat, pct), rowX, y, rowW);
        };

        DrawDmgBreakdown(DamageType::Physical, "物理");
        DrawDmgBreakdown(DamageType::Fire, "火焰");
        DrawDmgBreakdown(DamageType::Cold, "冰霜");
        DrawDmgBreakdown(DamageType::Lightning, "闪电");
        DrawDmgBreakdown(DamageType::Poison, "毒素");
        DrawDmgBreakdown(DamageType::Shadow, "暗影");

        DrawStatRow("攻击速度", TextFormat("%.2f", combatStats->attack_speed), rowX, y, rowW);
        DrawStatRow("施法速度", TextFormat("%.2f", combatStats->cast_speed), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("暴击属性", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("暴击几率", TextFormat("%.1f%%", combatStats->crit_chance * 100.0f), rowX, y, rowW);
        DrawStatRow("暴击伤害", TextFormat("%.0f%%", combatStats->crit_damage * 100.0f), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("技能形态", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("施法距离", TextFormat("%.0f", combatStats->cast_range), rowX, y, rowW);
        DrawStatRow("范围大小", TextFormat("%.0f%%", combatStats->area_scale * 100.0f), rowX, y, rowW);
        DrawStatRow("投射物速度", TextFormat("%.0f%%", combatStats->projectile_speed * 100.0f), rowX, y, rowW);
        DrawStatRow("持续时间", TextFormat("%.0f%%", combatStats->duration_scale * 100.0f), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("其他", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("护甲穿透", TextFormat("%.0f", combatStats->armor_pen), rowX, y, rowW);
        DrawStatRow("击退效果", TextFormat("%.1f", combatStats->knockback), rowX, y, rowW);
        DrawStatRow("生命偷取", TextFormat("%.1f%%", combatStats->life_steal * 100.0f), rowX, y, rowW);
        DrawStatRow("击中回复", TextFormat("%.1f", combatStats->life_on_hit), rowX, y, rowW);

    } else if (s_activeCharTab == 1) { // 防御
        UISystem::DrawTextUI("生存", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("生命值", TextFormat("%.0f / %.0f", combatStats->health, combatStats->max_health), rowX, y, rowW);
        DrawStatRow("生命回复", TextFormat("%.1f /秒", combatStats->health_regen), rowX, y, rowW);
        DrawStatRow("护甲", TextFormat("%.0f", combatStats->armor), rowX, y, rowW);
        DrawStatRow("闪避几率", TextFormat("%.1f%%", combatStats->dodge_chance * 100.0f), rowX, y, rowW);
        DrawStatRow("格挡几率", TextFormat("%.1f%%", combatStats->block_chance * 100.0f), rowX, y, rowW);
        DrawStatRow("格挡值", TextFormat("%.0f", combatStats->block_amount), rowX, y, rowW);
        DrawStatRow("伤害减免", TextFormat("%.1f%%", combatStats->damage_reduction * 100.0f), rowX, y, rowW);
        DrawStatRow("荆棘伤害", TextFormat("%.0f", combatStats->thorns), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("抗性", rowX, y, 18, YELLOW); y += 25.0f;
        
        DrawStatRow("火焰抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Fire] * 100.0f), rowX, y, rowW);
        DrawStatRow("冰霜抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Cold] * 100.0f), rowX, y, rowW);
        DrawStatRow("闪电抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Lightning] * 100.0f), rowX, y, rowW);
        DrawStatRow("毒素抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Poison] * 100.0f), rowX, y, rowW);
        DrawStatRow("暗影抗性", TextFormat("%.0f%%", combatStats->resistances[(int)DamageType::Shadow] * 100.0f), rowX, y, rowW);
        
    } else if (s_activeCharTab == 2) { // 召唤 (暂无详细属性，显示预留信息)
        UISystem::DrawTextUI("召唤物属性", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("召唤物伤害", "100%", rowX, y, rowW);
        DrawStatRow("召唤物生命", "100%", rowX, y, rowW);
        DrawStatRow("召唤物速度", "100%", rowX, y, rowW);
        
    } else if (s_activeCharTab == 3) { // 其他
        UISystem::DrawTextUI("综合", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("移动速度", TextFormat("%.0f", combatStats->move_speed), rowX, y, rowW);
        DrawStatRow("法力值", TextFormat("%.0f / %.0f", combatStats->mana, combatStats->max_mana), rowX, y, rowW);
        DrawStatRow("法力回复", TextFormat("%.1f /秒", combatStats->mana_regen), rowX, y, rowW);
        DrawStatRow("消耗降低", TextFormat("%.1f%%", combatStats->resource_cost_reduction * 100.0f), rowX, y, rowW);
        DrawStatRow("冷却缩减", TextFormat("%.1f%%", combatStats->cooldown_reduction * 100.0f), rowX, y, rowW);
        DrawStatRow("冷却回复", TextFormat("%.1f%%", (combatStats->cooldown_recovery_speed - 1.0f) * 100.0f), rowX, y, rowW);
        DrawStatRow("拾取范围", TextFormat("%.0f", combatStats->pickup_range), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("冒险", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("魔法寻宝", TextFormat("%.0f%%", combatStats->magic_find * 100.0f), rowX, y, rowW);
        DrawStatRow("金币加成", TextFormat("%.0f%%", combatStats->gold_bonus * 100.0f), rowX, y, rowW);
        DrawStatRow("经验加成", TextFormat("%.0f%%", combatStats->experience_gain_mult * 100.0f), rowX, y, rowW);
        
        y += 10.0f;
        UISystem::DrawTextUI("统计", rowX, y, 18, YELLOW); y += 25.0f;
        DrawStatRow("击杀数量", TextFormat("%llu", pStats->killCount), rowX, y, rowW);
        DrawStatRow("死亡次数", TextFormat("%llu", pStats->deathCount), rowX, y, rowW);
    }

    s_lastContentHeight = y - startY;
    EndScissorMode();

    // --- 6. 确认按钮 (如果有临时加点) ---
    if (totalTemp > 0) {
        float btnY = panelY + panelH - 45.0f;
        Rectangle confirmRect = {panelX + panelW - padding - 100, btnY, 100, 30};
        if (CheckCollisionPointRec(GetMousePosition(), confirmRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            attrUI.showConfirmPopup = true;
        }
        DrawRectangleRec(confirmRect, GREEN);
        UISystem::DrawTextUI("确认加点", confirmRect.x + 15, confirmRect.y + 6, 18, BLACK);
    }
}

void UICharacter::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize) {
    float labelMaxWidth = width * 0.6f;
    UISystem::DrawTextScaled(label, x, y, fontSize, labelMaxWidth, LIGHTGRAY);
    
    float textWidth = (float)MeasureText(value, (int)fontSize); // 简化，实际应使用 Font
    float valueMaxWidth = width - labelMaxWidth - 5.0f;
    
    if (textWidth > valueMaxWidth) {
        UISystem::DrawTextScaled(value, x + width - valueMaxWidth, y, fontSize, valueMaxWidth, WHITE);
    } else {
        UISystem::DrawTextUI(value, x + width - textWidth, y, fontSize, WHITE);
    }
    y += fontSize + 5.0f;
}