#pragma once
#include "TestCommon.hpp"
#include "components/Buff.hpp"

TEST_CASE("ActiveEffectsComponent Logic") {
    ActiveEffectsComponent component;

    SUBCASE("AddOrRefresh adds new effect") {
        BuffEffect effect;
        effect.id = "test_buff";
        effect.duration = 10.0f;
        effect.remaining = 10.0f;
        effect.stacks = 1;
        effect.max_stacks = 5;

        component.AddOrRefresh(effect);

        REQUIRE(component.effects.size() == 1);
        CHECK(component.effects[0].id == "test_buff");
        CHECK(component.effects[0].remaining == 10.0f);
    }

    SUBCASE("AddOrRefresh refreshes existing effect") {
        BuffEffect effect;
        effect.id = "test_buff";
        effect.duration = 10.0f;
        effect.remaining = 10.0f;
        effect.stacks = 1;
        effect.max_stacks = 5;

        component.AddOrRefresh(effect);
        
        // Simulate time passing
        component.effects[0].remaining = 5.0f;

        // Add again
        component.AddOrRefresh(effect);

        REQUIRE(component.effects.size() == 1);
        CHECK(component.effects[0].remaining == 10.0f); // Reset duration
        CHECK(component.effects[0].stacks == 2);        // Stack incremented
    }

    SUBCASE("AddOrRefresh respects max stacks") {
        BuffEffect effect;
        effect.id = "test_buff";
        effect.duration = 10.0f;
        effect.remaining = 10.0f;
        effect.stacks = 1;
        effect.max_stacks = 2;

        component.AddOrRefresh(effect); // stack 1
        component.AddOrRefresh(effect); // stack 2
        component.AddOrRefresh(effect); // stack 2 (max)

        REQUIRE(component.effects.size() == 1);
        CHECK(component.effects[0].stacks == 2);
    }

    SUBCASE("Update removes expired effects") {
        BuffEffect effect;
        effect.id = "short_buff";
        effect.duration = 1.0f;
        effect.remaining = 1.0f;

        component.AddOrRefresh(effect);
        
        component.Update(0.5f);
        CHECK(component.effects.size() == 1);
        CHECK(component.effects[0].remaining == doctest::Approx(0.5f));

        component.Update(0.6f);
        CHECK(component.effects.empty());
    }

    SUBCASE("Remove deletes specific effect") {
        BuffEffect e1; e1.id = "b1";
        BuffEffect e2; e2.id = "b2";

        component.AddOrRefresh(e1);
        component.AddOrRefresh(e2);

        component.Remove("b1");

        REQUIRE(component.effects.size() == 1);
        CHECK(component.effects[0].id == "b2");
    }
}