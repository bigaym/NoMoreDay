#include "PauseState.hpp"
#include "MainMenuState.hpp"
#include "SettingsState.hpp"
#include "../core/StateManager.hpp"
#include "../systems/UISystem.hpp"
#include "../systems/MapSystem.hpp"
#include "../core/LevelManager.hpp"
#include "../components/Common.hpp"
#include <raylib.h>

namespace NoMoreDay {

    // Forced recompile marker
    PauseState::PauseState(StateManager& manager, SharedContext& context)
        : IState(manager, context) 
    {
        float screenWidth = (float)GetScreenWidth();
        float screenHeight = (float)GetScreenHeight();

        float btnWidth = 200;
        float btnHeight = 50;
        float centerX = (screenWidth - btnWidth) / 2.0f;

        m_resumeButton = { { centerX, screenHeight * 0.4f, btnWidth, btnHeight }, "RESUME", false };
        m_unstuckButton = { { centerX, screenHeight * 0.4f + 70, btnWidth, btnHeight }, "UNSTUCK", false };
        m_settingsButton = { { centerX, screenHeight * 0.4f + 140, btnWidth, btnHeight }, "SETTINGS", false };
        m_menuButton = { { centerX, screenHeight * 0.4f + 210, btnWidth, btnHeight }, "MAIN MENU", false };
    }

    void PauseState::OnEnter() {}
    void PauseState::OnExit() {}

    bool PauseState::OnUpdate(float dt) {
        Vector2 mousePos = GetMousePosition();
        
        m_resumeButton.hovered = CheckCollisionPointRec(mousePos, m_resumeButton.bounds);
        m_unstuckButton.hovered = CheckCollisionPointRec(mousePos, m_unstuckButton.bounds);
        m_settingsButton.hovered = CheckCollisionPointRec(mousePos, m_settingsButton.bounds);
        m_menuButton.hovered = CheckCollisionPointRec(mousePos, m_menuButton.bounds);

        if (IsButtonClicked(m_resumeButton) || IsKeyReleased(KEY_ESCAPE)) {
            m_stateManager->PopState();
        }
        else if (IsButtonClicked(m_unstuckButton)) {
            if (m_context->registry && m_context->levelManager) {
                auto view = m_context->registry->view<PlayerTag, Position>();
                if (view.begin() != view.end()) {
                    auto entity = view.front();
                    auto& pos = view.get<Position>(entity);
                    const auto& mapSystem = m_context->levelManager->getMapSystem();
                    
                    int startX = static_cast<int>(pos.x / 10.0f);
                    int startY = static_cast<int>(pos.y / 10.0f);
                    
                    bool found = false;
                    for (int radius = 0; radius < 15 && !found; ++radius) {
                        for (int y = -radius; y <= radius && !found; ++y) {
                            for (int x = -radius; x <= radius && !found; ++x) {
                                if (abs(x) == radius || abs(y) == radius) {
                                    int checkX = startX + x;
                                    int checkY = startY + y;
                                    if (mapSystem.isWalkable(checkX, checkY)) {
                                        pos.x = checkX * 10.0f + 5.0f;
                                        pos.y = checkY * 10.0f + 5.0f;
                                        found = true;
                                    }
                                }
                            }
                        }
                    }
                    if (found) {
                        m_stateManager->PopState();
                    }
                }
            }
        }
        else if (IsButtonClicked(m_settingsButton)) {
            m_stateManager->PushState<SettingsState>();
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
        
        Font font = UISystem::GetFont();
        const char* text = "PAUSED";
        float fontSize = 40.0f;
        
        float textWidth = IsFontValid(font) ? MeasureTextEx(font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);

        if (IsFontValid(font)) {
            DrawTextEx(font, text, { (GetScreenWidth() - textWidth) / 2.0f, GetScreenHeight() * 0.25f }, fontSize, 1.0f, RAYWHITE);
        } else {
            DrawText(text, (int)(GetScreenWidth() - textWidth) / 2, (int)(GetScreenHeight() * 0.25f), (int)fontSize, RAYWHITE);
        }

        DrawButton(m_resumeButton);
        DrawButton(m_unstuckButton);
        DrawButton(m_settingsButton);
        DrawButton(m_menuButton);
    }

    void PauseState::DrawButton(const Button& btn) {
        Color baseColor = btn.hovered ? RAYWHITE : Color{ 50, 50, 50, 200 };
        Color textColor = btn.hovered ? BLACK : RAYWHITE;

        DrawRectangleRec(btn.bounds, baseColor);
        DrawRectangleLinesEx(btn.bounds, 2, btn.hovered ? YELLOW : LIGHTGRAY);

        Font font = UISystem::GetFont();
        float fontSize = 20.0f;
        float textWidth = IsFontValid(font) ? MeasureTextEx(font, btn.text.c_str(), fontSize, 1.0f).x : (float)MeasureText(btn.text.c_str(), (int)fontSize);
        
        Vector2 textPos = {
            btn.bounds.x + (btn.bounds.width - textWidth) / 2.0f,
            btn.bounds.y + (btn.bounds.height - fontSize) / 2.0f
        };
        
        if (IsFontValid(font)) {
            DrawTextEx(font, btn.text.c_str(), textPos, fontSize, 1.0f, textColor);
        } else {
            DrawText(btn.text.c_str(), (int)textPos.x, (int)textPos.y, (int)fontSize, textColor);
        }
    }

    bool PauseState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

}