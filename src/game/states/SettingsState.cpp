#include "game/states/SettingsState.hpp"
#include "game/scene/StateManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UIRenderer.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
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
        float tabWidth = 220.0f;
        float tabHeight = 56.0f;
        float tabGap = 20.0f;
        float tabsX = (screenWidth - (tabWidth * 2.0f + tabGap)) / 2.0f;
        float tabsY = screenHeight * 0.28f;

        m_zoomSlider = {
            { centerX, screenHeight * 0.4f, sliderWidth, sliderHeight },
            &m_context->settings->cameraZoom,
            1.0f,
            2.0f,
            "Camera Zoom",
            false
        };

        m_shakeSlider = {
            { centerX, screenHeight * 0.5f, sliderWidth, sliderHeight },
            &m_context->settings->shakeIntensity,
            0.0f,
            2.0f,
            "Screen Shake Intensity",
            false
        };

        m_gameplayTabButton = {{tabsX, tabsY, tabWidth, tabHeight}, "GAMEPLAY", false};
        m_graphicsTabButton = {
            {tabsX + tabWidth + tabGap, tabsY, tabWidth, tabHeight}, "GRAPHICS", false};

        float qualityCenterX = screenWidth * 0.5f;
        float qualityRowY = screenHeight * 0.48f;
        m_qualityLeftButton = {
            {qualityCenterX - 220.0f, qualityRowY - 28.0f, 64.0f, 56.0f},
            "<",
            false};
        m_qualityRightButton = {
            {qualityCenterX + 156.0f, qualityRowY - 28.0f, 64.0f, 56.0f},
            ">",
            false};

        float toggleButtonW = 140.0f;
        float toggleButtonH = 48.0f;
        float toggleButtonX = qualityCenterX + 110.0f;
        float toggleBaseY = screenHeight * 0.55f;
        float toggleRowGap = screenHeight * 0.065f;
        m_v3ToggleButton = {{toggleButtonX, toggleBaseY, toggleButtonW, toggleButtonH},
                            "TOGGLE",
                            false};
        m_clusteredToggleButton = {
            {toggleButtonX, toggleBaseY + toggleRowGap, toggleButtonW, toggleButtonH},
            "TOGGLE",
            false};
        m_normalToggleButton = {
            {toggleButtonX, toggleBaseY + toggleRowGap * 2.0f, toggleButtonW, toggleButtonH},
            "TOGGLE",
            false};
        m_specularToggleButton = {
            {toggleButtonX, toggleBaseY + toggleRowGap * 3.0f, toggleButtonW, toggleButtonH},
            "TOGGLE",
            false};

        float btnWidth = 260;
        float btnHeight = 70;
        m_backButton = { { (screenWidth - btnWidth) / 2.0f, screenHeight * 0.87f, btnWidth, btnHeight }, "BACK", false };
    }

    void SettingsState::OnEnter() {
        if (m_context->settings) {
            LOG_INFO("SettingsState: Enter (tab=Gameplay, tier={}, zoom={:.1f}, shake={:.1f})",
                     std::string(GameSettings::RenderQualityTierToStringView(
                                     m_context->settings->renderQualityTier)),
                     m_context->settings->cameraZoom,
                     m_context->settings->shakeIntensity);
        } else {
            LOG_WARN("SettingsState: Enter without settings context.");
        }
    }
    
    void SettingsState::OnExit() {
        if (m_context->settings) {
            m_context->settings->Save();
            LOG_INFO("SettingsState: Saved settings (tier={}, zoom={:.1f}, shake={:.1f}, fps={})",
                     std::string(GameSettings::RenderQualityTierToStringView(
                                     m_context->settings->renderQualityTier)),
                     m_context->settings->cameraZoom,
                     m_context->settings->shakeIntensity,
                     m_context->settings->targetFPS);
        }
    }

    bool SettingsState::OnUpdate(float dt) {
        Vector2 mousePos = GetMousePosition();

        m_gameplayTabButton.hovered =
            CheckCollisionPointRec(mousePos, m_gameplayTabButton.bounds);
        m_graphicsTabButton.hovered =
            CheckCollisionPointRec(mousePos, m_graphicsTabButton.bounds);

        if (IsButtonClicked(m_gameplayTabButton)) {
            m_activeTab = Tab::Gameplay;
            LOG_INFO("SettingsState: Switched tab -> Gameplay");
        } else if (IsButtonClicked(m_graphicsTabButton)) {
            m_activeTab = Tab::Graphics;
            LOG_INFO("SettingsState: Switched tab -> Graphics");
        }

        if (m_activeTab == Tab::Gameplay) {
            UpdateSlider(m_zoomSlider);
            UpdateSlider(m_shakeSlider);

            // Push update to RenderSystem immediately so we can see it in real-time
            if (m_context->settings) {
                RenderSystem::SetShakeMultiplier(m_context->settings->shakeIntensity);
            }
        } else {
            m_qualityLeftButton.hovered =
                CheckCollisionPointRec(mousePos, m_qualityLeftButton.bounds);
            m_qualityRightButton.hovered =
                CheckCollisionPointRec(mousePos, m_qualityRightButton.bounds);
            m_v3ToggleButton.hovered =
                CheckCollisionPointRec(mousePos, m_v3ToggleButton.bounds);
            m_clusteredToggleButton.hovered =
                CheckCollisionPointRec(mousePos, m_clusteredToggleButton.bounds);
            m_normalToggleButton.hovered =
                CheckCollisionPointRec(mousePos, m_normalToggleButton.bounds);
            m_specularToggleButton.hovered =
                CheckCollisionPointRec(mousePos, m_specularToggleButton.bounds);

            if (IsButtonClicked(m_qualityLeftButton)) {
                CycleRenderQualityTier(-1);
            } else if (IsButtonClicked(m_qualityRightButton)) {
                CycleRenderQualityTier(+1);
            }

            if (IsButtonClicked(m_v3ToggleButton)) {
                ToggleGraphicsOption(GraphicsOption::V3Enabled);
            } else if (IsButtonClicked(m_clusteredToggleButton)) {
                ToggleGraphicsOption(GraphicsOption::ClusteredLighting);
            } else if (IsButtonClicked(m_normalToggleButton)) {
                ToggleGraphicsOption(GraphicsOption::NormalLighting);
            } else if (IsButtonClicked(m_specularToggleButton)) {
                ToggleGraphicsOption(GraphicsOption::Specular);
            }
        }

        m_backButton.hovered = CheckCollisionPointRec(mousePos, m_backButton.bounds);

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

        Button gameplayTab = m_gameplayTabButton;
        Button graphicsTab = m_graphicsTabButton;
        gameplayTab.hovered = gameplayTab.hovered || (m_activeTab == Tab::Gameplay);
        graphicsTab.hovered = graphicsTab.hovered || (m_activeTab == Tab::Graphics);
        DrawButton(gameplayTab);
        DrawButton(graphicsTab);

        if (m_activeTab == Tab::Gameplay) {
            DrawSlider(m_zoomSlider);
            DrawSlider(m_shakeSlider);
        } else {
            DrawQualityTierSelector();
            DrawButton(m_qualityLeftButton);
            DrawButton(m_qualityRightButton);
            DrawGraphicsToggleRow("V3 Rendering",
                                  GetGraphicsOptionValueText(GraphicsOption::V3Enabled),
                                  GetScreenHeight() * 0.55f);
            DrawGraphicsToggleRow(
                "Clustered Lighting",
                GetGraphicsOptionValueText(GraphicsOption::ClusteredLighting),
                GetScreenHeight() * 0.615f);
            DrawGraphicsToggleRow(
                "Normal Lighting",
                GetGraphicsOptionValueText(GraphicsOption::NormalLighting),
                GetScreenHeight() * 0.68f);
            DrawGraphicsToggleRow("Specular Highlights",
                                  GetGraphicsOptionValueText(GraphicsOption::Specular),
                                  GetScreenHeight() * 0.745f);
            DrawButton(m_v3ToggleButton);
            DrawButton(m_clusteredToggleButton);
            DrawButton(m_normalToggleButton);
            DrawButton(m_specularToggleButton);
        }

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
        Texture2D tex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Menu.id);
        
        UIRenderer::DrawButton(
            UISystem::GetFont(),
            tex,
            btn.bounds,
            btn.text.c_str(),
            26.0f,
            btn.hovered ? YELLOW : WHITE,
            WHITE,
            btn.hovered,
            btn.hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)
        );
    }

    bool SettingsState::IsButtonClicked(const Button& btn) {
        return btn.hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

    void SettingsState::ApplyRenderQualityTier() {
        if (m_context->settings == nullptr) {
            return;
        }
        render::core::QualityTierManager::Get().ForceTier(
            m_context->settings->renderQualityTier);
    }

    void SettingsState::CycleRenderQualityTier(int direction) {
        if (m_context->settings == nullptr) {
            return;
        }

        const auto previousTier = m_context->settings->renderQualityTier;
        int tier = static_cast<int>(m_context->settings->renderQualityTier);
        tier += direction;
        if (tier < static_cast<int>(render::core::QualityTier::Low)) {
            tier = static_cast<int>(render::core::QualityTier::Ultra);
        } else if (tier > static_cast<int>(render::core::QualityTier::Ultra)) {
            tier = static_cast<int>(render::core::QualityTier::Low);
        }

        m_context->settings->renderQualityTier =
            static_cast<render::core::QualityTier>(tier);
        ApplyRenderQualityTier();
        LOG_INFO("SettingsState: Render tier {} -> {}",
                 std::string(GameSettings::RenderQualityTierToStringView(previousTier)),
                 std::string(GameSettings::RenderQualityTierToStringView(
                     m_context->settings->renderQualityTier)));
    }

    void SettingsState::DrawQualityTierSelector() const {
        if (m_context->settings == nullptr) {
            return;
        }

        const std::string label = "Render Quality";
        const std::string current =
            std::string(GameSettings::RenderQualityTierToStringView(
                m_context->settings->renderQualityTier));
        const std::string valueText = "[" + current + "]";
        Font font = UISystem::GetFont();

        const float labelY = GetScreenHeight() * 0.43f;
        const float valueY = GetScreenHeight() * 0.48f;
        const float centerX = GetScreenWidth() * 0.5f;

        if (IsFontValid(font)) {
            const float labelW = MeasureTextEx(font, label.c_str(), 24.0f, 1.0f).x;
            const float valueW = MeasureTextEx(font, valueText.c_str(), 32.0f, 1.0f).x;
            DrawTextEx(font, label.c_str(), {centerX - labelW * 0.5f, labelY}, 24.0f,
                       1.0f, RAYWHITE);
            DrawTextEx(font, valueText.c_str(), {centerX - valueW * 0.5f, valueY}, 32.0f,
                       1.0f, GOLD);
        } else {
            const int labelW = MeasureText(label.c_str(), 24);
            const int valueW = MeasureText(valueText.c_str(), 32);
            DrawText(label.c_str(), static_cast<int>(centerX - labelW * 0.5f),
                     static_cast<int>(labelY), 24, RAYWHITE);
            DrawText(valueText.c_str(), static_cast<int>(centerX - valueW * 0.5f),
                     static_cast<int>(valueY), 32, GOLD);
        }
    }

    void SettingsState::DrawGraphicsToggleRow(const std::string& label,
                                              const std::string& valueText,
                                              float y) const {
        Font font = UISystem::GetFont();
        const float labelX = GetScreenWidth() * 0.32f;
        const float valueX = GetScreenWidth() * 0.52f;
        const float valueY = y + 8.0f;

        if (IsFontValid(font)) {
            DrawTextEx(font, label.c_str(), {labelX, y}, 24.0f, 1.0f, RAYWHITE);
            DrawTextEx(font, valueText.c_str(), {valueX, valueY}, 28.0f, 1.0f, GOLD);
        } else {
            DrawText(label.c_str(), static_cast<int>(labelX), static_cast<int>(y), 24,
                     RAYWHITE);
            DrawText(valueText.c_str(), static_cast<int>(valueX),
                     static_cast<int>(valueY), 28, GOLD);
        }
    }

    std::string SettingsState::GetGraphicsOptionValueText(GraphicsOption option) const {
        const auto& config =
            render::core::QualityTierManager::Get().GetConfig();
        switch (option) {
        case GraphicsOption::V3Enabled:
            return config.v3Enabled ? "ON" : "OFF";
        case GraphicsOption::ClusteredLighting:
            return config.clusteredLightingEnabled ? "ON" : "OFF";
        case GraphicsOption::NormalLighting:
            return config.normalLightingEnabled ? "ON" : "OFF";
        case GraphicsOption::Specular:
            return config.specularEnabled ? "ON" : "OFF";
        }
        return "OFF";
    }

    void SettingsState::ToggleGraphicsOption(GraphicsOption option) {
        auto& qualityManager = render::core::QualityTierManager::Get();
        const auto& config = qualityManager.GetConfig();

        switch (option) {
        case GraphicsOption::V3Enabled: {
            const bool next = !config.v3Enabled;
            qualityManager.SetV3Enabled(next);
            LOG_INFO("SettingsState: V3 rendering -> {}", next ? "ON" : "OFF");
            break;
        }
        case GraphicsOption::ClusteredLighting: {
            const bool next = !config.clusteredLightingEnabled;
            qualityManager.SetClusteredLightingEnabled(next);
            LOG_INFO("SettingsState: Clustered lighting -> {}", next ? "ON" : "OFF");
            break;
        }
        case GraphicsOption::NormalLighting: {
            const bool next = !config.normalLightingEnabled;
            qualityManager.SetNormalLightingEnabled(next);
            LOG_INFO("SettingsState: Normal lighting -> {}", next ? "ON" : "OFF");
            break;
        }
        case GraphicsOption::Specular: {
            const bool next = !config.specularEnabled;
            qualityManager.SetSpecularEnabled(next);
            LOG_INFO("SettingsState: Specular highlights -> {}", next ? "ON" : "OFF");
            break;
        }
        }
    }

}
