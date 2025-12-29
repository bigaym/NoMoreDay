#pragma once

#include "../core/State.hpp"
#include <raylib.h>
#include <string>

namespace NoMoreDay {

    class MainMenuState : public IState {
    public:
        MainMenuState(StateManager& manager, SharedContext& context);
        virtual ~MainMenuState() = default;

        void OnEnter() override;
        void OnExit() override;

        bool OnUpdate(float dt) override;
        void OnRender() override;

    private:
        struct Button {
            Rectangle bounds;
            std::string text;
            bool hovered;
        };

        void DrawButton(const Button& btn);
        bool IsButtonClicked(const Button& btn);

        Button m_startButton;
        Button m_exitButton;
        
        Font m_titleFont;
        float m_titleOpacity = 0.0f;
    };

}
