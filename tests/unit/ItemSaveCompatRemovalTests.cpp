#include "TestCommon.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerProfile.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using namespace NoMoreDay;

TEST_CASE("[Unit][Save] - Legacy slot JSON is not imported and does not generate binary save") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);

  std::string oldJsonPath = "saves/slot_77.json";
  std::string nmdPath = "saves/slot_77.nmd";
  std::string bakPath = "saves/slot_77.nmd.bak";

  std::error_code ec;
  std::filesystem::remove(oldJsonPath, ec);
  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);

  // 写入旧版 JSON 格式存档
  {
    nlohmann::json oldJson;
    oldJson["header"] = {
        {"name", "OldHero"},
        {"characterClass", "Warrior"},
        {"level", 10},
        {"playtime", 100},
        {"timestamp", 1000},
        {"version", 3}
    };
    oldJson["primaryStats"] = {
        {"strength", 10.0f},
        {"agility", 10.0f},
        {"intelligence", 10.0f},
        {"vitality", 10.0f}
    };
    oldJson["position"] = {{"x", 1.0f}, {"y", 2.0f}};
    oldJson["mapId"] = "Town_01";
    oldJson["gold"] = 9999;
    oldJson["inventory"] = nlohmann::json::array();
    oldJson["equipment"] = nlohmann::json::array();
    oldJson["bagSlots"] = nlohmann::json::array();
    oldJson["materialBank"] = nlohmann::json::array();

    std::filesystem::create_directories("saves");
    std::ofstream file(oldJsonPath);
    REQUIRE(file.is_open());
    file << oldJson.dump(4);
    file.close();
  }

  entt::registry reg;
  ItemStorageService service;
  sm.SetItemStorageService(&service);

  bool loadResult = sm.loadCharacter(reg, 77);
  // 单轨模式：旧版 JSON 存档不再兼容导入，直接拒绝
  CHECK(loadResult == false);
  CHECK(!std::filesystem::exists(nmdPath));

  std::filesystem::remove(oldJsonPath, ec);
  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);
}

TEST_CASE("[Unit][Save] - Legacy global.json is not imported") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);

  std::string oldGlobalJson = "saves/global.json";
  std::string globalNmd = "saves/global.nmd";
  std::string globalBak = "saves/global.nmd.bak";

  std::error_code ec;
  std::filesystem::remove(oldGlobalJson, ec);
  std::filesystem::remove(globalNmd, ec);
  std::filesystem::remove(globalBak, ec);

  // 写入旧版 global.json
  {
    nlohmann::json oldJson;
    oldJson["unlockedTabs"] = 5;
    oldJson["tabs"] = nlohmann::json::array();

    std::filesystem::create_directories("saves");
    std::ofstream file(oldGlobalJson);
    REQUIRE(file.is_open());
    file << oldJson.dump(4);
    file.close();
  }

  entt::registry reg;
  SharedStash sharedStash;
  ItemStorageService service;
  SharedContext ctx;
  ctx.sharedStash = &sharedStash;
  ctx.itemStorage = &service;
  ctx.saveManager = &sm;
  reg.ctx().emplace<SharedContext*>(&ctx);

  bool loadResult = sm.loadGlobal(reg);
  CHECK(loadResult == true);
  // 确认未从 global.json 导入 5 个标签页，保持初始状态
  CHECK(service.getUnlockedPages(ContainerKind::SharedStash) == 1);
  CHECK(sharedStash.getUnlockedTabCount() == 1);

  std::filesystem::remove(oldGlobalJson, ec);
  std::filesystem::remove(globalNmd, ec);
  std::filesystem::remove(globalBak, ec);
}

