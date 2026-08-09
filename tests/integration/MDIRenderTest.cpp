#pragma once

#include "TestCommon.hpp"
#include "game/foundation/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/application/render/GPUEntityAdapter.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] MDIRender - MDI Rendering Integration") {
  // Note: Raylib Window should be initialized by main.cpp runner

  // 1. Setup Resources & Context
  ResourceManager resources;
  systems::GPUEntitySystem gpuEntitySystem;
  render::MDIRenderer mdiRenderer;
  RenderContext renderContext;
  renderContext.gpuEntitySystem = &gpuEntitySystem;
  renderContext.mdiRenderer = &mdiRenderer;
  renderContext.gpuFlowFieldSystem = &systems::GPUFlowFieldSystem::Get();
  renderContext.resources = &resources;

  SharedContext context;
  context.resources = &resources;
  context.renderAlpha = 0.0f;
  context.renderContext = &renderContext;

  LOG_INFO("TEST: Init GPUEntitySystem");
  gpuEntitySystem.Init(resources, 100);
  mdiRenderer.Init(resources, 100);

  REQUIRE(mdiRenderer.IsInitialized());

  LOG_INFO("TEST: Create Entities");
  entt::registry registry;
  int entityCount = 50;
  for (int i = 0; i < entityCount; ++i) {
    auto e = registry.create();
    registry.emplace<::Position>(e, (float)(i * 10), (float)(i * 10));
    registry.emplace<::Velocity>(e, 1.0f, 0.0f);
    registry.emplace<::Radius>(e, 5.0f);
    registry.emplace<::GPUIndex>(e, -1);
    if (i % 2 == 0) {
      registry.emplace<::EnemyTag>(e);
    }
  }
  context.registry = &registry;
  GPUEntityAdapter gpuEntityAdapter;
  gpuEntityAdapter.Init(100, &registry, gpuEntitySystem);

  LOG_INFO("TEST: Update");
  gpuEntityAdapter.Update(registry, gpuEntitySystem, 0.016f, 0.0f);
  gpuEntitySystem.UploadGPU({&resources, &mdiRenderer, context.renderAlpha});

  LOG_INFO("TEST: Render Frame");
  Camera2D camera = {0};
  camera.zoom = 1.0f;
  BeginDrawing();
  ClearBackground(BLACK);
  gpuEntitySystem.Render({&resources, &mdiRenderer, context.renderAlpha},
                         camera);
  EndDrawing();

  LOG_INFO("TEST: Render Done");

  gpuEntitySystem.Shutdown();
  mdiRenderer.Shutdown();
  LOG_INFO("TEST: Shutdown Done");

  CHECK(!mdiRenderer.IsInitialized());
  LOG_INFO("TEST: Finished");
}

} // namespace NoMoreDay
