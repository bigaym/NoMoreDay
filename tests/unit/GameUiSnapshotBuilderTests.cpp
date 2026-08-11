#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"

#include <entt/entt.hpp>
#include <cstdint>

using namespace NoMoreDay;

namespace {

entt::entity CreateWorldItem(entt::registry& registry, float x, float y,
                             std::uint32_t itemId) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.name = "Test Item";
  registry.emplace<Position>(item, x, y);
  return item;
}

} // namespace

TEST_CASE("[Unit] GameUiSnapshot - player fields are extracted from components") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<HealthComponent>(player, 75.0f, 100.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 7;
  auto& inventory = registry.emplace<InventoryComponent>(player);
  inventory.gold = 42;

  const entt::entity bagItem = registry.create();
  inventory.items[0] = bagItem;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(75.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(100.0f));
  CHECK(snapshot.player.level == 7);
  CHECK(snapshot.player.inventoryUsed == 1);
  CHECK(snapshot.player.inventoryCapacity == InventoryComponent::BASE_CAPACITY);
  CHECK(snapshot.player.gold == 42);
  CHECK(snapshot.pickups.empty());
  CHECK(snapshot.notifications.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - pickups include in-range items and skip far ones") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  // 150 units away: in range.
  const entt::entity nearItem = CreateWorldItem(registry, 100.0f, 350.0f, 1);
  // Exactly 180 units away: boundary, still in range (<=).
  const entt::entity boundaryItem = CreateWorldItem(registry, 100.0f, 380.0f, 2);
  // 300 units away: out of range.
  CreateWorldItem(registry, 100.0f, 500.0f, 3);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.pickups.size() == 2);
  CHECK(snapshot.pickups[0].domainId == entt::to_integral(nearItem));
  CHECK(snapshot.pickups[0].distance == doctest::Approx(150.0f));
  CHECK(snapshot.pickups[0].source ==
        NoMoreDay::ui::GameUiPickupSource::World);
  CHECK(snapshot.pickups[1].domainId == entt::to_integral(boundaryItem));
  CHECK(snapshot.pickups[1].distance == doctest::Approx(180.0f));
}

TEST_CASE("[Unit] GameUiSnapshot - pickups are sorted by distance ascending") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Far first on purpose: the builder must sort them.
  CreateWorldItem(registry, 0.0f, 160.0f, 1);
  const entt::entity closest = CreateWorldItem(registry, 0.0f, 40.0f, 2);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.pickups.size() == 2);
  CHECK(snapshot.pickups[0].domainId == entt::to_integral(closest));
  CHECK(snapshot.pickups[0].distance == doctest::Approx(40.0f));
  CHECK(snapshot.pickups[1].distance == doctest::Approx(160.0f));
}

TEST_CASE("[Unit] GameUiSnapshot - non-pickable entities are skipped") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Entity with Position but no ItemComponent: not pickable.
  const entt::entity noItem = registry.create();
  registry.emplace<Position>(noItem, 0.0f, 50.0f);

  // Entity with ItemComponent but no Position: not a ground item.
  const entt::entity noPosition = registry.create();
  registry.emplace<ItemComponent>(noPosition);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.pickups.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - destroyed entities do not crash the builder") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  const entt::entity item = CreateWorldItem(registry, 0.0f, 50.0f, 1);
  registry.destroy(item);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.pickups.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - empty registry yields a safe default snapshot") {
  entt::registry registry;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK_FALSE(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(0.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(0.0f));
  CHECK(snapshot.player.level == 1);
  CHECK(snapshot.pickups.empty());
  CHECK(snapshot.notifications.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - player without optional components keeps defaults") {
  entt::registry registry;

  // PlayerTag and Position only; no Health/PlayerStats/Inventory components.
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(0.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(0.0f));
  CHECK(snapshot.player.level == 1);
  CHECK(snapshot.player.inventoryUsed == 0);
  CHECK(snapshot.player.inventoryCapacity == 0);
  CHECK(snapshot.pickups.empty());
}