TEST_CASE("[Unit][Save] - Version-mismatched binary save is rejected and falls back to backup") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);

  std::string nmdPath = "saves/slot_76.nmd";
  std::string bakPath = "saves/slot_76.nmd.bak";

  std::error_code ec;
  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);

  entt::registry reg;
  auto player = reg.create();
  reg.emplace<PlayerTag>(player);
  reg.emplace<PlayerName>(player, "BackupHero");
  reg.emplace<PlayerLevel>(player, 50);
  reg.emplace<Position>(player, 10.0f, 20.0f);
  reg.emplace<PrimaryStats>(player);

  ItemStorageService service;
  service.setGold(7777);
  sm.SetItemStorageService(&service);

  // 1. 正常保存生成备份档
  auto fut1 = sm.saveCharacterAsync(reg, 76);
  REQUIRE(fut1.valid());
  REQUIRE(fut1.get());

  auto fut2 = sm.saveCharacterAsync(reg, 76);
  REQUIRE(fut2.valid());
  REQUIRE(fut2.get());
  REQUIRE(std::filesystem::exists(bakPath));

  // 2. 将主档版本号篡改为旧版本 (version = 3)
  {
    CharacterSaveData oldData = sm.createSnapshot(reg);
    oldData.header.version = 3;
    nlohmann::json progJson = SaveManager::BuildProgressionJson(oldData);
    std::string payload = progJson.dump();

    std::stringstream ss;
    bool encOk = ItemPersistenceCodec::encode(service, ss, nullptr, ContainerDirtyFlags::All, payload);
    REQUIRE(encOk);

    std::string str = ss.str();
    std::vector<uint8_t> binaryData(str.begin(), str.end());
    std::string tempPath = "saves/temp/slot_76.tmp";
    REQUIRE(ItemPersistenceCodec::SaveFileAtomic(nmdPath, tempPath, binaryData, false));
  }

  // 3. 读取时主档版本不匹配被拒绝，自动从 .bak 恢复
  entt::registry loadedReg;
  ItemStorageService loadedService;
  sm.SetItemStorageService(&loadedService);
  bool loadResult = sm.loadCharacter(loadedReg, 76);
  REQUIRE(loadResult == true);

  auto view = loadedReg.view<PlayerTag>();
  REQUIRE(view.begin() != view.end());
  auto loadedPlayer = *view.begin();
  CHECK(loadedReg.get<PlayerName>(loadedPlayer).value == "BackupHero");
  CHECK(loadedService.getGold() == 7777);

  // 4. 若备份档亦不存在，则加载彻底失败
  std::filesystem::remove(bakPath, ec);
  entt::registry failReg;
  ItemStorageService failService;
  sm.SetItemStorageService(&failService);
  bool failResult = sm.loadCharacter(failReg, 76);
  CHECK(failResult == false);

  std::filesystem::remove(nmdPath, ec);
  std::filesystem::remove(bakPath, ec);
}

TEST_CASE("[Unit][Save] - saveCharacterAsync and saveGlobalAsync fail fast without ItemStorageService") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  tf::Executor executor;
  SaveManager sm;
  sm.Initialize(&executor);
  sm.SetItemStorageService(nullptr);

  std::string nmdPath = "saves/slot_75.nmd";
  std::string globalNmd = "saves/global.nmd";
  std::error_code ec;
  std::filesystem::remove(nmdPath, ec);

  entt::registry reg;
  auto player = reg.create();
  reg.emplace<PlayerTag>(player);
  reg.emplace<Position>(player, 0.0f, 0.0f);
  reg.emplace<PrimaryStats>(player);

  // 未挂载 ItemStorageService，保存必须立即返回 false，杜绝静默丢物品
  auto charFut = sm.saveCharacterAsync(reg, 75);
  REQUIRE(charFut.valid());
  CHECK(charFut.get() == false);
  CHECK(!std::filesystem::exists(nmdPath));

  auto globalFut = sm.saveGlobalAsync(reg);
  REQUIRE(globalFut.valid());
  CHECK(globalFut.get() == false);

  std::filesystem::remove(nmdPath, ec);
}
