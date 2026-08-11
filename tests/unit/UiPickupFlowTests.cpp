#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshotBuilder.hpp"
#include "game/application/ui/UISystem.hpp"

#include "engine/resource/ResourceManager.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/systems/world/LevelManager.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace NoMoreDay {
namespace {

// --- U6b source guards ---------------------------------------------------
// The render/Draw path must never execute gameplay mutations: pickup intents
// are only enqueued (read-only) during Draw and executed by
// GameUiCommandHandler during the Update phase (design §6.2).

TEST_CASE("[Unit] UiPickupFlow - render path never calls InventorySystem::pickUpItem") {
  // The raylib backend only renders draw lists; it must never touch the
  // inventory system.
  {
    std::ifstream source("src/game/application/ui/UiRaylibBackend.cpp");
    REQUIRE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source),
                               std::istreambuf_iterator<char>()};
    CHECK(contents.find("InventorySystem::pickUpItem") == std::string::npos);
  }
  // UISystem.cpp keeps two legitimate Update-phase pickUpItem calls (the F-key
  // quick pickup and the dev test-item injection), so the guard targets the
  // exact argument pattern of the removed Draw click-execution block. Any
  // reintroduction of click-to-pickup in the Draw path re-inserts this call
  // shape and fails the check.
  {
    std::ifstream source("src/game/application/ui/UISystem.cpp");
    REQUIRE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source),
                               std::istreambuf_iterator<char>()};
    CHECK(contents.find("pickUpItem(registry, playerEntity, itemData.entity)") ==
          std::string::npos);
  }
}

// --- U6b host intent queue (pure data, no GL / initialization needed) -----

TEST_CASE("[Unit] UiPickupFlow - host intent queue round-trips through drain") {
  ui::GameUiHost host;

  ui::GameUiIntent first;
  first.kind = ui::GameUiIntentKind::PickupItem;
  first.domainId = 42;
  host.EnqueueIntent(first);

  ui::GameUiIntent second;
  second.kind = ui::GameUiIntentKind::PickupItem;
  second.domainId = 7;
  host.EnqueueIntent(second);

  std::vector<ui::GameUiIntent> drained = host.DrainUpdateIntents();
  REQUIRE(drained.size() == 2);
  CHECK(drained[0].domainId == 42);
  CHECK(drained[1].domainId == 7);

  // The queue is cleared: a second drain returns nothing.
  CHECK(host.DrainUpdateIntents().empty());
}

// --- U6b notification compatibility bridge -------------------------------
// Failed intents surface their message through the legacy message box on the
// next Update; successful results must not show anything.

TEST_CASE("[Unit] UiPickupFlow - failed result surfaces via the legacy message box") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  host.EnterGameplay();

  LevelManager levelManager;
  entt::registry registry;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  UISystem::State.showMessageBox = false;
  UISystem::State.messageBoxText[0] = '\0';

  // Refresh raylib's frame timer so the 2.0s message box timer set by the
  // compatibility bridge does not expire inside UISystem::Update mid-test.
  BeginDrawing();
  EndDrawing();

  host.Publish({false, "Inventory is full"});
  host.Update(registry, levelManager, ui::GameUiSnapshot{});

  CHECK(UISystem::State.showMessageBox);
  CHECK(std::string(UISystem::State.messageBoxText).find("Inventory is full") !=
        std::string::npos);

  // A successful result must not trigger the failure box.
  UISystem::State.showMessageBox = false;
  UISystem::State.messageBoxText[0] = '\0';
  host.Publish({true, ""});
  host.Update(registry, levelManager, ui::GameUiSnapshot{});
  CHECK_FALSE(UISystem::State.showMessageBox);

  host.LeaveGameplay();
  host.Shutdown();
}

// --- U6b intent closed loop: snapshot -> intent -> command handler ---------

TEST_CASE("[Unit] UiPickupFlow - snapshot pickup intent closes the loop through the handler") {
  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<InventoryComponent>(player);

  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = 3;
  itemComp.quantity = 1;
  itemComp.maxStack = 1;
  registry.emplace<Position>(item, 50.0f, 0.0f);

  // Build the frame snapshot (as GameplayState does), derive an intent from
  // the pickup's domainId, then execute it through the command handler.
  ui::GameUiSnapshotBuilder builder;
  const ui::GameUiSnapshot snapshot = builder.Build(registry);
  REQUIRE(snapshot.pickups.size() == 1);

  ui::GameUiIntent intent;
  intent.kind = ui::GameUiIntentKind::PickupItem;
  intent.domainId = snapshot.pickups[0].domainId;

  ui::GameUiCommandHandler handler;
  const ui::GameUiResult result = handler.Execute(registry, intent);
  CHECK(result.success);

  auto& inventory = registry.get<InventoryComponent>(player);
  CHECK(std::find(inventory.items.begin(), inventory.items.end(), item) !=
        inventory.items.end());
  CHECK_FALSE(registry.all_of<Position>(item));

  // An intent with an invalid target fails without touching the world.
  registry.destroy(item);
  const entt::entity deadTarget = registry.create();
  const std::uint64_t staleDomainId = entt::to_integral(deadTarget);
  registry.destroy(deadTarget);

  ui::GameUiIntent stale;
  stale.kind = ui::GameUiIntentKind::PickupItem;
  stale.domainId = staleDomainId;
  const ui::GameUiResult staleResult = handler.Execute(registry, stale);
  CHECK_FALSE(staleResult.success);
  CHECK_FALSE(staleResult.notification.empty());
}

} // namespace
} // namespace NoMoreDay
