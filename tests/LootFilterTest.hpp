#pragma once

#include "../src/core/LootFilter.hpp"
#include "../src/components/ItemComponent.hpp"
#include <fstream>
#include <cstdio>
#include <string> // For std::string

// Helper to write a temp filter file
void writeTempFilter(const std::string& filename, const std::string& content) {
    std::ofstream out(filename);
    out << content;
    out.close();
}

TEST_CASE("LootFilter - Loading and Evaluation") {
    // 1. Setup a test filter
    std::string filterJson = R"({
        "name": "Test Filter",
        "rules": [
            {
                "name": "Hide Low Level Common",
                "action": "HIDE",
                "conditions": {
                    "max_rarity": "COMMON",
                    "max_level": 5
                }
            },
            {
                "name": "Highlight Legendaries",
                "action": "EMPHASIZE",
                "color": [255, 0, 0],
                "scale": 1.5,
                "conditions": {
                    "min_rarity": "LEGENDARY"
                }
            },
            {
                "name": "Show Swords",
                "action": "SHOW",
                "conditions": {
                    "base_name": "Sword"
                }
            }
        ]
    })";
    
    std::string tempPath = "test_filter.json";
    writeTempFilter(tempPath, filterJson);
    
    // 2. Load it
    LootFilter::load(tempPath);
    CHECK(LootFilter::getCurrentProfile().name == "Test Filter");
    CHECK(LootFilter::getCurrentProfile().rules.size() == 3);

    // 3. Test Cases
    
    // Case A: Level 1 Common Dagger -> Should Hide
    ItemComponent itemA;
    itemA.name = "Rusty Dagger";
    itemA.rarity = Rarity::Common;
    FilterAction actionA = LootFilter::evaluate(itemA, 1);
    CHECK(actionA.type == FilterActionType::HIDE);
    
    // Case B: Level 10 Common Dagger -> Should Show (Rules don't match -> Default Show)
    FilterAction actionB = LootFilter::evaluate(itemA, 10);
    CHECK(actionB.type == FilterActionType::SHOW);

    // Case C: Legendary Item -> Should Emphasize
    ItemComponent itemC;
    itemC.name = "Godslayer";
    itemC.rarity = Rarity::Legendary;
    FilterAction actionC = LootFilter::evaluate(itemC, 50);
    CHECK(actionC.type == FilterActionType::EMPHASIZE);
    CHECK(actionC.scale == 1.5f);
    CHECK(actionC.colorOverride.has_value());
    CHECK(actionC.colorOverride->r == 255);

    // Case D: Sword Base Name -> Show (Explicit rule)
    ItemComponent itemD;
    itemD.name = "Iron Sword";
    itemD.rarity = Rarity::Common; // Level 1 common sword
    // Rule 1 (Hide Low Level Common) matches first! Order matters.
    // "max_level": 5. So level 1 matches Rule 1.
    FilterAction actionD1 = LootFilter::evaluate(itemD, 1);
    CHECK(actionD1.type == FilterActionType::HIDE);
    
    // Level 6 Common Sword. Rule 1 fails. Rule 2 fails. Rule 3 matches.
    FilterAction actionD6 = LootFilter::evaluate(itemD, 6);
    CHECK(actionD6.type == FilterActionType::SHOW); // Explicit show

    // Cleanup
    std::remove(tempPath.c_str());
}
