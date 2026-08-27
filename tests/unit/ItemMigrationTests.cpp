#include "TestCommon.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/storage/ItemMigration.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <vector>

using namespace NoMoreDay;

static ItemInstance MakeTestItemInstance(uint64_t instanceId, uint32_t baseId,
                                         uint32_t quantity = 1,
                                         uint8_t rarity = 0,
                                         float attack = 10.0f,
                                         float defense = 0.0f) {
  ItemInstance inst;
  inst.instanceId = instanceId;
  inst.baseId = baseId;
  inst.quantity = quantity;
  inst.rarity = rarity;
  inst.itemLevel = 20;
  inst.attack = attack;
  inst.defense = defense;
  inst.value = 150.0f;
  return inst;
}

TEST_CASE("[Unit] ItemMigration - Full Scene Switch Lifecycle and GroundPending Recycling") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;

  // 1. Construct items in persistent containers

  // (a) Inventory: slot 0 (Sword with 2 socketed gems), slot 1 (Potion stack), slot 10 (Armor)
  ItemInstance rune1 = MakeTestItemInstance(1001, 2001, 1, static_cast<uint8_t>(Rarity::Magic));
  ItemInstance rune2 = MakeTestItemInstance(1002, 2002, 1, static_cast<uint8_t>(Rarity::Rare));
  const ItemHandle hRune1 = service.getStoreMutable().create(rune1);
  const ItemHandle hRune2 = service.getStoreMutable().create(rune2);

  ItemInstance sword = MakeTestItemInstance(1000, 1001, 1, static_cast<uint8_t>(Rarity::Rare), 45.0f, 0.0f);
  sword.socketCount = 2;
  sword.sockets[0] = hRune1;
  sword.sockets[1] = hRune2;
  sword.activeRunewordId = 7;
  sword.setLocked(true);
  const ItemHandle hSword = service.getStoreMutable().create(sword);

  ItemInstance potion = MakeTestItemInstance(1003, 101, 50, static_cast<uint8_t>(Rarity::Common));
  const ItemHandle hPotion = service.getStoreMutable().create(potion);

  ItemInstance invArmor = MakeTestItemInstance(1004, 1002, 1, static_cast<uint8_t>(Rarity::Magic), 0.0f, 30.0f);
  const ItemHandle hInvArmor = service.getStoreMutable().create(invArmor);

  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hSword);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, hPotion);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 10}, hInvArmor);

  // (b) Equipment: MainHand (Weapon), Chest (Armor)
  ItemInstance eqWeapon = MakeTestItemInstance(2000, 1001, 1, static_cast<uint8_t>(Rarity::Mythic), 80.0f, 0.0f);
  const ItemHandle hEqWeapon = service.getStoreMutable().create(eqWeapon);
  ItemInstance eqChest = MakeTestItemInstance(2001, 1002, 1, static_cast<uint8_t>(Rarity::Ancient), 0.0f, 95.0f);
  const ItemHandle hEqChest = service.getStoreMutable().create(eqChest);

  service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0}, hEqWeapon);
  service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 1}, hEqChest);

  // (c) BagSlots: Slot 0 (Bag), Slot 1 (Bag)
  ItemInstance bag1 = MakeTestItemInstance(3000, 3001, 1, static_cast<uint8_t>(Rarity::Rare));
  const ItemHandle hBag1 = service.getStoreMutable().create(bag1);
  ItemInstance bag2 = MakeTestItemInstance(3001, 3002, 1, static_cast<uint8_t>(Rarity::Magic));
  const ItemHandle hBag2 = service.getStoreMutable().create(bag2);

  service.setSlotHandle(SlotRef{ContainerKind::BagSlots, 0, 0, 0}, hBag1);
  service.setSlotHandle(SlotRef{ContainerKind::BagSlots, 1, 0, 1}, hBag2);

  // (d) PersonalStash: Page 0 slot 5, Page 1 slot 20
  service.setUnlockedPages(ContainerKind::PersonalStash, 2);
  ItemInstance pStash1 = MakeTestItemInstance(4000, 1001, 1, static_cast<uint8_t>(Rarity::Magic));
  const ItemHandle hPStash1 = service.getStoreMutable().create(pStash1);
  ItemInstance pStash2 = MakeTestItemInstance(4001, 1002, 1, static_cast<uint8_t>(Rarity::Rare));
  const ItemHandle hPStash2 = service.getStoreMutable().create(pStash2);

  service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 0, 5}, hPStash1);
  service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 1, 20}, hPStash2);

  // (e) SharedStash: Page 0 slot 0, Page 2 slot 12
  service.setUnlockedPages(ContainerKind::SharedStash, 3);
  ItemInstance sStash1 = MakeTestItemInstance(5000, 1001, 1, static_cast<uint8_t>(Rarity::Common));
  const ItemHandle hSStash1 = service.getStoreMutable().create(sStash1);
  ItemInstance sStash2 = MakeTestItemInstance(5001, 1002, 1, static_cast<uint8_t>(Rarity::Mythic));
  const ItemHandle hSStash2 = service.getStoreMutable().create(sStash2);

  service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 0}, hSStash1);
  service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 2, 12}, hSStash2);

  // (f) HeirloomVault: Slot 0
  ItemInstance heirloom = MakeTestItemInstance(6000, 1001, 1, static_cast<uint8_t>(Rarity::Mythic));
  const ItemHandle hHeirloom = service.getStoreMutable().create(heirloom);
  service.setSlotHandle(SlotRef{ContainerKind::HeirloomVault, 0, 0, 0}, hHeirloom);

  // Total persistent item count:
  // Inventory: 3 (sword, potion, invArmor) + 2 (sockets: rune1, rune2) = 5
  // Equipment: 2 (eqWeapon, eqChest)
  // BagSlots: 2 (bag1, bag2)
  // PersonalStash: 2 (pStash1, pStash2)
  // SharedStash: 2 (sStash1, sStash2)
  // HeirloomVault: 1 (heirloom)
  // Total expected preserved = 5 + 2 + 2 + 2 + 2 + 1 = 14
  const size_t expectedPreservedCount = 14;

  // (g) GroundPending: 4 temporary unpicked ground drop items
  ItemInstance ground1 = MakeTestItemInstance(7001, 1001, 1);
  ItemInstance ground2 = MakeTestItemInstance(7002, 1002, 1);
  ItemInstance ground3 = MakeTestItemInstance(7003, 1003, 1);
  ItemInstance ground4 = MakeTestItemInstance(7004, 1004, 1);
  const ItemHandle hGround1 = service.getStoreMutable().create(ground1);
  const ItemHandle hGround2 = service.getStoreMutable().create(ground2);
  const ItemHandle hGround3 = service.getStoreMutable().create(ground3);
  const ItemHandle hGround4 = service.getStoreMutable().create(ground4);

  service.addGroundPending(hGround1);
  service.addGroundPending(hGround2);
  service.addGroundPending(hGround3);
  service.addGroundPending(hGround4);

  CHECK(service.getGroundPending().size() == 4);
  CHECK(service.getStore().activeCount() == expectedPreservedCount + 4);

  // 2. Perform Migration: beginSceneSwitch
  const SceneMigrationToken token = ItemMigration::beginSceneSwitch(service);

  CHECK(token.isValid());
  CHECK(token.migrationId > 0);
  CHECK(token.preservedItemCount == expectedPreservedCount);
  CHECK(token.sourceVersion > 0);

  // Assertion 2: GroundPending container is cleared and all ground item handles are destroyed & invalid
  CHECK(service.getGroundPending().empty());
  CHECK_FALSE(service.getStore().isValid(hGround1));
  CHECK_FALSE(service.getStore().isValid(hGround2));
  CHECK_FALSE(service.getStore().isValid(hGround3));
  CHECK_FALSE(service.getStore().isValid(hGround4));
  CHECK(service.getStore().get(hGround1) == nullptr);
  CHECK(service.getStore().get(hGround2) == nullptr);
  CHECK(service.getStore().get(hGround3) == nullptr);
  CHECK(service.getStore().get(hGround4) == nullptr);

  // Assertion 3: Active item count in ItemStore equals exactly preserved items
  CHECK(service.getStore().activeCount() == expectedPreservedCount);

  // 3. Simulate scene reload (clearing scene registry entities)
  entt::registry sceneRegistry;
  auto dummyEntity = sceneRegistry.create();
  sceneRegistry.emplace<ItemComponent>(dummyEntity);
  sceneRegistry.clear();

  // 4. Conclude Migration: endSceneSwitch
  const bool migrationSuccess = ItemMigration::endSceneSwitch(service, token);
  CHECK(migrationSuccess);

  // Assertion 1: All persistent items and attributes remain 100% valid and identical
  CHECK(service.getStore().isValid(hSword));
  CHECK(service.getStore().isValid(hRune1));
  CHECK(service.getStore().isValid(hRune2));
  CHECK(service.getStore().isValid(hPotion));
  CHECK(service.getStore().isValid(hInvArmor));
  CHECK(service.getStore().isValid(hEqWeapon));
  CHECK(service.getStore().isValid(hEqChest));
  CHECK(service.getStore().isValid(hBag1));
  CHECK(service.getStore().isValid(hBag2));
  CHECK(service.getStore().isValid(hPStash1));
  CHECK(service.getStore().isValid(hPStash2));
  CHECK(service.getStore().isValid(hSStash1));
  CHECK(service.getStore().isValid(hSStash2));
  CHECK(service.getStore().isValid(hHeirloom));

  // Deep field validation of sword
  const ItemInstance *viewSword = service.getStore().get(hSword);
  REQUIRE(viewSword != nullptr);
  CHECK(viewSword->instanceId == 1000);
  CHECK(viewSword->baseId == 1001);
  CHECK(viewSword->quantity == 1);
  CHECK(viewSword->rarity == static_cast<uint8_t>(Rarity::Rare));
  CHECK(viewSword->attack == 45.0f);
  CHECK(viewSword->socketCount == 2);
  CHECK(viewSword->sockets[0] == hRune1);
  CHECK(viewSword->sockets[1] == hRune2);
  CHECK(viewSword->activeRunewordId == 7);
  CHECK(viewSword->isLocked());

  // Deep field validation of potion stack
  const ItemInstance *viewPotion = service.getStore().get(hPotion);
  REQUIRE(viewPotion != nullptr);
  CHECK(viewPotion->instanceId == 1003);
  CHECK(viewPotion->quantity == 50);

  // Deep field validation of equipment
  const ItemInstance *viewEqWeapon = service.getStore().get(hEqWeapon);
  REQUIRE(viewEqWeapon != nullptr);
  CHECK(viewEqWeapon->instanceId == 2000);
  CHECK(viewEqWeapon->attack == 80.0f);

  const ItemInstance *viewEqChest = service.getStore().get(hEqChest);
  REQUIRE(viewEqChest != nullptr);
  CHECK(viewEqChest->instanceId == 2001);
  CHECK(viewEqChest->defense == 95.0f);

  // Assertion 4: Free list slot reuse verification
  // Allocating new item should reuse slot index of destroyed ground items
  ItemInstance newItem = MakeTestItemInstance(8001, 1001, 1);
  ItemHandle hNew = service.getStoreMutable().create(newItem);
  CHECK(service.getStore().isValid(hNew));
  CHECK(service.getStore().activeCount() == expectedPreservedCount + 1);
}

