#pragma once
#include "doctest.h"
#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/Common.hpp"

using namespace NoMoreDay;

TEST_CASE("Astrolabe UI System Logic") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);

    SUBCASE("Toggle Logic") {
        // Initial state: No component
        CHECK(registry.try_get<AstrolabeUIComponent>(player) == nullptr);
        CHECK_FALSE(UIAstrolabe::IsVisible(registry, player));

        // Toggle On
        UIAstrolabe::Toggle(registry, player);
        auto* ui = registry.try_get<AstrolabeUIComponent>(player);
        REQUIRE(ui != nullptr);
        CHECK(ui->isOpen);
        CHECK(UIAstrolabe::IsVisible(registry, player));
        CHECK(ui->zoom == 1.0f);
        CHECK(ui->offset.x == 0.0f);
        CHECK(ui->offset.y == 0.0f);

        // Toggle Off
        UIAstrolabe::Toggle(registry, player);
        CHECK_FALSE(ui->isOpen);
        CHECK_FALSE(UIAstrolabe::IsVisible(registry, player));
    }
}
