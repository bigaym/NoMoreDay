#pragma once

#include "game/application/scene/State.hpp"
#include <raylib.h>
#include <string>

namespace NoMoreDay {

    class PauseState : public IState {
    public:
        PauseState(StateManager& manager, SharedContext& context);
        virtual ~PauseState() = default;

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

        Button m_resumeButton;
        Button m_unstuckButton;
        Button m_settingsButton;
        Button m_menuButton;

        bool m_confirmMainMenu = false;
        float m_inputDebounce = 0.0f;
    };

}
