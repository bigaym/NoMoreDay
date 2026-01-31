#include "game/states/DimensionalLevelSelectState.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/states/MosaicEditorState.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/components/WorldState.hpp"
#include "core/logging/Logger.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace NoMoreDay {

DimensionalLevelSelectState::DimensionalLevelSelectState(StateManager& stateManager, SharedContext& context)
    : IState(stateManager, context) {}

void DimensionalLevelSelectState::OnEnter() {
    LOG_INFO("Entering DimensionalLevelSelectState");
    
    // 1. Calculate Max Level based on Player
    m_playerLevel = 1;
    auto view = m_context->registry->view<PlayerTag, PlayerStats>();
    if (view.begin() != view.end()) {
        m_playerLevel = view.get<PlayerStats>(view.front()).level;
    }
    
    m_minLevel = 1;
    m_maxLevel = std::min(100, m_playerLevel + 10);
    m_selectedLevel = m_maxLevel; // Default to max available
    
    // Auto-scroll to show selected
    if (m_maxLevel > VISIBLE_ITEMS) {
        m_scrollOffset = (float)(m_maxLevel - VISIBLE_ITEMS);
    } else {
        m_scrollOffset = 0.0f;
    }
}

void DimensionalLevelSelectState::OnExit() {
    LOG_INFO("Exiting DimensionalLevelSelectState");
}

bool DimensionalLevelSelectState::OnUpdate(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        m_stateManager->PopState();
        return false;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        m_scrollOffset -= wheel * 3.0f;
    }
    
    // Clamp scroll
    int totalItems = m_maxLevel - m_minLevel + 1;
    float maxScroll = (float)std::max(0, totalItems - VISIBLE_ITEMS);
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxScroll);

    // Handle Confirm
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ConfirmSelection();
    }

    return false; // Block updates below
}

void DimensionalLevelSelectState::OnRender() {
    // 1. Dim Background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 200});

    // 2. Window Body
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float winX = (screenW - WINDOW_WIDTH) / 2.0f;
    float winY = (screenH - WINDOW_HEIGHT) / 2.0f;

    DrawRectangleRounded(Rectangle{winX, winY, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT}, 0.05f, 8, Color{30, 30, 40, 255});
    DrawRectangleRoundedLinesEx(Rectangle{winX, winY, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT}, 0.05f, 8, 2.0f, DARKGRAY);

    // 3. Title
    Font font = UISystem::GetFont();
    const char* title = "选择维度等级 (Rift Level)";
    UIRenderer::DrawTextUI(font, title, winX + 20, winY + 20, 24, GOLD, 1.0f);
    
    char subBuf[64];
    snprintf(subBuf, sizeof(subBuf), "最大等级: %d (玩家 + 10)", m_maxLevel);
    UIRenderer::DrawTextUI(font, subBuf, winX + 20, winY + 50, 16, GRAY, 1.0f);

    // 4. List Area
    BeginScissorMode((int)winX + 20, (int)winY + 80, WINDOW_WIDTH - 40, VISIBLE_ITEMS * LIST_ITEM_HEIGHT);
    RenderList();
    EndScissorMode();
    
    // Scrollbar Indicator
    int totalItems = m_maxLevel - m_minLevel + 1;
    if (totalItems > VISIBLE_ITEMS) {
        float barH = (float)(VISIBLE_ITEMS * LIST_ITEM_HEIGHT);
        float scrollPct = m_scrollOffset / (float)(totalItems - VISIBLE_ITEMS);
        float knobH = barH * ((float)VISIBLE_ITEMS / totalItems);
        float knobY = winY + 80 + scrollPct * (barH - knobH);
        
        DrawRectangle((int)(winX + WINDOW_WIDTH - 15), (int)(winY + 80), 6, (int)barH, Color{20, 20, 20, 255});
        DrawRectangle((int)(winX + WINDOW_WIDTH - 15), (int)knobY, 6, (int)knobH, GRAY);
    }

    // 5. Buttons
    RenderButtons();
}

