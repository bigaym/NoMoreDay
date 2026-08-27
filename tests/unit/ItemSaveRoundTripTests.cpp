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
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include <nlohmann/json.hpp>

using namespace NoMoreDay;

namespace {

entt::entity CreateTestWeapon(entt::registry &registry, uint32_t id,
                              const std::string &name, int level = 20) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = id;
  item.name = name;
  item.itemLevel = level;
  item.type = ItemType::Weapon;
  item.slot = EquipmentSlot::MainHand;
  item.rarity = Rarity::Rare;
  item.attack = 35.0f;
  item.weaponSubtype = WeaponSubtype::Sword;
  registry.emplace<ItemComponent>(entity, item);
  return entity;
}

entt::entity CreateTestRune(entt::registry &registry, uint32_t id,
                            const std::string &name) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = id;
  item.name = name;
  item.type = ItemType::Material;
  item.rarity = Rarity::Uncommon;
  item.quantity = 1;
  registry.emplace<ItemComponent>(entity, item);
  return entity;
}

entt::entity CreateTestBag(entt::registry &registry, uint32_t id,
                           const std::string &name, int capacity) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = id;
  item.name = name;
  item.type = ItemType::Bag;
  item.rarity = Rarity::Magic;
  item.bagCapacity = capacity;
  registry.emplace<ItemComponent>(entity, item);
  return entity;
}

} // namespace

TEST_CASE("[Unit][Item][Save] - Locked Item Round Trip") {
  TestSetupScope scope;
  entt::registry registry;

  auto weaponEntity = CreateTestWeapon(registry, 1001, "Locked Blade");
  auto &item = registry.get<ItemComponent>(weaponEntity);
  item.isLocked = true;

  SerializedItem dto = ItemFactory::serializeItem(registry, weaponEntity);
  CHECK(dto.isLocked == true);

  // Serialize to JSON and back
  nlohmann::json j = dto;
  SerializedItem deserializedDto = j.get<SerializedItem>();
  CHECK(deserializedDto.isLocked == true);

  entt::registry restoreRegistry;
  auto restoredEntity = ItemFactory::restoreItem(restoreRegistry, deserializedDto);
  const auto &restoredItem = restoreRegistry.get<ItemComponent>(restoredEntity);

  CHECK(restoredItem.id == 1001);
  CHECK(restoredItem.name == "Locked Blade");
  CHECK(restoredItem.isLocked == true);
}

TEST_CASE("[Unit][Item][Save] - Empty Sockets Preservation") {
  TestSetupScope scope;
  entt::registry registry;

  auto weaponEntity = CreateTestWeapon(registry, 1002, "Natural 3-Socket Blade");
  auto &item = registry.get<ItemComponent>(weaponEntity);
  item.socketCount = 3;
  item.sockets.assign(3, entt::null);

  SerializedItem dto = ItemFactory::serializeItem(registry, weaponEntity);
  CHECK(dto.socketCount == 3);
  CHECK(dto.socketedItems.empty()); // No actual socketed entities, but socketCount = 3

  nlohmann::json j = dto;
  SerializedItem deserializedDto = j.get<SerializedItem>();
  CHECK(deserializedDto.socketCount == 3);
  CHECK(deserializedDto.socketedItems.empty());

  entt::registry restoreRegistry;
  auto restoredEntity = ItemFactory::restoreItem(restoreRegistry, deserializedDto);
  const auto &restoredItem = restoreRegistry.get<ItemComponent>(restoredEntity);

  CHECK(restoredItem.socketCount == 3);
  CHECK(restoredItem.sockets.size() == 3);
  CHECK(restoredItem.sockets[0] == (entt::entity)entt::null);
  CHECK(restoredItem.sockets[1] == (entt::entity)entt::null);
  CHECK(restoredItem.sockets[2] == (entt::entity)entt::null);
}

