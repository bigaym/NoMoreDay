#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiIntent.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/Stats.hpp"

#include "game/systems/item/ItemFactory.hpp" // loadAffixDefinitions (refine range table)
#include "game/systems/item/StashSystem.hpp"

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
  intent.payload.sourceDomainId = entt::to_integral(item);
  return intent;
}

NoMoreDay::ui::GameUiIntent UseIntent(entt::entity item) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = NoMoreDay::ui::GameUiIntentKind::UseItem;
  intent.payload.sourceDomainId = entt::to_integral(item);
  return intent;
}

NoMoreDay::ui::GameUiIntent PickupIntent(entt::entity item) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = NoMoreDay::ui::GameUiIntentKind::PickupItem;
  intent.payload.sourceDomainId = entt::to_integral(item);
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

// ---------------------------------------------------------------------------
// R1 intent matrix: inventory / stash / crafting / attribute domains.
// ---------------------------------------------------------------------------

namespace {

// Places an item at a specific inventory index (unlike AddItemToInventory which
// fills the first empty cell).
entt::entity AddItemAtSlot(entt::registry& registry, entt::entity player,
                           std::uint32_t itemId, ItemType type, int slotIndex,
                           int quantity = 1) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.name = "Test Item";
  itemComp.type = type;
  itemComp.quantity = quantity;
  itemComp.maxStack = std::max(1, quantity);
  auto& inventory = registry.get<InventoryComponent>(player);
  REQUIRE(slotIndex >= 0);
  REQUIRE(slotIndex < static_cast<int>(inventory.items.size()));
  REQUIRE_FALSE(registry.valid(inventory.items[slotIndex]));
  inventory.items[slotIndex] = item;
  return item;
}

void CreatePlayerWithStash(entt::registry& registry, entt::entity player) {
  registry.emplace<PersonalStashComponent>(player);
}

NoMoreDay::ui::GameUiIntent MakeIntent(NoMoreDay::ui::GameUiIntentKind kind) {
  NoMoreDay::ui::GameUiIntent intent;
  intent.kind = kind;
  return intent;
}

} // namespace

// --- DropItem --------------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - dropping part of a stack splits it") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0, /*quantity=*/5);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DropItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.quantity = 2;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // Original stack stays in the bag with the remaining quantity.
  CHECK(registry.get<InventoryComponent>(player).items[0] == item);
  CHECK(registry.get<ItemComponent>(item).quantity == 3);
  CHECK(result.clearedDomainIds.empty());
}

TEST_CASE("[Unit] GameUiCommandHandler - dropping a whole stack clears its id") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0, /*quantity=*/5);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DropItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.quantity = 5;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).items[0]));
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(item)) != result.clearedDomainIds.end());
  CHECK(registry.valid(item)); // Moved to the world, not destroyed.
}

TEST_CASE("[Unit] GameUiCommandHandler - dropping an invalid quantity fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DropItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.quantity = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

TEST_CASE("[Unit] GameUiCommandHandler - dropping a world item is rejected") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Material, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DropItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.quantity = 1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NotInInventory);
}

// --- DestroyItem -----------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - destroying a stack destroys the entity") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DestroyItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(item));
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).items[0]));
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(item)) != result.clearedDomainIds.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - destroying a locked item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);
  registry.get<ItemComponent>(item).isLocked = true;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DestroyItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::Locked);
  CHECK(registry.valid(item)); // Untouched.
}

TEST_CASE("[Unit] GameUiCommandHandler - destroying with a zero quantity fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::DestroyItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.quantity = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

// --- Lock / Unlock ---------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - locking and unlocking toggles isLocked") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto lock = MakeIntent(NoMoreDay::ui::GameUiIntentKind::LockItem);
  lock.payload.sourceDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult lockResult =
      handler.Execute(registry, lock);
  CHECK(lockResult.success);
  // EnTT safety: re-acquire the component after the mutator ran.
  CHECK(registry.get<ItemComponent>(item).isLocked);

  auto unlock = MakeIntent(NoMoreDay::ui::GameUiIntentKind::UnlockItem);
  unlock.payload.sourceDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult unlockResult =
      handler.Execute(registry, unlock);
  CHECK(unlockResult.success);
  CHECK_FALSE(registry.get<ItemComponent>(item).isLocked);
}

TEST_CASE("[Unit] GameUiCommandHandler - locking a world item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateWorldItem(registry, 50.0f, 0.0f, 1,
                                            ItemType::Material, 1, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::LockItem);
  intent.payload.sourceDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NotOwned);
}

