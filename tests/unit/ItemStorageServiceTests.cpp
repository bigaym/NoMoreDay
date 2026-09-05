#include "TestCommon.hpp"
#include "game/foundation/Settings.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"

using namespace NoMoreDay;

static ItemInstance MakeTestItem(uint32_t id, uint32_t baseId, uint32_t qty = 1,
                                 uint8_t rarity = 0) {
  ItemInstance inst;
  inst.instanceId = id;
  inst.baseId = baseId;
  inst.quantity = qty;
  inst.rarity = rarity;
  inst.itemLevel = 10;
  inst.attack = 25.0f;
  inst.defense = 0.0f;
  inst.value = 100.0f;
  return inst;
}

TEST_CASE("[Unit] ItemStorageService - Initialization and Capacities") {
  ItemStorageService service;

  CHECK(service.getContainerSlots(ContainerKind::Inventory).size() ==
        ItemStorageService::kInventoryCapacity);
  CHECK(service.getContainerSlots(ContainerKind::Equipment).size() ==
        ItemStorageService::kEquipmentCapacity);
  CHECK(service.getContainerSlots(ContainerKind::BagSlots).size() ==
        ItemStorageService::kBagSlotsCapacity);
  CHECK(service.getContainerSlots(ContainerKind::HeirloomVault).size() ==
        ItemStorageService::kHeirloomVaultCapacity);
  CHECK(service.getUnlockedPages(ContainerKind::PersonalStash) == 1);
  CHECK(service.getUnlockedPages(ContainerKind::SharedStash) == 1);
  CHECK(service.getStore().empty());
}

TEST_CASE("[Unit] ItemStorageService - Move and Swap Operations") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;

  const ItemHandle h1 =
      service.getStoreMutable().create(MakeTestItem(101, 1001));
  const ItemHandle h2 =
      service.getStoreMutable().create(MakeTestItem(102, 1002));

  const SlotRef slotA{ContainerKind::Inventory, 0, 0, 0};
  const SlotRef slotB{ContainerKind::Inventory, 0, 0, 1};
  const SlotRef slotC{ContainerKind::Inventory, 0, 0, 2};

  service.setSlotHandle(slotA, h1);
  service.setSlotHandle(slotB, h2);

  // Move from A to empty C
  StorageError err = service.moveItem(slotA, slotC);
  CHECK(err == StorageError::Ok);
  CHECK(service.getSlotHandle(slotA) == ItemHandle{0, 0});
  CHECK(service.getSlotHandle(slotC) == h1);

  // Move into already occupied B fails with ContainerFull
  err = service.moveItem(slotC, slotB);
  CHECK(err == StorageError::ContainerFull);
  CHECK(service.getSlotHandle(slotC) == h1);
  CHECK(service.getSlotHandle(slotB) == h2);

  // Swap B and C
  err = service.swapItem(slotB, slotC);
  CHECK(err == StorageError::Ok);
  CHECK(service.getSlotHandle(slotB) == h1);
  CHECK(service.getSlotHandle(slotC) == h2);
}

TEST_CASE("[Unit] ItemStorageService - Split and Merge Stack") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;

  // Stackable potion item with stack of 50
  const ItemHandle hStack =
      service.getStoreMutable().create(MakeTestItem(201, 101, 50));
  const SlotRef slotFrom{ContainerKind::Inventory, 0, 0, 0};
  const SlotRef slotTo{ContainerKind::Inventory, 0, 0, 1};
  service.setSlotHandle(slotFrom, hStack);

  // Split 20 items to slotTo
  StorageError err = service.splitStack(slotFrom, slotTo, 20);
  CHECK(err == StorageError::Ok);

  const ItemHandle hNew = service.getSlotHandle(slotTo);
  CHECK(hNew);
  CHECK(service.getStore().get(hStack)->quantity == 30);
  CHECK(service.getStore().get(hNew)->quantity == 20);

  // Merge back
  err = service.mergeStack(slotTo, slotFrom);
  CHECK(err == StorageError::Ok);
  CHECK(service.getStore().get(hStack)->quantity == 50);
  CHECK(service.getSlotHandle(slotTo) == ItemHandle{0, 0});
  CHECK_FALSE(service.getStore().isValid(hNew));
}

