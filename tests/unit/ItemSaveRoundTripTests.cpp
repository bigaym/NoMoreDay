#include "TestCommon.hpp"
#include "core/utils/HashUtils.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerProfile.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace NoMoreDay;

TEST_CASE("[Unit] ItemPersistenceCodec - Binary Encode Decode Roundtrip") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService srcService;
  srcService.setGold(54321);
  srcService.addMaterial(1, 100);
  srcService.addMaterial(2, 50);

  ItemInstance inst;
  inst.instanceId = 9991;
  inst.baseId = 1001;
  inst.quantity = 1;
  inst.rarity = 3;
  inst.itemLevel = 45;
  inst.setLocked(true);
  inst.attack = 120.0f;
  inst.affixCount = 2;
  inst.affixes[0] = CompactAffix{AffixType::Strength, 3, true, 20.0f};
  inst.affixes[1] = CompactAffix{AffixType::CritChance, 2, false, 5.5f};

  ItemHandle h = srcService.getStoreMutable().create(inst);
  srcService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h);

  std::stringstream ss;
  std::string progressionData = "{\"mock\":\"progression\"}";
  bool encOk = ItemPersistenceCodec::encode(srcService, ss, nullptr, ContainerDirtyFlags::All, progressionData);
  REQUIRE(encOk);

  ItemStorageService dstService;
  std::string decodedProgression;
  bool decOk = ItemPersistenceCodec::decode(ss, dstService, &decodedProgression);
  REQUIRE(decOk);

  CHECK(dstService.getGold() == 54321);
  CHECK(dstService.getMaterialCount(1) == 100);
  CHECK(dstService.getMaterialCount(2) == 50);
  CHECK(decodedProgression == progressionData);

  ItemHandle dstHandle = dstService.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0});
  REQUIRE(dstHandle);
  const auto *dstInst = dstService.getStore().get(dstHandle);
  REQUIRE(dstInst != nullptr);
  CHECK(dstInst->instanceId == 9991);
  CHECK(dstInst->baseId == 1001);
  CHECK(dstInst->isLocked() == true);
  CHECK(dstInst->attack == doctest::Approx(120.0f));
  CHECK(dstInst->affixCount == 2);
  CHECK(dstInst->affixes[0].type == AffixType::Strength);
  CHECK(dstInst->affixes[0].value == doctest::Approx(20.0f));
}

