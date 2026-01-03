#include "UISkillHub.hpp"
#include "UISystem.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/PlayerState.hpp"
#include "../core/SkillRegistry.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/UIRenderer.hpp"
#include "../tools/Logger.hpp"
#include <string>
#include <cmath>

namespace NoMoreDay {

void UISkillHub::Draw(entt::registry& registry, entt::entity player) {
    auto& state = UISystem::State;
    if (!state.showSkillTree) return;

    // --- Metrics ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 1000.0f * state.scaleFactor;
    float panelH = 700.0f * state.scaleFactor;
    float startX = (screenW - panelW) / 2.0f;
    float startY = (screenH - panelH) / 2.0f;

    // Draw Background
    DrawRectangleRec({startX, startY, panelW, panelH}, Fade(BLACK, 0.9f));
    DrawRectangleLinesEx({startX, startY, panelW, panelH}, 2.0f, DARKGRAY);

    // Draw Title
    UISystem::DrawTextUI("技能专精", startX + 20, startY + 20, 30, WHITE);

    // --- Specialization Slots (Top Row) ---
    auto* active = registry.try_get<ActiveSkillsComponent>(player);
    if (!active) return;

    float slotSize = 80.0f * state.scaleFactor;
    float slotPadding = 20.0f * state.scaleFactor;
    float slotsStartX = startX + (panelW - (slotSize * 5 + slotPadding * 4)) / 2.0f;
    float slotsStartY = startY + 80.0f * state.scaleFactor;

    for (int i = 0; i < 5; ++i) {
        float x = slotsStartX + i * (slotSize + slotPadding);
        float y = slotsStartY;
        Rectangle slotRect = {x, y, slotSize, slotSize};

        // Draw Slot BG
        DrawRectangleRec(slotRect, Fade(DARKGRAY, 0.5f));
        DrawRectangleLinesEx(slotRect, 2.0f, LIGHTGRAY);

        uint32_t skillId = active->specialized_slots[i].skill_id;
        
        // Draw Icon
        if (skillId != 0) {
            const auto* skill = SkillRegistry::Get().GetSkill(skillId);
            if (skill && skill->icon_id != 0) {
                Texture2D icon = AssetLoadingSystem::GetTexture(skill->icon_id);
                DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height}, slotRect, {0, 0}, 0.0f, WHITE);
            }
        } else {
            UISystem::DrawTextUI("Empty", x + 10, y + 30, 16, GRAY);
        }

        // Handle Click (Unassign)
        if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
            DrawRectangleRec(slotRect, Fade(WHITE, 0.2f));
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && skillId != 0) {
                active->specialized_slots[i].skill_id = 0;
                active->specialized_slots[i].allocated_points.clear();
                // Refund points (simplified for now: refund all invested points)
                // TODO: Need to track total invested points per slot to refund correctly
                LOG_INFO("Unassigned skill from slot {}", i);
            }
        }
    }

    // --- Available Skills (Grid) ---
    float gridStartX = startX + 50.0f * state.scaleFactor;
    float gridStartY = slotsStartY + slotSize + 50.0f * state.scaleFactor;
    float gridW = panelW - 100.0f * state.scaleFactor;
    
    UISystem::DrawTextUI("可用技能", gridStartX, gridStartY - 30, 24, LIGHTGRAY);

    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    int col = 0;
    int row = 0;
    float gridSize = 64.0f * state.scaleFactor;
    
    for (const auto& [id, skill] : allSkills) {
        if (id == 0) continue; // Skip placeholder

        float x = gridStartX + col * (gridSize + slotPadding);
        float y = gridStartY + row * (gridSize + slotPadding);
        Rectangle skillRect = {x, y, gridSize, gridSize};

        // Check if already specialized
        bool isSpecialized = false;
        for (const auto& s : active->specialized_slots) {
            if (s.skill_id == id) {
                isSpecialized = true;
                break;
            }
        }

        // Draw Skill Icon
        Texture2D icon = {0};
        if (skill.icon_id != 0) icon = AssetLoadingSystem::GetTexture(skill.icon_id);
        
        if (icon.id != 0) {
            DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height}, skillRect, {0, 0}, 0.0f, isSpecialized ? Fade(WHITE, 0.5f) : WHITE);
        } else {
            DrawRectangleRec(skillRect, DARKGRAY);
            UISystem::DrawTextUI(skill.name_key.c_str(), x, y, 14, WHITE);
        }
        
        DrawRectangleLinesEx(skillRect, 1.0f, GRAY);

        if (isSpecialized) {
            // Draw "Equipped" indicator
            UISystem::DrawTextUI("已专精", x, y + gridSize - 16, 14, GREEN);
        }

        // Handle Click (Assign)
        if (CheckCollisionPointRec(GetMousePosition(), skillRect)) {
            DrawRectangleRec(skillRect, Fade(WHITE, 0.2f));
            
            // Tooltip
            state.hoveredSkillSlot = -1; // Override hotbar hover
            // We need a way to force tooltip render for arbitrary skill ID
            // For now, let's just use the UIRenderer helper directly in the next frame or immediately
            UIRenderer::DrawSkillTooltip(state.globalFont, registry, id, 1.0f, true); // Force Draw

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isSpecialized) {
                // Find first empty slot
                for (auto& s : active->specialized_slots) {
                    if (s.skill_id == 0) {
                        s.skill_id = id;
                        s.allocated_points.clear();
                        LOG_INFO("Assigned skill {} to slot", id);
                        break;
                    }
                }
            }
        }

        col++;
        if (x + gridSize + slotPadding > gridStartX + gridW) {
            col = 0;
            row++;
        }
    }

    // Points Display
    char buf[64];
    snprintf(buf, 64, "可用专精点数: %d", active->available_talent_points);
    UISystem::DrawTextUI(buf, startX + panelW - 200, startY + 20, 20, GOLD);
}

} // namespace NoMoreDay