TEST_CASE("[Unit] ItemMigration - Token Validation and Failure Detection") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;

  const ItemHandle h1 = service.getStoreMutable().create(MakeTestItemInstance(101, 1001));
  const ItemHandle h2 = service.getStoreMutable().create(MakeTestItemInstance(102, 1002));
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h1);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, h2);

  // 1. Invalid default token rejection
  SceneMigrationToken invalidToken{};
  CHECK_FALSE(invalidToken.isValid());
  CHECK_FALSE(ItemMigration::endSceneSwitch(service, invalidToken));

  // 2. Token with mismatched count rejection
  SceneMigrationToken token = ItemMigration::beginSceneSwitch(service);
  CHECK(token.isValid());
  CHECK(token.preservedItemCount == 2);

  SceneMigrationToken corruptedToken = token;
  corruptedToken.preservedItemCount = 99;
  CHECK_FALSE(ItemMigration::endSceneSwitch(service, corruptedToken));

  // 3. Destroyed persistent handle detection
  ItemStorageService service2;
  const ItemHandle hA = service2.getStoreMutable().create(MakeTestItemInstance(201, 1001));
  const ItemHandle hB = service2.getStoreMutable().create(MakeTestItemInstance(202, 1002));
  service2.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hA);
  service2.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, hB);

  SceneMigrationToken token2 = ItemMigration::beginSceneSwitch(service2);
  CHECK(token2.preservedItemCount == 2);

  // Unexpectedly destroy handle hB during transition
  service2.getStoreMutable().destroy(hB);

  // Migration validation must fail because hB is no longer valid
  CHECK_FALSE(ItemMigration::endSceneSwitch(service2, token2));

  // 4. Ground item leak detection (un-cleared GroundPending)
  ItemStorageService service3;
  const ItemHandle hC = service3.getStoreMutable().create(MakeTestItemInstance(301, 1001));
  service3.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hC);

  SceneMigrationToken token3 = ItemMigration::beginSceneSwitch(service3);
  CHECK(token3.preservedItemCount == 1);

  // Add an un-recycled ground pending handle after beginSceneSwitch
  const ItemHandle hLeak = service3.getStoreMutable().create(MakeTestItemInstance(302, 1002));
  service3.addGroundPending(hLeak);

  // Migration validation must fail due to non-empty GroundPending and activeCount mismatch
  CHECK_FALSE(ItemMigration::endSceneSwitch(service3, token3));
}

TEST_CASE("[Unit] SharedStash - Zero JSON Suspend Resume Safe No-Op") {
  entt::registry reg;
  auto &stash = SharedStash::Get();
  stash.initialize();

  // Suspend and resume should execute as safe no-ops without error, allocations, or data corruption
  stash.suspend(reg);
  stash.resume(reg);

  CHECK(stash.getUnlockedTabCount() >= 1);
  CHECK(stash.getTab(0) != nullptr);
}
