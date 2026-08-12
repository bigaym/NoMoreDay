#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/BladeMasteryUITheme.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace NoMoreDay {

// R8: the hub consumes the snapshot display views from NoMoreDay::ui.
using ui::GameUiIntent;
using ui::GameUiIntentKind;
using ui::GameUiMasteryCardView;
using ui::GameUiPlayerSnapshot;
using ui::GameUiSkillTarget;
using ui::GameUiSkillTreeView;
using ui::GameUiSnapshot;
using ui::GameUiSpecializedSlotView;
using ui::UiDrawLayer;
using ui::UiDrawList;
using ui::UiRect;
using ui::UiVec2;
using ui::UiViewport;
using ui::kSkillHubPainterResourceId;

void UISkillHub::UpdateInput(const GameUiSnapshot& snapshot,
                             const ui::UiInputFrame& input, float alpha) {
    (void)alpha;
    // R8: snapshot-driven interaction phase. Reads the skill-hub segment the
    // builder resolved (mastery cards / specialized slots / attunement /
    // debug override / locked signature skills) and routes every
    // gameplay-writing click through the host intent sink; the hub never
    // touches the registry (design §3.1/§3.3).
    if (m_uiHost == nullptr) {
        return;
    }
    const GameUiSkillTreeView& tree = snapshot.skillTree;
    const GameUiPlayerSnapshot& player = snapshot.player;
    const int playerLevel = player.hasPlayer ? player.level : 1;

    // Capture the render data for PaintCanvas (frame-scoped, read-only).
    m_paint.snapshot = &snapshot;
    m_paint.alpha = alpha;
    m_paint.hasBladeProfession = tree.hasBladeProfession;
    m_paint.debugUnlockEnabled = tree.debugUnlockEnabled;
    m_paint.selectedMastery = tree.selectedMastery;
    m_paint.heavenlyAttunement = tree.heavenlyAttunement;
    m_paint.playerLevel = playerLevel;
    m_paint.slots = tree.specializedSlots;
    m_paint.cards = tree.masteryCards;
    m_paint.lockedSignatureSkills = tree.lockedSignatureSkills;

    const float scaleFactor = UISystem::GetScaleFactor();
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const float panelW = 1000.0f * scaleFactor;
    const float panelH = 700.0f * scaleFactor;
    const float startX = (screenW - panelW) / 2.0f;
    const float startY = (screenH - panelH) / 2.0f;
    const ui::UiVec2 mouse = input.pointer.logicalPosition;

    const bool showHeavenlySwordAttunementControls =
        tree.selectedMastery == static_cast<std::uint8_t>(BladeMasteryId::HeavenlySword) &&
        tree.hasBladeProfession;

    const float masteryPanelX = startX + 20.0f * scaleFactor;
    const float masteryPanelY = startY + 56.0f * scaleFactor;
    const float masteryPanelW = panelW - 40.0f * scaleFactor;
    const float masteryPanelH =
        (showHeavenlySwordAttunementControls ? 132.0f : 92.0f) * scaleFactor;

    if (m_paint.cards.empty()) {
        // No mastery data loaded; nothing interactive to route.
        return;
    }

    const float cardGap = 10.0f * scaleFactor;
    const float cardW = (masteryPanelW - cardGap * 2.0f) / 3.0f;
    const float cardH = 48.0f * scaleFactor;

    // --- Mastery card select buttons (intent SkillSelectMastery) ---
    for (std::size_t index = 0; index < m_paint.cards.size(); ++index) {
        const GameUiMasteryCardView& card = m_paint.cards[index];
        const bool selected = card.selected;
        const bool canSelect = card.unlocked && !selected;

        const float cardX = masteryPanelX + static_cast<float>(index) * (cardW + cardGap);
        const float cardY = masteryPanelY + 34.0f * scaleFactor;
        const Rectangle cardRect = {cardX, cardY, cardW, cardH};
        const Rectangle button = {cardRect.x + cardRect.width - 82.0f * scaleFactor,
                                  cardRect.y + 10.0f * scaleFactor,
                                  70.0f * scaleFactor,
                                  24.0f * scaleFactor};
        (void)cardRect;

        if (canSelect &&
            CheckCollisionPointRec(Vector2{mouse.x, mouse.y}, button) &&
            input.pointer.pressed) {
            GameUiIntent intent;
            intent.sourceNode = 0;
            intent.kind = GameUiIntentKind::SkillSelectMastery;
            intent.payload.masteryId = card.masteryId;
            m_uiHost->EnqueueIntent(std::move(intent));
        }
    }

    // --- Debug unlock override button (intent SkillSetDebugUnlock) ---
    {
        Rectangle debugButton = {masteryPanelX + masteryPanelW - 240.0f * scaleFactor,
                                  masteryPanelY + 44.0f * scaleFactor,
                                  108.0f * scaleFactor,
                                  28.0f * scaleFactor};
        if (CheckCollisionPointRec(Vector2{mouse.x, mouse.y}, debugButton) &&
            input.pointer.pressed) {
            GameUiIntent intent;
            intent.sourceNode = 0;
            intent.kind = GameUiIntentKind::SkillSetDebugUnlock;
            intent.payload.flag = !m_paint.debugUnlockEnabled;
            m_uiHost->EnqueueIntent(std::move(intent));
        }
    }

    // --- Heavenly Sword attunement buttons (intent SkillSetAttunement) ---
    if (showHeavenlySwordAttunementControls) {
        constexpr std::array<std::pair<BladeAttunement, std::uint8_t>, 3> attunements = {{
            {BladeAttunement::Lightning, static_cast<std::uint8_t>(BladeAttunement::Lightning)},
            {BladeAttunement::Frost, static_cast<std::uint8_t>(BladeAttunement::Frost)},
            {BladeAttunement::Fire, static_cast<std::uint8_t>(BladeAttunement::Fire)},
        }};
        const float logicalButtonW = 92.0f;
        const float logicalButtonH = 24.0f;
        const float logicalButtonGap = 8.0f;
        const float logicalStartX = (masteryPanelX + 12.0f * scaleFactor) / scaleFactor;
        const float logicalStartY = (masteryPanelY + 100.0f * scaleFactor) / scaleFactor;

        for (std::size_t index = 0; index < attunements.size(); ++index) {
            const auto [attunement, element] = attunements[index];
            Rectangle buttonLogic = {
                logicalStartX + static_cast<float>(index) * (logicalButtonW + logicalButtonGap),
                logicalStartY,
                logicalButtonW,
                logicalButtonH,
            };
            const bool isSelected = m_paint.heavenlyAttunement == element;
            if (!isSelected &&
                CheckCollisionPointRec(Vector2{mouse.x, mouse.y}, buttonLogic) &&
                input.pointer.pressed) {
                GameUiIntent intent;
                intent.sourceNode = 0;
                intent.kind = GameUiIntentKind::SkillSetAttunement;
                intent.payload.attunementElement = element;
                m_uiHost->EnqueueIntent(std::move(intent));
            }
        }
    }

    // --- Specialized slots (drop -> SkillAssign, right-click -> SkillUnassign,
    //     click -> open tree / drag start) ---
    float slotSize = 80.0f * scaleFactor;
    float slotPadding = 20.0f * scaleFactor;
    float slotsStartX = startX + (panelW - (slotSize * 5 + slotPadding * 4)) / 2.0f;
    float slotsStartY = masteryPanelY + masteryPanelH + 18.0f * scaleFactor;

    UIDragSession& drag = m_uiHost->DragSession();
    for (int i = 0; i < 5; ++i) {
        float x = slotsStartX + static_cast<float>(i) * (slotSize + slotPadding);
        float y = slotsStartY;
        Rectangle slotRect_Logic = {x / scaleFactor, y / scaleFactor,
                                    slotSize / scaleFactor, slotSize / scaleFactor};

        const uint32_t skillId = m_paint.slots[i].skillId;
        const bool isHovered = CheckCollisionPointRec(Vector2{mouse.x, mouse.y}, slotRect_Logic);
        if (!isHovered) {
            continue;
        }

        if (skillId != NoMoreDay::INVALID_SKILL_ID) {
            m_uiHost->SetHoveredSkillId(skillId);
        }

        // Drop: clicked on existing skill -> open the talent tree.
        if (drag.isDraggingSkill && input.pointer.released) {
            if (drag.draggedSkillId == skillId) {
                m_selectedSkillId = skillId;
            } else {
                GameUiIntent intent;
                intent.sourceNode = 0;
                intent.kind = GameUiIntentKind::SkillAssign;
                intent.payload.skillId = drag.draggedSkillId;
                intent.payload.skillTarget =
                    static_cast<std::uint8_t>(GameUiSkillTarget::Specialized);
                intent.payload.sourceSlot = i;
                m_uiHost->EnqueueIntent(std::move(intent));
            }
            drag.isDraggingSkill = false;
            drag.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
        }

        if (skillId != NoMoreDay::INVALID_SKILL_ID) {
            // Drag start (assignment drag source).
            if (input.pointer.pressed) {
                drag.draggedSkillId = skillId;
                drag.isDraggingSkill = true;
            }
            // Right-click unassigns the specialized skill (intent).
            if (input.pointer.pressedRight) {
                GameUiIntent intent;
                intent.sourceNode = 0;
                intent.kind = GameUiIntentKind::SkillUnassign;
                intent.payload.sourceSlot = i;
                m_uiHost->EnqueueIntent(std::move(intent));
            }
        }
    }

    // --- Available skills grid (click -> drag start) ---
    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    float gridStartX = startX + 50.0f * scaleFactor;
    float gridStartY = slotsStartY + slotSize + 50.0f * scaleFactor;
    float gridW = panelW - 100.0f * scaleFactor;
    int col = 0;
    int row = 0;
    float gridSize = 64.0f * scaleFactor;

    for (const auto& [id, skill] : allSkills) {
        (void)skill;
        float gx = gridStartX + static_cast<float>(col) * (gridSize + slotPadding);
        float gy = gridStartY + static_cast<float>(row) * (gridSize + slotPadding);
        Rectangle skillRect_Logic = {gx / scaleFactor, gy / scaleFactor,
                                     gridSize / scaleFactor, gridSize / scaleFactor};

        const bool signatureLocked =
            std::find(m_paint.lockedSignatureSkills.begin(),
                      m_paint.lockedSignatureSkills.end(), id) !=
            m_paint.lockedSignatureSkills.end();

        if (CheckCollisionPointRec(Vector2{mouse.x, mouse.y}, skillRect_Logic)) {
            m_uiHost->SetHoveredSkillId(id);
            if (!signatureLocked && input.pointer.pressed) {
                drag.draggedSkillId = id;
                drag.isDraggingSkill = true;
                LOG_INFO("Started dragging skill {}", id);
            }
        }

        col++;
        if (gx + gridSize + slotPadding > gridStartX + gridW) {
            col = 0;
            row++;
        }
    }
}