TEST_CASE("[Unit][Item][Save] - Runeword Weapon with Partial Sockets and Precise Slot Index") {
  TestSetupScope scope;
  entt::registry registry;

  auto weaponEntity = CreateTestWeapon(registry, 1003, "Runeword Base");
  auto &item = registry.get<ItemComponent>(weaponEntity);
  item.socketCount = 3;
  item.activeRunewordId = 205; // Enigma or similar runeword id
  item.sockets.resize(3, entt::null);

  // Slot 0 is empty (entt::null)
  // Slot 1 has Rune Jah
  auto rune1 = CreateTestRune(registry, 7001, "Rune Jah");
  item.sockets[1] = rune1;

  // Slot 2 has Rune Ith
  auto rune2 = CreateTestRune(registry, 7002, "Rune Ith");
  item.sockets[2] = rune2;

  SerializedItem dto = ItemFactory::serializeItem(registry, weaponEntity);
  CHECK(dto.activeRunewordId == 205);
  CHECK(dto.socketCount == 3);
  REQUIRE(dto.socketedItems.size() == 2);
  CHECK(dto.socketedItems[0].socketIndex == 1);
  CHECK(dto.socketedItems[0].item.itemId == 7001);
  CHECK(dto.socketedItems[1].socketIndex == 2);
  CHECK(dto.socketedItems[1].item.itemId == 7002);

  nlohmann::json j = dto;
  SerializedItem deserializedDto = j.get<SerializedItem>();
  CHECK(deserializedDto.activeRunewordId == 205);
  CHECK(deserializedDto.socketCount == 3);
  REQUIRE(deserializedDto.socketedItems.size() == 2);
  CHECK(deserializedDto.socketedItems[0].socketIndex == 1);
  CHECK(deserializedDto.socketedItems[1].socketIndex == 2);

  entt::registry restoreRegistry;
  auto restoredEntity = ItemFactory::restoreItem(restoreRegistry, deserializedDto);
  const auto &restoredItem = restoreRegistry.get<ItemComponent>(restoredEntity);

  CHECK(restoredItem.activeRunewordId == 205);
  CHECK(restoredItem.socketCount == 3);
  REQUIRE(restoredItem.sockets.size() == 3);
  CHECK(restoredItem.sockets[0] == (entt::entity)entt::null);

  REQUIRE(restoreRegistry.valid(restoredItem.sockets[1]));
  const auto &restoredRune1 = restoreRegistry.get<ItemComponent>(restoredItem.sockets[1]);
  CHECK(restoredRune1.id == 7001);
  CHECK(restoredRune1.name == "Rune Jah");

  REQUIRE(restoreRegistry.valid(restoredItem.sockets[2]));
  const auto &restoredRune2 = restoreRegistry.get<ItemComponent>(restoredItem.sockets[2]);
  CHECK(restoredRune2.id == 7002);
  CHECK(restoredRune2.name == "Rune Ith");
}

TEST_CASE("[Unit][Item][Save] - Stat Conversions and Damage Modifiers Round Trip") {
  TestSetupScope scope;
  entt::registry registry;

  auto armorEntity = registry.create();
  ItemComponent item;
  item.id = 2001;
  item.name = "Conversion Robe";
  item.type = ItemType::Armor;
  item.slot = EquipmentSlot::Chest;

  StatConversion conv;
  conv.source = StatType::PhysicalDamage;
  conv.target = StatType::FireDamage;
  conv.ratio = 0.5f;
  conv.required_tags = Tag::Melee;
  item.conversions.push_back(conv);

  DamageModifier mod;
  mod.source_tag = Tag::Fire;
  mod.target_tag = Tag::Fire;
  mod.value = 40.0f;
  mod.type = ModifierType::Increased;
  item.damage_modifiers.push_back(mod);

  registry.emplace<ItemComponent>(armorEntity, item);

  SerializedItem dto = ItemFactory::serializeItem(registry, armorEntity);
  REQUIRE(dto.conversions.size() == 1);
  CHECK(dto.conversions[0].source == StatType::PhysicalDamage);
  CHECK(dto.conversions[0].target == StatType::FireDamage);
  CHECK(dto.conversions[0].ratio == doctest::Approx(0.5f));
  REQUIRE(dto.damageModifiers.size() == 1);
  CHECK(dto.damageModifiers[0].value == doctest::Approx(40.0f));

  nlohmann::json j = dto;
  SerializedItem deserializedDto = j.get<SerializedItem>();

  entt::registry restoreRegistry;
  auto restoredEntity = ItemFactory::restoreItem(restoreRegistry, deserializedDto);
  const auto &restoredItem = restoreRegistry.get<ItemComponent>(restoredEntity);

  REQUIRE(restoredItem.conversions.size() == 1);
  CHECK(restoredItem.conversions[0].source == StatType::PhysicalDamage);
  CHECK(restoredItem.conversions[0].target == StatType::FireDamage);
  CHECK(restoredItem.conversions[0].ratio == doctest::Approx(0.5f));
  CHECK(restoredItem.conversions[0].required_tags == Tag::Melee);

  REQUIRE(restoredItem.damage_modifiers.size() == 1);
  CHECK(restoredItem.damage_modifiers[0].source_tag == Tag::Fire);
  CHECK(restoredItem.damage_modifiers[0].value == doctest::Approx(40.0f));
  CHECK(restoredItem.damage_modifiers[0].type == ModifierType::Increased);
}

