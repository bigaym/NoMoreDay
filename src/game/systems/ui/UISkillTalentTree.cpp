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
#include <vector>

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
    case SpecNodeRole::Keystone: return "核心";
    case SpecNodeRole::Trigger: return "触发";
    case SpecNodeRole::Synergy: return "联动";
    case SpecNodeRole::Transmuter: return "转化";
    default: return "被动";
    }
}

const char* ScopePolicyToText(ScopePolicy scope) {
    switch (scope) {
    case ScopePolicy::GlobalAlways: return "全局常驻";
    case ScopePolicy::GlobalWhileBuffActive: return "增益期间全局";
    default: return "仅本技能";
    }
}

bool IsPrerequisiteSatisfiedOr(const TalentNode& node, const SkillTreeDefinition& tree,
                               const SpecializedSkill& specialized) {
    if (node.prerequisites.empty()) {
        return true;
    }

    bool hasValidPrereq = false;
    for (uint32_t preId : node.prerequisites) {
        if (preId == 0 || !tree.nodes.contains(preId)) {
            continue;
        }
        hasValidPrereq = true;
        int prePts = specialized.allocated_points.contains(preId) ? specialized.allocated_points.at(preId) : 0;
        if (prePts > 0) {
            return true;
        }
    }
    return !hasValidPrereq;
}

void DrawWrappedTextUI(const Font& font, const char* text, float x, float y, float width, float height,
                       float fontSize, Color color, float alpha, float scale) {
    if (!text || text[0] == '\0' || width <= 0.0f || height <= 0.0f) {
        return;
    }

    const float sX = x * scale;
    const float sY = y * scale;
    const float sWidth = width * scale;
    const float sMaxY = (y + height) * scale;
    const float sFont = fontSize * scale;
    const float sSpacing = 1.0f * scale;
    const float sLineH = sFont + 4.0f * scale;

    std::vector<std::string> lines;
    std::string currentLine;
    const char* ptr = text;

    while (*ptr != '\0') {
        int bytes = 0;
        int cp = GetCodepoint(ptr, &bytes);
        if (bytes <= 0) {
            break;
        }

        if (cp == '\r') {
            ptr += bytes;
            continue;
        }
        if (cp == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
            ptr += bytes;
            continue;
        }

        int glyphBytes = 0;
        const char* glyphPtr = CodepointToUTF8(cp, &glyphBytes);
        std::string glyph(glyphPtr, glyphBytes);
        std::string testLine = currentLine + glyph;

        const float testW = IsFontValid(font)
            ? MeasureTextEx(font, testLine.c_str(), sFont, sSpacing).x
            : static_cast<float>(MeasureText(testLine.c_str(), static_cast<int>(sFont)));

        if (testW <= sWidth || currentLine.empty()) {
            currentLine = std::move(testLine);
        } else {
            lines.push_back(currentLine);
            currentLine = std::move(glyph);
        }

        ptr += bytes;
    }

    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    float drawY = sY;
    const Color finalColor = Fade(color, alpha);
    for (const auto& line : lines) {
        if (drawY + sLineH > sMaxY) {
            break;
        }
        if (IsFontValid(font)) {
            DrawTextEx(font, line.c_str(), {sX, drawY}, sFont, sSpacing, finalColor);
        } else {
            DrawText(line.c_str(), static_cast<int>(sX), static_cast<int>(drawY),
                     static_cast<int>(sFont), finalColor);
        }
        drawY += sLineH;
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
                bool canUnlock = IsPrerequisiteSatisfiedOr(node, *tree, *specialized);
                
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
        float tw = 440;
        float th = 280;
        
        if (tx + tw > logicW) tx -= (tw + 60);
        if (ty + th > logicH) ty -= (th + 60);

        // Draw Tooltip Box
        DrawRectangleRec({tx * scale, ty * scale, tw * scale, th * scale}, Fade(BLACK, 0.95f * alpha));
        DrawRectangleLinesEx({tx * scale, ty * scale, tw * scale, th * scale}, 1.0f, Fade(GOLD, alpha));
        
        UISystem::DrawTextUI(hoveredNode->name_key.c_str(), tx + 20, ty + 20, 28, GOLD, alpha);
        
        std::vector<std::pair<std::string, Color>> footerLines;
        if (nodeContract) {
            footerLines.emplace_back(TextFormat("定位: %s", NodeRoleToText(nodeContract->role)), ORANGE);
            footerLines.emplace_back(TextFormat("范围: %s", ScopePolicyToText(nodeContract->scope_policy)), SKYBLUE);
        }
        if (!hoveredNode->stat_modifiers.empty()) {
            footerLines.emplace_back("数值加成已启用", SKYBLUE);
        }

        const float footerFont = 22.0f;
        const float footerLineH = 24.0f;
        const float footerBottomPad = 16.0f;
        const float footerTopPad = 10.0f;
        const float footerBlockH = footerLines.empty() ? 0.0f :
            (footerTopPad + footerLineH * static_cast<float>(footerLines.size()));

        const float descX = tx + 20.0f;
        const float descY = ty + 64.0f;
        const float descW = tw - 40.0f;
        const float descH = std::max(56.0f, th - (descY - ty) - footerBlockH - footerBottomPad);
        DrawWrappedTextUI(UISystem::GetFont(), hoveredNode->desc_key.c_str(), descX, descY, descW, descH, 20.0f, WHITE, alpha, scale);

        if (!footerLines.empty()) {
            float lineY = ty + th - footerBottomPad - footerLineH * static_cast<float>(footerLines.size());
            for (const auto& line : footerLines) {
                UISystem::DrawTextUI(line.first.c_str(), tx + 20.0f, lineY, footerFont, line.second, alpha * 0.9f);
                lineY += footerLineH;
            }
        }
    }

    if (targetNodeId != 0) {
        SkillSystem::AddTalentPoint(registry, player, skillId, targetNodeId);
    }
}

} // namespace NoMoreDay
