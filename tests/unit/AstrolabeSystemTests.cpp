#include "doctest.h"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/foundation/data/TalentData.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/Stats.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

// 辅助函数：创建测试用 TalentGraph
TalentGraph CreateTestGraph() {
    TalentGraph graph;
    
    // Blade Ascendant Tier 1 Minor
    AstrolabeTalentNode n1;
    n1.id = 1100; n1.profession = ProfessionID::BladeAscendant; 
    n1.tier = 1; n1.type = TalentNodeType::Minor; n1.maxPoints = 5;
    graph.nodes[1100] = n1;
    
    // Blade Ascendant Tier 2 Major
    AstrolabeTalentNode n2;
    n2.id = 1200; n2.profession = ProfessionID::BladeAscendant; 
    n2.tier = 2; n2.type = TalentNodeType::Major; n2.maxPoints = 3;
    graph.nodes[1200] = n2;
    
    // Blade Ascendant Tier 3 Core
    AstrolabeTalentNode n3;
    n3.id = 1300; n3.profession = ProfessionID::BladeAscendant; 
    n3.tier = 3; n3.type = TalentNodeType::Core; n3.maxPoints = 1;
    graph.nodes[1300] = n3;
    
    // Mage Core (for testing cross-profession seal)
    AstrolabeTalentNode n4;
    n4.id = 2300; n4.profession = ProfessionID::Mage; 
    n4.tier = 3; n4.type = TalentNodeType::Core; n4.maxPoints = 1;
    graph.nodes[2300] = n4;
    
    return graph;
}

TEST_SUITE("AstrolabeSystemTests") {

    TEST_CASE("[Unit] Tier 1 Unlock - No Affinity Required") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Tier 1 should be unlockable with 0 affinity
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1100) == true);
    }
    
    TEST_CASE("[Unit] Tier 2 Unlock - Affinity Threshold") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Tier 2 requires 10 affinity
        comp.professionAffinity[(int)ProfessionID::BladeAscendant] = 9;
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1200) == false);
        
        comp.professionAffinity[(int)ProfessionID::BladeAscendant] = 10;
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1200) == true);
    }
    
    TEST_CASE("[Unit] Core Unlock - Requires Vow") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        comp.professionAffinity[(int)ProfessionID::BladeAscendant] = 30;  // Exceeds Tier 3 threshold
        
        // Core node requires vow
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1300) == false);
        
        // After vow
        comp.mainProfession = (int)ProfessionID::BladeAscendant;
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 1300) == true);
        
        // Cross-profession Core should be sealed
        CHECK(AstrolabeSystem::canUnlockNode(graph, comp, 2300) == false);
    }
    
    TEST_CASE("[Unit] Take Vow - Once Only") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<AstrolabeComponent>(player);
        
        CHECK(AstrolabeSystem::canTakeVow(registry.get<AstrolabeComponent>(player), ProfessionID::BladeAscendant) == true);
        
        bool result = AstrolabeSystem::takeVow(registry, player, ProfessionID::BladeAscendant);
        CHECK(result == true);
        CHECK(registry.get<AstrolabeComponent>(player).mainProfession == (int)ProfessionID::BladeAscendant);
        
        // Second vow should fail
        CHECK(AstrolabeSystem::canTakeVow(registry.get<AstrolabeComponent>(player), ProfessionID::Mage) == false);
        result = AstrolabeSystem::takeVow(registry, player, ProfessionID::Mage);
        CHECK(result == false);
    }
    
    TEST_CASE("[Unit] Add Point - Affinity Accumulation") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<AstrolabeComponent>(player);
        registry.get<AstrolabeComponent>(player).available_points = 5;
        
        // Need to emplace CombatStats for AttributePipeline
        registry.emplace<CombatStats>(player);
        
        auto graph = CreateTestGraph();
        
        // Add 5 points to node 1100
        for (int i = 0; i < 5; ++i) {
            bool added = AstrolabeSystem::addPointToNode(registry, player, graph, 1100);
            CHECK(added == true);
        }
        
        auto& comp = registry.get<AstrolabeComponent>(player);
        CHECK(comp.nodePoints[1100] == 5);
        CHECK(comp.professionAffinity[(int)ProfessionID::BladeAscendant] == 5);
        CHECK(comp.available_points == 0);
        
        // 6th point should fail (max reached)
        bool added = AstrolabeSystem::addPointToNode(registry, player, graph, 1100);
        CHECK(added == false);
    }
    
    TEST_CASE("[Unit] Node Status - All States") {
        auto graph = CreateTestGraph();
        AstrolabeComponent comp;
        comp.available_points = 10;
        
        // Locked (no points, Tier 2)
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1200) == AstrolabeSystem::NodeStatus::Locked);
        
        // Available (Tier 1)
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::Available);
        
        // Activated (partial points)
        comp.nodePoints[1100] = 2;
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::Activated);
        
        // Fully Activated
        comp.nodePoints[1100] = 5;
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1100) == AstrolabeSystem::NodeStatus::FullyActivated);
        
        // Sealed (Core, wrong vow)
        comp.mainProfession = (int)ProfessionID::BladeAscendant;
        comp.professionAffinity[(int)ProfessionID::Mage] = 30;  // Mage affinity
        CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 2300) == AstrolabeSystem::NodeStatus::Sealed);
    }
}