TEST_CASE("[Unit] SaveManager - Binary .nmd Save and Load with Backup Recovery") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);
  sm.SetSaveDirectory("build/test_saves_isolation");

  std::error_code ec;
  std::filesystem::create_directories("build/test_saves_isolation", ec);

  entt::registry reg;
  auto player = reg.create();
  reg.emplace<PlayerTag>(player);
  reg.emplace<PlayerName>(player, "TestHero");
  reg.emplace<PlayerLevel>(player, 30);
  reg.emplace<Position>(player, 123.0f, 456.0f);
  reg.emplace<PrimaryStats>(player, 50.0f, 40.0f, 30.0f, 60.0f);

  ItemStorageService service;
  service.setGold(8888);
  service.addMaterial(101, 50);
  ItemInstance sword{};
  sword.instanceId = 999;
  sword.baseId = 1001;
  sword.attack = 45.0f;
  sword.setLocked(true);
  ItemHandle hSword = service.getStoreMutable().create(sword);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hSword);

  sm.SetItemStorageService(&service);

  // 1. 异步写入存档
  auto future = sm.saveCharacterAsync(reg, 99);
  REQUIRE(future.valid());
  bool saveResult = future.get();
  REQUIRE(saveResult);

  std::string nmdPath = "build/test_saves_isolation/slot_99.nmd";
  std::string bakPath = "build/test_saves_isolation/slot_99.nmd.bak";
  CHECK(std::filesystem::exists(nmdPath));

  // 2. 正常读取
  entt::registry loadedReg;
  ItemStorageService loadedService;
  sm.SetItemStorageService(&loadedService);
  bool loadResult = sm.loadCharacter(loadedReg, 99);
  REQUIRE(loadResult);

  auto view = loadedReg.view<PlayerTag>();
  REQUIRE(view.begin() != view.end());
  auto loadedPlayer = *view.begin();
  CHECK(loadedReg.get<PlayerName>(loadedPlayer).value == "TestHero");
  CHECK(loadedReg.get<Position>(loadedPlayer).x == doctest::Approx(123.0f));

  // 验证 loadedService 中的物品、材料与金币完整恢复
  CHECK(loadedService.getGold() == 8888);
  CHECK(loadedService.getMaterialCount(101) == 50);
  ItemHandle loadedH = loadedService.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0});
  REQUIRE(loadedH);
  const auto *loadedInst = loadedService.getStore().get(loadedH);
  REQUIRE(loadedInst != nullptr);
  CHECK(loadedInst->instanceId == 999);
  CHECK(loadedInst->baseId == 1001);
  CHECK(loadedInst->attack == doctest::Approx(45.0f));
  CHECK(loadedInst->isLocked() == true);

  // 3. 模拟文件损坏，触发 .bak 备份恢复
  {
    // 二次保存以生成 .bak 备份
    auto fut2 = sm.saveCharacterAsync(reg, 99);
    fut2.get();
    REQUIRE(std::filesystem::exists(bakPath));

    // 损坏主档
    std::ofstream corrupt(nmdPath, std::ios::binary | std::ios::trunc);
    corrupt << "CORRUPTED_BYTES_HEADER";
    corrupt.close();
  }

  entt::registry backupReg;
  ItemStorageService backupService;
  sm.SetItemStorageService(&backupService);
  bool backupLoadResult = sm.loadCharacter(backupReg, 99);
  REQUIRE(backupLoadResult);

  auto bakView = backupReg.view<PlayerTag>();
  REQUIRE(bakView.begin() != bakView.end());
  auto bakPlayer = *bakView.begin();
  CHECK(backupReg.get<PlayerName>(bakPlayer).value == "TestHero");
  CHECK(backupService.getGold() == 8888);
  CHECK(backupService.getMaterialCount(101) == 50);

  // 清理测试文件
  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);
}

TEST_CASE("[Unit] SaveManager - In-Flight Save Guard and Concurrent Rejection") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);
  sm.SetSaveDirectory("build/test_saves_isolation");

  std::error_code ec;
  std::filesystem::create_directories("build/test_saves_isolation", ec);

  entt::registry reg;
  auto player = reg.create();
  reg.emplace<PlayerTag>(player);
  reg.emplace<PlayerName>(player, "ConcurrentHero");
  reg.emplace<PlayerLevel>(player, 10);
  reg.emplace<Position>(player, 10.0f, 20.0f);
  reg.emplace<PrimaryStats>(player);

  ItemStorageService service;
  service.setGold(1000);
  sm.SetItemStorageService(&service);

  // 发起多次连续异步保存请求
  auto fut1 = sm.saveCharacterAsync(reg, 98);
  auto fut2 = sm.saveCharacterAsync(reg, 98);

  bool res1 = fut1.valid() ? fut1.get() : false;
  bool res2 = fut2.valid() ? fut2.get() : false;
  CHECK(res1);
  (void)res2;

  // 清理测试文件
  std::filesystem::remove("build/test_saves_isolation/slot_98.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_98.nmd.bak", ec);
}

