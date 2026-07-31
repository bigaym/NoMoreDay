#pragma once

#include "game/scene/State.hpp"
#include <raylib.h>
#include <future>
#include <functional>
#include <string>

namespace NoMoreDay {

    class LoadingState : public IState {
    public:
        using LoadTask = std::function<void()>;
        using OnComplete = std::function<void(StateManager&)>;

        LoadingState(StateManager& manager, SharedContext& context, 
                     LoadTask task, OnComplete onComplete);
        virtual ~LoadingState();

        void OnEnter() override;
        void OnExit() override;

        bool OnUpdate(float dt) override;
        void OnRender() override;

    private:
        LoadTask m_loadTask;
        OnComplete m_onComplete;
        std::future<void> m_future;
        
        float m_timer = 0.0f;
        std::string m_loadingText = "LOADING";
        int m_dots = 0;
    };

}
