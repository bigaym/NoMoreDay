#pragma once

#include <entt/entt.hpp>
#include "engine/scene/SceneManager.hpp"
#include "raylib.h"

// Forward declarations
struct PortalComponent;
enum class PortalType : uint8_t;

namespace NoMoreDay {

class PortalSystem {
public:
    PortalSystem(SceneManager& sceneManager);
    
    void Update(entt::registry& registry, float dt);
    void Render(entt::registry& registry, const Camera2D& camera);
    
    // Town Portal API
    static void StartTownPortalCast(entt::registry& registry, entt::entity caster);
    static void CancelTownPortalCast(entt::registry& registry, entt::entity caster);
    
private:
    void UpdatePortalCollision(entt::registry& registry);
    void UpdateTownPortalCasting(entt::registry& registry, float dt);
    void SpawnTownPortal(entt::registry& registry, entt::entity caster);
    void UpdatePortalAnimations(entt::registry& registry, float dt);
    
    SceneManager& m_sceneManager;
};

}
