#pragma once

#include "game/application/scene/State.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>

namespace NoMoreDay {

    class SettingsState : public IState {
    public:
        SettingsState(StateManager& manager, SharedContext& context);
        virtual ~SettingsState() = default;

        void OnEnter() override;
        void OnExit() override;

        bool OnUpdate(float dt) override;
        void OnRender() override;

    private:
        enum class Tab : uint8_t {
            Gameplay = 0,
            Graphics = 1
        };

        enum class GraphicsOption : uint8_t {
            V3Enabled = 0,
            ClusteredLighting = 1,
            NormalLighting = 2,
            Specular = 3
        };

        struct Slider {
            Rectangle bounds;
            float* value;
            float min;
            float max;
            std::string label;
            bool dragging;
        };

        struct Button {
            Rectangle bounds;
            std::string text;
            bool hovered;
        };

        void DrawSlider(Slider& slider);
        void UpdateSlider(Slider& slider);
        void DrawButton(const Button& btn);
        bool IsButtonClicked(const Button& btn);
        void ApplyRenderQualityTier();
        void CycleRenderQualityTier(int direction);
        void DrawQualityTierSelector() const;
        void DrawGraphicsToggleRow(const std::string& label,
                                   const std::string& valueText,
                                   float y) const;
        void ToggleGraphicsOption(GraphicsOption option);
        std::string GetGraphicsOptionValueText(GraphicsOption option) const;

        Slider m_zoomSlider;
        Slider m_shakeSlider;
        Button m_gameplayTabButton;
        Button m_graphicsTabButton;
        Button m_qualityLeftButton;
        Button m_qualityRightButton;
        Button m_v3ToggleButton;
        Button m_clusteredToggleButton;
        Button m_normalToggleButton;
        Button m_specularToggleButton;
        Button m_backButton;
        Tab m_activeTab = Tab::Gameplay;
    };

}