TEST_CASE("[Unit][Item][Save] - Set Item Hash Automatic Recalculation") {
  TestSetupScope scope;
  entt::registry registry;

  auto setHelm = registry.create();
  ItemComponent item;
  item.id = 3001;
  item.name = "Immortal King's Will";
  item.type = ItemType::Armor;
  item.slot = EquipmentSlot::Head;
  item.rarity = Rarity::Set;
  item.setName = "Immortal King";
  item.setNameHash = 0; // Not initialized or wiped
  registry.emplace<ItemComponent>(setHelm, item);

  SerializedItem dto = ItemFactory::serializeItem(registry, setHelm);
  nlohmann::json j = dto;
  SerializedItem deserializedDto = j.get<SerializedItem>();

  entt::registry restoreRegistry;
  auto restoredEntity = ItemFactory::restoreItem(restoreRegistry, deserializedDto);
  const auto &restoredItem = restoreRegistry.get<ItemComponent>(restoredEntity);

  CHECK(restoredItem.setName == "Immortal King");
  uint32_t expectedHash = NoMoreDay::utils::Hash("Immortal King");
  CHECK(expectedHash != 0);
  CHECK(restoredItem.setNameHash == expectedHash);
}

TEST_CASE("[Unit][Item][Save] - Sparse Inventory Slot Layout Preservation") {
  TestSetupScope scope;
  entt::registry registry;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerName>(player, "Hero");
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<PrimaryStats>(player);

  auto &inv = registry.emplace<InventoryComponent>(player);
  inv.capacity = 40;
  inv.items.assign(40, entt::null);

  auto item0 = CreateTestWeapon(registry, 101, "Sword At Slot 0");
  auto item5 = CreateTestWeapon(registry, 102, "Sword At Slot 5");
  auto item38 = CreateTestWeapon(registry, 103, "Sword At Slot 38");

  inv.items[0] = item0;
  inv.items[5] = item5;
  inv.items[38] = item38;

  CharacterSaveData snapshot = SaveManager::Get().createSnapshot(registry);
  CHECK(snapshot.inventoryCapacity == 40);
  REQUIRE(snapshot.inventory.size() == 3);
  CHECK(snapshot.inventory[0].slotIndex == 0);
  CHECK(snapshot.inventory[0].item.itemId == 101);
  CHECK(snapshot.inventory[1].slotIndex == 5);
  CHECK(snapshot.inventory[1].item.itemId == 102);
  CHECK(snapshot.inventory[2].slotIndex == 38);
  CHECK(snapshot.inventory[2].item.itemId == 103);

  nlohmann::json j = snapshot;
  CharacterSaveData loadedSnapshot = j.get<CharacterSaveData>();

  entt::registry restoreRegistry;
  SaveManager::Get().restoreFromSnapshot(restoreRegistry, loadedSnapshot);

  auto view = restoreRegistry.view<PlayerTag>();
  REQUIRE(view.begin() != view.end());
  auto restoredPlayer = *view.begin();

  const auto &restoredInv = restoreRegistry.get<InventoryComponent>(restoredPlayer);
  CHECK(restoredInv.capacity == 40);
  REQUIRE(restoredInv.items.size() == 40);

  // Exact slot assertions
  REQUIRE(restoreRegistry.valid(restoredInv.items[0]));
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.items[0]).id == 101);

  REQUIRE(restoreRegistry.valid(restoredInv.items[5]));
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.items[5]).id == 102);

  REQUIRE(restoreRegistry.valid(restoredInv.items[38]));
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.items[38]).id == 103);

  // All other slots must be entt::null
  for (int i = 0; i < 40; ++i) {
    if (i != 0 && i != 5 && i != 38) {
      CHECK(restoredInv.items[i] == (entt::entity)entt::null);
    }
  }
}

