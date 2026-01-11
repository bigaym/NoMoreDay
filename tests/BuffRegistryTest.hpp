#pragma once
#include "TestCommon.hpp"
#include "game/data/BuffRegistry.hpp"

TEST_CASE("BuffRegistry Lookups") {
    // Ensure initialized
    NoMoreDay::BuffRegistry::Initialize();

    SUBCASE("Retrieves known buffs correctly") {
        auto& data = NoMoreDay::BuffRegistry::GetVisualData(BuffType::AttackUp);
        CHECK(data.icon_text == "攻 ↑");
        CHECK(data.name == "攻击提升");
        CHECK(data.border_color.r == GREEN.r);
        CHECK(data.border_color.g == GREEN.g);
        CHECK(data.border_color.b == GREEN.b);
        CHECK(data.border_color.a == GREEN.a);
    }

    SUBCASE("Retrieves known debuffs correctly") {
        auto& data = NoMoreDay::BuffRegistry::GetVisualData(BuffType::DefenseDown);
        CHECK(data.icon_text == "防 ↓");
        CHECK(data.border_color.r == RED.r);
    }

    SUBCASE("Returns default data for unknown types") {
        auto& data = NoMoreDay::BuffRegistry::GetVisualData(BuffType::None);
        CHECK(data.name == "Unknown");
    }
}