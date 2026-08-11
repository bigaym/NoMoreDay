#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiIntent.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Stats.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>

using namespace NoMoreDay;

namespace {

entt::entity CreatePlayer(entt::registry& registry, float x, float y) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, x, y);
  registry.emplace<InventoryComponent>(player);
  registry.emplace<EquipmentComponent>(player);
  return player;
}

entt::entity CreateWorldItem(entt::registry& registry, float x, float y,
                             std::uint32_t itemId, ItemType type,
                             int quantity, int maxStack) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.name = "Test Item";
  itemComp.type = type;
  itemComp.quantity = quantity;
  itemComp.maxStack = maxStack;
  registry.emplace<Position>(item, x, y);
  return item;
}

// Creates an item entity and stores it in the player's bag (no Position:
// bag items live outside the world space).
entt::entity AddItemToInventory(entt::registry& registry, entt::entity player,
                                std::uint32_t itemId, ItemType type,
                                EquipmentSlot slot, int quantity = 1) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.name = "Test Item";
  itemComp.type = type;
  itemComp.slot = slot;
  itemComp.quantity = quantity;
  itemComp.maxStack = std::max(1, quantity);
  auto& inventory = registry.get<InventoryComponent>(player);
  for (auto& cell : inventory.items) {
    if (cell == entt::null) {
      cell = item;
      break;
    }
  }
  return item;
}

NoMoreDay::ui::GameUiIntent EquipIntent(entt::entity item) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = NoMoreDay::ui::GameUiIntentKind::EquipItem;
  intent.domainId = entt::to_integral(item);
  return intent;
}

NoMoreDay::ui::GameUiIntent UseIntent(entt::entity item) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = NoMoreDay::ui::GameUiIntentKind::UseItem;
  intent.domainId = entt::to_integral(item);
  return intent;
}

NoMoreDay::ui::GameUiIntent PickupIntent(entt::entity item) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = NoMoreDay::ui::GameUiIntentKind::PickupItem;
  intent.domainId = entt::to_integral(item);
  return intent;
}

} // namespace

TEST_CASE("[Unit] GameUiCommandHandler - pickup succeeds and stores the item") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(item));

  CHECK(result.success);
  CHECK(result.notification.empty());
  auto& inventory = registry.get<InventoryComponent>(player);
  CHECK(std::find(inventory.items.begin(), inventory.items.end(), item) !=
        inventory.items.end());
  CHECK_FALSE(registry.all_of<Position>(item));
}

TEST_CASE("[Unit] GameUiCommandHandler - invalid item entity fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Consumable, 1, 1);
  registry.destroy(item);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - entity without ItemComponent fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity notItem = registry.create();
  registry.emplace<Position>(notItem, 50.0f, 0.0f);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(notItem));

  CHECK_FALSE(result.success);
}

TEST_CASE("[Unit] GameUiCommandHandler - no player entity fails") {
  entt::registry registry;
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - pickup beyond 180 units fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity farItem = CreateWorldItem(registry, 181.0f, 0.0f, 1,
                                               ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(farItem));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - pickup at exactly 180 units succeeds") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity boundaryItem = CreateWorldItem(
      registry, 180.0f, 0.0f, 1, ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(boundaryItem));

  CHECK(result.success);
}

TEST_CASE("[Unit] GameUiCommandHandler - item without Position cannot be picked") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity noPosition = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(noPosition);
  itemComp.id = 1;
  itemComp.quantity = 1;
  itemComp.maxStack = 1;

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(noPosition));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - full inventory fails with notification") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& inventory = registry.get<InventoryComponent>(player);
  inventory.capacity = 1;
  const entt::entity filler = registry.create(); // Occupies every slot.
  for (auto& slot : inventory.items) {
    slot = filler;
  }
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - stackable pickup merges into existing stack") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& inventory = registry.get<InventoryComponent>(player);

  const entt::entity bagItem = registry.create();
  auto& bagComp = registry.emplace<ItemComponent>(bagItem);
  bagComp.id = 7;
  bagComp.quantity = 5;
  bagComp.maxStack = 10;
  inventory.items[0] = bagItem;

  const entt::entity groundItem =
      CreateWorldItem(registry, 50.0f, 0.0f, 7, ItemType::Material, 3, 10);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(groundItem));

  CHECK(result.success);
  CHECK(registry.get<ItemComponent>(bagItem).quantity == 8);
  CHECK_FALSE(registry.valid(groundItem)); // Fully absorbed stack is destroyed.
}

