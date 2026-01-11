#pragma once
#include "TestCommon.hpp"
#include "game/components/Buff.hpp"

using namespace NoMoreDay;

TEST_CASE("Buff Stacking and Duration Logic") {
    ActiveEffectsComponent container;
    
    BuffEffect b1;
    b1.id = "test_buff";
    b1.duration = 10.0f;
    b1.remaining = 10.0f;
    b1.stacks = 1;
    b1.max_stacks = 3;
    
    // 1. Initial Add
    container.AddOrRefresh(b1);
    CHECK(container.effects.size() == 1);
    CHECK(container.effects[0].stacks == 1);
    CHECK(container.effects[0].remaining == 10.0f);

    // 2. Refresh (Stack Increase)
    container.AddOrRefresh(b1);
    CHECK(container.effects[0].stacks == 2);
    
    // 3. Refresh with time elapsed
    container.Update(5.0f);
    CHECK(container.effects[0].remaining == 5.0f);
    container.AddOrRefresh(b1);
    CHECK(container.effects[0].remaining == 10.0f); // Duration reset
    CHECK(container.effects[0].stacks == 3);        // Stack increased

    // 4. Max Stack Limit
    container.AddOrRefresh(b1);
    CHECK(container.effects[0].stacks == 3); // Capped at 3

    SUBCASE("Metadata and Multi-Stack Refresh") {
        ActiveEffectsComponent container;
        
        BuffEffect b1;
        b1.id = "meta_test";
        b1.name = "Old Name";
        b1.stacks = 1;
        b1.max_stacks = 5;
        
        container.AddOrRefresh(b1);
        
        BuffEffect b2 = b1;
        b2.name = "New Name";
        b2.stacks = 2; // Add 2 more stacks
        
        container.AddOrRefresh(b2);
        
        CHECK(container.effects[0].name == "New Name");
        CHECK(container.effects[0].stacks == 3); // 1 + 2
    }
}