// --- Organize --------------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - organize compacts and sorts the bag") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity weapon = AddItemAtSlot(registry, player, 1,
                                            ItemType::Weapon, 2);
  const entt::entity material = AddItemAtSlot(registry, player, 2,
                                              ItemType::Material, 5);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, MakeIntent(
          NoMoreDay::ui::GameUiIntentKind::OrganizeInventory));

  CHECK(result.success);
  const auto& items = registry.get<InventoryComponent>(player).items;
  // Sorted by type: Weapon(0) before Material(5), both compacted to the front.
  CHECK(items[0] == weapon);
  CHECK(items[1] == material);
  CHECK_FALSE(registry.valid(items[2]));
}

TEST_CASE("[Unit] GameUiCommandHandler - organize without an inventory fails") {
  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, MakeIntent(
          NoMoreDay::ui::GameUiIntentKind::OrganizeInventory));

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::MissingComponent);
}

// --- Move / Swap -----------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - moving an item to an empty slot") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::MoveItem);
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 2;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  const auto& items = registry.get<InventoryComponent>(player).items;
  CHECK_FALSE(registry.valid(items[0]));
  CHECK(items[2] == item);
}

TEST_CASE("[Unit] GameUiCommandHandler - moving into an occupied slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity a = AddItemAtSlot(registry, player, 1, ItemType::Material, 0);
  const entt::entity b = AddItemAtSlot(registry, player, 2, ItemType::Material, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::MoveItem);
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
  const auto& items = registry.get<InventoryComponent>(player).items;
  CHECK(items[0] == a);
  CHECK(items[1] == b);
}

TEST_CASE("[Unit] GameUiCommandHandler - moving with a negative slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  AddItemAtSlot(registry, player, 1, ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::MoveItem);
  intent.payload.sourceSlot = -1;
  intent.payload.targetSlot = 2;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
}

TEST_CASE("[Unit] GameUiCommandHandler - swapping two occupied slots") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity a = AddItemAtSlot(registry, player, 1, ItemType::Material, 0);
  const entt::entity b = AddItemAtSlot(registry, player, 2, ItemType::Material, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::SwapItems);
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  const auto& items = registry.get<InventoryComponent>(player).items;
  CHECK(items[0] == b);
  CHECK(items[1] == a);
}

TEST_CASE("[Unit] GameUiCommandHandler - swapping with an empty source fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  AddItemAtSlot(registry, player, 1, ItemType::Material, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::SwapItems);
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 1; // Empty.
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
}

// --- Bag operations --------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - bag equip and unequip round trip") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity bag = AddItemAtSlot(registry, player, 100,
                                         ItemType::Bag, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto equip = MakeIntent(NoMoreDay::ui::GameUiIntentKind::BagEquip);
  equip.payload.sourceDomainId = entt::to_integral(bag);
  equip.payload.targetSlot = 0;
  const NoMoreDay::ui::GameUiResult equipResult =
      handler.Execute(registry, equip);
  CHECK(equipResult.success);
  CHECK(registry.get<InventoryComponent>(player).bag_slots[0] == bag);
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).items[0]));

  auto unequip = MakeIntent(NoMoreDay::ui::GameUiIntentKind::BagUnequip);
  unequip.payload.sourceSlot = 0;
  unequip.payload.bagAction =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiBagAction::Unequip);
  const NoMoreDay::ui::GameUiResult unequipResult =
      handler.Execute(registry, unequip);
  CHECK(unequipResult.success);
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).bag_slots[0]));
  const auto& items = registry.get<InventoryComponent>(player).items;
  CHECK(std::find(items.begin(), items.end(), bag) != items.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - bag unequip with an invalid slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::BagUnequip);
  intent.payload.sourceSlot = 4; // MAX_BAG_SLOTS.
  intent.payload.bagAction =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiBagAction::Unequip);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
}

TEST_CASE("[Unit] GameUiCommandHandler - equipping a non-bag item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity weapon = AddItemAtSlot(registry, player, 1,
                                            ItemType::Weapon, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::BagEquip);
  intent.payload.sourceDomainId = entt::to_integral(weapon);
  intent.payload.targetSlot = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::DomainPrecondition);
}