TEST_CASE("[Unit] GameUiCommandHandler - material pickup banks into material bank") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& bank = registry.emplace<MaterialBankComponent>(player);

  const entt::entity material = CreateWorldItem(
      registry, 50.0f, 0.0f, 9, ItemType::Material, 4, 99);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, PickupIntent(material));

  CHECK(result.success);
  CHECK(bank.GetCount(9) == 4);
  CHECK_FALSE(registry.valid(material));
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping a world item not in the player's bag fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiIntent intent = PickupIntent(item);
  intent.kind = NoMoreDay::ui::GameUiIntentKind::EquipItem;

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

// --- EquipItem ---

TEST_CASE("[Unit] GameUiCommandHandler - equipping a weapon fills the resolved slot") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity sword = AddItemToInventory(registry, player, 1,
                                                ItemType::Weapon,
                                                EquipmentSlot::MainHand);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(sword));

  CHECK(result.success);
  CHECK(result.notification.empty());
  CHECK(registry.get<EquipmentComponent>(player).Get(EquipmentSlot::MainHand) ==
        sword);
  const auto& inventory = registry.get<InventoryComponent>(player);
  CHECK(std::find(inventory.items.begin(), inventory.items.end(), sword) ==
        inventory.items.end());
  CHECK(registry.all_of<StatsDirty>(player));
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping a bag fills an empty bag slot") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity bag =
      AddItemToInventory(registry, player, 3, ItemType::Bag, EquipmentSlot::None);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(bag));

  CHECK(result.success);
  CHECK(result.notification.empty());
  CHECK(registry.get<InventoryComponent>(player).bag_slots[0] == bag);
  const auto& inventory = registry.get<InventoryComponent>(player);
  CHECK(std::find(inventory.items.begin(), inventory.items.end(), bag) ==
        inventory.items.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping without a player fails") {
  entt::registry registry;
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping a destroyed entity fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity sword = AddItemToInventory(registry, player, 1,
                                                ItemType::Weapon,
                                                EquipmentSlot::MainHand);
  registry.destroy(sword);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(sword));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping an item without an equipment slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  // Weapon that never resolved a slot cannot be equipped (slot None).
  const entt::entity weapon = AddItemToInventory(registry, player, 1,
                                                 ItemType::Weapon,
                                                 EquipmentSlot::None);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(weapon));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping without an equipment component fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  registry.remove<EquipmentComponent>(player);
  const entt::entity sword = AddItemToInventory(registry, player, 1,
                                                ItemType::Weapon,
                                                EquipmentSlot::MainHand);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, EquipIntent(sword));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

// --- UseItem ---

TEST_CASE("[Unit] GameUiCommandHandler - using a health potion restores health and consumes it") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& stats = registry.emplace<CombatStats>(player);
  stats.health = 50.0f;
  stats.max_health = 100.0f;
  const entt::entity potion = AddItemToInventory(registry, player, 101,
                                                 ItemType::Consumable,
                                                 EquipmentSlot::None);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, UseIntent(potion));

  CHECK(result.success);
  CHECK(result.notification.empty());
  CHECK(stats.health == doctest::Approx(100.0f));
  CHECK_FALSE(registry.valid(potion)); // Quantity 1: fully consumed.
}

TEST_CASE("[Unit] GameUiCommandHandler - using a non-consumable item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);
  const entt::entity material = AddItemToInventory(registry, player, 9,
                                                   ItemType::Material,
                                                   EquipmentSlot::None);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, UseIntent(material));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
  CHECK(registry.valid(material)); // Not consumed.
}

TEST_CASE("[Unit] GameUiCommandHandler - using a world item not in the player's bag fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 101,
                                            ItemType::Consumable, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, UseIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - using an item without a player fails") {
  entt::registry registry;
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, UseIntent(item));

  CHECK_FALSE(result.success);
  CHECK_FALSE(result.notification.empty());
}