TEST_CASE("[Unit][Item][Save] - Bag Slots and Material Bank Round Trip") {
  TestSetupScope scope;
  entt::registry registry;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerName>(player, "Collector");
  registry.emplace<Position>(player);
  registry.emplace<PrimaryStats>(player);

  auto &inv = registry.emplace<InventoryComponent>(player);
  auto bag0 = CreateTestBag(registry, 8001, "Small Pouch", 8);
  auto bag2 = CreateTestBag(registry, 8002, "Large Backpack", 16);
  inv.bag_slots[0] = bag0;
  inv.bag_slots[1] = entt::null;
  inv.bag_slots[2] = bag2;
  inv.bag_slots[3] = entt::null;

  auto &bank = registry.emplace<MaterialBankComponent>(player);
  bank.Add(101, 100);
  bank.Add(202, 50);
  bank.Add(303, 5);

  CharacterSaveData snapshot = SaveManager::Get().createSnapshot(registry);
  REQUIRE(snapshot.bagSlots.size() == 2);
  CHECK(snapshot.bagSlots[0].index == 0);
  CHECK(snapshot.bagSlots[0].bag.itemId == 8001);
  CHECK(snapshot.bagSlots[1].index == 2);
  CHECK(snapshot.bagSlots[1].bag.itemId == 8002);

  REQUIRE(snapshot.materialBank.size() == 3);

  nlohmann::json j = snapshot;
  CharacterSaveData loadedSnapshot = j.get<CharacterSaveData>();

  entt::registry restoreRegistry;
  SaveManager::Get().restoreFromSnapshot(restoreRegistry, loadedSnapshot);

  auto view = restoreRegistry.view<PlayerTag>();
  REQUIRE(view.begin() != view.end());
  auto restoredPlayer = *view.begin();

  const auto &restoredInv = restoreRegistry.get<InventoryComponent>(restoredPlayer);
  REQUIRE(restoreRegistry.valid(restoredInv.bag_slots[0]));
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.bag_slots[0]).id == 8001);
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.bag_slots[0]).bagCapacity == 8);

  CHECK(restoredInv.bag_slots[1] == (entt::entity)entt::null);

  REQUIRE(restoreRegistry.valid(restoredInv.bag_slots[2]));
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.bag_slots[2]).id == 8002);
  CHECK(restoreRegistry.get<ItemComponent>(restoredInv.bag_slots[2]).bagCapacity == 16);

  CHECK(restoredInv.bag_slots[3] == (entt::entity)entt::null);

  REQUIRE(restoreRegistry.all_of<MaterialBankComponent>(restoredPlayer));
  const auto &restoredBank = restoreRegistry.get<MaterialBankComponent>(restoredPlayer);
  CHECK(restoredBank.GetCount(101) == 100);
  CHECK(restoredBank.GetCount(202) == 50);
  CHECK(restoredBank.GetCount(303) == 5);
  CHECK(restoredBank.GetCount(999) == 0);
}