// --- Socket / Unsocket -----------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - socketing a rune into an item") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(item).socketCount = 1;
  const entt::entity rune = AddItemAtSlot(registry, player, 500,
                                          ItemType::Material, 1);
  registry.emplace<RuneComponent>(rune);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::SocketRune);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.sourceDomainId = entt::to_integral(rune);
  intent.payload.socketIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // EnTT safety: re-acquire after the mutator resized the sockets vector.
  const auto& sockets = registry.get<ItemComponent>(item).sockets;
  REQUIRE(sockets.size() == 1);
  CHECK(sockets[0] == rune);
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(rune)) != result.clearedDomainIds.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - socketing a non-rune fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(item).socketCount = 1;
  // A plain material without RuneComponent.
  const entt::entity filler = AddItemAtSlot(registry, player, 2,
                                            ItemType::Material, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::SocketRune);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.sourceDomainId = entt::to_integral(filler);
  intent.payload.socketIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::MaterialMissing);
}

TEST_CASE("[Unit] GameUiCommandHandler - socketing with an out-of-range index fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(item).socketCount = 1;
  const entt::entity rune = AddItemAtSlot(registry, player, 500,
                                          ItemType::Material, 1);
  registry.emplace<RuneComponent>(rune);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::SocketRune);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.sourceDomainId = entt::to_integral(rune);
  intent.payload.socketIndex = 5;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::CraftingFailure);
}

TEST_CASE("[Unit] GameUiCommandHandler - unsocketing clears the socket") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  const entt::entity rune = AddItemAtSlot(registry, player, 500,
                                          ItemType::Material, 1);
  registry.get<ItemComponent>(item).sockets = {rune};

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::UnsocketRune);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.socketIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // The socket slot is freed; the rune entity itself is preserved.
  CHECK_FALSE(registry.valid(registry.get<ItemComponent>(item).sockets[0]));
  CHECK(registry.valid(rune));
}

TEST_CASE("[Unit] GameUiCommandHandler - unsocketing an empty socket fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::UnsocketRune);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.socketIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::CraftingFailure);
}

// --- Stash -----------------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - transferring between stash slots") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  auto& stash = registry.get<PersonalStashComponent>(player);
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  stash.tabs[0].items[0] = item;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashTransfer);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.sourceTab = 0;
  intent.payload.sourceSlot = 0;
  intent.payload.targetTab = 0;
  intent.payload.targetSlot = 2;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(stash.tabs[0].items[0]));
  CHECK(stash.tabs[0].items[2] == item);
}

TEST_CASE("[Unit] GameUiCommandHandler - transferring from an invalid tab fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  auto& stash = registry.get<PersonalStashComponent>(player);
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  stash.tabs[0].items[0] = item;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashTransfer);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.sourceTab = 0;
  intent.payload.sourceSlot = 0;
  intent.payload.targetTab = 9; // Not unlocked.
  intent.payload.targetSlot = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
}

TEST_CASE("[Unit] GameUiCommandHandler - transferring from an empty slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashTransfer);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.sourceTab = 0;
  intent.payload.sourceSlot = 0;
  intent.payload.targetTab = 0;
  intent.payload.targetSlot = 2;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidSlot);
}

TEST_CASE("[Unit] GameUiCommandHandler - depositing into the stash") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashDeposit);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.sourceSlot = 0;
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.targetTab = 0;
  intent.payload.targetSlot = 3;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).items[0]));
  // EnTT safety: re-acquire the stash tab after the move.
  const auto& tabs = registry.get<PersonalStashComponent>(player).tabs;
  CHECK(tabs[0].items[3] == item);
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(item)) != result.clearedDomainIds.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - depositing without a stash fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashDeposit);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.sourceSlot = 0;
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.targetTab = 0;
  intent.payload.targetSlot = 3;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::CapacityFull);
}

TEST_CASE("[Unit] GameUiCommandHandler - deposit requires the claimed source slot") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  // Item is at index 0 but the intent claims index 1.
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashDeposit);
  intent.payload.sourceDomainId = entt::to_integral(item);
  intent.payload.sourceSlot = 1;
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.targetTab = 0;
  intent.payload.targetSlot = 3;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(registry.get<InventoryComponent>(player).items[0] == item); // Unmoved.
}

TEST_CASE("[Unit] GameUiCommandHandler - withdrawing from the stash") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  auto& stash = registry.get<PersonalStashComponent>(player);
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  stash.tabs[0].items[0] = item;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashWithdraw);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.sourceTab = 0;
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(stash.tabs[0].items[0]));
  CHECK(registry.get<InventoryComponent>(player).items[1] == item);
}