TEST_CASE("[Unit] ItemStorageService - Transfer and AutoDeposit") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;
  service.setUnlockedPages(ContainerKind::PersonalStash, 2);

  const ItemHandle h1 =
      service.getStoreMutable().create(MakeTestItem(301, 1001, 1));
  const SlotRef invSlot{ContainerKind::Inventory, 0, 0, 5};
  const SlotRef stashSlot{ContainerKind::PersonalStash, 0, 0, 10};
  service.setSlotHandle(invSlot, h1);

  // Transfer moves to empty slot
  StorageError err = service.transferItem(invSlot, stashSlot);
  CHECK(err == StorageError::Ok);
  CHECK(service.getSlotHandle(invSlot) == ItemHandle{0, 0});
  CHECK(service.getSlotHandle(stashSlot) == h1);

  // Auto-deposit another item into PersonalStash
  const ItemHandle h2 =
      service.getStoreMutable().create(MakeTestItem(302, 1002, 1));
  const SlotRef invSlot2{ContainerKind::Inventory, 0, 0, 0};
  service.setSlotHandle(invSlot2, h2);

  err = service.autoDeposit(invSlot2, ContainerKind::PersonalStash);
  CHECK(err == StorageError::Ok);
  CHECK(service.getSlotHandle(invSlot2) == ItemHandle{0, 0});
  // Item should be deposited in first empty slot (page 0, index 0)
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 0, 0}) ==
        h2);
}

TEST_CASE("[Unit] ItemStorageService - Container Sorting") {
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;

  // Insert items with varying rarities: Common, Rare, Mythic, Magic
  const ItemHandle hCommon = service.getStoreMutable().create(
      MakeTestItem(401, 1001, 1, static_cast<uint8_t>(Rarity::Common)));
  const ItemHandle hRare = service.getStoreMutable().create(
      MakeTestItem(402, 1002, 1, static_cast<uint8_t>(Rarity::Rare)));
  const ItemHandle hMythic = service.getStoreMutable().create(
      MakeTestItem(403, 1003, 1, static_cast<uint8_t>(Rarity::Mythic)));
  const ItemHandle hMagic = service.getStoreMutable().create(
      MakeTestItem(404, 1004, 1, static_cast<uint8_t>(Rarity::Magic)));

  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 3}, hCommon);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 10}, hRare);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 15}, hMythic);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 20}, hMagic);

  service.sortContainer(ContainerKind::Inventory);

  // Sorted order: Mythic, Rare, Magic, Common at slots 0, 1, 2, 3
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}) ==
        hMythic);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}) ==
        hRare);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 2}) ==
        hMagic);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 3}) ==
        hCommon);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 4}) ==
        ItemHandle{0, 0});
}

TEST_CASE("[Unit] ItemStorageService - Destroy and Locked Protection") {
  ItemStorageService service;

  ItemInstance inst = MakeTestItem(501, 1001, 10);
  inst.setLocked(true);
  const ItemHandle hLocked = service.getStoreMutable().create(inst);
  const SlotRef slot{ContainerKind::Inventory, 0, 0, 0};
  service.setSlotHandle(slot, hLocked);

  // Locked item cannot be destroyed
  StorageError err = service.destroyItem(slot);
  CHECK(err == StorageError::Locked);
  CHECK(service.getSlotHandle(slot) == hLocked);
  CHECK(service.getStore().isValid(hLocked));

  // Unlock and destroy partially
  service.getStoreMutable().mutate(
      hLocked, [](ItemInstance &i) { i.setLocked(false); });
  err = service.destroyItem(slot, 4);
  CHECK(err == StorageError::Ok);
  CHECK(service.getStore().get(hLocked)->quantity == 6);

  // Destroy remaining
  err = service.destroyItem(slot, 6);
  CHECK(err == StorageError::Ok);
  CHECK(service.getSlotHandle(slot) == ItemHandle{0, 0});
  CHECK_FALSE(service.getStore().isValid(hLocked));
}

