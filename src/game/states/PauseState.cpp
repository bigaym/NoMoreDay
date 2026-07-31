#include "game/states/PauseState.hpp"
#include "game/states/MainMenuState.hpp"
#include "game/states/SettingsState.hpp"
#include "game/scene/StateManager.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/components/Common.hpp"
#include "engine/render/UIRenderer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/systems/ui/UICommon.hpp"
#include <raylib.h>

namespace NoMoreDay {

    // Forced recompile marker
    PauseState::PauseState(StateManager& manager, SharedContext& context)
        : IState(manager, context) 
    {
        float screenWidth = UI_REF_WIDTH;
        float screenHeight = UI_REF_HEIGHT;

        float btnWidth = 260;
        float btnHeight = 70;
        float centerX = (screenWidth - btnWidth) / 2.0f;

        m_resumeButton = { { centerX, screenHeight * 0.35f, btnWidth, btnHeight }, "RESUME", false };
        m_unstuckButton = { { centerX, screenHeight * 0.35f + 85, btnWidth, btnHeight }, "UNSTUCK", false };
        m_settingsButton = { { centerX, screenHeight * 0.35f + 170, btnWidth, btnHeight }, "SETTINGS", false };
        m_menuButton = { { centerX, screenHeight * 0.35f + 255, btnWidth, btnHeight }, "MAIN MENU", false };
    }

    void PauseState::OnEnter() {
        m_confirmMainMenu = false;
        m_menuButton.text = "MAIN MENU";
        m_inputDebounce = 0.15f; // Avoid click-through from gameplay frame.
        LOG_INFO("PauseState: Entered.");
    }

    void PauseState::OnExit() {
        LOG_INFO("PauseState: Exited.");
    }

    bool PauseState::OnUpdate(float dt) {
        m_inputDebounce = std::max(0.0f, m_inputDebounce - dt);

        Vector2 mousePos = UISystem::GetMousePositionLogic();
        
        m_resumeButton.hovered = CheckCollisionPointRec(mousePos, m_resumeButton.bounds);
        m_unstuckButton.hovered = CheckCollisionPointRec(mousePos, m_unstuckButton.bounds);
        m_settingsButton.hovered = CheckCollisionPointRec(mousePos, m_settingsButton.bounds);
        m_menuButton.hovered = CheckCollisionPointRec(mousePos, m_menuButton.bounds);

        const bool canClick = (m_inputDebounce <= 0.0f);

        if ((canClick && IsButtonClicked(m_resumeButton)) || IsKeyPressed(KEY_ESCAPE)) {
            LOG_INFO("PauseState: Resume requested.");
            m_confirmMainMenu = false;
            m_menuButton.text = "MAIN MENU";
            m_stateManager->PopState();
        }
        else if (canClick && IsButtonClicked(m_unstuckButton)) {
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
                        LOG_INFO("PauseState: Unstuck succeeded.");
                        m_stateManager->PopState();
                    }
                }
            }
        }
        else if (canClick && IsButtonClicked(m_settingsButton)) {
            LOG_INFO("PauseState: Opening SettingsState.");
            m_confirmMainMenu = false;
            m_menuButton.text = "MAIN MENU";
            m_stateManager->PushState<SettingsState>();
        }
        else if (canClick && IsButtonClicked(m_menuButton)) {
            if (!m_confirmMainMenu) {
                m_confirmMainMenu = true;
                m_menuButton.text = "CONFIRM MENU";
                LOG_WARN("PauseState: Main menu requested. Waiting second confirmation.");
            } else {
                LOG_WARN("PauseState: Returning to MainMenuState (clear state stack).");
                m_stateManager->ClearStates();
                m_stateManager->PushState<MainMenuState>();
            }
        }

        return false; // PAUSE underlying states
    }

    void PauseState::OnRender() {
        // Draw overlay
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 150 });
        
        Font font = UISystem::GetFont();
        const char* text = "PAUSED";
        UIRenderer::DrawTextUI(font, text, UI_REF_WIDTH * 0.5f - 120.0f, UI_REF_HEIGHT * 0.25f, 40.0f, RAYWHITE, 1.0f);

        DrawButton(m_resumeButton);
        DrawButton(m_unstuckButton);
        DrawButton(m_settingsButton);
        DrawButton(m_menuButton);
    }

    void PauseState::DrawButton(const Button& btn) {
        Texture2D tex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Menu.id);
        
        // Use a slightly darker tint for the button texture to make the light text pop
        Color tint = Color{200, 200, 200, 255};
        bool isPressed = btn.hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        Color textColor = isPressed ? components::Colors::MENU_BTN_TEXT_PRESS : (btn.hovered ? components::Colors::MENU_BTN_TEXT_HOVER : components::Colors::MENU_BTN_TEXT_NORMAL);

        UIRenderer::DrawButton(
            UISystem::GetFont(),
            tex,
            btn.bounds,
            btn.text.c_str(),
            26.0f,
            textColor,
            tint,
            btn.hovered,
            isPressed
        );
    }

    bool PauseState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

}
