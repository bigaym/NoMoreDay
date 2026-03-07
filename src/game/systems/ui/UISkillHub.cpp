#include "game/systems/ui/UISkillHub.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/SkillRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/render/UIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include <string>
#include <cmath>

namespace NoMoreDay {

bool UISkillHub::TrySelectMastery(entt::registry& registry, entt::entity player,
                                  BladeMasteryId masteryId) {
    if (systems::BladeMasteryService::SelectMastery(registry, player, masteryId)) {
        return true;
    }

    auto& state = UISystem::State;
    state.showMessageBox = true;
    std::snprintf(state.messageBoxText, sizeof(state.messageBoxText),
                  "%s", "等级或基础职业不满足职业专精条件");
    state.messageBoxTimer = 2.0f;
    return false;
}

void UISkillHub::Draw(entt::registry& registry, entt::entity player) {
    auto& state = UISystem::State;
    if (state.skillTreeAlpha <= 0.0f) return;

    float alpha = state.skillTreeAlpha;

    // --- Metrics ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 1000.0f * state.scaleFactor;
    float panelH = 700.0f * state.scaleFactor;
    float startX = (screenW - panelW) / 2.0f;
    float startY = (screenH - panelH) / 2.0f;

    // Draw Background
    DrawRectangleRec({startX, startY, panelW, panelH}, Fade(BLACK, 0.9f * alpha));
    DrawRectangleLinesEx({startX, startY, panelW, panelH}, 2.0f, Fade(DARKGRAY, alpha));

    // Draw Title
    UISystem::DrawTextUI("技能专精", startX + 20, startY + 20, 30, WHITE, alpha);
    UISystem::DrawTextUI("左键分配 / 进入天赋树 | 右键取消专精", startX + 200, startY + 28, 16, GRAY, alpha);

    // --- Specialization Slots (Top Row) ---
    auto* active = registry.try_get<ActiveSkillsComponent>(player);
    if (!active) return;

    systems::BladeMasteryService::RefreshPlayerState(registry, player);

    const auto* playerStats = registry.try_get<PlayerStats>(player);
    const int playerLevel = playerStats ? playerStats->level : 1;
    const auto selectedMastery = systems::BladeMasteryService::GetSelectedMastery(registry, player);
    const auto* masteryState = registry.try_get<BladeMasteryComponent>(player);

    float masteryPanelX = startX + 20.0f * state.scaleFactor;
    float masteryPanelY = startY + 56.0f * state.scaleFactor;
    float masteryPanelW = panelW - 40.0f * state.scaleFactor;
    float masteryPanelH = 92.0f * state.scaleFactor;
    DrawRectangleRec({masteryPanelX, masteryPanelY, masteryPanelW, masteryPanelH},
                     Fade(BLACK, 0.45f * alpha));
    DrawRectangleLinesEx({masteryPanelX, masteryPanelY, masteryPanelW, masteryPanelH},
                         1.0f * state.scaleFactor, Fade(DARKGREEN, alpha));
    UISystem::DrawTextUI("职业专精 / Mastery", masteryPanelX + 12, masteryPanelY + 10,
                         18, GOLD, alpha);

    const auto& masteryProfiles = data::BladeMasteryRegistry::Get().GetAllProfiles();
    if (masteryProfiles.empty()) {
        UISystem::DrawTextUI("未加载 Blade Ascendant Mastery 数据。", masteryPanelX + 12,
                             masteryPanelY + 40, 14, LIGHTGRAY, alpha);
    } else {
        const auto& profile = masteryProfiles.front();
        const bool hasBladeProfession =
            systems::BladeMasteryService::HasBladeAscendantProfession(registry, player);
        const bool unlocked = systems::BladeMasteryService::IsMasteryUnlocked(
            registry, player, profile.id);
        const bool selected = (selectedMastery == profile.id);
        const bool debugUnlocked = masteryState != nullptr && masteryState->debug_unlock_active;

        std::string status = selected
                                 ? "已选择"
                                 : unlocked ? (debugUnlocked ? "调试解锁" : "已解锁")
                                            : !hasBladeProfession
                                                  ? "需先在星盘立誓为 Blade Ascendant 主职业"
                                                  : "Lv." + std::to_string(profile.unlock_level) +
                                                        " / Debug Lv." +
                                                        std::to_string(profile.debug_unlock_level_override);

        UISystem::DrawTextUI(profile.name.c_str(), masteryPanelX + 12, masteryPanelY + 34,
                             20, WHITE, alpha);
        UISystem::DrawTextUI(status.c_str(), masteryPanelX + 12, masteryPanelY + 58, 14,
                             unlocked ? GREEN : LIGHTGRAY, alpha);

        char levelBuf[64];
        utils::FormatToBuffer(levelBuf, "当前等级: {}", playerLevel);
        UISystem::DrawTextUI(levelBuf, masteryPanelX + masteryPanelW - 160,
                             masteryPanelY + 12, 14, LIGHTGRAY, alpha);

        Rectangle debugButton = {masteryPanelX + masteryPanelW - 240.0f * state.scaleFactor,
                                 masteryPanelY + 44.0f * state.scaleFactor,
                                 108.0f * state.scaleFactor,
                                 28.0f * state.scaleFactor};
        const bool debugOverrideEnabled =
            systems::BladeMasteryService::IsDebugUnlockOverrideEnabled();
        DrawRectangleRec(debugButton,
                         Fade(debugOverrideEnabled ? DARKGREEN : DARKPURPLE,
                              0.82f * alpha));
        DrawRectangleLinesEx(debugButton, 1.0f * state.scaleFactor,
                             Fade(debugOverrideEnabled ? GREEN : VIOLET, alpha));
        UISystem::DrawTextUI(debugOverrideEnabled ? "Debug Lv5: ON"
                                                  : "Debug Lv5: OFF",
                             debugButton.x + 8, debugButton.y + 6, 12, WHITE,
                             alpha);
        if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), debugButton) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            systems::BladeMasteryService::SetDebugUnlockOverrideEnabled(
                !debugOverrideEnabled);
            systems::BladeMasteryService::RefreshPlayerState(registry, player);
        }

        Rectangle button = {masteryPanelX + masteryPanelW - 120.0f * state.scaleFactor,
                            masteryPanelY + 44.0f * state.scaleFactor,
                            96.0f * state.scaleFactor,
                            28.0f * state.scaleFactor};
        const bool canSelect = unlocked && !selected;
        DrawRectangleRec(button, Fade(canSelect ? DARKBLUE : DARKGRAY, 0.85f * alpha));
        DrawRectangleLinesEx(button, 1.0f * state.scaleFactor,
                             Fade(canSelect ? SKYBLUE : GRAY, alpha));
        UISystem::DrawTextUI(selected ? "已生效"
                                     : canSelect ? "选择"
                                                 : hasBladeProfession ? "未解锁"
                                                                     : "先立誓",
                             button.x + 16, button.y + 6, 14, WHITE, alpha);

        if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), button) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (canSelect) {
                TrySelectMastery(registry, player, profile.id);
            } else if (!selected) {
                TrySelectMastery(registry, player, profile.id);
            }
        }
    }

    float slotSize = 80.0f * state.scaleFactor;
    float slotPadding = 20.0f * state.scaleFactor;
    float slotsStartX = startX + (panelW - (slotSize * 5 + slotPadding * 4)) / 2.0f;
    float slotsStartY = startY + 166.0f * state.scaleFactor;

    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    Texture2D squareTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Square.id);

    for (int i = 0; i < 5; ++i) {
        float x = slotsStartX + i * (slotSize + slotPadding);
        float y = slotsStartY;
        Rectangle slotRect_Logic = {x / state.scaleFactor, y / state.scaleFactor, slotSize / state.scaleFactor, slotSize / state.scaleFactor};

        uint32_t skillId = active->specialized_slots[i].skill_id;
        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), slotRect_Logic);
        
        // Use a consistent frame for both empty and assigned slots
        float scale = state.scaleFactor;
        Rectangle dest_Phys = {slotRect_Logic.x * scale, slotRect_Logic.y * scale, slotRect_Logic.width * scale, slotRect_Logic.height * scale};
        
        DrawRectangleRec(dest_Phys, Fade(BLACK, 0.6f * alpha));
        DrawRectangleLinesEx(dest_Phys, 2.0f * scale, Fade(isHovered ? WHITE : LIGHTGRAY, alpha));

        if (skillId == NoMoreDay::INVALID_SKILL_ID) {
            // Empty slots: Show low-key hint text
            UIRenderer::DrawTextUI(state.globalFont, "Empty", (dest_Phys.x + dest_Phys.width * 0.5f) / scale - 25, (dest_Phys.y + dest_Phys.height * 0.5f) / scale - 10, 16, GRAY, alpha);
        } else {
            // Assigned skills: Draw Icon
            const auto* skill = SkillRegistry::Get().GetSkill(skillId);
            if (skill && skill->icon_id != 0) {
                Texture2D icon = AssetLoadingSystem::GetTexture(skill->icon_id);
                Rectangle iconDest = {dest_Phys.x + 4 * scale, dest_Phys.y + 4 * scale, dest_Phys.width - 8 * scale, dest_Phys.height - 8 * scale};
                DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height}, iconDest, {0, 0}, 0.0f, Fade(WHITE, alpha));
            }
        }

        // Handle Click (Unassign or Open Tree)
        if (isHovered) {
            if (skillId != NoMoreDay::INVALID_SKILL_ID) {
                state.hoveredSkillSlot = -1;
                state.hoveredSkillId = skillId;
            }

            // Drop logic
            if (state.isDraggingSkill && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (state.draggedSkillId == skillId) {
                    // Clicked on existing skill -> Enter Talent Tree
                    state.selectedSkillId = skillId;
                } else {
                    // Check if this skill is already specialized elsewhere
                    bool alreadyInOtherSlot = false;
                    for (int j = 0; j < 5; ++j) {
                        if (active->specialized_slots[j].skill_id == state.draggedSkillId) {
                            alreadyInOtherSlot = true;
                            break;
                        }
                    }

                    if (!alreadyInOtherSlot) {
                        if (active->specialized_slots[i].skill_id != NoMoreDay::INVALID_SKILL_ID) {
                            SkillSystem::ResetTalents(registry, player, active->specialized_slots[i].skill_id);
                        }
                        active->specialized_slots[i].skill_id = state.draggedSkillId;
                        active->specialized_slots[i].allocated_points.clear();
                        LOG_INFO("Assigned skill {} to specialized slot {}", state.draggedSkillId, i);
                    } else {
                        LOG_INFO("Skill {} already specialized", state.draggedSkillId);
                    }
                }
                state.isDraggingSkill = false;
                state.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
            }

            if (skillId != NoMoreDay::INVALID_SKILL_ID) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state.draggedSkillId = skillId;
                    state.isDraggingSkill = true;
                }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                    SkillSystem::ResetTalents(registry, player, active->specialized_slots[i].skill_id);
                    active->specialized_slots[i].skill_id = NoMoreDay::INVALID_SKILL_ID;
                    LOG_INFO("Unassigned skill from slot {}", i);
                }
            }
        }
    }

    // --- Available Skills (Grid) ---
    float gridStartX = startX + 50.0f * state.scaleFactor;
    float gridStartY = slotsStartY + slotSize + 50.0f * state.scaleFactor;
    float gridW = panelW - 100.0f * state.scaleFactor;
    
    UISystem::DrawTextUI("可用技能", gridStartX, gridStartY - 30, 24, LIGHTGRAY, alpha);

    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    int col = 0;
    int row = 0;
    float gridSize = 64.0f * state.scaleFactor;
    
    for (const auto& [id, skill] : allSkills) {
        float x = gridStartX + col * (gridSize + slotPadding);
        float y = gridStartY + row * (gridSize + slotPadding);
        Rectangle skillRect_Logic = {x / state.scaleFactor, y / state.scaleFactor, gridSize / state.scaleFactor, gridSize / state.scaleFactor};

        // Check if already specialized
        bool isSpecialized = false;
        for (const auto& s : active->specialized_slots) {
            if (s.skill_id == id) {
                isSpecialized = true;
                break;
            }
        }

        const bool signatureLocked =
            (id == 10) &&
            !systems::BladeMasteryService::IsSignatureSkillUnlocked(registry, player, id);

        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), skillRect_Logic);

        // Draw simple slot instead of full button texture
        float scale = state.scaleFactor;
        Rectangle dest_Phys = {skillRect_Logic.x * scale, skillRect_Logic.y * scale, skillRect_Logic.width * scale, skillRect_Logic.height * scale};
        
        DrawRectangleRec(dest_Phys, Fade(signatureLocked ? DARKGRAY : BLACK, 0.5f * alpha));
        DrawRectangleLinesEx(dest_Phys, 1.0f * scale, Fade(isHovered ? GOLD : GRAY, alpha));

        // Draw Skill Icon
        Texture2D icon = {0};
        if (skill.icon_id != 0) icon = AssetLoadingSystem::GetTexture(skill.icon_id);
        
        if (icon.id != 0) {
            Rectangle iconDest = {dest_Phys.x + 4 * scale, dest_Phys.y + 4 * scale, dest_Phys.width - 8 * scale, dest_Phys.height - 8 * scale};
            float iconAlpha = isSpecialized ? 0.5f : (signatureLocked ? 0.25f : 1.0f);
            DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height}, iconDest, {0, 0}, 0.0f, Fade(WHITE, iconAlpha * alpha));
        } else {
            UISystem::DrawTextUI(skill.name_key.c_str(), x, y + 10, 14, WHITE, alpha);
        }

        if (signatureLocked) {
            UISystem::DrawTextUI("未解锁", x + 5, y + gridSize - 18, 14, LIGHTGRAY, alpha);
        } else if (isSpecialized) {
            // Draw "Equipped" indicator
            UISystem::DrawTextUI("已专精", x + 5, y + gridSize - 18, 14, GREEN, alpha);
        }

        // Handle Click (Assign)
        if (isHovered) {
            // Tooltip (deferred to top-most overlay render path)
            state.hoveredSkillSlot = -1; // Override hotbar hover
            state.hoveredSkillId = id;

            if (!signatureLocked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.draggedSkillId = id;
                state.isDraggingSkill = true;
                LOG_INFO("Started dragging skill {}", id);
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
    utils::FormatToBuffer(buf, "可用专精点数: {}", active->available_talent_points);
    UISystem::DrawTextUI(buf, startX + panelW - 200, startY + 20, 20, GOLD, alpha);
}

} // namespace NoMoreDay
