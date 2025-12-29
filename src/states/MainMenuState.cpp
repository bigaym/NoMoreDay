#include "MainMenuState.hpp"
#include "GameplayState.hpp"
#include "LoadingState.hpp"
#include "../core/StateManager.hpp"
#include "../core/LevelManager.hpp"
#include <raylib.h>
#include <memory>
#include <thread> // For sleep simulation if desired

namespace NoMoreDay {

    MainMenuState::MainMenuState(StateManager& manager, SharedContext& context)
        : IState(manager, context) 
    {
        float screenWidth = (float)GetScreenWidth();
        float screenHeight = (float)GetScreenHeight();

        float btnWidth = 200;
        float btnHeight = 50;
        float centerX = (screenWidth - btnWidth) / 2.0f;

        m_startButton = { { centerX, screenHeight * 0.5f, btnWidth, btnHeight }, "START GAME", false };
        m_exitButton = { { centerX, screenHeight * 0.5f + 70, btnWidth, btnHeight }, "EXIT", false };
    }

    void MainMenuState::OnEnter() {
        // Here we could load specific menu music or assets
        m_titleOpacity = 0.0f;
    }

    void MainMenuState::OnExit() {
        // Cleanup
    }

    bool MainMenuState::OnUpdate(float dt) {
        m_titleOpacity = std::min(1.0f, m_titleOpacity + dt * 2.0f);

        Vector2 mousePos = GetMousePosition();
        
        m_startButton.hovered = CheckCollisionPointRec(mousePos, m_startButton.bounds);
        m_exitButton.hovered = CheckCollisionPointRec(mousePos, m_exitButton.bounds);

        if (IsButtonClicked(m_startButton)) {
            auto levelData = std::make_shared<LevelManager::LevelData>();
            auto* levelMgr = m_context->levelManager;

            // Transition to Loading State
            // We use a shared_ptr to pass data from worker thread to main thread callback
            m_stateManager->ChangeState<LoadingState>(
                [levelMgr, levelData]() {
                    // Simulate a bit of load time to show off the screen (optional)
                    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    *levelData = levelMgr->prepareLevel("cave", 128, 128);
                },
                [levelMgr, levelData](StateManager& mgr) {
                    levelMgr->activateLevel(std::move(*levelData));
                    mgr.ChangeState<GameplayState>();
                }
            );
        }
        else if (IsButtonClicked(m_exitButton)) {
            m_stateManager->PopState(); // Popping the last state will exit the game loop
        }

        return true; 
    }

    void MainMenuState::OnRender() {
        ClearBackground(BLACK);

        // Draw Title
        const char* title = "NOMOREDAY";
        int fontSize = 80;
        int titleWidth = MeasureText(title, fontSize);
        DrawText(title, (GetScreenWidth() - titleWidth) / 2, GetScreenHeight() * 0.2f, fontSize, 
                 Fade(RED, m_titleOpacity));

        // Draw Buttons
        DrawButton(m_startButton);
        DrawButton(m_exitButton);

        // Version Info
        DrawText("v0.1 Alpha - State Manager Demo", 10, GetScreenHeight() - 25, 20, DARKGRAY);
    }

    void MainMenuState::DrawButton(const Button& btn) {
        Color baseColor = btn.hovered ? RAYWHITE : GRAY;
        Color textColor = btn.hovered ? BLACK : RAYWHITE;

        DrawRectangleRec(btn.bounds, baseColor);
        DrawRectangleLinesEx(btn.bounds, 2, btn.hovered ? RED : LIGHTGRAY);

        int fontSize = 20;
        int textWidth = MeasureText(btn.text.c_str(), fontSize);
        DrawText(btn.text.c_str(), 
                 (int)(btn.bounds.x + (btn.bounds.width - textWidth) / 2), 
                 (int)(btn.bounds.y + (btn.bounds.height - fontSize) / 2), 
                 fontSize, textColor);
    }

    bool MainMenuState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

}
