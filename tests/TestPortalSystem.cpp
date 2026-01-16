#include "TestCommon.hpp"
#include "game/systems/world/PortalSystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/PlayerState.hpp"

namespace NoMoreDay {

TEST_CASE("PortalSystem - Town Portal Casting") {
    entt::registry registry;
    LevelManager lm;
    SceneManager sm(lm, registry);
    PortalSystem ps(sm);
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    
    SUBCASE("Start casting") {
        PortalSystem::StartTownPortalCast(registry, player);
        auto* casting = registry.try_get<TownPortalCastingComponent>(player);
        REQUIRE(casting != nullptr);
        CHECK(casting->isCasting == true);
        CHECK(casting->elapsedTime == 0.0f);
    }
    
    SUBCASE("Interruption by movement") {
        PortalSystem::StartTownPortalCast(registry, player);
        auto& pos = registry.get<Position>(player);
        pos.x += 10.0f; // Move player
        
        ps.Update(registry, 0.1f);
        auto& casting = registry.get<TownPortalCastingComponent>(player);
        CHECK(casting.isCasting == false);
    }
    
    SUBCASE("Complete casting") {
        PortalSystem::StartTownPortalCast(registry, player);
        auto& casting = registry.get<TownPortalCastingComponent>(player);
        
        ps.Update(registry, casting.castTime + 0.1f);
        
        CHECK(casting.isCasting == false);
        
        // Should have spawned a portal
        auto portalView = registry.view<PortalComponent>();
        CHECK(portalView.size() == 1);
        
        auto portal = portalView.front();
        const auto& pc = portalView.get<PortalComponent>(portal);
        CHECK(pc.type == PortalType::Town);
    }
}

TEST_CASE("PortalSystem - Collision and Transition") {
    entt::registry registry;
    LevelManager lm;
    SceneManager sm(lm, registry);
    PortalSystem ps(sm);
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    
    auto portal = registry.create();
    PortalComponent pc;
    pc.type = PortalType::Dungeon;
    pc.targetBiome = "town";
    pc.isActive = true;
    registry.emplace<PortalComponent>(portal, pc);
    registry.emplace<Position>(portal, 105.0f, 100.0f); // Close to player
    
    SUBCASE("Collision triggers transition") {
        ps.Update(registry, 0.1f);
        CHECK(sm.IsTransitioning() == true);
    }
    
    SUBCASE("Disabled portal no transition") {
        registry.get<PortalComponent>(portal).isActive = false;
        ps.Update(registry, 0.1f);
        CHECK(sm.IsTransitioning() == false);
    }
}

} // namespace NoMoreDay
