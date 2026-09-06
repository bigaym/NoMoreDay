#include "TestCommon.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace NoMoreDay;

TEST_CASE("[Unit] ItemPersistenceCodec - CRC32 Calculation") {
  SUBCASE("Null and empty data") {
    CHECK(calculateCrc32(nullptr, 0) == 0);
    const char empty[] = "";
    CHECK(calculateCrc32(empty, 0) == 0);
  }

  SUBCASE("Standard IEEE 802.3 test vectors") {
    // "123456789" -> 0xCBF43926 (标准测试向量)
    const char testStr[] = "123456789";
    const uint32_t crc = calculateCrc32(testStr, 9);
    CHECK(crc == 0xCBF43926u);
  }

  SUBCASE("Deterministic consistency") {
    const std::string text = "NoMoreDay Storage Persistence Engine";
    const uint32_t crc1 = calculateCrc32(text.data(), text.size());
    const uint32_t crc2 = calculateCrc32(text.data(), text.size());
    CHECK(crc1 == crc2);
    CHECK(crc1 != 0);
  }
}

TEST_CASE("[Unit] ItemPersistenceCodec - Round-Trip Full Service") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService service;

  // 1. 构造符文子物品
  ItemInstance runeProto{};
  runeProto.instanceId = 10001;
  runeProto.baseId = 10001;
  runeProto.quantity = 1;
  runeProto.itemLevel = 10;
  runeProto.rarity = 3;
  const ItemHandle runeHandle = service.getStoreMutable().create(runeProto);
  REQUIRE(runeHandle);

  // 2. 构造带孔武器并镶嵌符文
  ItemInstance weaponProto{};
  weaponProto.instanceId = 20001;
  weaponProto.baseId = 1001; // 锈蚀铁剑
  weaponProto.quantity = 1;
  weaponProto.itemLevel = 25;
  weaponProto.rarity = 4; // Legendary
  weaponProto.setLocked(true);
  weaponProto.socketCount = 3;
  weaponProto.sockets[0] = runeHandle;
  weaponProto.activeRunewordId = 777;
  weaponProto.attack = 45.5f;
  weaponProto.defense = 0.0f;
  weaponProto.value = 120.0f;
  weaponProto.affixCount = 3;
  weaponProto.affixes[0] = CompactAffix{AffixType::Strength, 3, true, 20.0f};
  weaponProto.affixes[1] = CompactAffix{AffixType::CritChance, 2, false, 8.5f};
  weaponProto.affixes[2] = CompactAffix{AffixType::FlatPhysicalDamage, 4, true, 15.0f};
  const ItemHandle weaponHandle = service.getStoreMutable().create(weaponProto);
  REQUIRE(weaponHandle);

  // 附加旁表数据给武器
  ItemSideTableData sideData;
  StatConversion conv;
  conv.source = StatType::PhysicalDamage;
  conv.target = StatType::FireDamage;
  conv.ratio = 0.5f;
  conv.required_tags = Tag::Melee;
  sideData.conversions.push_back(conv);

  DamageModifier mod;
  mod.source_tag = Tag::Physical;
  mod.target_tag = Tag::Fire;
  mod.value = 10.0f;
  mod.type = ModifierType::Flat;
  sideData.damage_modifiers.push_back(mod);

  ItemSkillModifier skillMod;
  skillMod.target_skill_id = 8;
  skillMod.flat_cooldown_delta = 1.5f;
  skillMod.mana_cost_delta = -10.0f;
  skillMod.extra_projectiles = 2;
  skillMod.area_radius_mult = 1.25f;
  skillMod.convert_from = Tag::Physical;
  skillMod.convert_to = Tag::Lightning;
  skillMod.conversion_ratio = 0.5f;
  skillMod.inject_ailment_id = 1001;
  skillMod.inject_ailment_chance = 0.35f;
  sideData.skill_modifiers.push_back(skillMod);

  service.getStoreMutable().setSideTable(weaponHandle, sideData);

  // 3. 构造装备与背包物品
  ItemInstance armorProto{};
  armorProto.instanceId = 30001;
  armorProto.baseId = 2011; // 破旧法袍
  armorProto.quantity = 1;
  armorProto.itemLevel = 15;
  armorProto.rarity = 2;
  armorProto.defense = 30.0f;
  const ItemHandle armorHandle = service.getStoreMutable().create(armorProto);

  ItemInstance bagProto{};
  bagProto.instanceId = 40001;
  bagProto.baseId = 5001; // 粗布背包
  bagProto.quantity = 1;
  const ItemHandle bagHandle = service.getStoreMutable().create(bagProto);

  // 4. 将物品放入各个容器
  // Inventory 槽位 0 和 5
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, weaponHandle));
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 5}, armorHandle));

  // Equipment MainHand (0) 和 Chest (3)
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0}, weaponHandle));
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0}, armorHandle));

  // BagSlots 槽位 0
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::BagSlots, 0, 0, 0}, bagHandle));

  // PersonalStash: 解锁 3 页，在第 0 页槽 10 与第 2 页槽 20 放置物品
  service.setUnlockedPages(ContainerKind::PersonalStash, 3);
  service.setPersonalStashMeta({
      StashTabMeta{"Weapon Tab", 1, 101, 0xFF0000FF},
      StashTabMeta{"Armor Tab", 2, 102, 0x00FF00FF},
      StashTabMeta{"Misc Tab", 0, 103, 0x0000FFFF}
  });
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 0, 10}, weaponHandle));
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, 2, 20}, armorHandle));

  // SharedStash: 解锁 2 页，在第 1 页槽 5 放置物品
  service.setUnlockedPages(ContainerKind::SharedStash, 2);
  service.setSharedStashMeta({
      StashTabMeta{"Shared General", 0, 201, 0xAA00AAFF},
      StashTabMeta{"Shared Rare", 1, 202, 0x00AAAAFF}
  });
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, 1, 5}, armorHandle));

  // HeirloomVault: 槽位 2
  CHECK(service.setSlotHandle(SlotRef{ContainerKind::HeirloomVault, 0, 0, 2}, weaponHandle));

  // 材料银行
  service.addMaterial(101, 50);
  service.addMaterial(102, 100);
  service.addMaterial(205, 5);

  // 金币与实例计数
  service.setGold(88888);
  service.setNextInstanceId(99999);

  // ProgressionData 字符串
  const std::string originalProgression = "{\"character_level\":50,\"astrolabe_nodes\":[1,2,7,12]}";

  // 5. 编码为二进制
  std::stringstream binaryStream(std::ios::in | std::ios::out | std::ios::binary);
  const bool encodeSuccess = ItemPersistenceCodec::encode(
      service, binaryStream, nullptr, ContainerDirtyFlags::All, originalProgression);
  REQUIRE(encodeSuccess);

  // 6. 解码到新的 ItemStorageService
  ItemStorageService restoredService;
  std::string restoredProgression;
  binaryStream.seekg(0, std::ios::beg);
  const bool decodeSuccess = ItemPersistenceCodec::decode(
      binaryStream, restoredService, &restoredProgression);
  REQUIRE(decodeSuccess);

  // 7. 深度断言
  CHECK(restoredProgression == originalProgression);
  CHECK(restoredService.getGold() == 88888);
  CHECK(restoredService.getNextInstanceId() == 99999);
  CHECK(restoredService.getUnlockedPages(ContainerKind::PersonalStash) == 3);
  // 仓库元数据断言
  const auto &pMetas = restoredService.getPersonalStashMeta();
  REQUIRE(pMetas.size() == 3);
  CHECK(pMetas[0].name == "Weapon Tab");
  CHECK(pMetas[0].type == 1);
  CHECK(pMetas[0].iconId == 101);
  CHECK(pMetas[0].color == 0xFF0000FF);
  CHECK(pMetas[1].name == "Armor Tab");
  CHECK(pMetas[2].name == "Misc Tab");

  const auto &sMetas = restoredService.getSharedStashMeta();
  REQUIRE(sMetas.size() == 2);
  CHECK(sMetas[0].name == "Shared General");
  CHECK(sMetas[0].iconId == 201);
  CHECK(sMetas[1].name == "Shared Rare");
  CHECK(sMetas[1].color == 0x00AAAAFF);

  // 材料断言
  CHECK(restoredService.getMaterialCount(101) == 50);
  CHECK(restoredService.getMaterialCount(102) == 100);
  CHECK(restoredService.getMaterialCount(205) == 5);

  // 槽位断言
  const ItemHandle restoredWeaponH =
      restoredService.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0});
  REQUIRE(restoredWeaponH);
  CHECK(restoredWeaponH.index == weaponHandle.index);
  CHECK(restoredWeaponH.gen == weaponHandle.gen);

  const ItemInstance *restoredWeapon =
      restoredService.getStore().get(restoredWeaponH);
  REQUIRE(restoredWeapon != nullptr);
  CHECK(restoredWeapon->instanceId == 20001);
  CHECK(restoredWeapon->baseId == 1001);
  CHECK(restoredWeapon->isLocked() == true);
  CHECK(restoredWeapon->socketCount == 3);
  CHECK(restoredWeapon->activeRunewordId == 777);
  CHECK(restoredWeapon->attack == doctest::Approx(45.5f));
  CHECK(restoredWeapon->affixCount == 3);
  CHECK(restoredWeapon->affixes[0].type == AffixType::Strength);
  CHECK(restoredWeapon->affixes[0].value == doctest::Approx(20.0f));
  CHECK(restoredWeapon->affixes[1].type == AffixType::CritChance);
  CHECK(restoredWeapon->affixes[2].type == AffixType::FlatPhysicalDamage);

  // 镶嵌符文断言
  const ItemHandle restoredRuneH = restoredWeapon->sockets[0];
  REQUIRE(restoredRuneH);
  CHECK(restoredRuneH.index == runeHandle.index);
  CHECK(restoredRuneH.gen == runeHandle.gen);
  const ItemInstance *restoredRune =
      restoredService.getStore().get(restoredRuneH);
  REQUIRE(restoredRune != nullptr);
  CHECK(restoredRune->instanceId == 10001);
  CHECK(restoredRune->baseId == 10001);

  // 旁表断言
  const auto *side = restoredService.getStore().getSideTable(restoredWeaponH);
  REQUIRE(side != nullptr);
  REQUIRE(side->conversions.size() == 1);
  CHECK(side->conversions[0].source == StatType::PhysicalDamage);
  CHECK(side->conversions[0].target == StatType::FireDamage);
  CHECK(side->conversions[0].ratio == doctest::Approx(0.5f));
  CHECK(side->conversions[0].required_tags == Tag::Melee);
  REQUIRE(side->damage_modifiers.size() == 1);
  CHECK(side->damage_modifiers[0].source_tag == Tag::Physical);
  CHECK(side->damage_modifiers[0].target_tag == Tag::Fire);
  CHECK(side->damage_modifiers[0].value == doctest::Approx(10.0f));
  CHECK(side->damage_modifiers[0].type == ModifierType::Flat);
  REQUIRE(side->skill_modifiers.size() == 1);
  CHECK(side->skill_modifiers[0].target_skill_id == 8);
  CHECK(side->skill_modifiers[0].flat_cooldown_delta == doctest::Approx(1.5f));
  CHECK(side->skill_modifiers[0].mana_cost_delta == doctest::Approx(-10.0f));
  CHECK(side->skill_modifiers[0].extra_projectiles == 2);
  CHECK(side->skill_modifiers[0].area_radius_mult == doctest::Approx(1.25f));
  CHECK(side->skill_modifiers[0].convert_from == Tag::Physical);
  CHECK(side->skill_modifiers[0].convert_to == Tag::Lightning);
  CHECK(side->skill_modifiers[0].conversion_ratio == doctest::Approx(0.5f));
  CHECK(side->skill_modifiers[0].inject_ailment_id == 1001);
  CHECK(side->skill_modifiers[0].inject_ailment_chance == doctest::Approx(0.35f));

  // 个人仓库各页断言
  const ItemHandle pStashH1 = restoredService.getSlotHandle(
      SlotRef{ContainerKind::PersonalStash, 0, 0, 10});
  CHECK(pStashH1 == weaponHandle);
  const ItemHandle pStashH2 = restoredService.getSlotHandle(
      SlotRef{ContainerKind::PersonalStash, 0, 2, 20});
  CHECK(pStashH2 == armorHandle);
  const ItemHandle pStashEmpty = restoredService.getSlotHandle(
      SlotRef{ContainerKind::PersonalStash, 0, 1, 0});
  CHECK(!pStashEmpty);

  // 共享仓库断言
  const ItemHandle sStashH = restoredService.getSlotHandle(
      SlotRef{ContainerKind::SharedStash, 0, 1, 5});
  CHECK(sStashH == armorHandle);

  // 扩展背包槽位断言
  const ItemHandle bagSlotH = restoredService.getSlotHandle(
      SlotRef{ContainerKind::BagSlots, 0, 0, 0});
  CHECK(bagSlotH == bagHandle);

  // 传家宝断言
  const ItemHandle vaultH = restoredService.getSlotHandle(
      SlotRef{ContainerKind::HeirloomVault, 0, 0, 2});
  CHECK(vaultH == weaponHandle);
}

