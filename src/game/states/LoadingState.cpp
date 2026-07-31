#include "game/states/LoadingState.hpp"
#include "game/scene/StateManager.hpp"
#include "game/systems/ui/UISystem.hpp" // Include UISystem
#include <cmath>

namespace NoMoreDay {

    LoadingState::LoadingState(StateManager& manager, SharedContext& context, 
                               LoadTask task, OnComplete onComplete)
        : IState(manager, context), m_loadTask(std::move(task)), m_onComplete(std::move(onComplete))
    {
    }

    LoadingState::~LoadingState() {
        if (m_future.valid()) {
            m_future.wait();
        }
    }

    void LoadingState::OnEnter() {
        // Launch the loading task asynchronously
        m_future = std::async(std::launch::async, m_loadTask);
    }

    void LoadingState::OnExit() {
        // Cleanup if needed
    }

    bool LoadingState::OnUpdate(float dt) {
        m_timer += dt;
        if (m_timer > 0.5f) {
            m_timer = 0.0f;
            m_dots = (m_dots + 1) % 4;
            m_loadingText = "LOADING";
            for (int i = 0; i < m_dots; ++i) m_loadingText += ".";
        }

        // Check if future is ready
        if (m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            m_future.get(); // Propagate exceptions if any
            
            // Execute completion callback (Main Thread)
            if (m_onComplete) {
                m_onComplete(*m_stateManager);
            }
            // Note: m_onComplete usually changes state, so this object might be destroyed here.
            // Return false to stop updates.
            return false;
        }

        return true; // Block updates below
    }

    void LoadingState::OnRender() {
        // Draw Loading Screen
        ClearBackground(BLACK);

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Draw Spinner or Text
        Font font = UISystem::GetFont();
        float fontSize = 40.0f;
        float textWidth = IsFontValid(font) ? MeasureTextEx(font, m_loadingText.c_str(), fontSize, 1.0f).x : (float)MeasureText(m_loadingText.c_str(), (int)fontSize);
        
        if (IsFontValid(font)) {
            DrawTextEx(font, m_loadingText.c_str(), { (screenWidth - textWidth) / 2.0f, screenHeight / 2.0f - fontSize / 2.0f }, fontSize, 1.0f, RAYWHITE);
        } else {
            DrawText(m_loadingText.c_str(), (int)(screenWidth - textWidth) / 2, (int)(screenHeight / 2 - fontSize / 2), (int)fontSize, RAYWHITE);
        }

        // Optional: Draw spinning rect
        float time = (float)GetTime();
        DrawRectanglePro(
            { (float)screenWidth/2, (float)screenHeight/2 + 60, 20, 20 }, 
            { 10, 10 }, 
            time * 360.0f, 
            SKYBLUE
        );
    }

}