void DimensionalLevelSelectState::RenderList() {
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float winX = (screenW - WINDOW_WIDTH) / 2.0f;
    float startY = (screenH - WINDOW_HEIGHT) / 2.0f + 80.0f;
    
    Vector2 mouse = GetMousePosition();
    m_hoveredIndex = -1;

    // Items are Levels: m_minLevel to m_maxLevel
    // Display them from m_minLevel up.
    // Implementation: Ascending (1 at top). m_selected is max. Scroll is max.
    
    int totalItems = m_maxLevel - m_minLevel + 1;
    
    for (int i = 0; i < totalItems; ++i) {
        int level = m_minLevel + i;
        float itemY = startY + (i - m_scrollOffset) * LIST_ITEM_HEIGHT;
        
        // Visibility Check
        if (itemY < startY - LIST_ITEM_HEIGHT || itemY > startY + VISIBLE_ITEMS * LIST_ITEM_HEIGHT) {
            continue;
        }

        Rectangle itemRect = {winX + 20, itemY, (float)(WINDOW_WIDTH - 60), (float)LIST_ITEM_HEIGHT - 2};
        bool isHovered = CheckCollisionPointRec(mouse, itemRect);
        bool isSelected = (level == m_selectedLevel);

        if (isHovered) {
            m_hoveredIndex = level;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m_selectedLevel = level;
            }
        }

        Color bgColor = isSelected ? Color{60, 80, 60, 255} : (isHovered ? Color{50, 50, 60, 255} : Color{40, 40, 50, 255});
        DrawRectangleRounded(itemRect, 0.2f, 4, bgColor);
        
        Font font = UISystem::GetFont();
        char buf[32];
        snprintf(buf, sizeof(buf), "等级 %d", level);
        
        Color textColor = WHITE;
        if (level > m_playerLevel) textColor = RED;      // Dangerous
        else if (level == m_playerLevel) textColor = GOLD; // Match
        else textColor = LIGHTGRAY; // Easy

        UIRenderer::DrawTextUI(font, buf, itemRect.x + 20, itemRect.y + 10, 20, textColor, 1.0f);
        
        if (isSelected) {
            DrawRectangleLinesEx(itemRect, 2.0f, GOLD);
        }
    }
}

void DimensionalLevelSelectState::RenderButtons() {
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float winX = (screenW - WINDOW_WIDTH) / 2.0f;
    float winY = (screenH - WINDOW_HEIGHT) / 2.0f;
    
    float btnY = winY + WINDOW_HEIGHT - 60;
    
    // Confirm Button
    Rectangle confirmRect = {winX + WINDOW_WIDTH - 140, btnY, 120, 40};
    bool hoverConfirm = CheckCollisionPointRec(GetMousePosition(), confirmRect);
    DrawRectangleRounded(confirmRect, 0.2f, 4, hoverConfirm ? GREEN : DARKGREEN);
    Font font = UISystem::GetFont();
    UIRenderer::DrawTextUI(font, "进入维度", confirmRect.x + 20, confirmRect.y + 10, 18, WHITE, 1.0f);
    
    if (hoverConfirm && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ConfirmSelection();
    }

    // Cancel Button
    Rectangle cancelRect = {winX + 20, btnY, 100, 40};
    bool hoverCancel = CheckCollisionPointRec(GetMousePosition(), cancelRect);
    DrawRectangleRounded(cancelRect, 0.2f, 4, hoverCancel ? RED : MAROON);
    UIRenderer::DrawTextUI(font, "取消", cancelRect.x + 30, cancelRect.y + 10, 18, WHITE, 1.0f);

    if (hoverCancel && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        m_stateManager->PopState();
    }
}

void DimensionalLevelSelectState::ConfirmSelection() {
    // Write to ActiveDimensionalState
    if (!m_context->registry->ctx().contains<ActiveDimensionalState>()) {
        m_context->registry->ctx().emplace<ActiveDimensionalState>();
    }
    
    auto& state = m_context->registry->ctx().get<ActiveDimensionalState>();
    state.selectedBaseLevel = m_selectedLevel;
    
    LOG_INFO("Dimensional Level Selected: {}", m_selectedLevel);
    
    // Transition: Replace current state (LevelSelect) with MosaicEditor
    m_stateManager->ChangeState<MosaicEditorState>();
}

} // namespace NoMoreDay
