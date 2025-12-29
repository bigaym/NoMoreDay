#pragma once

#include "../core/State.hpp"

namespace NoMoreDay {

    class InventoryState : public IState {
    public:
        using IState::IState;

        void OnEnter() override;
        void OnExit() override;
        bool OnUpdate(float dt) override;
        void OnRender() override;
        
        bool IsTransparent() const override { return true; }
    };

}
