#pragma once

#include "TestCommon.hpp"
#include "app/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/AIComponent.hpp"

namespace NoMoreDay {

TEST_CASE("MDI Rendering Integration") {
    // Note: Raylib Window should be initialized by main.cpp runner

    // 1. Setup Resources
    ResourceManager resources;
    SharedContext context;
    context.resources = &resources;
    context.renderAlpha = 0.0f;
    // Minimal mock if needed, but we rely on assets existing or fails gracefully
    
    LOG_INFO("TEST: Init GPUEntitySystem");
    systems::GPUEntitySystem::Get().Init(resources, 100);
    
    REQUIRE(render::MDIRenderer::Get().IsInitialized());

    LOG_INFO("TEST: Create Entities");
    entt::registry registry;
    // ... (rest of loop)
    int entityCount = 50;
    for(int i=0; i<entityCount; ++i) {
        auto e = registry.create();
        registry.emplace<::Position>(e, (float)(i * 10), (float)(i * 10));
        registry.emplace<::Velocity>(e, 1.0f, 0.0f);
        registry.emplace<::Radius>(e, 5.0f);
        registry.emplace<::GPUIndex>(e, -1);
        if (i % 2 == 0) {
            registry.emplace<::EnemyTag>(e);
        }
    }
    
    LOG_INFO("TEST: Update");
    systems::GPUEntitySystem::Get().Update(registry, 0.016f);
    
    LOG_INFO("TEST: Render Frame");
    Camera2D camera = {0};
    camera.zoom = 1.0f;
    BeginDrawing();
        ClearBackground(BLACK);
        systems::GPUEntitySystem::Get().Render(context, camera);
    EndDrawing();
    
    LOG_INFO("TEST: Render Done");
    
    systems::GPUEntitySystem::Get().Shutdown();
    LOG_INFO("TEST: Shutdown Done");
    
    CHECK(!render::MDIRenderer::Get().IsInitialized());
    LOG_INFO("TEST: Finished");
}

} // namespace NoMoreDay
