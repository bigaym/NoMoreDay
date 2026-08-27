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

  // 1. 在持久化容器中构造物品

  // (a) 背包: 槽位 0 (带有 2 个镶嵌宝石的剑), 槽位 1 (药水堆叠), 槽位 10 (护甲)
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

  // (b) 装备: 主手 (武器), 胸甲 (护甲)
  ItemInstance eqWeapon = MakeTestItemInstance(2000, 1001, 1, static_cast<uint8_t>(Rarity::Mythic), 80.0f, 0.0f);
  const ItemHandle hEqWeapon = service.getStoreMutable().create(eqWeapon);
  ItemInstance eqChest = MakeTestItemInstance(2001, 1002, 1, static_cast<uint8_t>(Rarity::Ancient), 0.0f, 95.0f);
  const ItemHandle hEqChest = service.getStoreMutable().create(eqChest);

  service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0}, hEqWeapon);
  service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0}, hEqChest);

  // (c) 背包栏: 槽位 0 (背包), 槽位 1 (背包)
  ItemInstance bag1 = MakeTestItemInstance(3000, 3001, 1, static_cast<uint8_t>(Rarity::Rare));
  const ItemHandle hBag1 = service.getStoreMutable().create(bag1);
  ItemInstance bag2 = MakeTestItemInstance(3001, 3002, 1, static_cast<uint8_t>(Rarity::Magic));
  const ItemHandle hBag2 = service.getStoreMutable().create(bag2);

  service.setSlotHandle(SlotRef{ContainerKind::BagSlots, 0, 0, 0}, hBag1);
  service.setSlotHandle(SlotRef{ContainerKind::BagSlots, 1, 0, 0}, hBag2);

  // (d) 个人仓库: 第 0 页槽位 5, 第 1 页槽位 20
  service.setUnlockedPages(ContainerKind::PersonalStash, 2);
  ItemInstance pStash1 = MakeTestItemInstance(4000, 1001, 1, static_cast<uint8_t>(Rarity::Magic));
  const ItemHandle hPStash1 = service.getStoreMutable().create(pStash1);
  ItemInstance pStash2 = MakeTestItemInstance(4001, 1002, 1, static_cast<uint8_t>(Rarity::Rare));
  const ItemHandle hPStash2 = service.getStoreMutable().create(pStash2);

  service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 0, 5}, hPStash1);
  service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 1, 20}, hPStash2);

  // (e) 共享仓库: 第 0 页槽位 0, 第 2 页槽位 12
  service.setUnlockedPages(ContainerKind::SharedStash, 3);
  ItemInstance sStash1 = MakeTestItemInstance(5000, 1001, 1, static_cast<uint8_t>(Rarity::Common));
  const ItemHandle hSStash1 = service.getStoreMutable().create(sStash1);
  ItemInstance sStash2 = MakeTestItemInstance(5001, 1002, 1, static_cast<uint8_t>(Rarity::Mythic));
  const ItemHandle hSStash2 = service.getStoreMutable().create(sStash2);

  service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 0}, hSStash1);
  service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 2, 12}, hSStash2);

  // (f) 传家宝库: 槽位 0
  ItemInstance heirloom = MakeTestItemInstance(6000, 1001, 1, static_cast<uint8_t>(Rarity::Mythic));
  const ItemHandle hHeirloom = service.getStoreMutable().create(heirloom);
  service.setSlotHandle(SlotRef{ContainerKind::HeirloomVault, 0, 0, 0}, hHeirloom);

  // 槽位寻址约定 (SlotRef::isWellFormed): Equipment/BagSlots
  // 通过 container 字段解析，因此句柄必须从具名槽位中读取而非 index 0。
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0}) == hEqWeapon);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0}) == hEqChest);
  CHECK(service.getSlotHandle(SlotRef{ContainerKind::BagSlots, 1, 0, 0}) == hBag2);

  // 持久化物品总数:
  // 背包: 3 (sword, potion, invArmor) + 2 (插槽: rune1, rune2) = 5
  // 装备: 2 (eqWeapon, eqChest)
  // 背包栏: 2 (bag1, bag2)
  // 个人仓库: 2 (pStash1, pStash2)
  // 共享仓库: 2 (sStash1, sStash2)
  // 传家宝库: 1 (heirloom)
  // 预期保留总数 = 5 + 2 + 2 + 2 + 2 + 1 = 14
  const size_t expectedPreservedCount = 14;

  // (g) GroundPending: 4 个临时的未拾取地面掉落物品
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

  // 2. 执行迁移: beginSceneSwitch
  const SceneMigrationToken token = ItemMigration::beginSceneSwitch(service);

  CHECK(token.isValid());
  CHECK(token.migrationId > 0);
  CHECK(token.preservedItemCount == expectedPreservedCount);
  CHECK(token.sourceVersion > 0);

  // 断言 2: GroundPending 容器被清空且所有地面物品句柄均被销毁并失效
  CHECK(service.getGroundPending().empty());
  CHECK_FALSE(service.getStore().isValid(hGround1));
  CHECK_FALSE(service.getStore().isValid(hGround2));
  CHECK_FALSE(service.getStore().isValid(hGround3));
  CHECK_FALSE(service.getStore().isValid(hGround4));
  CHECK(service.getStore().get(hGround1) == nullptr);
  CHECK(service.getStore().get(hGround2) == nullptr);
  CHECK(service.getStore().get(hGround3) == nullptr);
  CHECK(service.getStore().get(hGround4) == nullptr);

  // 断言 3: ItemStore 中的活动物品数量严格等于保留的物品数量
  CHECK(service.getStore().activeCount() == expectedPreservedCount);

  // 3. 模拟场景重载 (清理场景注册表实体)
  entt::registry sceneRegistry;
  auto dummyEntity = sceneRegistry.create();
  sceneRegistry.emplace<ItemComponent>(dummyEntity);
  sceneRegistry.clear();

  // 4. 结束迁移: endSceneSwitch
  const bool migrationSuccess = ItemMigration::endSceneSwitch(service, token);
  CHECK(migrationSuccess);

  // 断言 1: 所有持久化物品及其属性均 100% 有效且完全一致
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

  // 对剑的深层字段校验
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

  // 对药水堆叠的深层字段校验
  const ItemInstance *viewPotion = service.getStore().get(hPotion);
  REQUIRE(viewPotion != nullptr);
  CHECK(viewPotion->instanceId == 1003);
  CHECK(viewPotion->quantity == 50);

  // 对装备的深层字段校验
  const ItemInstance *viewEqWeapon = service.getStore().get(hEqWeapon);
  REQUIRE(viewEqWeapon != nullptr);
  CHECK(viewEqWeapon->instanceId == 2000);
  CHECK(viewEqWeapon->attack == 80.0f);

  const ItemInstance *viewEqChest = service.getStore().get(hEqChest);
  REQUIRE(viewEqChest != nullptr);
  CHECK(viewEqChest->instanceId == 2001);
  CHECK(viewEqChest->defense == 95.0f);

  // 断言 4: 空闲列表槽位复用验证
  // 分配新物品应复用已被销毁的地面物品的槽位索引
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

  // 1. 拒绝无效的默认 token
  SceneMigrationToken invalidToken{};
  CHECK_FALSE(invalidToken.isValid());
  CHECK_FALSE(ItemMigration::endSceneSwitch(service, invalidToken));

  // 2. 拒绝数量不匹配的 token
  SceneMigrationToken token = ItemMigration::beginSceneSwitch(service);
  CHECK(token.isValid());
  CHECK(token.preservedItemCount == 2);

  SceneMigrationToken corruptedToken = token;
  corruptedToken.preservedItemCount = 99;
  CHECK_FALSE(ItemMigration::endSceneSwitch(service, corruptedToken));

  // 3. 检测被销毁的持久化句柄
  ItemStorageService service2;
  const ItemHandle hA = service2.getStoreMutable().create(MakeTestItemInstance(201, 1001));
  const ItemHandle hB = service2.getStoreMutable().create(MakeTestItemInstance(202, 1002));
  service2.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hA);
  service2.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, hB);

  SceneMigrationToken token2 = ItemMigration::beginSceneSwitch(service2);
  CHECK(token2.preservedItemCount == 2);

  // 在过渡期间意外销毁句柄 hB
  service2.getStoreMutable().destroy(hB);

  // 迁移校验必须失败，因为 hB 已不再有效
  CHECK_FALSE(ItemMigration::endSceneSwitch(service2, token2));

  // 4. 地面物品泄漏检测 (未清空的 GroundPending)
  ItemStorageService service3;
  const ItemHandle hC = service3.getStoreMutable().create(MakeTestItemInstance(301, 1001));
  service3.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hC);

  SceneMigrationToken token3 = ItemMigration::beginSceneSwitch(service3);
  CHECK(token3.preservedItemCount == 1);

  // 在 beginSceneSwitch 之后添加一个未回收的地面待决句柄
  const ItemHandle hLeak = service3.getStoreMutable().create(MakeTestItemInstance(302, 1002));
  service3.addGroundPending(hLeak);

  // 由于 GroundPending 非空且 activeCount 不匹配，迁移校验必须失败
  CHECK_FALSE(ItemMigration::endSceneSwitch(service3, token3));
}

