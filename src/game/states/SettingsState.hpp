#pragma once

#include "engine/scene/State.hpp"
#include <raylib.h>
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

        Slider m_zoomSlider;
        Slider m_shakeSlider;
        Button m_backButton;
    };

}