TEST_CASE("[Unit] GameUiCommandHandler - withdrawing to an invalid bag slot fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  auto& stash = registry.get<PersonalStashComponent>(player);
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  stash.tabs[0].items[0] = item;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashWithdraw);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.sourceTab = 0;
  intent.payload.sourceSlot = 0;
  intent.payload.targetSlot = 40; // Out of range.
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::CapacityFull);
}

TEST_CASE("[Unit] GameUiCommandHandler - unlocking a stash tab spends gold") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  registry.get<InventoryComponent>(player).gold = 5000;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashUnlockTab);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(registry.get<PersonalStashComponent>(player).unlockedTabs == 2);
  CHECK(registry.get<InventoryComponent>(player).gold == 0);
}

TEST_CASE("[Unit] GameUiCommandHandler - unlocking a tab without gold fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  registry.get<InventoryComponent>(player).gold = 4999;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashUnlockTab);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::DomainPrecondition);
  CHECK(registry.get<PersonalStashComponent>(player).unlockedTabs == 1);
}

TEST_CASE("[Unit] GameUiCommandHandler - sorting a stash tab") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  auto& stash = registry.get<PersonalStashComponent>(player);
  const entt::entity item = registry.create();
  registry.emplace<ItemComponent>(item);
  stash.tabs[0].items[0] = item;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashSort);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.targetTab = 0;
  intent.payload.sortMode =
      static_cast<std::uint8_t>(NoMoreDay::StashSortMode::RarityDesc);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // Item remains inside the tab (order may change, contents do not).
  const auto& items = stash.tabs[0].items;
  CHECK(std::find(items.begin(), items.end(), item) != items.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - sorting a negative tab fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashSort);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  intent.payload.targetTab = -1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

TEST_CASE("[Unit] GameUiCommandHandler - auto-depositing the whole bag") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);
  const entt::entity a = AddItemAtSlot(registry, player, 1, ItemType::Weapon, 0);
  const entt::entity b = AddItemAtSlot(registry, player, 2, ItemType::Weapon, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashAutoDeposit);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(result.notification == "Auto-deposited 2 items");
  const auto& items = registry.get<InventoryComponent>(player).items;
  CHECK_FALSE(registry.valid(items[0]));
  CHECK_FALSE(registry.valid(items[1]));
  const auto& stashItems = registry.get<PersonalStashComponent>(player).tabs[0].items;
  CHECK(std::find(stashItems.begin(), stashItems.end(), a) != stashItems.end());
  CHECK(std::find(stashItems.begin(), stashItems.end(), b) != stashItems.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - auto-depositing an empty bag reports zero") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  CreatePlayerWithStash(registry, player);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::StashAutoDeposit);
  intent.payload.stashTarget =
      static_cast<std::uint8_t>(NoMoreDay::ui::GameUiStashTarget::Personal);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(result.notification == "No items to auto-deposit");
}

// --- Crafting --------------------------------------------------------------

// Item with one prefix affix and some forging potential.
entt::entity CreateForgeableWeapon(entt::registry& registry,
                                   entt::entity player, int slotIndex) {
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, slotIndex);
  auto& comp = registry.get<ItemComponent>(item);
  comp.forgingPotential = 10;
  Affix affix;
  affix.type = AffixType::Strength;
  affix.value = 5.0f;
  affix.tier = 1;
  affix.isPrefix = true;
  comp.affixes.push_back(affix);
  return item;
}

TEST_CASE("[Unit] GameUiCommandHandler - upgrading an affix tier") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftAffixUpgrade);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // EnTT safety: re-acquire the component after the mutator ran.
  const auto& comp = registry.get<ItemComponent>(item);
  REQUIRE(comp.affixes.size() == 1);
  CHECK(comp.affixes[0].tier == 2);
  CHECK(comp.forgingPotential < 10); // Potential was spent.
}

TEST_CASE("[Unit] GameUiCommandHandler - upgrading an invalid affix index fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftAffixUpgrade);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 5;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

TEST_CASE("[Unit] GameUiCommandHandler - upgrading without potential fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);
  registry.get<ItemComponent>(item).forgingPotential = 0;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftAffixUpgrade);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.notification == "Item has no potential left");
}

TEST_CASE("[Unit] GameUiCommandHandler - chaos rerolls an affix") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftChaos);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(registry.valid(item));
}

