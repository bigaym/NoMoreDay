#pragma once

#include <entt/entt.hpp>
#include "game/application/scene/SceneManager.hpp"
#include "raylib.h"

// Forward declarations
struct PortalComponent;
enum class PortalType : uint8_t;

namespace NoMoreDay {

class PortalSystem {
public:
    PortalSystem(SceneManager& sceneManager);
    ~PortalSystem();
    
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
    void AdvanceRiftLayer(entt::registry& registry, entt::entity player);
    
    SceneManager& m_sceneManager;
    entt::entity m_lastTriggeredPortal = entt::null;

    // VFX Resources
    Shader m_vortexShader;
    Texture2D m_noiseTexture;
    int m_locTime = -1;
    int m_locColor = -1;
    int m_locSwirl = -1;
    int m_locCore = -1;
};

}