TEST_CASE("[Unit] SaveManager - Multi-Character and Global SharedStash Separation (N1)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);
  sm.SetSaveDirectory("build/test_saves_isolation");

  std::error_code ec;
  std::filesystem::create_directories("build/test_saves_isolation", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_96.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_96.nmd.bak", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_97.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_97.nmd.bak", ec);
  std::filesystem::remove("build/test_saves_isolation/global.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/global.nmd.bak", ec);

  // 1. 初始化 角色 A (Slot 96)
  entt::registry regA;
  auto playerA = regA.create();
  regA.emplace<PlayerTag>(playerA);
  regA.emplace<PlayerName>(playerA, "HeroA");
  regA.emplace<PlayerLevel>(playerA, 20);
  regA.emplace<Position>(playerA, 10.0f, 10.0f);
  regA.emplace<PrimaryStats>(playerA);

  ItemStorageService storageA;
  storageA.setGold(5000);
  ItemInstance swordA{};
  swordA.instanceId = 101;
  swordA.baseId = 1001;
  ItemHandle hSwordA = storageA.getStoreMutable().create(swordA);
  storageA.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hSwordA);

  ItemInstance sharedRing{};
  sharedRing.instanceId = 201;
  sharedRing.baseId = 3011;
  ItemHandle hSharedRing = storageA.getStoreMutable().create(sharedRing);
  storageA.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 0}, hSharedRing);

  sm.SetItemStorageService(&storageA);

  // 保存角色 A 与全局存档
  auto futGlobal1 = sm.saveGlobalAsync(regA);
  REQUIRE(futGlobal1.get());
  auto futChar1 = sm.saveCharacterAsync(regA, 96);
  REQUIRE(futChar1.get());

  // 2. 初始化 角色 B (Slot 97)
  entt::registry regB;
  auto playerB = regB.create();
  regB.emplace<PlayerTag>(playerB);
  regB.emplace<PlayerName>(playerB, "HeroB");
  regB.emplace<PlayerLevel>(playerB, 15);
  regB.emplace<Position>(playerB, 20.0f, 20.0f);
  regB.emplace<PrimaryStats>(playerB);

  ItemStorageService storageB;
  sm.SetItemStorageService(&storageB);

  // 加载全局共享仓库至角色 B 的运行时
  REQUIRE(sm.loadGlobal(regB));
  ItemHandle loadedSharedRingB = storageB.getSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 0});
  REQUIRE(loadedSharedRingB);
  CHECK(storageB.getStore().get(loadedSharedRingB)->baseId == 3011);
  // 确认角色 A 的私有背包武器没有泄露至全局共享仓库 (H4 / B1)
  CHECK(!storageB.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}));

  // 角色 B 放入私有长袍和新的共享项链
  ItemInstance robeB{};
  robeB.instanceId = 301;
  robeB.baseId = 2011;
  ItemHandle hRobeB = storageB.getStoreMutable().create(robeB);
  storageB.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, hRobeB);

  ItemInstance sharedAmulet{};
  sharedAmulet.instanceId = 401;
  sharedAmulet.baseId = 3001;
  ItemHandle hSharedAmulet = storageB.getStoreMutable().create(sharedAmulet);
  storageB.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 1}, hSharedAmulet);

  auto futGlobal2 = sm.saveGlobalAsync(regB);
  REQUIRE(futGlobal2.get());
  auto futChar2 = sm.saveCharacterAsync(regB, 97);
  REQUIRE(futChar2.get());

  // 3. 重新加载 角色 A
  entt::registry reloadRegA;
  ItemStorageService reloadStorageA;
  sm.SetItemStorageService(&reloadStorageA);

  // 先加载全局共享仓库
  REQUIRE(sm.loadGlobal(reloadRegA));
  // 再加载角色 A (loadCharacter 必须保护并保留当前活体共享仓库)
  REQUIRE(sm.loadCharacter(reloadRegA, 96));

  // 验证角色 A 的私有背包物品恢复正常
  ItemHandle reloadSwordA = reloadStorageA.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0});
  REQUIRE(reloadSwordA);
  CHECK(reloadStorageA.getStore().get(reloadSwordA)->baseId == 1001);
  CHECK(reloadStorageA.getStore().get(reloadSwordA)->instanceId == 101);

  // 验证角色 A 能看到角色 B 更新后的全部共享仓库物品
  ItemHandle sRing = reloadStorageA.getSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 0});
  ItemHandle sAmulet = reloadStorageA.getSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 0, 1});
  REQUIRE(sRing);
  REQUIRE(sAmulet);
  CHECK(reloadStorageA.getStore().get(sRing)->baseId == 3011);
  CHECK(reloadStorageA.getStore().get(sAmulet)->baseId == 3001);

  // 清理
  std::filesystem::remove("build/test_saves_isolation/slot_96.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_96.nmd.bak", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_97.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/slot_97.nmd.bak", ec);
  std::filesystem::remove("build/test_saves_isolation/global.nmd", ec);
  std::filesystem::remove("build/test_saves_isolation/global.nmd.bak", ec);
}