TEST_CASE("[Unit] GameUiCommandHandler - chaos with an invalid affix index fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftChaos);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

TEST_CASE("[Unit] GameUiCommandHandler - refining an affix value") {
  // The affix value range is data-driven (assets/data/affixes.json); load the
  // table so the refine roll has a range to sample (ctest runs with the repo
  // root as the working directory).
  ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftRefine);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // EnTT safety: re-acquire after the mutator; Strength tier 1 ranges [3,5].
  const auto& affix = registry.get<ItemComponent>(item).affixes[0];
  CHECK(affix.value >= 3.0f);
  CHECK(affix.value <= 5.0f);
  CHECK(registry.get<ItemComponent>(item).forgingPotential < 10);
}

TEST_CASE("[Unit] GameUiCommandHandler - refining with an invalid affix index fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = CreateForgeableWeapon(registry, player, 0);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftRefine);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixIndex = 9;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::InvalidIndex);
}

TEST_CASE("[Unit] GameUiCommandHandler - adding an affix to an item") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(item).forgingPotential = 10;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftAddAffix);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixType = static_cast<std::uint16_t>(AffixType::Strength);
  intent.payload.isPrefix = true;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  const auto& comp = registry.get<ItemComponent>(item);
  REQUIRE(comp.affixes.size() == 1);
  // createAffix decides the affix flags from the data tables; the handler
  // contract only guarantees a tier-1 affix was appended and potential spent.
  CHECK(comp.affixes[0].tier == 1);
  CHECK(comp.forgingPotential < 10);
}

TEST_CASE("[Unit] GameUiCommandHandler - adding a prefix when prefix slots are full") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  auto& comp = registry.get<ItemComponent>(item);
  comp.forgingPotential = 10;
  for (int i = 0; i < 2; ++i) {
    Affix affix;
    affix.type = static_cast<AffixType>(static_cast<int>(AffixType::Strength) + i);
    affix.value = 1.0f;
    affix.tier = 1;
    affix.isPrefix = true;
    comp.affixes.push_back(affix);
  }

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftAddAffix);
  intent.payload.targetDomainId = entt::to_integral(item);
  intent.payload.affixType = static_cast<std::uint16_t>(AffixType::Strength);
  intent.payload.isPrefix = true;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.notification == "No free affix slot");
}

TEST_CASE("[Unit] GameUiCommandHandler - plain fusion consumes forging potential") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity base = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(base).forgingPotential = 10;
  const entt::entity fodder = AddItemAtSlot(registry, player, 2,
                                            ItemType::Weapon, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftFuse);
  intent.payload.sourceDomainId = entt::to_integral(base);
  intent.payload.targetDomainId = entt::to_integral(fodder);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  // min(10, potential) = 10 was spent; both items stay in the bag.
  CHECK(registry.get<ItemComponent>(base).forgingPotential == 0);
  CHECK(registry.valid(fodder));
}

TEST_CASE("[Unit] GameUiCommandHandler - plain fusion without potential fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity base = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  const entt::entity fodder = AddItemAtSlot(registry, player, 2,
                                            ItemType::Weapon, 1);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftFuse);
  intent.payload.sourceDomainId = entt::to_integral(base);
  intent.payload.targetDomainId = entt::to_integral(fodder);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.notification == "Item has no potential left");
}

TEST_CASE("[Unit] GameUiCommandHandler - legendary fusion consumes fodder and catalyst") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  // Base: MainHand weapon with legendary potential.
  const entt::entity base = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  auto& baseComp = registry.get<ItemComponent>(base);
  baseComp.slot = EquipmentSlot::MainHand;
  baseComp.legendaryPotential = 1;
  // Fodder: exactly 4 affixes, same slot.
  const entt::entity fodder = AddItemAtSlot(registry, player, 2,
                                            ItemType::Weapon, 1);
  auto& fodderComp = registry.get<ItemComponent>(fodder);
  fodderComp.slot = EquipmentSlot::MainHand;
  for (int i = 0; i < 4; ++i) {
    Affix affix;
    affix.type = static_cast<AffixType>(static_cast<int>(AffixType::Strength) + i);
    affix.value = 1.0f;
    affix.tier = 1;
    fodderComp.affixes.push_back(affix);
  }
  // Catalyst: Legendary Core.
  const entt::entity catalyst = AddItemAtSlot(registry, player, 900,
                                              ItemType::Material, 2);
  registry.get<ItemComponent>(catalyst).name = "Legendary Core";

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftFuse);
  intent.payload.sourceDomainId = entt::to_integral(base);
  intent.payload.targetDomainId = entt::to_integral(fodder);
  intent.payload.catalystDomainId = entt::to_integral(catalyst);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(fodder));
  CHECK_FALSE(registry.valid(catalyst));
  CHECK(registry.get<ItemComponent>(base).rarity == Rarity::Ancient);
  CHECK(registry.get<ItemComponent>(base).legendaryPotential == 0);
  REQUIRE(result.clearedDomainIds.size() == 2);
  CHECK(result.clearedDomainIds[0] == entt::to_integral(fodder));
  CHECK(result.clearedDomainIds[1] == entt::to_integral(catalyst));
}

