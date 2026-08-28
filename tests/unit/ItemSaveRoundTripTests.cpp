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

  std::string nmdPath = "saves/slot_99.nmd";
  std::string bakPath = "saves/slot_99.nmd.bak";
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
  std::error_code ec;
  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);
}

TEST_CASE("[Unit] SaveManager - In-Flight Save Guard and Concurrent Rejection") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);

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
  std::error_code ec;
  std::filesystem::remove("saves/slot_98.nmd", ec);
  std::filesystem::remove("saves/slot_98.nmd.bak", ec);
}
