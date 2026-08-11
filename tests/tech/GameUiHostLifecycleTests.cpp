#pragma once
#include "TestCommon.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UISystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/world/LevelManager.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

// U4 lifecycle tech test: mirrors the Game composition-root sequence
//   initialize -> enter gameplay -> leave -> re-enter -> cleanup
// against the real GameUiHost and its legacy facade. The test harness
// provides a GL context (tests/main.cpp InitWindow), so font and texture
// loading work; the draw path is exercised against an empty registry inside a
// BeginDrawing/EndDrawing window, mirroring GameplayState::OnRender.
TEST_CASE("[Tech] GameUiHost - UI lifecycle across gameplay sessions") {
  {
    ResourceManager resourceManager;
    ui::GameUiHost host;

    // Game boot: initialize once (as Game::init does).
    host.Initialize(resourceManager);
    CHECK(host.IsInitialized());
    CHECK_FALSE(host.IsInGameplay());

    entt::registry registry;
    // Mirror GameplayState::OnEnter: the level systems must exist before the
    // UI draw path touches them (UIMinimap reads levelManager.getFogSystem()).
    LevelManager levelManager;
    levelManager.initialize(resourceManager, registry);
    levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);
    Camera2D camera{};
    systems::SpatialHashGrid spatialGrid(100, 100, 50);

    // Run 1: enter gameplay, dirty some session state, run a frame, leave.
    host.EnterGameplay();
    CHECK(host.IsInGameplay());
    UISystem::State.showInventory = true;
    UISystem::State.showCharacterPanel = true;
    UISystem::State.showContextMenu = true;
    UISystem::State.showMessageBox = true;
    UISystem::State.isDraggingSkill = true;
    UISystem::State.draggedSkillId = 1;
    UISystem::State.tooltipAlpha = 0.5f;
    UISystem::State.draggedItem = registry.create();

    host.Update(registry, levelManager);
    host.PrepareRender();
    BeginDrawing();
    host.Draw(registry, levelManager, camera, &spatialGrid);
    EndDrawing();

    host.LeaveGameplay();

    // Session state must not leak into the next run.
    CHECK_FALSE(host.IsInGameplay());
    CHECK_FALSE(UISystem::State.showInventory);
    CHECK_FALSE(UISystem::State.showCharacterPanel);
    CHECK_FALSE(UISystem::State.showContextMenu);
    CHECK_FALSE(UISystem::State.showMessageBox);
    CHECK_FALSE(UISystem::State.isDraggingSkill);
    CHECK_FALSE(UISystem::State.isTyping);
    CHECK(UISystem::State.draggedItem == entt::entity{entt::null});
    CHECK(UISystem::State.draggedSkillId == NoMoreDay::INVALID_SKILL_ID);
    CHECK(UISystem::State.tooltipAlpha == doctest::Approx(0.0f));

    // Run 2: re-entering gameplay after a session reset works and can render.
    host.EnterGameplay();
    CHECK(host.IsInGameplay());
    host.Update(registry, levelManager);
    host.PrepareRender();
    BeginDrawing();
    host.Draw(registry, levelManager, camera, &spatialGrid);
    EndDrawing();
    host.LeaveGameplay();
    CHECK_FALSE(host.IsInGameplay());

    // Cleanup: host shutdown precedes resource manager teardown (Game::cleanup).
    host.Shutdown();
    CHECK_FALSE(host.IsInitialized());
    CHECK_FALSE(host.IsInGameplay());

    // Shutdown is idempotent.
    host.Shutdown();
    CHECK_FALSE(host.IsInitialized());
  }
}

} // namespace NoMoreDay
