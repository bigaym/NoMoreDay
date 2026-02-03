#include "doctest.h"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("[Unit] AstrolabeSystem - Logic") {
    // Setup
    entt::registry registry;
    entt::entity player = registry.create();
    auto& comp = registry.emplace<AstrolabeComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<GlobalModifierComponent>(player);
    registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<PrimaryStats>(player);
    
    TalentGraph graph;
    REQUIRE(TalentLoader::LoadProfessionTalents("assets/data/profession_talents.json", graph));
    
    // Sync Registry (needed for AttributePipeline)
    AstrolabeRegistry::Get().SetGraph(graph);
    
    // Give Points
    comp.available_points = 100;
    
    // Test 1: Tier 1 Unlock
    // Blade Ascendant Node 1001 (Tier 1) should be available
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1001) == AstrolabeSystem::NodeStatus::Available);
    
    // Invest 5 points
    for(int i=0; i<5; ++i) {
        CHECK(AstrolabeSystem::addPointToNode(registry, player, graph, 1001));
    }
    
    CHECK(comp.available_points == 95);
    CHECK(comp.getAffinity(ProfessionID::BladeAscendant) == 5);
    CHECK(comp.getNodePoints(1001) == 5);
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 1001) == AstrolabeSystem::NodeStatus::FullyActivated);
    
    // Node 1002 (Tier 1) should also be available
    CHECK(AstrolabeSystem::addPointToNode(registry, player, graph, 1002));
    CHECK(comp.getAffinity(ProfessionID::BladeAscendant) == 6);
    
    // Node 2001 (Tier 2, Req 10) should be Locked
    // Affinity is 6. Req 10.
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 2001) == AstrolabeSystem::NodeStatus::Locked);
    
    // Invest 4 more into 1002 -> Total Affinity 10
    for(int i=0; i<4; ++i) {
        AstrolabeSystem::addPointToNode(registry, player, graph, 1002);
    }
    CHECK(comp.getAffinity(ProfessionID::BladeAscendant) == 10);
    
    // Now Node 2001 should be Available
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 2001) == AstrolabeSystem::NodeStatus::Available);
    
    // Test 2: Vow & Core
    // Node 3001 (Tier 3, Core)
    // Needs Affinity 25 + Vow
    
    // Cheat affinity to 25
    comp.professionAffinity[(int)ProfessionID::BladeAscendant] = 25;
    
    // Check status -> Locked (No Vow)
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 3001) == AstrolabeSystem::NodeStatus::Locked);
    
    // Take Vow
    CHECK(AstrolabeSystem::takeVow(registry, player, ProfessionID::BladeAscendant));
    CHECK(comp.hasVow());
    CHECK(comp.mainProfession == (int)ProfessionID::BladeAscendant);
    
    // Now should be Available
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 3001) == AstrolabeSystem::NodeStatus::Available);
    
    // Test Sealed Logic
    AstrolabeTalentNode mageCore;
    mageCore.id = 9999;
    mageCore.profession = ProfessionID::Mage;
    mageCore.type = TalentNodeType::Core;
    mageCore.tier = 3;
    mageCore.maxPoints = 1;
    graph.nodes[9999] = mageCore;
    
    // Cheat Mage affinity
    comp.professionAffinity[(int)ProfessionID::Mage] = 25;
    
    // Should be Sealed because we vowed to BladeAscendant
    CHECK(AstrolabeSystem::getNodeStatus(graph, comp, 9999) == AstrolabeSystem::NodeStatus::Sealed);
}