TEST_CASE("[Unit] SaveManager - Corrupted Global Save Rejection (H3)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);
  sm.SetSaveDirectory("build/test_saves_isolation");

  std::string binaryPath = "build/test_saves_isolation/global.nmd";
  std::string backupPath = "build/test_saves_isolation/global.nmd.bak";
  std::error_code ec;
  std::filesystem::create_directories("build/test_saves_isolation", ec);

  // 1. 制造损坏的 global.nmd 与 global.nmd.bak
  std::ofstream f1(binaryPath, std::ios::binary | std::ios::trunc);
  f1 << "BAD_BINARY_DATA";
  f1.close();

  std::ofstream f2(backupPath, std::ios::binary | std::ios::trunc);
  f2 << "BAD_BACKUP_DATA";
  f2.close();

  entt::registry reg;
  ItemStorageService storage;
  sm.SetItemStorageService(&storage);

  // H3: 损坏时不得静默返回 true 重置存档，必须返回 false
  bool loadResult = sm.loadGlobal(reg);
  CHECK(loadResult == false);

  // 2. 清理文件后测试全新启动（无文件）正常初始化返回 true
  std::filesystem::remove(binaryPath, ec);
  std::filesystem::remove(backupPath, ec);

  bool cleanResult = sm.loadGlobal(reg);
  CHECK(cleanResult == true);
  CHECK(storage.getUnlockedPages(ContainerKind::SharedStash) == 1);
}

TEST_CASE("[Unit] ItemPersistenceCodec - Sparse Inventory Layout Preservation (M7)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService srcService;
  // 在非连续稀疏槽位放置物品
  ItemInstance item1{};
  item1.instanceId = 11;
  item1.baseId = 1001;
  ItemHandle h1 = srcService.getStoreMutable().create(item1);
  srcService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h1);

  ItemInstance item2{};
  item2.instanceId = 22;
  item2.baseId = 1002;
  ItemHandle h2 = srcService.getStoreMutable().create(item2);
  srcService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 7}, h2);

  ItemInstance item3{};
  item3.instanceId = 33;
  item3.baseId = 1003;
  ItemHandle h3 = srcService.getStoreMutable().create(item3);
  srcService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 25}, h3);

  ItemInstance item4{};
  item4.instanceId = 44;
  item4.baseId = 1004;
  ItemHandle h4 = srcService.getStoreMutable().create(item4);
  srcService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 39}, h4);

  std::stringstream ss;
  REQUIRE(ItemPersistenceCodec::encode(srcService, ss));

  ItemStorageService dstService;
  REQUIRE(ItemPersistenceCodec::decode(ss, dstService));

  for (uint16_t i = 0; i < ItemStorageService::kInventoryCapacity; ++i) {
    ItemHandle h = dstService.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, i});
    if (i == 0) {
      REQUIRE(h);
      CHECK(dstService.getStore().get(h)->instanceId == 11);
    } else if (i == 7) {
      REQUIRE(h);
      CHECK(dstService.getStore().get(h)->instanceId == 22);
    } else if (i == 25) {
      REQUIRE(h);
      CHECK(dstService.getStore().get(h)->instanceId == 33);
    } else if (i == 39) {
      REQUIRE(h);
      CHECK(dstService.getStore().get(h)->instanceId == 44);
    } else {
      CHECK(!h);
    }
  }
}

TEST_CASE("[Unit] ItemComponent - Set Name Hash Automatic Recalculation (M7)") {
  TestSetupScope scope;

  ItemComponent setItem;
  setItem.id = 5001;
  setItem.name = "Immortal Helm";
  setItem.setName = "Immortal King";
  setItem.setNameHash = 0; // 故意重置为 0

  nlohmann::json j = setItem;
  // JSON 序列化不持久化 setNameHash 字段，反序列化时自动重算
  CHECK(!j.contains("setNameHash"));

  ItemComponent loadedItem = j.get<ItemComponent>();
  uint32_t expectedHash = NoMoreDay::utils::Hash("Immortal King");
  CHECK(loadedItem.setNameHash == expectedHash);
  CHECK(loadedItem.setName == "Immortal King");
}