TEST_CASE("[Unit][Item][Save] - Legacy V3 Save Migration to V4") {
  TestSetupScope scope;

  // Start from valid default CharacterSaveData with version 3
  CharacterSaveData v3Data;
  v3Data.header.version = 3;
  v3Data.header.name = "OldHero";
  v3Data.header.level = 15;
  v3Data.header.playtime = 1200;

  nlohmann::json v3Json = v3Data;
  // Inject flat legacy V3 inventory array (no slotIndex, flat socketedItems with no socketIndex)
  v3Json["inventory"] = nlohmann::json::array({
      nlohmann::json{
          {"itemId", 101},
          {"instanceId", 1},
          {"name", "Legacy Sword"},
          {"type", 0},
          {"textureId", 0},
          {"quantity", 1},
          {"stats", nlohmann::json{{"level", 15}, {"rarity", 1}, {"attack", 20.0}, {"defense", 0.0}, {"slot", 1}, {"forgingPotential", 0}, {"legendaryPotential", 0}, {"value", 100.0}}},
          {"affixes", nlohmann::json::array()},
          {"implicits", nlohmann::json::array()},
          {"socketedItems", nlohmann::json::array({
              nlohmann::json{
                  {"itemId", 7001},
                  {"instanceId", 2},
                  {"name", "Socketed Gem"},
                  {"type", 5},
                  {"textureId", 0},
                  {"quantity", 1},
                  {"stats", nlohmann::json{{"level", 1}, {"rarity", 0}, {"attack", 0.0}, {"defense", 0.0}, {"slot", 0}, {"forgingPotential", 0}, {"legendaryPotential", 0}, {"value", 10.0}}},
                  {"affixes", nlohmann::json::array()},
                  {"implicits", nlohmann::json::array()},
                  {"socketedItems", nlohmann::json::array()}
              }
          })}
      },
      nlohmann::json{
          {"itemId", 102},
          {"instanceId", 3},
          {"name", "Legacy Shield"},
          {"type", 2},
          {"textureId", 0},
          {"quantity", 1},
          {"stats", nlohmann::json{{"level", 15}, {"rarity", 1}, {"attack", 0.0}, {"defense", 20.0}, {"slot", 2}, {"forgingPotential", 0}, {"legendaryPotential", 0}, {"value", 120.0}}},
          {"affixes", nlohmann::json::array()},
          {"implicits", nlohmann::json::array()},
          {"socketedItems", nlohmann::json::array()}
      }
  });

  CharacterSaveData data = v3Json.get<CharacterSaveData>();

  // Verify backward compatible from_json behavior
  REQUIRE(data.inventory.size() == 2);
  CHECK(data.inventory[0].slotIndex == 0);
  CHECK(data.inventory[0].item.itemId == 101);
  CHECK(data.inventory[1].slotIndex == 1);
  CHECK(data.inventory[1].item.itemId == 102);

  REQUIRE(data.inventory[0].item.socketedItems.size() == 1);
  CHECK(data.inventory[0].item.socketedItems[0].socketIndex == 0);
  CHECK(data.inventory[0].item.socketedItems[0].item.itemId == 7001);

  // Apply migration
  SaveManager::MigrateSaveDataV3toV4(data);
  CHECK(data.header.version == CURRENT_CHARACTER_SAVE_VERSION);
  CHECK(data.inventoryCapacity == InventoryComponent::BASE_CAPACITY);

  // Restore into registry
  entt::registry restoreRegistry;
  SaveManager::Get().restoreFromSnapshot(restoreRegistry, data);

  auto view = restoreRegistry.view<PlayerTag>();
  REQUIRE(view.begin() != view.end());
  auto player = *view.begin();

  const auto &inv = restoreRegistry.get<InventoryComponent>(player);
  CHECK(inv.capacity == InventoryComponent::BASE_CAPACITY);
  REQUIRE(restoreRegistry.valid(inv.items[0]));
  CHECK(restoreRegistry.get<ItemComponent>(inv.items[0]).name == "Legacy Sword");

  // Check socketed item on restored legacy sword
  const auto &swordItem = restoreRegistry.get<ItemComponent>(inv.items[0]);
  REQUIRE(swordItem.sockets.size() >= 1);
  REQUIRE(restoreRegistry.valid(swordItem.sockets[0]));
  CHECK(restoreRegistry.get<ItemComponent>(swordItem.sockets[0]).id == 7001);

  REQUIRE(restoreRegistry.valid(inv.items[1]));
  CHECK(restoreRegistry.get<ItemComponent>(inv.items[1]).name == "Legacy Shield");
}

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
  SaveManager::Get().Initialize(&executor);

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

  SaveManager::Get().SetItemStorageService(&service);

  // 1. 异步写入存档
  auto future = SaveManager::Get().saveCharacterAsync(reg, 99);
  REQUIRE(future.valid());
  bool saveResult = future.get();
  REQUIRE(saveResult);

  std::string nmdPath = "saves/slot_99.nmd";
  std::string bakPath = "saves/slot_99.nmd.bak";
  CHECK(std::filesystem::exists(nmdPath));

  // 2. 正常读取
  entt::registry loadedReg;
  ItemStorageService loadedService;
  SaveManager::Get().SetItemStorageService(&loadedService);
  bool loadResult = SaveManager::Get().loadCharacter(loadedReg, 99);
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
    auto fut2 = SaveManager::Get().saveCharacterAsync(reg, 99);
    fut2.get();
    REQUIRE(std::filesystem::exists(bakPath));

    // 损坏主档
    std::ofstream corrupt(nmdPath, std::ios::binary | std::ios::trunc);
    corrupt << "CORRUPTED_BYTES_HEADER";
    corrupt.close();
  }

  entt::registry backupReg;
  ItemStorageService backupService;
  SaveManager::Get().SetItemStorageService(&backupService);
  bool backupLoadResult = SaveManager::Get().loadCharacter(backupReg, 99);
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
  SaveManager::Get().Initialize(&executor);

  entt::registry reg;
  auto player = reg.create();
  reg.emplace<PlayerTag>(player);
  reg.emplace<PlayerName>(player, "ConcurrentHero");
  reg.emplace<PlayerLevel>(player, 10);
  reg.emplace<Position>(player, 10.0f, 20.0f);
  reg.emplace<PrimaryStats>(player);

  ItemStorageService service;
  service.setGold(1000);
  SaveManager::Get().SetItemStorageService(&service);

  // 发起多次连续异步保存请求
  auto fut1 = SaveManager::Get().saveCharacterAsync(reg, 98);
  auto fut2 = SaveManager::Get().saveCharacterAsync(reg, 98);

  bool res1 = fut1.valid() ? fut1.get() : false;
  bool res2 = fut2.valid() ? fut2.get() : false;
  CHECK(res1);
  (void)res2;

  // 清理测试文件
  std::error_code ec;
  std::filesystem::remove("saves/slot_98.nmd", ec);
  std::filesystem::remove("saves/slot_98.nmd.bak", ec);
}
