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
#include <algorithm>

namespace NoMoreDay {

// Persistent view state for panning and zooming (Logic units)
static Vector2 s_viewOffset = { 0, 0 };
static float s_viewZoom = 1.0f;
static uint32_t s_lastSkillId = 0;
static Vector2 s_lastMouseLogicPos = { 0, 0 };

void UISkillTalentTree::Draw(entt::registry& registry, entt::entity player, uint32_t skillId) {
    auto& state = UISystem::State;
    if (state.skillTreeAlpha <= 0.0f) return;

    float alpha = state.skillTreeAlpha;

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

    // --- Metrics (ALL IN LOGIC UNITS 2560x1440) ---
    float logicW = UI_REF_WIDTH;
    float logicH = UI_REF_HEIGHT;
    float panelW = 1800.0f; 
    float panelH = 1100.0f;
    float startX = (logicW - panelW) / 2.0f;
    float startY = (logicH - panelH) / 2.0f;

    Vector2 mouseLogicPos = UISystem::GetMousePositionLogic();
    float scale = state.scaleFactor;

    // Draw Background (Screen space but logic sized via UIRenderer helpers if possible, or direct)
    // UISystem::DrawRectangleLogic(startX, startY, panelW, panelH, Fade(BLACK, 0.95f * alpha));
    // Since we don't have DrawRectangleLogic, we scale it manually for Raylib calls
    DrawRectangleRec({startX * scale, startY * scale, panelW * scale, panelH * scale}, Fade(BLACK, 0.95f * alpha));
    DrawRectangleLinesEx({startX * scale, startY * scale, panelW * scale, panelH * scale}, 2.0f, Fade(DARKGRAY, alpha));

    // Reset view if skill changed
    if (skillId != s_lastSkillId) {
        s_viewOffset = { 0, 0 }; // Center (0,0)
        s_viewZoom = 1.0f;
        s_lastSkillId = skillId;
    }

    // --- Panning & Zooming Interaction ---
    Rectangle viewBoundsLogic = { startX + 20, startY + 120, panelW - 40, panelH - 140 };
    bool mouseInView = CheckCollisionPointRec(mouseLogicPos, viewBoundsLogic);

    if (mouseInView) {
        // Panning with Right Mouse Button
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            s_viewOffset.x += (mouseLogicPos.x - s_lastMouseLogicPos.x);
            s_viewOffset.y += (mouseLogicPos.y - s_lastMouseLogicPos.y);
        }

        // Zooming with Mouse Wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            s_viewZoom += wheel * 0.1f;
            s_viewZoom = std::clamp(s_viewZoom, 0.4f, 2.5f);
        }
    }
    s_lastMouseLogicPos = mouseLogicPos;

    // Header & Points
    UISystem::DrawTextUI(TextFormat("%s - 专精天赋", skillData->name_key.c_str()), startX + 40, startY + 30, 40, GOLD, alpha);
    UISystem::DrawTextUI(TextFormat("可用点数: %d", active->available_talent_points), startX + 40, startY + 80, 24, WHITE, alpha);
    UISystem::DrawTextUI("右键拖拽平移, 滚轮缩放", startX + panelW - 400, startY + 85, 20, GRAY, alpha * 0.7f);
    
    // Back Button
    Rectangle backRectLogic = {startX + panelW - 150, startY + 30, 120, 50};
    bool backHover = CheckCollisionPointRec(mouseLogicPos, backRectLogic);
    DrawRectangleRec({backRectLogic.x * scale, backRectLogic.y * scale, backRectLogic.width * scale, backRectLogic.height * scale}, Fade(backHover ? GRAY : DARKGRAY, alpha));
    UISystem::DrawTextUI("返回", backRectLogic.x + 35, backRectLogic.y + 12, 22, WHITE, alpha);
    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.selectedSkillId = 0;
        return;
    }

    // --- Scissor Mode for Content ---
    // Scissor needs physical screen coordinates
    BeginScissorMode((int)(viewBoundsLogic.x * scale), (int)(viewBoundsLogic.y * scale), 
                     (int)(viewBoundsLogic.width * scale), (int)(viewBoundsLogic.height * scale));

    float centerX = startX + panelW / 2.0f + s_viewOffset.x;
    float centerY = startY + panelH / 2.0f + s_viewOffset.y;
    float gridStep = 140.0f * s_viewZoom;

    auto GetNodePos = [&](const TalentNode& node) -> Vector2 {
        return { centerX + node.x * gridStep, centerY + node.y * gridStep };
    };

    // --- Draw Connections ---
    for (const auto& [id, node] : tree->nodes) {
        Vector2 start = GetNodePos(node);
        for (uint32_t preId : node.prerequisites) {
            if (tree->nodes.count(preId)) {
                Vector2 end = GetNodePos(tree->nodes.at(preId));
                int prePts = specialized->allocated_points.contains(preId) ? specialized->allocated_points.at(preId) : 0;
                DrawLineEx({start.x * scale, start.y * scale}, {end.x * scale, end.y * scale}, 4.0f * s_viewZoom * scale, Fade(prePts > 0 ? GOLD : DARKGRAY, alpha * 0.6f));
            }
        }
    }

    // --- Draw Nodes ---
    float nodeRadius = 45.0f * s_viewZoom;
    uint32_t targetNodeId = 0;

    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = GetNodePos(node);
        
        // Culling (Logic space)
        if (pos.x < viewBoundsLogic.x - nodeRadius || pos.x > viewBoundsLogic.x + viewBoundsLogic.width + nodeRadius ||
            pos.y < viewBoundsLogic.y - nodeRadius || pos.y > viewBoundsLogic.y + viewBoundsLogic.height + nodeRadius) {
            continue;
        }

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
        if (currentPts > 0) nodeColor = isMaxed ? GOLD : YELLOW;
        else if (canUnlock) nodeColor = GRAY;

        bool hovered = CheckCollisionPointCircle(mouseLogicPos, pos, nodeRadius);
        if (hovered && mouseInView) {
            nodeColor = ColorBrightness(nodeColor, 0.3f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && canUnlock && !isMaxed && active->available_talent_points > 0) {
                targetNodeId = id; 
            }
        }

        // Physical Position for Raylib primitives
        Vector2 physPos = { pos.x * scale, pos.y * scale };
        float physRadius = nodeRadius * scale;

        DrawCircleV(physPos, physRadius, Fade(BLACK, 0.9f * alpha));
        DrawCircleLinesV(physPos, physRadius, Fade(nodeColor, alpha));
        
        if (currentPts > 0) {
            float pct = (float)currentPts / node.max_points;
            DrawCircleSector(physPos, physRadius * 0.85f, 0, 360.0f * pct, 32, Fade(nodeColor, 0.4f * alpha));
        }

        // Point Text
        int fontSize = (int)(20 * s_viewZoom);
        const char* ptsText = TextFormat("%d/%d", currentPts, node.max_points);
        UISystem::DrawTextUI(ptsText, pos.x - 15 * s_viewZoom, pos.y - 10 * s_viewZoom, (float)fontSize, WHITE, alpha);
    }

    EndScissorMode();

    // Interaction
    if (targetNodeId != 0) {
        SkillSystem::AddTalentPoint(registry, player, skillId, targetNodeId);
    }

    // --- Tooltips (Logic Space) ---
    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = GetNodePos(node);
        if (CheckCollisionPointCircle(mouseLogicPos, pos, nodeRadius) && mouseInView) {
            float tx = mouseLogicPos.x + 30;
            float ty = mouseLogicPos.y + 30;
            float tw = 400;
            float th = 180;
            
            if (tx + tw > logicW) tx -= (tw + 60);
            if (ty + th > logicH) ty -= (th + 60);

            // Draw Tooltip Box
            DrawRectangleRec({tx * scale, ty * scale, tw * scale, th * scale}, Fade(BLACK, 0.95f * alpha));
            DrawRectangleLinesEx({tx * scale, ty * scale, tw * scale, th * scale}, 1.0f, Fade(GOLD, alpha));
            
            UISystem::DrawTextUI(node.name_key.c_str(), tx + 20, ty + 20, 28, GOLD, alpha);
            UISystem::DrawTextScaled(node.desc_key.c_str(), tx + 20, ty + 60, 20, tw - 40, WHITE, alpha);

            if (!node.stat_modifiers.empty()) {
                UISystem::DrawTextUI("数值加成已启用", tx + 20, ty + th - 35, 18, SKYBLUE, alpha * 0.8f);
            }
        }
    }
}

} // namespace NoMoreDay
