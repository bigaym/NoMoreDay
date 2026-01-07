#pragma once

#include "../src/core/AssetLoadingSystem.hpp"
#include "../src/core/ResourceManager.hpp"
#include "../src/core/EquipmentAssetRegistry.hpp" // Added
#include <raylib.h> // For Font, Texture2D
#include "TestCommon.hpp" // Added

TEST_CASE("AssetLoadingSystem - Initialization and Management") {
    LoggerScope scope;
    ResourceManager resourceManager;
    resourceManager.SetHeadless(true); // Enable headless mode for testing
    
    SUBCASE("Initialization") {
        AssetLoadingSystem::Initialize(resourceManager);
        // No direct way to check m_resourceManager, but we can try calling methods
        AssetLoadingSystem::Shutdown();
    }

    SUBCASE("Font Loading (Headless)") {
        AssetLoadingSystem::Initialize(resourceManager);
        
        // This will fail to load real font in headless but shouldn't crash
        Font font = AssetLoadingSystem::LoadUIFont("non_existent_font.ttf", 20);
        
        // In headless mode, Raylib might return GetFontDefault() or {0}
        // Since we are not in a real window context here usually, we just check it doesn't crash
        
        AssetLoadingSystem::Shutdown();
    }

    SUBCASE("Texture Management (Headless)") {
        AssetLoadingSystem::Initialize(resourceManager);
        
        Texture2D tex = AssetLoadingSystem::GetTexture(123);
        // In headless mode, GetTexture might return a dummy or {0} depending on logic
        // Our mock returns a dummy with ID=1 if loaded via loadTexture, but GetTexture(123) fails if not loaded
        // Let's try loading one
        Texture2D loaded = AssetLoadingSystem::LoadUITexture(123, "dummy_path");
        CHECK(loaded.id == 1); // Dummy ID
        
        AssetLoadingSystem::Shutdown();
    }

    SUBCASE("Load All Equipment (Smoke Test)") {
        AssetLoadingSystem::Initialize(resourceManager);
        // This should now be safe in headless mode
        AssetLoadingSystem::LoadAllEquipment();
        
        // Verify one asset
        using namespace assets::equipment;
        auto id = amulet::amulet_0.id;
        Texture2D tex = AssetLoadingSystem::GetTexture(id);
        CHECK(tex.id == 1); // Should be dummy texture
        
        AssetLoadingSystem::Shutdown();
    }
}