TEST_CASE("[Unit] ItemPersistenceCodec - Corruption & Fault Injection") {
  TestSetupScope scope;
  ItemStorageService service;

  ItemInstance inst{};
  inst.instanceId = 999;
  inst.baseId = 1001;
  const ItemHandle h = service.getStoreMutable().create(inst);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, h);

  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  REQUIRE(ItemPersistenceCodec::encode(service, ss));
  const std::string validBytes = ss.str();
  REQUIRE(validBytes.size() > sizeof(FileHeader));

  SUBCASE("Truncated stream below FileHeader size") {
    std::string truncated = validBytes.substr(0, 8);
    std::stringstream in(truncated, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }

  SUBCASE("Corrupted Magic Number") {
    std::string corrupted = validBytes;
    corrupted[0] = 'X';
    corrupted[1] = 'Y';
    std::stringstream in(corrupted, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }

  SUBCASE("Unsupported Version Number") {
    std::string corrupted = validBytes;
    // version 位于 offset 4..7
    corrupted[4] = 0x7F;
    std::stringstream in(corrupted, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }

  SUBCASE("Corrupted Header CRC32") {
    std::string corrupted = validBytes;
    // headerCrc32 位于 offset 12..15
    corrupted[12] ^= 0xFF;
    std::stringstream in(corrupted, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }

  SUBCASE("Flipped Payload Byte Triggers CRC32 Failure") {
    std::string corrupted = validBytes;
    // 翻转文件末尾某个字节
    corrupted.back() ^= 0xA5;
    std::stringstream in(corrupted, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }

  SUBCASE("Truncated Payload Length") {
    std::string truncated = validBytes.substr(0, validBytes.size() - 10);
    std::stringstream in(truncated, std::ios::in | std::ios::binary);
    ItemStorageService dst;
    CHECK(ItemPersistenceCodec::decode(in, dst) == false);
  }
}

TEST_CASE("[Unit] ItemPersistenceCodec - Incremental Section Caching") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService service;

  ItemInstance sword{};
  sword.instanceId = 111;
  sword.baseId = 1001;
  const ItemHandle hSword = service.getStoreMutable().create(sword);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, hSword);

  ItemInstance armor{};
  armor.instanceId = 222;
  armor.baseId = 2011;
  const ItemHandle hArmor = service.getStoreMutable().create(armor);
  service.setSlotHandle(
      SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0},
      hArmor);

  InMemorySectionCache cache;

  // 1. 首次全量编码
  std::stringstream ss1(std::ios::in | std::ios::out | std::ios::binary);
  REQUIRE(ItemPersistenceCodec::encode(service, ss1, &cache, ContainerDirtyFlags::All));
  CHECK(cache.contains(SectionType::Inventory));
  CHECK(cache.contains(SectionType::Equipment));
  CHECK(cache.contains(SectionType::ItemInstances));

  const auto *cachedEquipBefore = cache.get(SectionType::Equipment);
  REQUIRE(cachedEquipBefore != nullptr);
  const uint32_t equipCrcBefore = cachedEquipBefore->crc32;

  // 2. 仅修改 Inventory 槽位（移动物品）
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}, hSword);
  service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}, ItemHandle{0, 0});

  // 增量编码，只标记 Inventory 为脏
  std::stringstream ss2(std::ios::in | std::ios::out | std::ios::binary);
  REQUIRE(ItemPersistenceCodec::encode(service, ss2, &cache, ContainerDirtyFlags::Inventory));

  // 验证 Equipment 缓存未改变
  const auto *cachedEquipAfter = cache.get(SectionType::Equipment);
  REQUIRE(cachedEquipAfter != nullptr);
  CHECK(cachedEquipAfter->crc32 == equipCrcBefore);

  // 3. 解码验证增量编码生成的文件是否完全合法
  ItemStorageService restored;
  ss2.seekg(0, std::ios::beg);
  REQUIRE(ItemPersistenceCodec::decode(ss2, restored));

  CHECK(restored.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 0}) == ItemHandle{0, 0});
  CHECK(restored.getSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, 1}) == hSword);
  CHECK(restored.getSlotHandle(
            SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::Chest), 0, 0}) ==
        hArmor);
}

