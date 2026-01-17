#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <future>
#include "game/systems/world/LevelManager.hpp"
#include "game/data/MosaicData.hpp"

namespace NoMoreDay {

class SceneManager {
public:
    SceneManager(LevelManager& levelManager, entt::registry& registry);
    ~SceneManager();  // 确保异步任务在销毁前完成
    
    // Request a transition to a new biome/level
    void RequestTransition(const std::string& biome, int level = 1, const std::string& entranceId = "start");
    
    // Request a transition to a mosaic level
    void RequestMosaicTransition(const NoMoreDay::MosaicGrid& grid, const NoMoreDay::ResonanceResult& resonance);
    
    // Update logic (handling fade, async loading check)
    void Update(float dt);
    
    // Render transition overlay
    void RenderOverlay();
    
    bool IsTransitioning() const { return m_isTransitioning; }
    
    // Origin tracking for return portals
    void SetOriginInfo(const std::string& biome, int level, float x, float y);
    const std::string& GetCurrentBiome() const { return m_currentBiome; }
    int GetCurrentLevel() const { return m_currentLevel; }
    const std::string& GetOriginBiome() const { return m_originBiome; }
    int GetOriginLevel() const { return m_originLevel; }
    float GetOriginX() const { return m_originX; }
    float GetOriginY() const { return m_originY; }

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
    int m_targetLevel = 1;
    std::string m_targetEntranceId;
    
    // Current scene info
    std::string m_currentBiome = "cave";
    int m_currentLevel = 1;
    
    // Origin info (for return portals)
    std::string m_originBiome;
    int m_originLevel = 0;
    float m_originX = 0.0f;
    float m_originY = 0.0f;

    // Combat tracking for kill count resets
    std::string m_lastCombatBiome;
    int m_lastCombatLevel = 0;
    
    // Async data
    // Note: std::future cannot be copied, so we must be careful if SceneManager is moved (it shouldn't be).
    std::future<LevelManager::LevelData> m_loadingFuture;

    // Mosaic Transition Data
    bool m_isMosaicTransition = false;
    NoMoreDay::MosaicGrid m_pendingMosaicGrid;
    NoMoreDay::ResonanceResult m_pendingResonance;
};

}

