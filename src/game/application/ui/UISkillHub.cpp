#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/BladeMasteryUITheme.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include <array>
#include <string>
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

bool UISkillHub::TrySelectMastery(entt::registry& registry, entt::entity player,
                                  BladeMasteryId masteryId) {
    if (systems::BladeMasteryService::SelectMastery(registry, player, masteryId)) {
        return true;
    }

    // U8: the "requirement not met" message box routes through the host
    // channel (was the State.showMessageBox/messageBoxText/messageBoxTimer
    // write). Headless tests that exercise this path bind a host.
    if (m_uiHost != nullptr) {
        m_uiHost->ShowMessageBox("等级或基础职业不满足职业专精条件");
    }
    return false;
}

void UISkillHub::Draw(entt::registry& registry, entt::entity player,
                      float alpha) {
    if (alpha <= 0.0f) return;

    // U8: the skill drag/hover session is host-owned (single instance across
    // panels); the hub routes its writes through the host channel when the
    // composition root is present (headless tests skip them).
    NoMoreDay::ui::GameUiHost* uiHost = m_uiHost;

    const float scaleFactor = UISystem::GetScaleFactor();

    // --- Metrics ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 1000.0f * scaleFactor;
    float panelH = 700.0f * scaleFactor;
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

    const auto* playerStats = registry.try_get<PlayerStats>(player);
    const int playerLevel = playerStats ? playerStats->level : 1;
    const auto selectedMastery = systems::BladeMasteryService::GetSelectedMastery(registry, player);
    const auto* masteryState = registry.try_get<BladeMasteryComponent>(player);
    const bool showHeavenlySwordAttunementControls =
        selectedMastery == BladeMasteryId::HeavenlySword && masteryState != nullptr;

    float masteryPanelX = startX + 20.0f * scaleFactor;
    float masteryPanelY = startY + 56.0f * scaleFactor;
    float masteryPanelW = panelW - 40.0f * scaleFactor;
    float masteryPanelH =
        (showHeavenlySwordAttunementControls ? 132.0f : 92.0f) * scaleFactor;
    DrawRectangleRec({masteryPanelX, masteryPanelY, masteryPanelW, masteryPanelH},
                     Fade(BLACK, 0.45f * alpha));
    DrawRectangleLinesEx({masteryPanelX, masteryPanelY, masteryPanelW, masteryPanelH},
                         1.0f * scaleFactor, Fade(DARKGREEN, alpha));
    UISystem::DrawTextUI("职业专精 / Mastery", masteryPanelX + 12, masteryPanelY + 10,
                         18, GOLD, alpha);

    const auto& masteryProfiles = data::BladeMasteryRegistry::Get().GetAllProfiles();
    if (masteryProfiles.empty()) {
        UISystem::DrawTextUI("未加载 Blade Ascendant Mastery 数据。", masteryPanelX + 12,
                             masteryPanelY + 40, 14, LIGHTGRAY, alpha);
    } else {
        const bool hasBladeProfession =
            systems::BladeMasteryService::HasBladeAscendantProfession(registry, player);
        const bool debugOverrideEnabled =
            systems::BladeMasteryService::IsDebugUnlockOverrideEnabled();

        const float cardGap = 10.0f * scaleFactor;
        const float cardW = (masteryPanelW - cardGap * 2.0f) / 3.0f;
        const float cardH = 48.0f * scaleFactor;

        for (std::size_t index = 0; index < masteryProfiles.size(); ++index) {
            const auto& profile = masteryProfiles[index];
            const BladeMasteryUIThemeProfile& theme =
                GetBladeMasteryUIThemeProfile(profile.id);
            const bool unlocked = systems::BladeMasteryService::IsMasteryUnlocked(
                registry, player, profile.id);
            const bool selected = (selectedMastery == profile.id);
            const bool debugUnlocked = debugOverrideEnabled &&
                playerLevel < profile.unlock_level &&
                playerLevel >= profile.debug_unlock_level_override;

            const float cardX = masteryPanelX + index * (cardW + cardGap);
            const float cardY = masteryPanelY + 34.0f * scaleFactor;
            const Rectangle card = {cardX, cardY, cardW, cardH};
            const Rectangle button = {card.x + card.width - 82.0f * scaleFactor,
                                      card.y + 10.0f * scaleFactor,
                                      70.0f * scaleFactor,
                                      24.0f * scaleFactor};

            const Color cardFill = selected ? theme.secondary : Color{16, 18, 24, 255};
            const Color cardTint = selected ? theme.primary : theme.secondary;
            DrawRectangleRec(card, Fade(cardFill, (selected ? 0.48f : 0.34f) * alpha));
            DrawRectangleLinesEx(card, 1.2f * scaleFactor,
                                 Fade(selected ? theme.highlight : cardTint, alpha));

            switch (theme.background_pattern) {
            case BladeMasteryBackgroundPattern::DiagonalCuts:
                DrawLineEx({card.x + 8.0f * scaleFactor, card.y + card.height - 8.0f * scaleFactor},
                           {card.x + card.width - 92.0f * scaleFactor, card.y + 8.0f * scaleFactor},
                           2.0f * scaleFactor, Fade(theme.highlight, 0.22f * alpha));
                break;
            case BladeMasteryBackgroundPattern::OrbitArcs:
                DrawRingLines({card.x + card.width - 110.0f * scaleFactor, card.y + card.height * 0.5f},
                              8.0f * scaleFactor, 18.0f * scaleFactor,
                              220.0f, 20.0f, 18, Fade(theme.highlight, 0.24f * alpha));
                break;
            case BladeMasteryBackgroundPattern::BrokenPlate:
                DrawLineEx({card.x + 14.0f * scaleFactor, card.y + card.height * 0.38f},
                           {card.x + 58.0f * scaleFactor, card.y + card.height * 0.62f},
                           2.0f * scaleFactor, Fade(theme.danger, 0.20f * alpha));
                break;
            case BladeMasteryBackgroundPattern::None:
            default:
                break;
            }

            std::string status = selected
                                      ? "已选择"
                                      : unlocked ? (debugUnlocked ? "调试解锁" : "已解锁")
                                                 : !hasBladeProfession
                                                      ? "需先立誓"
                                                      : "Lv." + std::to_string(profile.unlock_level) +
                                                            " / Debug Lv." +
                                                            std::to_string(profile.debug_unlock_level_override);

            UISystem::DrawTextUI(profile.name.c_str(), card.x + 10, card.y + 8,
                                 18, selected ? theme.highlight : WHITE, alpha);
            UISystem::DrawTextUI(status.c_str(), card.x + 10, card.y + 28, 12,
                                 unlocked ? theme.highlight : LIGHTGRAY, alpha);

            const bool canSelect = unlocked && !selected;
            DrawRectangleRec(button, Fade(canSelect ? theme.primary : DARKGRAY, 0.72f * alpha));
            DrawRectangleLinesEx(button, 1.0f * scaleFactor,
                                 Fade(canSelect ? theme.highlight : GRAY, alpha));
            UISystem::DrawTextUI(selected ? "已生效"
                                          : canSelect ? "选择"
                                                      : hasBladeProfession ? "未解锁"
                                                                         : "先立誓",
                                 button.x + 10, button.y + 5, 12, WHITE, alpha);

            if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), button) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                TrySelectMastery(registry, player, profile.id);
            }
        }

        char levelBuf[64];
        utils::FormatToBuffer(levelBuf, "当前等级: {}", playerLevel);
        UISystem::DrawTextUI(levelBuf, masteryPanelX + masteryPanelW - 160,
                             masteryPanelY + 12, 14, LIGHTGRAY, alpha);

        Rectangle debugButton = {masteryPanelX + masteryPanelW - 240.0f * scaleFactor,
                                  masteryPanelY + 44.0f * scaleFactor,
                                  108.0f * scaleFactor,
                                  28.0f * scaleFactor};
        DrawRectangleRec(debugButton,
                         Fade(debugOverrideEnabled ? DARKGREEN : DARKPURPLE,
                               0.82f * alpha));
        DrawRectangleLinesEx(debugButton, 1.0f * scaleFactor,
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

        if (showHeavenlySwordAttunementControls) {
            UISystem::DrawTextUI("Heavenly Sword Attunement", masteryPanelX + 12,
                                 masteryPanelY + 82.0f * scaleFactor, 14,
                                 LIGHTGRAY, alpha);

            constexpr std::array<std::pair<BladeAttunement, const char*>, 3> attunements = {{
                {BladeAttunement::Lightning, "Lightning"},
                {BladeAttunement::Frost, "Frost"},
                {BladeAttunement::Fire, "Fire"},
            }};
            const float logicalButtonW = 92.0f;
            const float logicalButtonH = 24.0f;
            const float logicalButtonGap = 8.0f;
            const float logicalStartX = (masteryPanelX + 12.0f * scaleFactor) / scaleFactor;
            const float logicalStartY = (masteryPanelY + 100.0f * scaleFactor) / scaleFactor;

             for (std::size_t index = 0; index < attunements.size(); ++index) {
                 const auto [attunement, label] = attunements[index];
                 Rectangle buttonLogic = {
                     logicalStartX + index * (logicalButtonW + logicalButtonGap),
                     logicalStartY,
                     logicalButtonW,
                     logicalButtonH,
                 };
                 Rectangle buttonPhys = {
                     buttonLogic.x * scaleFactor,
                     buttonLogic.y * scaleFactor,
                     buttonLogic.width * scaleFactor,
                     buttonLogic.height * scaleFactor,
                 };
                 const bool isSelected = masteryState->heavenly_attunement == attunement;

                  DrawRectangleRec(buttonPhys, Fade(isSelected ? GOLD : DARKGRAY,
                                                0.72f * alpha));
                 DrawRectangleLinesEx(buttonPhys, 1.0f * scaleFactor,
                                      Fade(isSelected ? YELLOW : GRAY, alpha));
                 UISystem::DrawTextUI(label, buttonPhys.x + 9.0f * scaleFactor,
                                      buttonPhys.y + 5.0f * scaleFactor, 12,
                                      WHITE, alpha);

                  if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(), buttonLogic) &&
                      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                      systems::BladeMasteryService::SetHeavenlySwordAttunement(
                          registry, player, attunement);
                  }
            }
        }
    }

    float slotSize = 80.0f * scaleFactor;
    float slotPadding = 20.0f * scaleFactor;
    float slotsStartX = startX + (panelW - (slotSize * 5 + slotPadding * 4)) / 2.0f;
    float slotsStartY = masteryPanelY + masteryPanelH + 18.0f * scaleFactor;

    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    Texture2D squareTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Square.id);

    for (int i = 0; i < 5; ++i) {
        float x = slotsStartX + i * (slotSize + slotPadding);
        float y = slotsStartY;
        Rectangle slotRect_Logic = {x / scaleFactor, y / scaleFactor, slotSize / scaleFactor, slotSize / scaleFactor};

        uint32_t skillId = active->specialized_slots[i].skill_id;
        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), slotRect_Logic);
        
        // Use a consistent frame for both empty and assigned slots
        float scale = scaleFactor;
        Rectangle dest_Phys = {slotRect_Logic.x * scale, slotRect_Logic.y * scale, slotRect_Logic.width * scale, slotRect_Logic.height * scale};
        
        DrawRectangleRec(dest_Phys, Fade(BLACK, 0.6f * alpha));
        DrawRectangleLinesEx(dest_Phys, 2.0f * scale, Fade(isHovered ? WHITE : LIGHTGRAY, alpha));

        if (skillId == NoMoreDay::INVALID_SKILL_ID) {
            // Empty slots: Show low-key hint text
            UIRenderer::DrawTextUI(UISystem::GetFont(), "Empty", (dest_Phys.x + dest_Phys.width * 0.5f) / scale - 25, (dest_Phys.y + dest_Phys.height * 0.5f) / scale - 10, 16, GRAY, alpha);
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
                // U8: the hover write routes through the host channel to the
                // tooltip controller's hover source.
                if (uiHost) {
                    uiHost->SetHoveredSkillId(skillId);
                }
            }

            // Drop logic (U8: the skill drag session is host-owned; skipped
            // when the composition root is absent, e.g. headless tests).
            if (uiHost) {
                UIDragSession& drag = uiHost->DragSession();
                if (drag.isDraggingSkill &&
                    IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    if (drag.draggedSkillId == skillId) {
                        // Clicked on existing skill -> Enter Talent Tree
                        m_selectedSkillId = skillId;
                    } else {
                        // Check if this skill is already specialized elsewhere
                        bool alreadyInOtherSlot = false;
                        for (int j = 0; j < 5; ++j) {
                            if (active->specialized_slots[j].skill_id ==
                                drag.draggedSkillId) {
                                alreadyInOtherSlot = true;
                                break;
                            }
                        }

                        if (!alreadyInOtherSlot) {
                            if (active->specialized_slots[i].skill_id !=
                                NoMoreDay::INVALID_SKILL_ID) {
                                SkillSystem::ResetTalents(
                                    registry, player,
                                    active->specialized_slots[i].skill_id);
                            }
                            active->specialized_slots[i].skill_id =
                                drag.draggedSkillId;
                            active->specialized_slots[i].allocated_points.clear();
                            LOG_INFO("Assigned skill {} to specialized slot {}",
                                     drag.draggedSkillId, i);
                        } else {
                            LOG_INFO("Skill {} already specialized",
                                     drag.draggedSkillId);
                        }
                    }
                    drag.isDraggingSkill = false;
                    drag.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
                }
            }

            if (skillId != NoMoreDay::INVALID_SKILL_ID) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // U8: drag start routes through the host-owned session.
                    if (uiHost) {
                        UIDragSession& drag = uiHost->DragSession();
                        drag.draggedSkillId = skillId;
                        drag.isDraggingSkill = true;
                    }
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
    float gridStartX = startX + 50.0f * scaleFactor;
    float gridStartY = slotsStartY + slotSize + 50.0f * scaleFactor;
    float gridW = panelW - 100.0f * scaleFactor;
    
    UISystem::DrawTextUI("可用技能", gridStartX, gridStartY - 30, 24, LIGHTGRAY, alpha);

    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    int col = 0;
    int row = 0;
    float gridSize = 64.0f * scaleFactor;
    
    for (const auto& [id, skill] : allSkills) {
        float x = gridStartX + col * (gridSize + slotPadding);
        float y = gridStartY + row * (gridSize + slotPadding);
        Rectangle skillRect_Logic = {x / scaleFactor, y / scaleFactor, gridSize / scaleFactor, gridSize / scaleFactor};

        // Check if already specialized
        bool isSpecialized = false;
        for (const auto& s : active->specialized_slots) {
            if (s.skill_id == id) {
                isSpecialized = true;
                break;
            }
        }

        bool isBladeAscendantSignatureSkill = false;
        for (const auto& profile : data::BladeMasteryRegistry::Get().GetAllProfiles()) {
            if (id == profile.signature_skill_id) {
                isBladeAscendantSignatureSkill = true;
                break;
            }
        }
        const bool signatureLocked =
            isBladeAscendantSignatureSkill &&
            !systems::BladeMasteryService::IsSignatureSkillUnlocked(registry, player, id);

        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), skillRect_Logic);

        // Draw simple slot instead of full button texture
        float scale = scaleFactor;
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
            // Tooltip (deferred to top-most overlay render path). U8: the
            // hover write routes through the host channel (same as the
            // specialized-slot pass above).
            if (uiHost) {
                uiHost->SetHoveredSkillId(id);
            }

            if (!signatureLocked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // U8: drag start routes through the host-owned session.
                if (uiHost) {
                    UIDragSession& drag = uiHost->DragSession();
                    drag.draggedSkillId = id;
                    drag.isDraggingSkill = true;
                }
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