void UISkillHub::Paint(UiDrawList& drawList, const UiViewport& viewport,
                       const GameUiSnapshot& snapshot, float alpha) {
    (void)snapshot;
    m_paint.alpha = alpha;
    // R8: single custom command (Panels layer). The backend painter reads the
    // hub paint state captured by UpdateInput; no raylib here.
    drawList.Custom(UiDrawLayer::Panels, 0,
                    {0.0f, 0.0f, viewport.LogicalSize().x, viewport.LogicalSize().y},
                    kSkillHubPainterResourceId);
}

void UISkillHub::PaintCanvas(UiRect nativeBounds) {
    (void)nativeBounds;
    // R8: registered backend painter. Raylib draw calls live only here
    // (design §3.4: painter owns the special canvas); the data is the
    // frame-scoped paint state captured by UpdateInput.
    const GameUiSnapshot* snapshot = m_paint.snapshot;
    if (snapshot == nullptr || m_paint.alpha <= 0.0f) {
        return;
    }
    const float alpha = m_paint.alpha;

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

    const int playerLevel = m_paint.playerLevel;
    const BladeMasteryId selectedMastery = static_cast<BladeMasteryId>(m_paint.selectedMastery);
    const bool showHeavenlySwordAttunementControls =
        selectedMastery == BladeMasteryId::HeavenlySword && m_paint.hasBladeProfession;

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

    if (m_paint.cards.empty()) {
        UISystem::DrawTextUI("未加载 Blade Ascendant Mastery 数据。", masteryPanelX + 12,
                             masteryPanelY + 40, 14, LIGHTGRAY, alpha);
    } else {
        const bool hasBladeProfession = m_paint.hasBladeProfession;
        const bool debugOverrideEnabled = m_paint.debugUnlockEnabled;

        const float cardGap = 10.0f * scaleFactor;
        const float cardW = (masteryPanelW - cardGap * 2.0f) / 3.0f;
        const float cardH = 48.0f * scaleFactor;

        for (std::size_t index = 0; index < m_paint.cards.size(); ++index) {
            const GameUiMasteryCardView& profile = m_paint.cards[index];
            const BladeMasteryUIThemeProfile& theme =
                GetBladeMasteryUIThemeProfile(static_cast<BladeMasteryId>(profile.masteryId));
            const bool unlocked = profile.unlocked;
            const bool selected = (selectedMastery == static_cast<BladeMasteryId>(profile.masteryId));
            const bool debugUnlocked = profile.debugUnlocked;

            const float cardX = masteryPanelX + static_cast<float>(index) * (cardW + cardGap);
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
                                                      : "Lv." + std::to_string(profile.unlockLevel) +
                                                            " / Debug Lv." +
                                                            std::to_string(profile.debugUnlockLevelOverride);

            UISystem::DrawTextUI(profile.name.data(), card.x + 10, card.y + 8,
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
                (void)attunement;
                Rectangle buttonLogic = {
                    logicalStartX + static_cast<float>(index) * (logicalButtonW + logicalButtonGap),
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
                const bool isSelected = m_paint.heavenlyAttunement ==
                    static_cast<std::uint8_t>(attunement);
                // Hover highlight (read-only; the click routing runs in
                // UpdateInput through the SkillSetAttunement intent).
                const bool attunementHovered =
                    CheckCollisionPointRec(UISystem::GetMousePositionLogic(), buttonLogic);

                DrawRectangleRec(buttonPhys,
                                 Fade(isSelected ? GOLD
                                                 : attunementHovered ? LIGHTGRAY
                                                                     : DARKGRAY,
                                      0.72f * alpha));
                DrawRectangleLinesEx(buttonPhys, 1.0f * scaleFactor,
                                     Fade(isSelected ? YELLOW : GRAY, alpha));
                UISystem::DrawTextUI(label, buttonPhys.x + 9.0f * scaleFactor,
                                     buttonPhys.y + 5.0f * scaleFactor, 12,
                                     WHITE, alpha);
            }
        }
    }

    float slotSize = 80.0f * scaleFactor;
    float slotPadding = 20.0f * scaleFactor;
    float slotsStartX = startX + (panelW - (slotSize * 5 + slotPadding * 4)) / 2.0f;
    float slotsStartY = masteryPanelY + masteryPanelH + 18.0f * scaleFactor;

    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    (void)rectTex;

    for (int i = 0; i < 5; ++i) {
        float x = slotsStartX + static_cast<float>(i) * (slotSize + slotPadding);
        float y = slotsStartY;
        Rectangle slotRect_Logic = {x / scaleFactor, y / scaleFactor,
                                    slotSize / scaleFactor, slotSize / scaleFactor};

        const uint32_t skillId = m_paint.slots[i].skillId;
        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), slotRect_Logic);

        // Use a consistent frame for both empty and assigned slots
        float scale = scaleFactor;
        Rectangle dest_Phys = {slotRect_Logic.x * scale, slotRect_Logic.y * scale,
                               slotRect_Logic.width * scale, slotRect_Logic.height * scale};

        DrawRectangleRec(dest_Phys, Fade(BLACK, 0.6f * alpha));
        DrawRectangleLinesEx(dest_Phys, 2.0f * scale, Fade(isHovered ? WHITE : LIGHTGRAY, alpha));

        if (skillId == NoMoreDay::INVALID_SKILL_ID) {
            // Empty slots: Show low-key hint text
            UIRenderer::DrawTextUI(UISystem::GetFont(), "Empty",
                                   (dest_Phys.x + dest_Phys.width * 0.5f) / scale - 25,
                                   (dest_Phys.y + dest_Phys.height * 0.5f) / scale - 10,
                                   16, GRAY, alpha);
        } else {
            // Assigned skills: Draw Icon
            const auto* skill = SkillRegistry::Get().GetSkill(skillId);
            if (skill && skill->icon_id != 0) {
                Texture2D icon = AssetLoadingSystem::GetTexture(skill->icon_id);
                Rectangle iconDest = {dest_Phys.x + 4 * scale, dest_Phys.y + 4 * scale,
                                      dest_Phys.width - 8 * scale, dest_Phys.height - 8 * scale};
                DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height},
                               iconDest, {0, 0}, 0.0f, Fade(WHITE, alpha));
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
        float x = gridStartX + static_cast<float>(col) * (gridSize + slotPadding);
        float y = gridStartY + static_cast<float>(row) * (gridSize + slotPadding);
        Rectangle skillRect_Logic = {x / scaleFactor, y / scaleFactor,
                                     gridSize / scaleFactor, gridSize / scaleFactor};

        // R8: the locked-signature set is resolved by the snapshot builder
        // (single registry read point); the painter stays read-only.
        const bool signatureLocked =
            std::find(m_paint.lockedSignatureSkills.begin(),
                      m_paint.lockedSignatureSkills.end(), id) !=
            m_paint.lockedSignatureSkills.end();

        // R8: assignment indicator resolved from the snapshot specialized
        // slots (never re-queries the registry).
        const bool isSpecialized = std::any_of(
            m_paint.slots.begin(), m_paint.slots.end(),
            [id](const GameUiSpecializedSlotView& slotView) {
                return slotView.skillId == id;
            });

        bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(), skillRect_Logic);

        // Draw simple slot instead of full button texture
        float scale = scaleFactor;
        Rectangle dest_Phys = {skillRect_Logic.x * scale, skillRect_Logic.y * scale,
                               skillRect_Logic.width * scale, skillRect_Logic.height * scale};

        DrawRectangleRec(dest_Phys, Fade(signatureLocked ? DARKGRAY : BLACK, 0.5f * alpha));
        DrawRectangleLinesEx(dest_Phys, 1.0f * scale, Fade(isHovered ? GOLD : GRAY, alpha));

        // Draw Skill Icon
        Texture2D icon = {0};
        if (skill.icon_id != 0) icon = AssetLoadingSystem::GetTexture(skill.icon_id);

        if (icon.id != 0) {
            Rectangle iconDest = {dest_Phys.x + 4 * scale, dest_Phys.y + 4 * scale,
                                  dest_Phys.width - 8 * scale, dest_Phys.height - 8 * scale};
            float iconAlpha = isSpecialized ? 0.5f
                             : (signatureLocked ? 0.25f : 1.0f);
            DrawTexturePro(icon, {0, 0, (float)icon.width, (float)icon.height},
                           iconDest, {0, 0}, 0.0f, Fade(WHITE, iconAlpha * alpha));
        } else {
            UISystem::DrawTextUI(skill.name_key.c_str(), x, y + 10, 14, WHITE, alpha);
        }

        if (signatureLocked) {
            UISystem::DrawTextUI("未解锁", x + 5, y + gridSize - 18, 14, LIGHTGRAY, alpha);
        } else if (isSpecialized) {
            // Draw "Equipped" indicator
            UISystem::DrawTextUI("已专精", x + 5, y + gridSize - 18, 14, GREEN, alpha);
        }

        col++;
        if (x + gridSize + slotPadding > gridStartX + gridW) {
            col = 0;
            row++;
        }
    }

    // Points Display
    char buf[64];
    utils::FormatToBuffer(buf, "可用专精点数: {}", snapshot->skillTree.availableTalentPoints);
    UISystem::DrawTextUI(buf, startX + panelW - 200, startY + 20, 20, GOLD, alpha);
}

// R8: backend painter callback. Registered by GameUiHost with
// kSkillHubPainterResourceId; userData points to the hub instance.
void SkillHubPaintCallback(void* userData, UiRect nativeBounds) {
  if (userData == nullptr) {
    return;
  }
  static_cast<UISkillHub*>(userData)->PaintCanvas(nativeBounds);
}

} // namespace NoMoreDay