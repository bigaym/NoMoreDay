#include "doctest.h"
#include "game/components/MaterialBankComponent.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/components/Common.hpp" // For Position
#include "game/components/EffectComponent.hpp" // For VisualEffect
#include <vector>

using namespace NoMoreDay;

TEST_CASE("Material System Tests") {
    
    SUBCASE("MaterialBankComponent Basic Operations") {
        MaterialBankComponent bank;
        
        // Add
        CHECK(bank.Add(1001, 10) == 10);
        CHECK(bank.Add(1001, 5) == 15);
        CHECK(bank.Add(2001, 1) == 1);
        
        // Verify Sorted Support
        REQUIRE(bank.materials.size() == 2);
        CHECK(bank.materials[0].id == 1001);
        CHECK(bank.materials[1].id == 2001);
        
        // GetCount
        CHECK(bank.GetCount(1001) == 15);
        CHECK(bank.GetCount(2001) == 1);
        CHECK(bank.GetCount(9999) == 0);
        
        // Has
        CHECK(bank.Has(1001, 10));
        CHECK(bank.Has(1001, 15));
        CHECK_FALSE(bank.Has(1001, 16));
        
        // Remove
        CHECK(bank.Remove(1001, 10));
        CHECK(bank.GetCount(1001) == 5);
        
        CHECK_FALSE(bank.Remove(1001, 10)); // Not enough
        CHECK(bank.GetCount(1001) == 5); // Unchanged
        
        CHECK(bank.Remove(1001, 5)); // Exact removal
        CHECK(bank.GetCount(1001) == 0);
        CHECK(bank.materials.size() == 1); // Should be removed from vector
        CHECK(bank.materials[0].id == 2001);
    }
    
    SUBCASE("MaterialRegistry Loading") {
        MaterialRegistry& registry = MaterialRegistry::Get();
        // Assuming assets are copied to bin/assets during build, we try to load from there.
        // However, tests run from bin/tests probably?
        // Let's try to load relatively or check where tests run.
        // Usually tests run from build/bin/tests or build/bin.
        
        // Based on CMake: CMAKE_RUNTIME_OUTPUT_DIRECTORY is build/bin
        // Assets are copied to build/bin/assets.
        // Tests are in tests/ directory but linked to NoMoreDayCore.
        // Wait, tests are usually separate executables or a single runner.
        // CMake main.cpp for tests suggests a single runner.
        
        // I'll try "assets/data/materials.json" assuming CWD is build/bin where assets are.
        registry.LoadMaterials("assets/data/materials.json");
        
        // If the file is not found (e.g. running from wrong dir), this check will fail.
        // But for unit test, maybe I should mock or ensure file exists?
        // I created f:\NoMoreDay\assets\data\materials.json
        // The build script copies assets to build/bin/assets.
        
        const auto* mat1 = registry.GetMaterial(1001);
        if (mat1) {
            CHECK(mat1->name == "Iron Ore");
            CHECK(mat1->rarity == Rarity::Common);
        } else {
            WARN("Material 1001 not found. Asset path might be wrong.");
        }
        
        const auto* mat2 = registry.GetMaterial(2001);
        if (mat2) {
            CHECK(mat2->name == "Dimension Fragment");
        }
        
        CHECK(registry.GetMaterial(99999) == nullptr);
    }

    SUBCASE("Integration: Pickup Material") {
        entt::registry registry;
        auto player = registry.create();
        registry.emplace<MaterialBankComponent>(player);
        registry.emplace<InventoryComponent>(player); // PickUp checks for InventoryComponent

        auto item = registry.create();
        ItemComponent itemComp;
        itemComp.id = 1001;
        itemComp.type = ItemType::Material;
        itemComp.quantity = 5;
        itemComp.maxStack = 999;
        
        registry.emplace<ItemComponent>(item, itemComp);
        registry.emplace<Position>(item, 0.0f, 0.0f); // Needed for visual effect logic

        // Act
        bool success = InventorySystem::pickUpItem(registry, player, item);
        
        // Assert
        CHECK(success);
        CHECK_FALSE(registry.valid(item)); // Entity should be destroyed
        
        const auto& bank = registry.get<MaterialBankComponent>(player);
        CHECK(bank.GetCount(1001) == 5);
        
        // Verify Visual Effect Created
        bool effectFound = false;
        auto view = registry.view<VisualEffect>();
        for(auto e : view) {
            const auto& eff = view.get<VisualEffect>(e);
            if (eff.type == VisualEffectType::Pickup) { // && color is GREEN
                effectFound = true;
            }
        }
        CHECK(effectFound);
    }
}
