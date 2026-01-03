#include "UISkillTalentTree.hpp"
#include "UISystem.hpp"
#include "UISkillHub.hpp"
#include "../components/SkillSystem.hpp"
#include "../core/SkillRegistry.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/UIRenderer.hpp"
#include "SkillSystem.hpp"
#include "../tools/Logger.hpp"
#include "raymath.h"
#include <string>

namespace NoMoreDay {

void UISkillTalentTree::Draw(entt::registry& registry, entt::entity player, uint32_t skillId) {
    auto& state = UISystem::State;
    const auto* skillData = SkillRegistry::Get().GetSkill(skillId);
    const auto* tree = SkillRegistry::Get().GetSkillTree(skillId);
    if (!skillData || !tree) return;

    auto* active = registry.try_get<ActiveSkillsComponent>(player);
    if (!active) return;

    const SpecializedSkill* specialized = nullptr;
    for (const auto& s : active->specialized_slots) {
        if (s.skill_id == skillId) {
            specialized = &s;
            break;
        }
    }
    if (!specialized) return;

    // --- Metrics ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 1200.0f * state.scaleFactor;
    float panelH = 800.0f * state.scaleFactor;
    float startX = (screenW - panelW) / 2.0f;
    float startY = (screenH - panelH) / 2.0f;

    // Draw Background
    DrawRectangleRec({startX, startY, panelW, panelH}, Fade(BLACK, 0.95f));
    DrawRectangleLinesEx({startX, startY, panelW, panelH}, 2.0f, DARKGRAY);

    // Header
    UISystem::DrawTextUI(TextFormat("%s - 专精天赋", skillData->name_key.c_str()), startX + 20, startY + 20, 30, GOLD);
    
    // Back Button
    Rectangle backRect = {startX + panelW - 120, startY + 20, 100, 40};
    bool backHover = CheckCollisionPointRec(GetMousePosition(), backRect);
    DrawRectangleRec(backRect, backHover ? GRAY : DARKGRAY);
    UISystem::DrawTextUI("返回 (Hub)", backRect.x + 10, backRect.y + 10, 18, WHITE);
    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.selectedSkillId = 0;
        return;
    }

    // Points
    UISystem::DrawTextUI(TextFormat("可用点数: %d", active->available_talent_points), startX + 20, startY + 60, 20, WHITE);

    // --- Draw Connections ---
    float centerX = startX + panelW / 2.0f;
    float centerY = startY + panelH / 2.0f + 50.0f * state.scaleFactor;
    float gridStep = 100.0f * state.scaleFactor;

    auto GetNodePos = [&](const TalentNode& node) -> Vector2 {
        return { centerX + node.x * gridStep, centerY + node.y * gridStep };
    };

    for (const auto& [id, node] : tree->nodes) {
        Vector2 start = GetNodePos(node);
        for (uint32_t preId : node.prerequisites) {
            if (tree->nodes.count(preId)) {
                Vector2 end = GetNodePos(tree->nodes.at(preId));
                int prePts = specialized->allocated_points.contains(preId) ? specialized->allocated_points.at(preId) : 0;
                DrawLineEx(start, end, 3.0f, prePts > 0 ? GOLD : DARKGRAY);
            }
        }
    }

    // --- Draw Nodes ---
    float nodeRadius = 30.0f * state.scaleFactor;
    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = GetNodePos(node);
        int currentPts = specialized->allocated_points.contains(id) ? specialized->allocated_points.at(id) : 0;
        bool isMaxed = currentPts >= node.max_points;
        
        bool canUnlock = true;
        for (uint32_t preId : node.prerequisites) {
            int prePts = specialized->allocated_points.contains(preId) ? specialized->allocated_points.at(preId) : 0;
            if (prePts <= 0) {
                canUnlock = false;
                break;
            }
        }

        Color nodeColor = DARKGRAY;
        if (currentPts > 0) nodeColor = GOLD;
        else if (canUnlock) nodeColor = GRAY;

        bool hovered = CheckCollisionPointCircle(GetMousePosition(), pos, nodeRadius);
        if (hovered) {
            nodeColor = ColorBrightness(nodeColor, 0.3f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && canUnlock && !isMaxed && active->available_talent_points > 0) {
                SkillSystem::AddTalentPoint(registry, player, skillId, id);
            }
        }

        DrawCircleV(pos, nodeRadius, Fade(BLACK, 0.8f));
        DrawCircleLinesV(pos, nodeRadius, nodeColor);
        if (currentPts > 0) {
            DrawCircleV(pos, nodeRadius * 0.8f, Fade(nodeColor, 0.3f));
        }

        // Icon (Placeholder)
        UISystem::DrawTextUI(TextFormat("%d/%d", currentPts, node.max_points), pos.x - 15, pos.y - 8, 14, WHITE);

        // Tooltip
        if (hovered) {
            // Simple Node Tooltip
            float tx = GetMousePosition().x + 20;
            float ty = GetMousePosition().y + 20;
            DrawRectangle(tx, ty, 200, 100, Fade(BLACK, 0.9f));
            DrawRectangleLines(tx, ty, 200, 100, nodeColor);
            UISystem::DrawTextUI(node.name_key.c_str(), tx + 10, ty + 10, 18, GOLD);
            UISystem::DrawTextUI(node.desc_key.c_str(), tx + 10, ty + 35, 14, WHITE);
            
            // Draw Modifiers
            float curMY = ty + 60;
            for (const auto& mod : node.stat_modifiers) {
                // UISystem::DrawTextUI(...)
            }
        }
    }
}

} // namespace NoMoreDay
