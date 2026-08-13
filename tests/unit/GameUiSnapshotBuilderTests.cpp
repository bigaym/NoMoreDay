#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"

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

TEST_CASE("[Unit] GameUiSnapshot - all-empty skill slots yield empty skill bar views") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  // Default-constructed ActiveSkillsComponent: every slot has id == 0 and every
  // specialized slot has skill_id == INVALID_SKILL_ID (the empty sentinels).
  registry.emplace<ActiveSkillsComponent>(player);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.player.hasPlayer);
  // No non-empty skill slot: the bar view must not expose any slot.
  CHECK(snapshot.skillBar.slots.empty());
  CHECK(snapshot.skillBar.availableTalentPoints == 0);
  // No learned skills are reported for all-empty slots.
  CHECK(snapshot.skillTree.skills.empty());
  CHECK(snapshot.skillTree.availableTalentPoints == 0);
  // Every specialized slot view stays at its default value.
  for (const auto& specialized : snapshot.skillTree.specializedSlots) {
    CHECK(specialized.skillId == NoMoreDay::ui::kInvalidSkillId);
    CHECK(specialized.level == 0);
    CHECK(specialized.iconAssetId == 0);
    CHECK(specialized.allocatedPoints.empty());
  }
}

TEST_CASE("[Unit] GameUiSnapshot - skill bar keeps only non-empty slots in source order") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 2001;
  active.slots[0].cooldown = 1.5f;
  active.slots[0].current_charges = 2;
  // slots[1] intentionally left as the default empty sentinel (id == 0).
  active.slots[3].id = 2003;
  active.slots[3].cooldown = 0.75f;
  active.slots[3].current_charges = 1;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  // Only the two non-empty slots are exposed, in ascending source order.
  REQUIRE(snapshot.skillBar.slots.size() == 2);
  CHECK(snapshot.skillBar.slots[0].slotIndex == 0);
  CHECK(snapshot.skillBar.slots[0].skillId == 2001);
  CHECK(snapshot.skillBar.slots[0].cooldown == doctest::Approx(1.5f));
  CHECK(snapshot.skillBar.slots[0].currentCharges == 2);
  CHECK(snapshot.skillBar.slots[1].slotIndex == 3);
  CHECK(snapshot.skillBar.slots[1].skillId == 2003);
  CHECK(snapshot.skillBar.slots[1].cooldown == doctest::Approx(0.75f));
  CHECK(snapshot.skillBar.slots[1].currentCharges == 1);
  // Views must be ordered by ascending source slot index.
  CHECK(snapshot.skillBar.slots[0].slotIndex < snapshot.skillBar.slots[1].slotIndex);
  // Learned-skill views mirror the same non-empty slots only.
  REQUIRE(snapshot.skillTree.skills.size() == 2);
  CHECK(snapshot.skillTree.skills[0].skillId == 2001);
  CHECK(snapshot.skillTree.skills[1].skillId == 2003);
}
