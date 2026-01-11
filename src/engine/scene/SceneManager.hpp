#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <future>
#include "game/systems/world/LevelManager.hpp"

namespace NoMoreDay {

class SceneManager {
public:
    SceneManager(LevelManager& levelManager, entt::registry& registry);
    ~SceneManager();  // 确保异步任务在销毁前完成
    
    // Request a transition to a new biome/level
    void RequestTransition(const std::string& biome, int level = 1, const std::string& entranceId = "start");
    
    // Update logic (handling fade, async loading check)
    void Update(float dt);
    
    // Render transition overlay
    void RenderOverlay();
    
    bool IsTransitioning() const { return m_isTransitioning; }

private:
    void StartTransition();
    void PerformLevelLoad(); // Trigger async load
    void ApplyLoadedLevel(); // Apply loaded data to main thread
    
    LevelManager& m_levelManager;
    entt::registry& m_registry;
    
    // Transition State
    bool m_isTransitioning = false;
    float m_fadeAlpha = 0.0f;
    enum class State { FADE_OUT, LOADING, WAIT_FOR_FUTURE, FADE_IN, IDLE };
    State m_state = State::IDLE;
    
    // Target Info
    std::string m_targetBiome;
    int m_targetLevel;
    std::string m_targetEntranceId;
    
    // Async data
    // Note: std::future cannot be copied, so we must be careful if SceneManager is moved (it shouldn't be).
    std::future<LevelManager::LevelData> m_loadingFuture;
};

}