TEST_CASE("[Unit] ItemPersistenceCodec - Pure ItemStore Fast Codec") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStore store;

  ItemInstance p1{};
  p1.instanceId = 501;
  p1.baseId = 1001;
  p1.attack = 33.0f;
  const ItemHandle h1 = store.create(p1);

  ItemInstance p2{};
  p2.instanceId = 502;
  p2.baseId = 2011;
  p2.defense = 44.0f;
  const ItemHandle h2 = store.create(p2);

  ItemSideTableData side;
  side.conversions.push_back(StatConversion{StatType::Strength, StatType::Vitality, 1.5f, Tag::None});
  store.setSideTable(h1, side);

  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  REQUIRE(ItemPersistenceCodec::encodeStore(store, ss));

  ItemStore restoredStore;
  ss.seekg(0, std::ios::beg);
  REQUIRE(ItemPersistenceCodec::decodeStore(ss, restoredStore));

  CHECK(restoredStore.activeCount() == 2);
  const ItemInstance *r1 = restoredStore.get(h1);
  REQUIRE(r1 != nullptr);
  CHECK(r1->instanceId == 501);
  CHECK(r1->attack == doctest::Approx(33.0f));

  const ItemInstance *r2 = restoredStore.get(h2);
  REQUIRE(r2 != nullptr);
  CHECK(r2->instanceId == 502);
  CHECK(r2->defense == doctest::Approx(44.0f));

  const auto *rSide = restoredStore.getSideTable(h1);
  REQUIRE(rSide != nullptr);
  REQUIRE(rSide->conversions.size() == 1);
  CHECK(rSide->conversions[0].source == StatType::Strength);
  CHECK(rSide->conversions[0].target == StatType::Vitality);
}

