#include "doctest.h"
#include "game/data/TalentLoader.hpp"

using namespace NoMoreDay;

TEST_SUITE("TalentDataTest") {
    TEST_CASE("[Unit] TalentLoader - Default Map Generation") {
        AstrolabeMap map;
        TalentLoader::CreateDefaultMap(map);
        
        CHECK(map.constellations.size() >= 3);
        CHECK(map.stars.size() >= 4);
        
        // Check root node
        auto it = map.stars.find(1001);
        REQUIRE(it != map.stars.end());
        CHECK(it->second.name_key == "star_origin_root");
        CHECK(it->second.connections.size() >= 2); // Connects to blade1 and guard1
    }
    
    TEST_CASE("[Unit] TalentLoader - JSON Serialization") {
        AstrolabeMap map;
        TalentLoader::CreateDefaultMap(map);
        
        nlohmann::json j;
        j["constellations"] = map.constellations;
        
        std::vector<StarNode> starList;
        for (auto& pair : map.stars) starList.push_back(pair.second);
        j["stars"] = starList;
        
        std::string serialized = j.dump();
        
        AstrolabeMap loadedMap;
        nlohmann::json j2 = nlohmann::json::parse(serialized);
        
        if (j2.contains("constellations")) {
            for (const auto& c_json : j2["constellations"]) {
                loadedMap.constellations.push_back(c_json.get<Constellation>());
            }
        }
        if (j2.contains("stars")) {
            for (const auto& s_json : j2["stars"]) {
                StarNode node = s_json.get<StarNode>();
                loadedMap.stars[node.id] = node;
            }
        }
        
        CHECK(loadedMap.constellations.size() == map.constellations.size());
        CHECK(loadedMap.stars.size() == map.stars.size());
        CHECK(loadedMap.stars[1001].name_key == map.stars[1001].name_key);
    }
}