TEST_CASE("[Unit] SharedStash - Suspend Preserves Items Across Registry Clear") {
  entt::registry reg;
  auto &stash = SharedStash::Get();
  stash.initialize();
  REQUIRE(stash.getUnlockedTabCount() >= 1);

  constexpr int kLastSlot = StashTab::CAPACITY - 1;

  // 单例状态可能在测试用例间泄漏: 先清理目标槽位
  (void)stash.takeItem(0, 0);
  (void)stash.takeItem(0, kLastSlot);

  // 真实的物品实体，以便 suspend/resume 路径演练完整的
  // 序列化 -> registry.clear() -> 恢复往返过程。
  const entt::entity sword = reg.create();
  auto &swordComp = reg.emplace<ItemComponent>(sword);
  swordComp.id = 700001;
  swordComp.baseId = 1001;
  swordComp.name = "SuspendTest Sword";
  swordComp.type = ItemType::Weapon;
  swordComp.slot = EquipmentSlot::MainHand;
  swordComp.rarity = Rarity::Rare;
  swordComp.attack = 42.5f;

  const entt::entity potion = reg.create();
  auto &potionComp = reg.emplace<ItemComponent>(potion);
  potionComp.id = 700002;
  potionComp.baseId = 101;
  potionComp.name = "SuspendTest Potion";
  potionComp.type = ItemType::Consumable;
  potionComp.quantity = 25;

  CHECK(stash.putItem(0, 0, sword));
  CHECK(stash.putItem(0, kLastSlot, potion));

  stash.suspend(reg);

  // 暂存的句柄必须被清理，以确保场景注册表销毁后不残留悬空引用 (SaveManager 旧版轨道正是这样做的)。
  // 比较保持在 CHECK 外部: doctest 的 Expression_lhs 包装器导致 `entity == entt::null` 与 entt 的模板运算符冲突。
  const bool slot0Cleared = (stash.getItem(0, 0) == entt::null);
  const bool slotLastCleared = (stash.getItem(0, kLastSlot) == entt::null);
  CHECK(slot0Cleared);
  CHECK(slotLastCleared);

  reg.clear();

  stash.resume(reg);

  const entt::entity restoredSwordEntity = stash.getItem(0, 0);
  const entt::entity restoredPotionEntity = stash.getItem(0, kLastSlot);
  REQUIRE(reg.valid(restoredSwordEntity));
  REQUIRE(reg.valid(restoredPotionEntity));

  const auto &restoredSword = reg.get<ItemComponent>(restoredSwordEntity);
  const auto &restoredPotion = reg.get<ItemComponent>(restoredPotionEntity);
  CHECK(restoredSword.name == "SuspendTest Sword");
  CHECK(restoredSword.baseId == 1001);
  CHECK(restoredSword.type == ItemType::Weapon);
  CHECK(restoredSword.slot == EquipmentSlot::MainHand);
  CHECK(restoredSword.rarity == Rarity::Rare);
  CHECK(restoredSword.attack == 42.5f);

  CHECK(restoredPotion.name == "SuspendTest Potion");
  CHECK(restoredPotion.type == ItemType::Consumable);
  CHECK(restoredPotion.quantity == 25);

  // 清理单例以使后续测试用例从空槽位开始
  (void)stash.takeItem(0, 0);
  (void)stash.takeItem(0, kLastSlot);
}
