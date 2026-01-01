#pragma once
#include "TestCommon.hpp"
#include "../src/systems/AstrolabeSystem.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/core/AstrolabeRegistry.hpp"
#include "../src/components/Progression.hpp"

TEST_CASE("AstrolabeSystem: Activation Logic") {
    using namespace NoMoreDay;
    entt::registry registry;
    auto entity = registry.create();
    
    // Setup scope for Registry and Logger
    TestSetupScope setup;
    AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

    auto& astrolabe = registry.emplace<AstrolabeComponent>(entity);
    astrolabe.available_points = 2;

    SUBCASE("Can activate root node") {
        CHECK(AstrolabeSystem::can_activate(registry, entity, 1) == true);
        CHECK(AstrolabeSystem::activate_node(registry, entity, 1) == true);
        CHECK(astrolabe.activated_nodes.contains(1));
        CHECK(astrolabe.available_points == 1);
        CHECK(registry.all_of<StatsDirty>(entity));
    }

    SUBCASE("Cannot activate without prerequisites") {
        // Node 2 depends on 1
        CHECK(AstrolabeSystem::can_activate(registry, entity, 2) == false);
        CHECK(AstrolabeSystem::activate_node(registry, entity, 2) == false);
    }

    SUBCASE("Can activate after prerequisites are met") {
        AstrolabeSystem::activate_node(registry, entity, 1);
        CHECK(AstrolabeSystem::can_activate(registry, entity, 2) == true);
        CHECK(AstrolabeSystem::activate_node(registry, entity, 2) == true);
        CHECK(astrolabe.available_points == 0);
    }

    SUBCASE("Cannot activate without points") {
        astrolabe.available_points = 0;
        CHECK(AstrolabeSystem::can_activate(registry, entity, 1) == false);
    }

    SUBCASE("Cannot activate already activated node") {
        AstrolabeSystem::activate_node(registry, entity, 1);
        CHECK(AstrolabeSystem::can_activate(registry, entity, 1) == false);
    }

    SUBCASE("Stat Integration: Activating node changes stats") {
        auto& combat = registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity); // Source of base stats
        
        // Recalculate once to get baseline
        StatsSystem::Recalculate(registry, entity);
        float baseline_str = combat.effective_strength;

        // Activate node 1 (+5 Strength)
        AstrolabeSystem::activate_node(registry, entity, 1);
        
        // Stats should be dirty, but we call Recalculate directly for testing
        StatsSystem::Recalculate(registry, entity);
        
        CHECK(combat.effective_strength == baseline_str + 5.0f);
    }

    SUBCASE("Persistence: JSON Serialization") {
        AstrolabeComponent c;
        c.available_points = 10;
        c.activated_nodes = {1, 2, 100};

        nlohmann::json j = c;
        
        CHECK(j["available_points"] == 10);
        CHECK(j["activated_nodes"].is_array());
        CHECK(j["activated_nodes"].size() == 3);

        AstrolabeComponent c2 = j.get<AstrolabeComponent>();
        CHECK(c2.available_points == 10);
        CHECK(c2.activated_nodes.size() == 3);
        CHECK(c2.activated_nodes.contains(1));
        CHECK(c2.activated_nodes.contains(100));
    }
}