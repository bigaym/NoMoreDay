#include "game/systems/ui/UISkillTalentTree.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UISkillHub.hpp"
#include "game/systems/ui/UISkillSpecRenderer.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include <string>
#include <algorithm>

// Redefine static members to match header
// struct SkillTreeUI_Vec2 { float x, y; };

namespace NoMoreDay {

SkillTreeUI::Vec2 SkillTreeUI::s_viewOffset = { 0, 0 };
float SkillTreeUI::s_viewZoom = 1.0f;
uint32_t SkillTreeUI::s_lastSkillId = 0;
SkillTreeUI::Vec2 SkillTreeUI::s_lastMouseLogicPos = { 0, 0 };

namespace {

const char* NodeRoleToText(SpecNodeRole role) {
    switch (role) {
    case SpecNodeRole::Keystone: return "Keystone";
    case SpecNodeRole::Trigger: return "Trigger";
    case SpecNodeRole::Synergy: return "Synergy";
    case SpecNodeRole::Transmuter: return "Transmuter";
    default: return "Passive";
    }
}

const char* ScopePolicyToText(ScopePolicy scope) {
    switch (scope) {
    case ScopePolicy::GlobalAlways: return "GlobalAlways";
    case ScopePolicy::GlobalWhileBuffActive: return "GlobalWhileBuffActive";
    default: return "SkillOnly";
    }
}

} // namespace

void SkillTreeUI::Draw(void* registryVoid, int playerEntity, uint32_t skillId) {
    entt::registry& registry = *static_cast<entt::registry*>(registryVoid);
    entt::entity player = (entt::entity)playerEntity;

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

    // Draw Background
    DrawRectangleRec({startX * scale, startY * scale, panelW * scale, panelH * scale}, Fade(BLACK, 0.95f * alpha));
    DrawRectangleLinesEx({startX * scale, startY * scale, panelW * scale, panelH * scale}, 2.0f, Fade(DARKGRAY, alpha));

    // Reset view if skill changed
    if (skillId != SkillTreeUI::s_lastSkillId) {
        SkillTreeUI::s_viewOffset = { 0, 0 }; // Center (0,0)
        SkillTreeUI::s_viewZoom = 1.0f;
        SkillTreeUI::s_lastSkillId = skillId;
    }

    // --- Panning & Zooming Interaction ---
    Rectangle viewBoundsLogic = { startX + 20, startY + 120, panelW - 40, panelH - 140 };
    bool mouseInView = CheckCollisionPointRec(mouseLogicPos, viewBoundsLogic);

    if (mouseInView) {
        // Panning with Right Mouse Button
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            SkillTreeUI::s_viewOffset.x += (mouseLogicPos.x - SkillTreeUI::s_lastMouseLogicPos.x);
            SkillTreeUI::s_viewOffset.y += (mouseLogicPos.y - SkillTreeUI::s_lastMouseLogicPos.y);
        }

        // Zooming with Mouse Wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            SkillTreeUI::s_viewZoom += wheel * 0.1f;
            SkillTreeUI::s_viewZoom = std::clamp(SkillTreeUI::s_viewZoom, 0.4f, 2.5f);
        }
    }
    SkillTreeUI::s_lastMouseLogicPos.x = mouseLogicPos.x;
    SkillTreeUI::s_lastMouseLogicPos.y = mouseLogicPos.y;

    // Header & Points
    UISystem::DrawTextUI(TextFormat("%s - 专精天赋", skillData->name_key.c_str()), startX + 40, startY + 30, 40, GOLD, alpha);
    UISystem::DrawTextUI(TextFormat("可用点数: %d", active->available_talent_points), startX + 40, startY + 80, 24, WHITE, alpha);
    
    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

    // Reset Button
    Rectangle resetRectLogic = {startX + 250, startY + 75, 120, 40};
    bool resetHover = CheckCollisionPointRec(mouseLogicPos, resetRectLogic);
    
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, resetRectLogic, "重置天赋", 20, WHITE, resetHover ? RED : MAROON, resetHover, resetHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (resetHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        SkillSystem::ResetTalents(registry, player, skillId);
    }

    UISystem::DrawTextUI("右键拖拽平移, 滚轮缩放", startX + panelW - 400, startY + 85, 20, GRAY, alpha * 0.7f);
    
    // Back Button
    Rectangle backRectLogic = {startX + panelW - 150, startY + 30, 120, 50};
    bool backHover = CheckCollisionPointRec(mouseLogicPos, backRectLogic);
    
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, backRectLogic, "返回", 22, WHITE, WHITE, backHover, backHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), alpha);

    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.selectedSkillId = 0;
        return;
    }

    // --- Scissor Mode & Render Content ---
    BeginScissorMode((int)(viewBoundsLogic.x * scale), (int)(viewBoundsLogic.y * scale), 
                     (int)(viewBoundsLogic.width * scale), (int)(viewBoundsLogic.height * scale));

    // SkillSpecView view setup
    SkillSpecView view;
    float centerX = startX + panelW / 2.0f;
    float centerY = startY + panelH / 2.0f;
    
    view.center = { centerX * scale, centerY * scale };
    view.offset = { SkillTreeUI::s_viewOffset.x * scale, SkillTreeUI::s_viewOffset.y * scale };
    view.zoom = SkillTreeUI::s_viewZoom * scale;
    view.alpha = alpha;  

    // --- Interaction Logic (Pre-Calculation) ---
    Vector2 mousePixelPos = GetMousePosition(); // Raylib raw mouse pos
    uint32_t targetNodeId = 0;
    uint32_t hoveredNodeId = 0;
    const TalentNode* hoveredNode = nullptr;

    for (const auto& [id, node] : tree->nodes) {
        Vector2 pos = UISkillSpecRenderer::GetNodeScreenPos(node, view);
        float r = UISkillSpecRenderer::GetNodeRadius(node, view);

        if (CheckCollisionPointCircle(mousePixelPos, pos, r) && mouseInView) {
            hoveredNodeId = id;
            hoveredNode = &node;

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Logic check
                int currentPts = specialized->allocated_points.contains(id) ? specialized->allocated_points.at(id) : 0;
                bool isMaxed = currentPts >= node.max_points;
                
                // Prereq check
                bool canUnlock = true;
                for (uint32_t preId : node.prerequisites) {
                    int prePts = specialized->allocated_points.contains(preId) ? specialized->allocated_points.at(preId) : 0;
                    if (prePts <= 0) {
                        canUnlock = false; 
                        break;
                    }
                }
                
                if (canUnlock && !isMaxed && active->available_talent_points > 0) {
                     targetNodeId = id;
                }
            }
            // Only handle one hover
            break; 
        }
    }

    // --- Scissor Mode & Render Content ---
    BeginScissorMode((int)(viewBoundsLogic.x * scale), (int)(viewBoundsLogic.y * scale), 
                     (int)(viewBoundsLogic.width * scale), (int)(viewBoundsLogic.height * scale));

    UISkillSpecRenderer::Draw(tree, specialized, active, skillData, view, hoveredNodeId);

    EndScissorMode();

    // --- Tooltip & Actions ---
    if (hoveredNode) {
        const NodeContractData* nodeContract = SkillRegistry::Get().GetNodeContract(skillId, hoveredNodeId);
        // Tooltip
        float tx = mouseLogicPos.x + 30;
        float ty = mouseLogicPos.y + 30;
        float tw = 400;
        float th = 220;
        
        if (tx + tw > logicW) tx -= (tw + 60);
        if (ty + th > logicH) ty -= (th + 60);

        // Draw Tooltip Box
        DrawRectangleRec({tx * scale, ty * scale, tw * scale, th * scale}, Fade(BLACK, 0.95f * alpha));
        DrawRectangleLinesEx({tx * scale, ty * scale, tw * scale, th * scale}, 1.0f, Fade(GOLD, alpha));
        
        UISystem::DrawTextUI(hoveredNode->name_key.c_str(), tx + 20, ty + 20, 28, GOLD, alpha);
        UISystem::DrawTextScaled(hoveredNode->desc_key.c_str(), tx + 20, ty + 60, 20, tw - 40, WHITE, alpha);

         if (!hoveredNode->stat_modifiers.empty()) {
            UISystem::DrawTextUI("数值加成已启用", tx + 20, ty + th - 35, 18, SKYBLUE, alpha * 0.8f);
        }
        if (nodeContract) {
            UISystem::DrawTextUI(TextFormat("Role: %s", NodeRoleToText(nodeContract->role)),
                                 tx + 20, ty + th - 70, 18, ORANGE, alpha * 0.9f);
            UISystem::DrawTextUI(TextFormat("Scope: %s", ScopePolicyToText(nodeContract->scope_policy)),
                                 tx + 20, ty + th - 48, 18, SKYBLUE, alpha * 0.9f);
        }
    }

    if (targetNodeId != 0) {
        SkillSystem::AddTalentPoint(registry, player, skillId, targetNodeId);
    }
}

} // namespace NoMoreDay
