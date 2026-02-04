#include "doctest.h"
#include "game/data/TalentData.hpp"
#include "game/data/TalentLoader.hpp"

using namespace NoMoreDay;

TEST_SUITE("TalentDataTest") {
    TEST_CASE("[Unit] TalentGraph - Serialization") {
        TalentGraph graph;
        
        // Setup some test data
        AstrolabeTalentNode n1;
        n1.id = 1100;
        n1.name_key = "test_node";
        n1.profession = ProfessionID::BladeAscendant;
        n1.type = TalentNodeType::Minor;
        n1.maxPoints = 5;
        graph.nodes[1100] = n1;
        
        graph.professionStars[(int)ProfessionID::BladeAscendant].name_key = "Blade Ascendant Star";
        
        // Manual JSON test
        nlohmann::json j;
        j["nodes"] = nlohmann::json::array();
        for (const auto& [id, node] : graph.nodes) {
            j["nodes"].push_back(node);
        }
        
        std::string serialized = j.dump();
        nlohmann::json j2 = nlohmann::json::parse(serialized);
        
        CHECK(j2.contains("nodes"));
        CHECK(j2["nodes"].is_array());
        CHECK(j2["nodes"].size() == 1);
        
        AstrolabeTalentNode loadedNode;
        from_json(j2["nodes"][0], loadedNode);
        
        CHECK(loadedNode.id == 1100);
        CHECK(loadedNode.name_key == "test_node");
        CHECK(loadedNode.profession == ProfessionID::BladeAscendant);
        CHECK(loadedNode.maxPoints == 5);
    }
}