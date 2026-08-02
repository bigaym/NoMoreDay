#pragma once

#include "game/SharedContext.hpp"

namespace NoMoreDay {

    class StateManager;

    class IState {
    public:
        IState(StateManager& stateManager, SharedContext& context)
            : m_stateManager(&stateManager), m_context(&context) {}

        virtual ~IState() = default;

        virtual void OnEnter() {}
        virtual void OnExit() {}
        
        // Returns false to prevent lower states from updating (e.g., Pause menu blocks Gameplay)
        virtual bool OnUpdate(float dt) = 0;

        // Render the state
        virtual void OnRender() = 0;

        // If true, states below this one will be rendered.
        virtual bool IsTransparent() const { return true; }

        // Optional: Called when another state is pushed on top of this one
        virtual void OnSuspend() {}

        // Optional: Called when the state on top is popped
        virtual void OnWakeup() {}

    protected:
        StateManager* m_stateManager;
        SharedContext* m_context;
    };

}