TEST_CASE("[Unit] GameUiCommandHandler - legendary fusion rejects a wrong catalyst") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity base = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  auto& baseComp = registry.get<ItemComponent>(base);
  baseComp.slot = EquipmentSlot::MainHand;
  baseComp.legendaryPotential = 1;
  const entt::entity fodder = AddItemAtSlot(registry, player, 2,
                                            ItemType::Weapon, 1);
  auto& fodderComp = registry.get<ItemComponent>(fodder);
  fodderComp.slot = EquipmentSlot::MainHand;
  for (int i = 0; i < 4; ++i) {
    Affix affix;
    affix.type = static_cast<AffixType>(static_cast<int>(AffixType::Strength) + i);
    affix.value = 1.0f;
    affix.tier = 1;
    fodderComp.affixes.push_back(affix);
  }
  const entt::entity catalyst = AddItemAtSlot(registry, player, 900,
                                              ItemType::Material, 2);
  registry.get<ItemComponent>(catalyst).name = "Wrong Catalyst";

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftFuse);
  intent.payload.sourceDomainId = entt::to_integral(base);
  intent.payload.targetDomainId = entt::to_integral(fodder);
  intent.payload.catalystDomainId = entt::to_integral(catalyst);
  intent.payload.affixIndex = 0;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(registry.valid(base));
  CHECK(registry.valid(fodder)); // Nothing was consumed.
}

// --- Salvage ---------------------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - salvaging a rare weapon destroys it") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  registry.emplace<MaterialBankComponent>(player);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  auto& comp = registry.get<ItemComponent>(item);
  comp.rarity = Rarity::Rare;
  Affix affix;
  affix.type = AffixType::Strength;
  affix.value = 5.0f;
  affix.tier = 1;
  comp.affixes.push_back(affix);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftSalvage);
  intent.payload.targetDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK_FALSE(registry.valid(item));
  CHECK_FALSE(registry.valid(registry.get<InventoryComponent>(player).items[0]));
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(item)) != result.clearedDomainIds.end());
}

TEST_CASE("[Unit] GameUiCommandHandler - salvaging a locked item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  auto& comp = registry.get<ItemComponent>(item);
  comp.rarity = Rarity::Rare;
  comp.isLocked = true;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftSalvage);
  intent.payload.targetDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::Locked);
  CHECK(registry.valid(item));
}

TEST_CASE("[Unit] GameUiCommandHandler - salvaging a common item fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity item = AddItemAtSlot(registry, player, 1,
                                          ItemType::Weapon, 0);
  registry.get<ItemComponent>(item).rarity = Rarity::Common;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftSalvage);
  intent.payload.targetDomainId = entt::to_integral(item);
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::DomainPrecondition);
}

TEST_CASE("[Unit] GameUiCommandHandler - batch salvaging honors the filter") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity a = AddItemAtSlot(registry, player, 1, ItemType::Weapon, 0);
  const entt::entity b = AddItemAtSlot(registry, player, 2, ItemType::Weapon, 1);
  const entt::entity common = AddItemAtSlot(registry, player, 3,
                                            ItemType::Weapon, 2);
  const entt::entity locked = AddItemAtSlot(registry, player, 4,
                                            ItemType::Weapon, 3);
  registry.get<ItemComponent>(a).rarity = Rarity::Rare;
  registry.get<ItemComponent>(b).rarity = Rarity::Rare;
  registry.get<ItemComponent>(common).rarity = Rarity::Common;
  auto& lockedComp = registry.get<ItemComponent>(locked);
  lockedComp.rarity = Rarity::Rare;
  lockedComp.isLocked = true;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftBatchSalvage);
  intent.payload.salvageRarityMask = 1u << static_cast<std::uint32_t>(Rarity::Rare);
  intent.payload.excludeLocked = true;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(result.notification == "Salvaged 2 items");
  REQUIRE(result.clearedDomainIds.size() == 2);
  CHECK_FALSE(registry.valid(a));
  CHECK_FALSE(registry.valid(b));
  CHECK(registry.valid(common)); // Below rarity filter.
  CHECK(registry.valid(locked)); // Excluded by the lock filter.
}