TEST_CASE("[Unit] ItemStorageService - Material Bank Accounting") {
  ItemStorageService service;

  CHECK(service.getMaterialCount(5001) == 0);
  CHECK_FALSE(service.hasMaterial(5001, 10));

  int32_t total = service.addMaterial(5001, 25);
  CHECK(total == 25);
  CHECK(service.getMaterialCount(5001) == 25);
  CHECK(service.hasMaterial(5001, 20));
  CHECK_FALSE(service.hasMaterial(5001, 30));

  total = service.addMaterial(5001, 15);
  CHECK(total == 40);

  bool removed = service.removeMaterial(5001, 10);
  CHECK(removed);
  CHECK(service.getMaterialCount(5001) == 30);

  bool overRemove = service.removeMaterial(5001, 50);
  CHECK_FALSE(overRemove);
  CHECK(service.getMaterialCount(5001) == 30);
}

TEST_CASE("[Unit] ItemStorageService - Currency Gold Transactions") {
  ItemStorageService service;

  CHECK(service.getGold() == 0);
  service.addGold(500);
  CHECK(service.getGold() == 500);

  CHECK(service.spendGold(200));
  CHECK(service.getGold() == 300);

  CHECK_FALSE(service.spendGold(1000));
  CHECK(service.getGold() == 300);
}

TEST_CASE("[Unit] ItemStorageService - Freeze and Snapshot Independence") {
  ItemStorageService service;

  const ItemHandle h1 =
      service.getStoreMutable().create(MakeTestItem(601, 1001));
  const ItemHandle h2 =
      service.getStoreMutable().create(MakeTestItem(602, 1002));

  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h1);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, h2);

  // Freeze 快照生成独立副本
  ItemStore snapshot = service.freeze();
  CHECK(snapshot.activeCount() == service.getStore().activeCount());
  CHECK(snapshot.version() == service.version());

  // 销毁活跃存储中的物品，不影响已冻结的快照
  service.destroyItem(SlotRef{ContainerKind::Inventory, 0, 0, 1});
  CHECK_FALSE(service.getStore().isValid(h2));
  CHECK(snapshot.isValid(h2));
  CHECK(snapshot.activeCount() == 2);
  CHECK(service.getStore().activeCount() == 1);
}

TEST_CASE("[Unit] ItemStorageService - Deep Copy and Move Semantics") {
  ItemStorageService src;
  src.setGold(5000);
  src.addMaterial(101, 20);
  src.setUnlockedPages(ContainerKind::PersonalStash, 3);
  src.setPersonalStashMeta({
      StashTabMeta{"Tab1", 0, 10, 0xFF0000FF},
      StashTabMeta{"Tab2", 1, 20, 0x00FF00FF},
      StashTabMeta{"Tab3", 2, 30, 0x0000FFFF}
  });

  const ItemHandle h1 = src.getStoreMutable().create(MakeTestItem(801, 1001));
  src.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h1);

  // 1. 测试拷贝构造（快照深拷贝语义）
  ItemStorageService copyConstructed(src);
  CHECK(copyConstructed.getGold() == 5000);
  CHECK(copyConstructed.getMaterialCount(101) == 20);
  CHECK(copyConstructed.getUnlockedPages(ContainerKind::PersonalStash) == 3);
  CHECK(copyConstructed.getPersonalStashMeta().size() == 3);
  CHECK(copyConstructed.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}) == h1);
  CHECK(copyConstructed.getStore().isValid(h1));

  // 修改原对象，副本不受任何影响
  src.setGold(9999);
  src.getStoreMutable().destroy(h1);
  CHECK(src.getGold() == 9999);
  CHECK_FALSE(src.getStore().isValid(h1));
  CHECK(copyConstructed.getGold() == 5000);
  CHECK(copyConstructed.getStore().isValid(h1));

  // 2. 测试拷贝赋值
  ItemStorageService copyAssigned;
  copyAssigned = copyConstructed;
  CHECK(copyAssigned.getGold() == 5000);
  CHECK(copyAssigned.getStore().isValid(h1));

  // 3. 测试移动构造
  ItemStorageService moveConstructed(std::move(copyAssigned));
  CHECK(moveConstructed.getGold() == 5000);
  CHECK(moveConstructed.getStore().isValid(h1));

  // 4. 测试移动赋值
  ItemStorageService moveAssigned;
  moveAssigned = std::move(moveConstructed);
  CHECK(moveAssigned.getGold() == 5000);
  CHECK(moveAssigned.getStore().isValid(h1));
}
