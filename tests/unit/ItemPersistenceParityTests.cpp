#include "TestCommon.hpp"
#include "core/utils/HashUtils.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/components/Combat.hpp"
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
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <sstream>

using namespace NoMoreDay;

namespace {

entt::entity CreateParityWeapon(entt::registry &registry, uint32_t id,
                                const std::string &name, int level = 30) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = id;
  item.name = name;
  item.itemLevel = level;
  item.type = ItemType::Weapon;
  item.slot = EquipmentSlot::MainHand;
  item.rarity = Rarity::Rare;
  item.attack = 42.0f;
  item.weaponSubtype = WeaponSubtype::Sword;
  item.isLocked = true;
  item.socketCount = 2;
  item.sockets.resize(2, entt::null);

  Affix aff1;
  aff1.type = AffixType::Strength;
  aff1.tier = 3;
  aff1.value = 15.0f;
  aff1.isPrefix = true;
  item.affixes.push_back(aff1);

  Affix aff2;
  aff2.type = AffixType::CritChance;
  aff2.tier = 2;
  aff2.value = 6.0f;
  aff2.isPrefix = false;
  item.affixes.push_back(aff2);

  registry.emplace<ItemComponent>(entity, item);
  return entity;
}

entt::entity CreateParityArmor(entt::registry &registry, uint32_t id,
                               const std::string &name, int level = 25) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = id;
  item.name = name;
  item.itemLevel = level;
  item.type = ItemType::Armor;
  item.slot = EquipmentSlot::Chest;
  item.rarity = Rarity::Magic;
  item.defense = 50.0f;

  StatConversion conv;
  conv.source = StatType::PhysicalDamage;
  conv.target = StatType::FireDamage;
  conv.ratio = 0.4f;
  conv.required_tags = Tag::Melee;
  item.conversions.push_back(conv);

  registry.emplace<ItemComponent>(entity, item);
  return entity;
}

