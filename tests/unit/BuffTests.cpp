#pragma once
#include "TestCommon.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/components/Buff.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] BuffRegistry - Lookup Logic") {
    BuffRegistry::Initialize();

    SUBCASE("Retrieves known buffs correctly") {
        auto& data = BuffRegistry::GetVisualData(BuffType::AttackUp);
        CHECK(data.icon_text == "攻 ↑");
        CHECK(data.name == "攻击提升");
        CHECK(data.border_color.r == GREEN.r);
    }

    SUBCASE("Retrieves known debuffs correctly") {
        auto& data = BuffRegistry::GetVisualData(BuffType::DefenseDown);
        CHECK(data.icon_text == "防 ↓");
        CHECK(data.border_color.r == RED.r);
    }

    SUBCASE("Retrieves Blood Sea buff icon data") {
        auto& data = BuffRegistry::GetVisualData(BuffType::BloodSea);
        CHECK(data.name == "Blood Sea");
        CHECK_FALSE(data.is_debuff);
        CHECK(data.icon_asset != nullptr);
    }

    SUBCASE("Returns default data for unknown types") {
        auto& data = BuffRegistry::GetVisualData(BuffType::None);
        CHECK(data.name == "Unknown");
    }
}

TEST_CASE("[Unit] ActiveEffectsComponent - Update and Stacking") {
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
        component.effects[0].remaining = 5.0f;
        component.AddOrRefresh(effect);

        REQUIRE(component.effects.size() == 1);
        CHECK(component.effects[0].remaining == 10.0f);
        CHECK(component.effects[0].stacks == 2);
    }

    SUBCASE("AddOrRefresh respects max stacks") {
        BuffEffect effect;
        effect.id = "test_buff";
        effect.max_stacks = 2;

        component.AddOrRefresh(effect);
        component.AddOrRefresh(effect);
        component.AddOrRefresh(effect);

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
        component.Update(0.6f);
        CHECK(component.effects.empty());
    }
}

TEST_CASE("[Unit] BuffSystem - Duration and Stacking") {
    ActiveEffectsComponent container;
    
    BuffEffect b1;
    b1.id = "test_buff";
    b1.duration = 10.0f;
    b1.remaining = 10.0f;
    b1.stacks = 1;
    b1.max_stacks = 3;
    
    container.AddOrRefresh(b1);
    CHECK(container.effects.size() == 1);
    CHECK(container.effects[0].stacks == 1);

    container.AddOrRefresh(b1);
    CHECK(container.effects[0].stacks == 2);
    
    container.Update(5.0f);
    container.AddOrRefresh(b1);
    CHECK(container.effects[0].remaining == 10.0f);
    CHECK(container.effects[0].stacks == 3);

    container.AddOrRefresh(b1);
    CHECK(container.effects[0].stacks == 3);

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
        b2.stacks = 2;
        container.AddOrRefresh(b2);
        
        CHECK(container.effects[0].name == "New Name");
        CHECK(container.effects[0].stacks == 3);
    }
}

} // namespace NoMoreDay
