#pragma once

#include <entt/entt.hpp>
#include "engine/scene/SceneManager.hpp"

class PortalSystem {
public:
    PortalSystem(NoMoreDay::SceneManager& sceneManager);
    
    void Update(entt::registry& registry, float dt);
    
private:
    NoMoreDay::SceneManager& m_sceneManager;
};