TEST_CASE("[Unit] ItemPersistenceCodec - SaveFileAtomic") {
  namespace fs = std::filesystem;
  const std::string testDir = "build/test_save_atomic";
  const std::string targetPath = testDir + "/test_slot.nmd";
  const std::string tempPath = testDir + "/temp_slot.nmd";
  const std::string backupPath = targetPath + ".bak";

  std::error_code ec;
  fs::create_directories(testDir, ec);

  // 1. 首次写入
  const std::vector<uint8_t> data1 = {'N', 'M', 'D', 'S', 0x01, 0x02, 0x03};
  CHECK(ItemPersistenceCodec::SaveFileAtomic(targetPath, tempPath, data1, true) == true);
  CHECK(fs::exists(targetPath));
  CHECK(fs::file_size(targetPath) == data1.size());

  // 2. 第二次覆盖写入并创建备份
  const std::vector<uint8_t> data2 = {'N', 'M', 'D', 'S', 0x0A, 0x0B, 0x0C, 0x0D};
  CHECK(ItemPersistenceCodec::SaveFileAtomic(targetPath, tempPath, data2, true) == true);
  CHECK(fs::exists(targetPath));
  CHECK(fs::file_size(targetPath) == data2.size());
  CHECK(fs::exists(backupPath));
  CHECK(fs::file_size(backupPath) == data1.size());

  // 清理测试产物
  fs::remove_all(testDir, ec);
}

TEST_CASE("[Unit] ItemPersistenceCodec - MaterialBank Dedicated Round-Trip (H5)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService srcService;
  srcService.addMaterial(1, 999);
  srcService.addMaterial(101, 50);
  srcService.addMaterial(5002, 10);
  srcService.addMaterial(99999, 1);

  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  REQUIRE(ItemPersistenceCodec::encode(srcService, ss, nullptr, ContainerDirtyFlags::MaterialBank));

  ItemStorageService dstService;
  REQUIRE(ItemPersistenceCodec::decode(ss, dstService));

  CHECK(dstService.getMaterialCount(1) == 999);
  CHECK(dstService.getMaterialCount(101) == 50);
  CHECK(dstService.getMaterialCount(5002) == 10);
  CHECK(dstService.getMaterialCount(99999) == 1);
  CHECK(dstService.getMaterialCount(999) == 0);
}
