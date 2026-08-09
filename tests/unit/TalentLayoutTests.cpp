#include "doctest.h"
#include "game/data/TalentLayoutService.hpp"
#include "game/data/TalentData.hpp"
#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeConstants.hpp"
#include <cmath>

using namespace NoMoreDay;

TEST_CASE("[Unit] TalentLayoutService - Sector Angles") {
    CHECK(TalentLayoutService::getSectorCenterAngle(ProfessionID::BladeAscendant) == 270.0f);
    CHECK(TalentLayoutService::getSectorCenterAngle(ProfessionID::Mage) == 330.0f);
    CHECK(TalentLayoutService::getSectorCenterAngle(ProfessionID::Berserker) == 90.0f);
}

TEST_CASE("[Unit] TalentLayoutService - Orbit Radii") {
    CHECK(TalentLayoutService::getOrbitRadius(1) == Constants::Astrolabe::ORBIT_R2);
    CHECK(TalentLayoutService::getOrbitRadius(2) == Constants::Astrolabe::ORBIT_R3);
    CHECK(TalentLayoutService::getOrbitRadius(3) == Constants::Astrolabe::ORBIT_R4);
}

TEST_CASE("[Unit] TalentLayoutService - Node Positioning") {
    TalentGraph graph;
    
    // Add 3 nodes for BladeAscendant (Tier 1)
    // Sector Center: 270
    // Width: 60, Padding: 5 -> Usable: 50 [245, 295]
    // 3 nodes -> Steps: -25, 0, +25
    // Expected Angles: 245, 270, 295
    
    AstrolabeTalentNode n1; n1.id=1; n1.profession=ProfessionID::BladeAscendant; n1.tier=1; n1.sectorIndex=0;
    AstrolabeTalentNode n2; n2.id=2; n2.profession=ProfessionID::BladeAscendant; n2.tier=1; n2.sectorIndex=1;
    AstrolabeTalentNode n3; n3.id=3; n3.profession=ProfessionID::BladeAscendant; n3.tier=1; n3.sectorIndex=2;
    
    graph.nodes[1] = n1;
    graph.nodes[2] = n2;
    graph.nodes[3] = n3;
    
    TalentLayoutService::computeNodePositions(graph);
    
    // Check Radius
    float expectedR = Constants::Astrolabe::ORBIT_R2;
    
    auto checkNode = [&](uint32_t id, float expectedAngleDeg) {
        const auto& node = graph.nodes[id];
        float r = std::sqrt(node.x*node.x + node.y*node.y);
        CHECK(r == doctest::Approx(expectedR).epsilon(0.01));
        
        float angleRad = std::atan2(node.y, node.x);
        float angleDeg = angleRad * RAD2DEG;
        if (angleDeg < 0) angleDeg += 360.0f;
        
        CHECK(angleDeg == doctest::Approx(expectedAngleDeg).epsilon(0.01));
    };
    
    checkNode(1, 245.0f);
    checkNode(2, 270.0f);
    checkNode(3, 295.0f);
}

TEST_CASE("[Unit] TalentLayoutService - Profession Stars") {
    TalentGraph graph;
    TalentLayoutService::computeNodePositions(graph);
    
    float expectedR = Constants::Astrolabe::ORBIT_R1;
    
    // Check Blade Ascendant Star
    const auto& star = graph.professionStars[(int)ProfessionID::BladeAscendant];
    float r = std::sqrt(star.x*star.x + star.y*star.y);
    CHECK(r == doctest::Approx(expectedR).epsilon(0.01));
    
    float angleRad = std::atan2(star.y, star.x);
    float angleDeg = angleRad * RAD2DEG;
    if (angleDeg < 0) angleDeg += 360.0f;
    CHECK(angleDeg == doctest::Approx(270.0f).epsilon(0.01));
}

TEST_CASE("[Integration] TalentLoader - Load Profession Talents") {
    TalentGraph graph;
    bool success = TalentLoader::LoadProfessionTalents("assets/data/profession_talents.json", graph);
    CHECK(success);
    
    if (success) {
        // Verify Profession Star
        const auto& bladeStar = graph.professionStars[(int)ProfessionID::BladeAscendant];
        CHECK(bladeStar.name_key == "剑修");
        // Verify position is computed
        CHECK(bladeStar.x != 0.0f);
        CHECK(bladeStar.y != 0.0f);
        
        // Verify Nodes
        const auto* node1001 = graph.findNode(1001);
        REQUIRE(node1001 != nullptr);
        CHECK(node1001->profession == ProfessionID::BladeAscendant);
        CHECK(node1001->tier == 1);
        CHECK(node1001->type == TalentNodeType::Minor);
        CHECK(node1001->maxPoints == 5);
        // Verify position is computed
        CHECK(node1001->x != 0.0f);
        CHECK(node1001->y != 0.0f);
        
        const auto* node1010 = graph.findNode(1010); // 剑意觉醒
        REQUIRE(node1010 != nullptr);
        CHECK(node1010->type == TalentNodeType::Core);
    }
}
