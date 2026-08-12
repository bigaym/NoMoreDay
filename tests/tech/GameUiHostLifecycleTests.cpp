#pragma once
#include "TestCommon.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UISystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/world/LevelManager.hpp"
#include <algorithm>
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
    // UI draw path touches them (MinimapController reads
    // levelManager.getFogSystem()).
    LevelManager levelManager;
    levelManager.initialize(resourceManager, registry);
    levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);
    Camera2D camera{};
    systems::SpatialHashGrid spatialGrid(100, 100, 50);

    // Run 1: enter gameplay, dirty some session state, run a frame, leave.
    host.EnterGameplay();
    CHECK(host.IsInGameplay());
    // U8 final: session state is dirtied through the host instance APIs (the
    // legacy UISystem::State static context is gone).
    host.SetInventoryVisible(true);
    host.SetCharacterPanelVisible(true);
    host.OpenContextMenu(entt::null, false, 0, NoMoreDay::EquipmentSlot::None);
    host.ShowMessageBox("test");
    auto &drag = host.DragSession();
    drag.isDraggingSkill = true;
    drag.draggedSkillId = 1;
    drag.draggedItemDomainId = entt::to_integral(registry.create());

    host.Update(registry, levelManager);
    host.PrepareRender();
    BeginDrawing();
    // R8: the registry parameter is gone from Draw (snapshot/intent surfaces).
    host.Draw(levelManager, camera, &spatialGrid);
    EndDrawing();

    host.LeaveGameplay();

    // Session state must not leak into the next run.
    CHECK_FALSE(host.IsInGameplay());
    CHECK_FALSE(host.IsInventoryVisible());
    CHECK_FALSE(host.IsCharacterPanelVisible());
    CHECK_FALSE(host.IsMessageBoxVisible());
    CHECK_FALSE(host.DragSession().isDraggingSkill);
    CHECK(host.DragSession().draggedItemDomainId == 0);
    CHECK(host.DragSession().draggedSkillId == NoMoreDay::INVALID_SKILL_ID);

    // Run 2: re-entering gameplay after a session reset works and can render.
    host.EnterGameplay();
    CHECK(host.IsInGameplay());
    host.Update(registry, levelManager);
    host.PrepareRender();
    BeginDrawing();
    host.Draw(levelManager, camera, &spatialGrid);
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

// D-01 (R2): the production host must never inject test data. Entering and
// leaving gameplay must not create a test bag, overwrite active skill slots,
// rebuild active effects or force a stats recalc on the real player.
TEST_CASE("[Tech] GameUiHost - gameplay enter/leave does not inject test data") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  CHECK(host.IsInitialized());

  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Real player state that must survive gameplay enter/leave untouched. The
  // bag is a fixed BASE_CAPACITY slot array of entt::null; place the owned
  // item into the first slot.
  auto &inventory = registry.emplace<InventoryComponent>(player);
  const entt::entity ownedItem = registry.create();
  registry.emplace<ItemComponent>(ownedItem);
  inventory.items[0] = ownedItem;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = {7, 0.0f, 2};
  active.slots[2] = {9, 0.0f, 1};

  auto &effects = registry.emplace<ActiveEffectsComponent>(player);
  BuffEffect keep;
  keep.id = "real_buff";
  keep.name = "真实增益";
  effects.effects.push_back(keep);

  // Level systems must exist before the UI update/draw path touches them.
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  // Run two gameplay sessions (mirroring the app boot -> session -> session
  // sequence) with the player present, so any production-side injection would
  // fire here.
  host.EnterGameplay();
  host.Update(registry, levelManager);
  host.LeaveGameplay();
  host.EnterGameplay();
  host.Update(registry, levelManager);
  host.LeaveGameplay();

  // Inventory unchanged: the base bag keeps its 40 slots, the owned item still
  // occupies exactly one slot, and no test bag was picked up into a slot.
  REQUIRE(inventory.items.size() == InventoryComponent::BASE_CAPACITY);
  CHECK(std::find(inventory.items.begin(), inventory.items.end(), ownedItem) !=
        inventory.items.end());
  int filledSlots = 0;
  for (entt::entity slot : inventory.items) {
    if (slot != entt::null) {
      ++filledSlots;
    }
  }
  CHECK(filledSlots == 1);
  for (auto [entity, item] : registry.view<ItemComponent>().each()) {
    (void)entity;
    CHECK(item.name != "破烂的背包");
    CHECK(item.name != "test_bag");
  }
  CHECK(inventory.items[0] == ownedItem);
  for (auto [entity, item] : registry.view<ItemComponent>().each()) {
    CHECK(item.name != "破烂的背包");
    CHECK(item.name != "test_bag");
  }

  // Active skill slots unchanged (no test skills written into Q/W/E/R/RMB).
  CHECK(active.slots[0].id == 7);
  CHECK(active.slots[0].current_charges == 2);
  CHECK(active.slots[2].id == 9);

  // Active effects unchanged: the real buff survived and no test_* buff was
  // added or replaced.
  REQUIRE(effects.effects.size() == 1);
  CHECK(effects.effects[0].id == "real_buff");

  // No stats recalc was forced by the host.
  CHECK_FALSE(registry.all_of<StatsDirty>(player));

  host.Shutdown();
}

} // namespace NoMoreDay
