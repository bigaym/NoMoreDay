#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/systems/UISystem.hpp"
#include "../src/systems/UIAnimationSystem.hpp"
#include "../src/components/UIAnimationComponent.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/tools/Logger.hpp"
#include <entt/entity/registry.hpp>

using namespace NoMoreDay;

TEST_CASE("UISystem - Rarity Colors") {
    try {
        tools::Logger::Init();
        ResourceManager resourceManager;
        UISystem::Initialize(resourceManager);

        SUBCASE("Rarity Colors") {
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

        SUBCASE("UIRenderer Scaling") {
            UIRenderer::SetScale(1.0f);
            CHECK(UIRenderer::GetScale() == doctest::Approx(1.0f));

            UIRenderer::SetScale(0.5f);
            CHECK(UIRenderer::GetScale() == doctest::Approx(0.5f));
            
            UIRenderer::SetScale(2.0f);
            CHECK(UIRenderer::GetScale() == doctest::Approx(2.0f));
        }

        SUBCASE("UI Transitions") {
            UISystem::State.inventoryAlpha = 0.0f;
            UISystem::State.showInventory = true;
            
            entt::registry registry;
            
            // Mock Delta Time transition (assuming Update uses GetFrameTime, which we can't easily mock here without more refactor)
            // But we can manually check if it would change if we call Update
            // Since we can't control GetFrameTime() in Raylib easily in tests, we can just verify initial state.
            
            CHECK(UISystem::State.inventoryAlpha == 0.0f);
            
            // We can test UIAnimationSystem separately
            entt::entity e = registry.create();
            auto& anim = registry.emplace<UIAnimationComponent>(e);
            anim.active = true;
            anim.startValue = 0.0f;
            anim.targetValue = 1.0f;
            anim.duration = 1.0f;
            anim.easing = EasingType::Linear;
            
            UIAnimationSystem::Update(registry, 0.5f);
            CHECK(anim.currentValue == doctest::Approx(0.5f));
            
            UIAnimationSystem::Update(registry, 0.5f);
            CHECK(anim.currentValue == doctest::Approx(1.0f));
            CHECK(anim.active == false); // Auto-disable
        }

        tools::Logger::Shutdown();
    } catch (const std::exception& e) {
        fprintf(stderr, "Exception in test: %s\n", e.what());
        FAIL(e.what());
    } catch (...) {
        fprintf(stderr, "Unknown exception in test\n");
        FAIL("Unknown exception");
    }
}
