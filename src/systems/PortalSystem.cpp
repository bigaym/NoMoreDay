#include "PortalSystem.hpp"
#include "../components/MapComponent.hpp"
#include "../components/Common.hpp"
#include "../tools/Logger.hpp"

PortalSystem::PortalSystem(NoMoreDay::SceneManager& sceneManager)
    : m_sceneManager(sceneManager) {}

void PortalSystem::Update(entt::registry& registry, float dt) {
    if (m_sceneManager.IsTransitioning()) return;

    auto portals = registry.view<PortalComponent, Position>();
    auto players = registry.view<PlayerTag, Position>();
    
    for (auto player : players) {
        const auto& pPos = players.get<Position>(player);
        
        for (auto portal : portals) {
            const auto& portalComp = portals.get<PortalComponent>(portal);
            if (!portalComp.isActive) continue;
            
            const auto& portPos = portals.get<Position>(portal);
            
            float dx = pPos.x - portPos.x;
            float dy = pPos.y - portPos.y;
            float distSq = dx*dx + dy*dy;
            
            // Interaction range (e.g., 20 units)
            if (distSq < 20.0f * 20.0f) {
                LOG_INFO("Player triggered portal to {} (Level {})", portalComp.targetBiome, portalComp.targetLevel);
                m_sceneManager.RequestTransition(portalComp.targetBiome, portalComp.targetLevel, portalComp.targetEntranceId);
                return; 
            }
        }
    }
}
