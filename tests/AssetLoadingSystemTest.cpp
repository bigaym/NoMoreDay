#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/core/AssetLoadingSystem.hpp"
#include "../src/core/ResourceManager.hpp"
#include "../src/tools/Logger.hpp"

using namespace NoMoreDay;

TEST_CASE("AssetLoadingSystem - Initialization and Management") {
    tools::Logger::Init();
    ResourceManager resourceManager;
    
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
        CHECK(tex.id == 0);
        
        AssetLoadingSystem::Shutdown();
    }

    tools::Logger::Shutdown();
}