entt::entity CreateParityBag(entt::registry &registry, uint32_t id,
                             const std::string &name, int capacity = 12) {
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

TEST_CASE("[Unit][Item][Persistence] - Parity Test: Legacy JSON Output vs New Binary Decode Equivalence") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  // 1. 构造一个包含丰富复合状态的玩家实体与仓库数据
  entt::registry sourceRegistry;
  auto player = sourceRegistry.create();
  sourceRegistry.emplace<PlayerTag>(player);
  sourceRegistry.emplace<PlayerName>(player, "ParityHero");
  sourceRegistry.emplace<Position>(player, 320.0f, 640.0f);
  sourceRegistry.emplace<PrimaryStats>(player, 50.0f, 40.0f, 30.0f, 20.0f);
  sourceRegistry.emplace<ActiveSkillsComponent>(player);
  sourceRegistry.emplace<AstrolabeComponent>(player);
  sourceRegistry.emplace<PlayerCombatHistory>(player);

  auto &inv = sourceRegistry.emplace<InventoryComponent>(player);
  inv.capacity = 40;
  inv.items.assign(40, entt::null);
  inv.gold = 77777;

  auto weapon1 = CreateParityWeapon(sourceRegistry, 101, "Parity Blade", 35);
  auto armor1 = CreateParityArmor(sourceRegistry, 102, "Parity Robe", 28);
  inv.items[0] = weapon1;
  inv.items[7] = armor1;

  auto bag1 = CreateParityBag(sourceRegistry, 501, "Medium Backpack", 12);
  inv.bag_slots[0] = bag1;

  auto &eq = sourceRegistry.emplace<EquipmentComponent>(player);
  auto eqWeapon = CreateParityWeapon(sourceRegistry, 201, "Equipped Sword", 40);
  auto eqChest = CreateParityArmor(sourceRegistry, 202, "Equipped Plate", 38);
  eq.set(EquipmentSlot::MainHand, eqWeapon);
  eq.set(EquipmentSlot::Chest, eqChest);

  auto &bank = sourceRegistry.emplace<MaterialBankComponent>(player);
  bank.Add(101, 80);
  bank.Add(202, 45);

  auto &stash = sourceRegistry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 2;
  stash.tabs.resize(2);
  stash.tabs[0].name = "Stash 1";
  stash.tabs[1].name = "Stash 2";
  auto stashItem1 = CreateParityWeapon(sourceRegistry, 301, "Stashed Weapon", 20);
  stash.tabs[0].items[3] = stashItem1;

  // 2. 路径 A: 生成旧版 JSON 快照并序列化为 JSON 文本
  CharacterSaveData legacyDto = SaveManager::Get().createSnapshot(sourceRegistry);
  nlohmann::json legacyJson = legacyDto;
  std::string legacyJsonStr = legacyJson.dump();

  // 3. 路径 B: 使用 SaveManager 保存二进制 .nmd 格式
  ItemStorageService storageService;
  storageService.setGold(inv.gold);
  for (const auto &entry : bank.materials) {
    storageService.addMaterial(entry.id, entry.count);
  }
  storageService.setUnlockedPages(ContainerKind::PersonalStash, stash.unlockedTabs);

  // 注册并填充各槽位
  ItemSideTableData weaponSide;
  ItemInstance weaponInst = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(weapon1), &weaponSide);
  ItemHandle hWeapon1 = storageService.getStoreMutable().create(weaponInst);
  if (!weaponSide.empty()) {
    storageService.getStoreMutable().setSideTable(hWeapon1, weaponSide);
  }
  storageService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hWeapon1);

  ItemSideTableData armorSide;
  ItemInstance armorInst = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(armor1), &armorSide);
  ItemHandle hArmor1 = storageService.getStoreMutable().create(armorInst);
  if (!armorSide.empty()) {
    storageService.getStoreMutable().setSideTable(hArmor1, armorSide);
  }
  storageService.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 7}, hArmor1);

  ItemInstance bagInst = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(bag1));
  ItemHandle hBag1 = storageService.getStoreMutable().create(bagInst);
  storageService.setSlotHandle(SlotRef{ContainerKind::BagSlots, 0, 0, 0}, hBag1);

  ItemInstance eqWeaponInst = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(eqWeapon));
  ItemHandle hEqWeapon = storageService.getStoreMutable().create(eqWeaponInst);
  storageService.setSlotHandle(
      SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0},
      hEqWeapon);

  ItemSideTableData eqChestSide;
  ItemInstance eqChestInst = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(eqChest), &eqChestSide);
  ItemHandle hEqChest = storageService.getStoreMutable().create(eqChestInst);
  if (!eqChestSide.empty()) {
    storageService.getStoreMutable().setSideTable(hEqChest, eqChestSide);
  }
  storageService.setSlotHandle(
      SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0},
      hEqChest);

  ItemInstance stashInst1 = ItemComponentToInstance(
      sourceRegistry.get<ItemComponent>(stashItem1));
  ItemHandle hStash1 = storageService.getStoreMutable().create(stashInst1);
  storageService.setSlotHandle(
      SlotRef{ContainerKind::PersonalStash, 0, 0, 3}, hStash1);

  // 4. 从路径 A 反序列化 JSON 恢复到 registry A
  nlohmann::json parsedJson = nlohmann::json::parse(legacyJsonStr);
  CharacterSaveData fromJsonData = parsedJson.get<CharacterSaveData>();
  entt::registry registryA;
  SaveManager::Get().restoreFromSnapshot(registryA, fromJsonData, /*restoreItems=*/true);

  // 5. 将 storageService 编码为二进制并解码到 decodedStorage
  std::stringstream binaryStream;
  nlohmann::json progJson = SaveManager::BuildProgressionJson(legacyDto);
  REQUIRE(ItemPersistenceCodec::encode(
      storageService, binaryStream, nullptr, ContainerDirtyFlags::All, progJson.dump()));

  ItemStorageService decodedStorage;
  std::string decodedProgPayload;
  binaryStream.seekg(0, std::ios::beg);
  REQUIRE(ItemPersistenceCodec::decode(binaryStream, decodedStorage, &decodedProgPayload));

  CharacterSaveData fromBinData = SaveManager::ParseProgressionJson(nlohmann::json::parse(decodedProgPayload));

  entt::registry registryB;
  SaveManager::Get().SetItemStorageService(&decodedStorage);
  SaveManager::Get().restoreFromSnapshot(registryB, fromBinData, /*restoreItems=*/false);

  // 6. 对拍断言：JSON 恢复结果与二进制解码结果在字段级、槽位级和数据内容上完全对齐
  auto viewA = registryA.view<PlayerTag>();
  auto viewB = registryB.view<PlayerTag>();
  REQUIRE(viewA.begin() != viewA.end());
  REQUIRE(viewB.begin() != viewB.end());
  auto playerA = *viewA.begin();
  auto playerB = *viewB.begin();

  // 核心标量
  CHECK(registryA.get<PlayerName>(playerA).value == registryB.get<PlayerName>(playerB).value);
  CHECK(registryA.get<Position>(playerA).x == doctest::Approx(registryB.get<Position>(playerB).x));
  CHECK(registryA.get<Position>(playerA).y == doctest::Approx(registryB.get<Position>(playerB).y));
  CHECK(registryA.get<PrimaryStats>(playerA).strength == doctest::Approx(registryB.get<PrimaryStats>(playerB).strength));

  // 金币与背包容量
  const auto &invA = registryA.get<InventoryComponent>(playerA);
  const auto &invB = registryB.get<InventoryComponent>(playerB);
  CHECK(invA.gold == 77777);
  CHECK(invB.gold == 77777);
  CHECK(decodedStorage.getGold() == 77777);
  CHECK(invA.capacity == 40);
  CHECK(invB.capacity == 40);
  CHECK(decodedStorage.getInventorySlots().size() == 40);

  // 背包槽位对拍
  REQUIRE(registryA.valid(invA.items[0]));
  const auto &itemA0 = registryA.get<ItemComponent>(invA.items[0]);
  ItemHandle h0 = decodedStorage.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0});
  REQUIRE(h0);
  const auto *inst0 = decodedStorage.getStore().get(h0);
  REQUIRE(inst0 != nullptr);
  CHECK(inst0->instanceId == 101);
  CHECK(inst0->isLocked() == true);
  CHECK(inst0->attack == doctest::Approx(42.0f));
  CHECK(inst0->socketCount == 2);
  CHECK(inst0->affixCount == 2);
  CHECK(itemA0.id == inst0->instanceId);

  REQUIRE(registryA.valid(invA.items[7]));
  const auto &itemA7 = registryA.get<ItemComponent>(invA.items[7]);
  ItemHandle h7 = decodedStorage.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 7});
  REQUIRE(h7);
  const auto *inst7 = decodedStorage.getStore().get(h7);
  REQUIRE(inst7 != nullptr);
  CHECK(inst7->instanceId == 102);
  CHECK(inst7->defense == doctest::Approx(50.0f));
  const auto *side7 = decodedStorage.getStore().getSideTable(h7);
  REQUIRE(side7 != nullptr);
  CHECK(side7->conversions.size() == 1);
  CHECK(side7->conversions[0].source == StatType::PhysicalDamage);
  CHECK(side7->conversions[0].target == StatType::FireDamage);
  CHECK(side7->conversions[0].ratio == doctest::Approx(0.4f));

  // 扩展背包槽位对拍
  REQUIRE(registryA.valid(invA.bag_slots[0]));
  ItemHandle hBag = decodedStorage.getSlotHandle(SlotRef{ContainerKind::BagSlots, 0, 0, 0});
  REQUIRE(hBag);
  const auto *instBag = decodedStorage.getStore().get(hBag);
  REQUIRE(instBag != nullptr);
  CHECK(instBag->instanceId == 501);

  // 材料银行对拍
  const auto &bankA = registryA.get<MaterialBankComponent>(playerA);
  CHECK(bankA.GetCount(101) == 80);
  CHECK(bankA.GetCount(202) == 45);
  CHECK(decodedStorage.getMaterialCount(101) == 80);
  CHECK(decodedStorage.getMaterialCount(202) == 45);

  // 个人仓库对拍
  const auto &stashA = registryA.get<PersonalStashComponent>(playerA);
  CHECK(stashA.unlockedTabs == 2);
  CHECK(decodedStorage.getUnlockedPages(ContainerKind::PersonalStash) == 2);
  ItemHandle hStash = decodedStorage.getSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 0, 3});
  REQUIRE(hStash);
  const auto *instStash = decodedStorage.getStore().get(hStash);
  REQUIRE(instStash != nullptr);
  CHECK(instStash->instanceId == 301);
}
