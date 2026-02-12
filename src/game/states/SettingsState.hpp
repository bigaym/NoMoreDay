#pragma once

#include "engine/scene/State.hpp"
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

        Slider m_zoomSlider;
        Slider m_shakeSlider;
        Button m_gameplayTabButton;
        Button m_graphicsTabButton;
        Button m_qualityLeftButton;
        Button m_qualityRightButton;
        Button m_backButton;
        Tab m_activeTab = Tab::Gameplay;
    };

}
