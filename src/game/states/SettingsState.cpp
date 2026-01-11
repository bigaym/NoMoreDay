#include "game/states/SettingsState.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/systems/ui/UISystem.hpp"
#include <raylib.h>
#include <iomanip>
#include <sstream>

namespace NoMoreDay {

    SettingsState::SettingsState(StateManager& manager, SharedContext& context)
        : IState(manager, context) 
    {
        float screenWidth = (float)GetScreenWidth();
        float screenHeight = (float)GetScreenHeight();

        float sliderWidth = 300;
        float sliderHeight = 20;
        float centerX = (screenWidth - sliderWidth) / 2.0f;

        m_zoomSlider = {
            { centerX, screenHeight * 0.4f, sliderWidth, sliderHeight },
            &m_context->settings->cameraZoom,
            1.0f,
            2.0f,
            "Camera Zoom",
            false
        };

        float btnWidth = 200;
        float btnHeight = 50;
        m_backButton = { { (screenWidth - btnWidth) / 2.0f, screenHeight * 0.7f, btnWidth, btnHeight }, "BACK", false };
    }

    void SettingsState::OnEnter() {}
    
    void SettingsState::OnExit() {
        if (m_context->settings) {
            m_context->settings->Save();
        }
    }

    bool SettingsState::OnUpdate(float dt) {
        UpdateSlider(m_zoomSlider);
        
        m_backButton.hovered = CheckCollisionPointRec(GetMousePosition(), m_backButton.bounds);

        if (IsButtonClicked(m_backButton) || IsKeyReleased(KEY_ESCAPE)) {
            m_stateManager->PopState();
        }

        return false;
    }

    void SettingsState::OnRender() {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 200 });
        
        Font font = UISystem::GetFont();
        const char* title = "SETTINGS";
        float titleSize = 40.0f;
        float titleWidth = IsFontValid(font) ? MeasureTextEx(font, title, titleSize, 1.0f).x : (float)MeasureText(title, (int)titleSize);
        
        if (IsFontValid(font)) {
            DrawTextEx(font, title, { (GetScreenWidth() - titleWidth) / 2.0f, GetScreenHeight() * 0.2f }, titleSize, 1.0f, RAYWHITE);
        } else {
            DrawText(title, (int)((GetScreenWidth() - titleWidth) / 2.0f), (int)(GetScreenHeight() * 0.2f), (int)titleSize, RAYWHITE);
        }

        DrawSlider(m_zoomSlider);
        DrawButton(m_backButton);
    }

    void SettingsState::UpdateSlider(Slider& slider) {
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, slider.bounds)) {
            slider.dragging = true;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            slider.dragging = false;
        }

        if (slider.dragging) {
            float pct = (mousePos.x - slider.bounds.x) / slider.bounds.width;
            pct = Clamp(pct, 0.0f, 1.0f);
            float rawVal = slider.min + (slider.max - slider.min) * pct;
            // Snap to 0.1 precision
            *slider.value = roundf(rawVal * 10.0f) / 10.0f;
        }
    }

    void SettingsState::DrawSlider(Slider& slider) {
        DrawRectangleRec(slider.bounds, DARKGRAY);
        
        float pct = (*slider.value - slider.min) / (slider.max - slider.min);
        Rectangle progress = { slider.bounds.x, slider.bounds.y, slider.bounds.width * pct, slider.bounds.height };
        DrawRectangleRec(progress, GOLD);
        DrawRectangleLinesEx(slider.bounds, 2, LIGHTGRAY);

        // Handle
        DrawCircle((int)(slider.bounds.x + slider.bounds.width * pct), (int)(slider.bounds.y + slider.bounds.height / 2.0f), 10, RAYWHITE);

        // Label and Value
        std::stringstream ss;
        ss << slider.label << ": " << std::fixed << std::setprecision(1) << *slider.value;
        std::string fullText = ss.str();

        Font font = UISystem::GetFont();
        float fontSize = 20.0f;
        if (IsFontValid(font)) {
            DrawTextEx(font, fullText.c_str(), { slider.bounds.x, slider.bounds.y - 30 }, fontSize, 1.0f, RAYWHITE);
        } else {
            DrawText(fullText.c_str(), (int)slider.bounds.x, (int)(slider.bounds.y - 30), (int)fontSize, RAYWHITE);
        }
    }

    void SettingsState::DrawButton(const Button& btn) {
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

    bool SettingsState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

}
