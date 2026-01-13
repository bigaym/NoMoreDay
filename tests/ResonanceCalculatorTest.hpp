#pragma once
#include "TestCommon.hpp"
#include "game/data/ResonanceCalculator.hpp"
#include "game/components/MapFragmentComponent.hpp"

TEST_CASE("Mosaic System - Resonance Calculator") {
    TestSetupScope setup;
    entt::registry registry;
    MosaicGrid grid;
    
    // Create fragments
    auto createFrag = [&](FragmentElement elem, float density, float drop) {
        auto entity = registry.create();
        auto& frag = registry.emplace<MapFragmentComponent>(entity);
        frag.element = elem;
        frag.enemyDensityMod = density;
        frag.dropRateMod = drop;
        return entity;
    };

    SUBCASE("Empty Grid") {
        auto result = ResonanceCalculator::Calculate(grid, registry);
        CHECK(result.totalEnemyDensity == 1.0f);
        CHECK(result.totalDropRate == 1.0f);
        CHECK(result.resonanceChainCount == 0);
    }

    SUBCASE("Single Fragment") {
        grid.SetCell(0, 0, createFrag(FragmentElement::Fire, 1.2f, 1.1f));
        auto result = ResonanceCalculator::Calculate(grid, registry);
        CHECK(result.totalEnemyDensity == doctest::Approx(1.2f));
        CHECK(result.totalDropRate == doctest::Approx(1.1f));
        CHECK(result.resonanceChainCount == 0);
    }

    SUBCASE("Adjacent Resonance") {
        // Two fire fragments at (0,0) and (1,0)
        grid.SetCell(0, 0, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
        grid.SetCell(1, 0, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
        
        auto result = ResonanceCalculator::Calculate(grid, registry);
        // Each has 1 neighbor: 1 * 0.25 = 0.25 bonus for each
        // Total bonus = 0.25 + 0.25 = 0.5
        // Drop rate = 1.0 * (1 + 0.5) = 1.5
        CHECK(result.resonanceChainCount == 2);
        CHECK(result.totalDropRate == doctest::Approx(1.5f));
    }

    SUBCASE("Line Resonance") {
        // Full row of Fire
        grid.SetCell(0, 0, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
        grid.SetCell(1, 0, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
        grid.SetCell(2, 0, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
        
        auto result = ResonanceCalculator::Calculate(grid, registry);
        // Connections: (0,0)-(1,0) and (1,0)-(2,0)
        // (0,0) has 1 neighbor -> 0.25
        // (1,0) has 2 neighbors -> 0.5
        // (2,0) has 1 neighbor -> 0.25
        // Total adj bonus = 1.0
        // Line bonus = 0.5
        // Total drop rate = 1.0 * (1.0 + 1.0 + 0.5) = 2.5
        CHECK(result.totalDropRate == doctest::Approx(2.5f));
    }

    SUBCASE("Perfect Resonance") {
        // Fill all 9 cells with Fire
        for(int y=0; y<3; ++y) {
            for(int x=0; x<3; ++x) {
                grid.SetCell(x, y, createFrag(FragmentElement::Fire, 1.0f, 1.0f));
            }
        }
        
        auto result = ResonanceCalculator::Calculate(grid, registry);
        
        // Adj connections:
        // Corners (4): 2 neighbors each = 4 * 0.5 = 2.0
        // Edges (4): 3 neighbors each = 4 * 0.75 = 3.0
        // Center (1): 4 neighbors = 1 * 1.0 = 1.0
        // Total adj bonus = 6.0
        // Line bonus = 0.5
        // Multiplier = 2.0 (Perfect)
        // Drop rate = 1.0 * (1 + 6.0 + 0.5) * 2.0 = 15.0
        CHECK(result.isPerfectResonance == true);
        CHECK(result.totalDropRate == doctest::Approx(15.0f));
    }

    SUBCASE("Entity Validation") {
        auto frag = createFrag(FragmentElement::Fire, 2.0f, 2.0f);
        grid.SetCell(0, 0, frag);
        
        // Destroy entity
        registry.destroy(frag);
        
        auto result = ResonanceCalculator::Calculate(grid, registry);
        CHECK(result.totalEnemyDensity == 1.0f); // Should ignore destroyed entity
        CHECK(grid.GetCell(0, 0) == entt::entity{entt::null}); // Grid should have been cleaned
    }
}