TEST_CASE("[Unit] GameUiCommandHandler - batch salvage with no matches fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity a = AddItemAtSlot(registry, player, 1, ItemType::Weapon, 0);
  registry.get<ItemComponent>(a).rarity = Rarity::Rare;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftBatchSalvage);
  intent.payload.salvageRarityMask =
      1u << static_cast<std::uint32_t>(Rarity::Ancient); // Nothing matches.
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.notification == "No items match the salvage filter");
  CHECK(result.clearedDomainIds.empty());
  CHECK(registry.valid(a));
}

TEST_CASE("[Unit] GameUiCommandHandler - batch salvage keeps tier 6 affixes") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  const entt::entity t6 = AddItemAtSlot(registry, player, 1, ItemType::Weapon, 0);
  auto& t6Comp = registry.get<ItemComponent>(t6);
  t6Comp.rarity = Rarity::Rare;
  Affix affix;
  affix.type = AffixType::Strength;
  affix.value = 5.0f;
  affix.tier = 6;
  t6Comp.affixes.push_back(affix);
  const entt::entity plain = AddItemAtSlot(registry, player, 2,
                                           ItemType::Weapon, 1);
  registry.get<ItemComponent>(plain).rarity = Rarity::Rare;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(NoMoreDay::ui::GameUiIntentKind::CraftBatchSalvage);
  intent.payload.keepIfTier6Plus = true;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  CHECK(result.notification == "Salvaged 1 items");
  CHECK(registry.valid(t6));   // Kept.
  CHECK_FALSE(registry.valid(plain));
}

// --- Attribute allocation --------------------------------------------------

TEST_CASE("[Unit] GameUiCommandHandler - confirming attribute allocation") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.available_attribute_points = 5;
  auto& primary = registry.emplace<PrimaryStats>(player);
  primary.strength = 10;
  primary.dexterity = 10;
  primary.intelligence = 10;
  primary.vitality = 10;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(
      NoMoreDay::ui::GameUiIntentKind::ConfirmAttributeAllocation);
  intent.payload.allocationStrength = 2;
  intent.payload.allocationVitality = 3;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK(result.success);
  const auto& primaryAfter = registry.get<PrimaryStats>(player);
  CHECK(primaryAfter.strength == 12);
  CHECK(primaryAfter.vitality == 13);
  CHECK(registry.get<PlayerStats>(player).available_attribute_points == 0);
  CHECK(registry.any_of<StatsDirty>(player)); // Recalc requested.
}

TEST_CASE("[Unit] GameUiCommandHandler - allocating more points than available fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.available_attribute_points = 5;
  auto& primary = registry.emplace<PrimaryStats>(player);
  primary.strength = 10;
  primary.vitality = 10;

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(
      NoMoreDay::ui::GameUiIntentKind::ConfirmAttributeAllocation);
  intent.payload.allocationStrength = 6;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::DomainPrecondition);
  CHECK(registry.get<PlayerStats>(player).available_attribute_points == 5);
  CHECK(registry.get<PrimaryStats>(player).strength == 10); // Unchanged.
}

TEST_CASE("[Unit] GameUiCommandHandler - allocating a negative amount fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.available_attribute_points = 5;
  registry.emplace<PrimaryStats>(player);

  NoMoreDay::ui::GameUiCommandHandler handler;
  auto intent = MakeIntent(
      NoMoreDay::ui::GameUiIntentKind::ConfirmAttributeAllocation);
  intent.payload.allocationStrength = -1;
  const NoMoreDay::ui::GameUiResult result = handler.Execute(registry, intent);

  CHECK_FALSE(result.success);
}

TEST_CASE("[Unit] GameUiCommandHandler - allocating without PlayerStats fails") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  registry.emplace<PrimaryStats>(player);

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, MakeIntent(
          NoMoreDay::ui::GameUiIntentKind::ConfirmAttributeAllocation));

  CHECK_FALSE(result.success);
  CHECK(result.code == NoMoreDay::ui::GameUiResultCode::DomainPrecondition);
}

// --- EnTT safety regression: cleared ids after mutators --------------------

