#include "PauseState.hpp"
#include "MainMenuState.hpp"
#include "../core/StateManager.hpp"
#include <raylib.h>

namespace NoMoreDay {

    PauseState::PauseState(StateManager& manager, SharedContext& context)
        : IState(manager, context) 
    {
        float screenWidth = (float)GetScreenWidth();
        float screenHeight = (float)GetScreenHeight();

        float btnWidth = 200;
        float btnHeight = 50;
        float centerX = (screenWidth - btnWidth) / 2.0f;

        m_resumeButton = { { centerX, screenHeight * 0.45f, btnWidth, btnHeight }, "RESUME", false };
        m_menuButton = { { centerX, screenHeight * 0.45f + 70, btnWidth, btnHeight }, "MAIN MENU", false };
    }

    void PauseState::OnEnter() {}
    void PauseState::OnExit() {}

    bool PauseState::OnUpdate(float dt) {
        Vector2 mousePos = GetMousePosition();
        
        m_resumeButton.hovered = CheckCollisionPointRec(mousePos, m_resumeButton.bounds);
        m_menuButton.hovered = CheckCollisionPointRec(mousePos, m_menuButton.bounds);

        if (IsButtonClicked(m_resumeButton) || IsKeyReleased(KEY_ESCAPE)) {
            m_stateManager->PopState();
        }
        else if (IsButtonClicked(m_menuButton)) {
            m_stateManager->ClearStates();
            m_stateManager->PushState<MainMenuState>();
        }

        return false; // PAUSE underlying states
    }

    void PauseState::OnRender() {
        // Draw overlay
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 150 });

        DrawText("PAUSED", GetScreenWidth() / 2 - MeasureText("PAUSED", 40) / 2, GetScreenHeight() * 0.3f, 40, RAYWHITE);

        DrawButton(m_resumeButton);
        DrawButton(m_menuButton);
    }

    void PauseState::DrawButton(const Button& btn) {
        Color baseColor = btn.hovered ? RAYWHITE : Color{ 50, 50, 50, 200 };
        Color textColor = btn.hovered ? BLACK : RAYWHITE;

        DrawRectangleRec(btn.bounds, baseColor);
        DrawRectangleLinesEx(btn.bounds, 2, btn.hovered ? YELLOW : LIGHTGRAY);

        int fontSize = 20;
        int textWidth = MeasureText(btn.text.c_str(), fontSize);
        DrawText(btn.text.c_str(), 
                 (int)(btn.bounds.x + (btn.bounds.width - textWidth) / 2), 
                 (int)(btn.bounds.y + (btn.bounds.height - fontSize) / 2), 
                 fontSize, textColor);
    }

    bool PauseState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

}
