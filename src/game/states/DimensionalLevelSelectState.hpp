#pragma once
#include "engine/scene/State.hpp"
#include "game/components/PlayerState.hpp"
#include <vector>

namespace NoMoreDay {

class DimensionalLevelSelectState : public IState {
public:
    DimensionalLevelSelectState(StateManager& stateManager, SharedContext& context);
    virtual ~DimensionalLevelSelectState() = default;

    void OnEnter() override;
    void OnExit() override;
    bool OnUpdate(float dt) override;
    void OnRender() override;

private:
    void RenderList();
    void RenderButtons();
    void ConfirmSelection();

    // Data
    int m_minLevel = 1;
    int m_maxLevel = 100;
    int m_selectedLevel = 1;
    int m_playerLevel = 1;

    // UI State
    float m_scrollOffset = 0.0f;
    int m_hoveredIndex = -1;
    
    // UI Constants
    const int WINDOW_WIDTH = 400;
    const int WINDOW_HEIGHT = 600;
    const int LIST_ITEM_HEIGHT = 40;
    const int VISIBLE_ITEMS = 10;
};

} // namespace NoMoreDay