TEST_CASE("[Unit] GameUiCommandHandler - using a potion reports the consumed id") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry, 0.0f, 0.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 1;
  auto& combat = registry.emplace<CombatStats>(player);
  combat.health = 10.0f;
  combat.max_health = 100.0f;
  const entt::entity potion = AddItemToInventory(registry, player, 101,
                                                 ItemType::Consumable,
                                                 EquipmentSlot::None);
  registry.get<ItemComponent>(potion).name = "Health Potion";

  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiResult result =
      handler.Execute(registry, UseIntent(potion));

  CHECK(result.success);
  CHECK_FALSE(registry.valid(potion)); // Consumed by the mutator.
  CHECK(std::find(result.clearedDomainIds.begin(),
                  result.clearedDomainIds.end(),
                  entt::to_integral(potion)) != result.clearedDomainIds.end());
}

// --- No-player matrix (shared TryResolvePlayer validation) -----------------

TEST_CASE("[Unit] GameUiCommandHandler - inventory intents fail without a player") {
  entt::registry registry;
  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiIntentKind kinds[] = {
      NoMoreDay::ui::GameUiIntentKind::DropItem,
      NoMoreDay::ui::GameUiIntentKind::DestroyItem,
      NoMoreDay::ui::GameUiIntentKind::LockItem,
      NoMoreDay::ui::GameUiIntentKind::UnlockItem,
      NoMoreDay::ui::GameUiIntentKind::OrganizeInventory,
      NoMoreDay::ui::GameUiIntentKind::MoveItem,
      NoMoreDay::ui::GameUiIntentKind::SwapItems,
      NoMoreDay::ui::GameUiIntentKind::BagEquip,
      NoMoreDay::ui::GameUiIntentKind::BagUnequip,
  };
  for (const auto kind : kinds) {
    const NoMoreDay::ui::GameUiResult result =
        handler.Execute(registry, MakeIntent(kind));
    CAPTURE(kind);
    CHECK_FALSE(result.success);
    CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NoPlayer);
  }
}

TEST_CASE("[Unit] GameUiCommandHandler - stash intents fail without a player") {
  entt::registry registry;
  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiIntentKind kinds[] = {
      NoMoreDay::ui::GameUiIntentKind::StashTransfer,
      NoMoreDay::ui::GameUiIntentKind::StashDeposit,
      NoMoreDay::ui::GameUiIntentKind::StashWithdraw,
      NoMoreDay::ui::GameUiIntentKind::StashUnlockTab,
      NoMoreDay::ui::GameUiIntentKind::StashSort,
      NoMoreDay::ui::GameUiIntentKind::StashAutoDeposit,
  };
  for (const auto kind : kinds) {
    const NoMoreDay::ui::GameUiResult result =
        handler.Execute(registry, MakeIntent(kind));
    CAPTURE(kind);
    CHECK_FALSE(result.success);
    CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NoPlayer);
  }
}

TEST_CASE("[Unit] GameUiCommandHandler - crafting intents fail without a player") {
  entt::registry registry;
  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiIntentKind kinds[] = {
      NoMoreDay::ui::GameUiIntentKind::CraftAffixUpgrade,
      NoMoreDay::ui::GameUiIntentKind::CraftChaos,
      NoMoreDay::ui::GameUiIntentKind::CraftRefine,
      NoMoreDay::ui::GameUiIntentKind::CraftAddAffix,
      NoMoreDay::ui::GameUiIntentKind::CraftFuse,
      NoMoreDay::ui::GameUiIntentKind::CraftSalvage,
      NoMoreDay::ui::GameUiIntentKind::CraftBatchSalvage,
  };
  for (const auto kind : kinds) {
    const NoMoreDay::ui::GameUiResult result =
        handler.Execute(registry, MakeIntent(kind));
    CAPTURE(kind);
    CHECK_FALSE(result.success);
    CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NoPlayer);
  }
}

TEST_CASE("[Unit] GameUiCommandHandler - socket and attribute intents fail without a player") {
  entt::registry registry;
  NoMoreDay::ui::GameUiCommandHandler handler;
  const NoMoreDay::ui::GameUiIntentKind kinds[] = {
      NoMoreDay::ui::GameUiIntentKind::SocketRune,
      NoMoreDay::ui::GameUiIntentKind::UnsocketRune,
      NoMoreDay::ui::GameUiIntentKind::ConfirmAttributeAllocation,
  };
  for (const auto kind : kinds) {
    const NoMoreDay::ui::GameUiResult result =
        handler.Execute(registry, MakeIntent(kind));
    CAPTURE(kind);
    CHECK_FALSE(result.success);
    CHECK(result.code == NoMoreDay::ui::GameUiResultCode::NoPlayer);
  }
}
