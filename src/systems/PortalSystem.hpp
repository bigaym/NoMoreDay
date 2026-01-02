#pragma once

#include <entt/entt.hpp>
#include "../core/SceneManager.hpp"

class PortalSystem {
public:
    PortalSystem(NoMoreDay::SceneManager& sceneManager);
    
    void Update(entt::registry& registry, float dt);
    
private:
    NoMoreDay::SceneManager& m_sceneManager;
};
