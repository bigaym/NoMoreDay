#define DOCTEST_CONFIG_IMPLEMENT
#include "../third_party/doctest/doctest.h"
#include "../src/systems/UISystem.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include <entt/entity/registry.hpp>

using namespace NoMoreDay;

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}

TEST_CASE("UISystem - Rarity Colors") {
    CHECK(UISystem::GetRarityColor(Rarity::Common).r == LIGHTGRAY.r);
    CHECK(UISystem::GetRarityColor(Rarity::Common).g == LIGHTGRAY.g);
    CHECK(UISystem::GetRarityColor(Rarity::Common).b == LIGHTGRAY.b);

    CHECK(UISystem::GetRarityColor(Rarity::Magic).r == SKYBLUE.r);
    CHECK(UISystem::GetRarityColor(Rarity::Magic).g == SKYBLUE.g);
    CHECK(UISystem::GetRarityColor(Rarity::Magic).b == SKYBLUE.b);

    CHECK(UISystem::GetRarityColor(Rarity::Rare).r == YELLOW.r);
    CHECK(UISystem::GetRarityColor(Rarity::Rare).g == YELLOW.g);
    CHECK(UISystem::GetRarityColor(Rarity::Rare).b == YELLOW.b);

    CHECK(UISystem::GetRarityColor(Rarity::Legendary).r == ORANGE.r);
    CHECK(UISystem::GetRarityColor(Rarity::Legendary).g == ORANGE.g);
    CHECK(UISystem::GetRarityColor(Rarity::Legendary).b == ORANGE.b);

    CHECK(UISystem::GetRarityColor(Rarity::Mythic).r == RED.r);
    CHECK(UISystem::GetRarityColor(Rarity::Mythic).g == RED.g);
    CHECK(UISystem::GetRarityColor(Rarity::Mythic).b == RED.b);
}
