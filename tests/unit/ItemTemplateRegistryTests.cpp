#include "TestCommon.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/ItemEquipValidationService.hpp"
#include "game/systems/item/LootFilter.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] ItemTemplateRegistry - Defaults, Registration, Lookup") {
  TestSetupScope scope;

  auto &registry = ItemTemplateRegistry::Instance();
  registry.initializeDefaults();

  SUBCASE("Registry contains expected baseline counts") {
    CHECK(registry.count() >= 50);
  }

  SUBCASE("Find by baseId returns correct template for weapons") {
    const auto *sword = registry.find(1001);
    REQUIRE(sword != nullptr);
    CHECK(sword->baseId == 1001);
    CHECK(sword->name == "锈蚀铁剑");
    CHECK(sword->kind == ItemKind::Weapon);
    CHECK(sword->type == ItemType::Weapon);
    CHECK(sword->slot == EquipmentSlot::MainHand);
    CHECK(sword->weaponSubtype == WeaponSubtype::Sword);
    CHECK(sword->isTwoHanded == false);
    CHECK(sword->minLevel == 1);
    CHECK(sword->baseStatMin == doctest::Approx(5.0f));
    CHECK(sword->baseStatMax == doctest::Approx(8.0f));

    const auto *greatsword = registry.find(1041);
    REQUIRE(greatsword != nullptr);
    CHECK(greatsword->baseId == 1041);
    CHECK(greatsword->name == "训练大剑");
    CHECK(greatsword->isTwoHanded == true);
    CHECK(greatsword->weaponSubtype == WeaponSubtype::Greatsword);
  }

  SUBCASE("Find by baseId returns correct template for armors & jewelry") {
    const auto *chest = registry.find(2011);
    REQUIRE(chest != nullptr);
    CHECK(chest->baseId == 2011);
    CHECK(chest->name == "破旧法袍");
    CHECK(chest->kind == ItemKind::Armor);
    CHECK(chest->type == ItemType::Armor);
    CHECK(chest->slot == EquipmentSlot::Chest);

    const auto *shield = registry.find(2061);
    REQUIRE(shield != nullptr);
    CHECK(shield->baseId == 2061);
    CHECK(shield->name == "圆盾");
    CHECK(shield->kind == ItemKind::Shield);
    CHECK(shield->type == ItemType::Shield);
    CHECK(shield->slot == EquipmentSlot::OffHand);

    const auto *neck = registry.find(3001);
    REQUIRE(neck != nullptr);
    CHECK(neck->baseId == 3001);
    CHECK(neck->name == "铜项链");
    CHECK(neck->kind == ItemKind::Jewelry);
    CHECK(neck->type == ItemType::Jewelry);
    CHECK(neck->slot == EquipmentSlot::Neck);

    const auto *ring = registry.find(3011);
    REQUIRE(ring != nullptr);
    CHECK(ring->baseId == 3011);
    CHECK(ring->name == "铁戒指");
    CHECK(ring->slot == EquipmentSlot::Ring);
  }

  SUBCASE("Find by baseId returns correct template for consumables, bags and catalysts") {
    const auto *hpPot = registry.find(101);
    REQUIRE(hpPot != nullptr);
    CHECK(hpPot->baseId == 101);
    CHECK(hpPot->kind == ItemKind::Consumable);
    CHECK(hpPot->type == ItemType::Consumable);
    CHECK(hpPot->maxStack == 99);

    const auto *bag = registry.find(5001);
    REQUIRE(bag != nullptr);
    CHECK(bag->baseId == 5001);
    CHECK(bag->kind == ItemKind::Bag);
    CHECK(bag->type == ItemType::Bag);
    CHECK(bag->bagCapacity == 56);

    const auto *catalyst = registry.find(10001);
    REQUIRE(catalyst != nullptr);
    CHECK(catalyst->baseId == 10001);
    CHECK(catalyst->kind == ItemKind::Consumable);
    CHECK(catalyst->type == ItemType::Consumable);
    CHECK(catalyst->catalystKind == CatalystKind::LegendaryCore);
  }

  SUBCASE("Lookup missing ID returns nullptr") {
    CHECK(registry.find(0) == nullptr);
    CHECK(registry.find(999999) == nullptr);
  }

  SUBCASE("Lookup by name") {
    const auto *found = registry.findByName("精铁长剑");
    REQUIRE(found != nullptr);
    CHECK(found->baseId == 1002);

    CHECK(registry.findByName("NonExistentItemNameXYZ") == nullptr);
  }

  SUBCASE("Query filters by subtype, slot, kind and type") {
    auto swords = registry.getTemplatesByWeaponSubtype(WeaponSubtype::Sword);
    CHECK(swords.size() == 6);

    auto helmets = registry.getTemplatesBySlot(EquipmentSlot::Head);
    CHECK(helmets.size() == 5);

    auto weapons = registry.getTemplatesByKind(ItemKind::Weapon);
    CHECK(weapons.size() == 42); // 7 subtypes * 6 tiers

    auto bags = registry.getTemplatesByType(ItemType::Bag);
    CHECK(bags.size() == 2);
  }

  SUBCASE("Custom template registration") {
    ItemTemplate custom;
    custom.baseId = 88888;
    custom.name = "TestCustomAxe";
    custom.kind = ItemKind::Weapon;
    custom.type = ItemType::Weapon;
    custom.slot = EquipmentSlot::MainHand;
    custom.weaponSubtype = WeaponSubtype::Axe;
    custom.minLevel = 50;
    custom.baseStatMin = 80.0f;
    custom.baseStatMax = 120.0f;

    registry.registerTemplate(custom);

    const auto *retrieved = registry.find(88888);
    REQUIRE(retrieved != nullptr);
    CHECK(retrieved->name == "TestCustomAxe");
    CHECK(registry.findByName("TestCustomAxe") == retrieved);
  }
}

TEST_CASE("[Unit] ItemFactory & Template Integration") {
  TestSetupScope scope;
  ItemFactory::initialize();

  entt::registry ecs;

  SUBCASE("ItemFactory::createWeapon populates baseId and template properties") {
    auto entity = ItemFactory::createWeapon(ecs, 25, Rarity::Common);
    REQUIRE(ecs.valid(entity));
    const auto &item = ecs.get<ItemComponent>(entity);
    CHECK(item.baseId >= 1001);
    CHECK(item.baseId <= 1066);
    CHECK(!item.name.empty());
    CHECK(item.type == ItemType::Weapon);
    CHECK(item.attack > 0.0f);

    auto range = ItemFactory::getBaseStatRange(item);
    CHECK(range.first > 0.0f);
    CHECK(range.second >= range.first);
  }

  SUBCASE("ItemFactory::createArmor populates baseId and template properties") {
    auto entity = ItemFactory::createArmor(ecs, 25, Rarity::Common, EquipmentSlot::Chest);
    REQUIRE(ecs.valid(entity));
    const auto &item = ecs.get<ItemComponent>(entity);
    CHECK(item.baseId >= 2011);
    CHECK(item.baseId <= 2015);
    CHECK(item.slot == EquipmentSlot::Chest);
    CHECK(item.defense > 0.0f);

    auto range = ItemFactory::getBaseStatRange(item);
    CHECK(range.first > 0.0f);
    CHECK(range.second >= range.first);
  }

  SUBCASE("ItemFactory::createBag populates baseId") {
    auto entity = ItemFactory::createBag(ecs, 1, Rarity::Common);
    REQUIRE(ecs.valid(entity));
    const auto &item = ecs.get<ItemComponent>(entity);
    CHECK(item.baseId == 5001);
    CHECK(item.bagCapacity == 56);
  }

  SUBCASE("ItemFactory::createPotion populates baseId") {
    auto hpEntity = ItemFactory::createPotion(ecs, 0, 5);
    REQUIRE(ecs.valid(hpEntity));
    const auto &hpItem = ecs.get<ItemComponent>(hpEntity);
    CHECK(hpItem.baseId == 101);
    CHECK(hpItem.quantity == 5);
    CHECK(hpItem.maxStack == 99);

    auto manaEntity = ItemFactory::createPotion(ecs, 1, 3);
    REQUIRE(ecs.valid(manaEntity));
    const auto &manaItem = ecs.get<ItemComponent>(manaEntity);
    CHECK(manaItem.baseId == 102);
    CHECK(manaItem.quantity == 3);
  }

  SUBCASE("ItemFactory::createMaterial for LegendaryCore populates catalystKind and baseId") {
    auto entity = ItemFactory::createMaterial(ecs, 10001, 1);
    REQUIRE(ecs.valid(entity));
    const auto &item = ecs.get<ItemComponent>(entity);
    CHECK(item.baseId == 10001);
    CHECK(item.catalystKind == CatalystKind::LegendaryCore);
    CHECK(item.type == ItemType::Consumable);
  }

  SUBCASE("ItemFactory::serializeItem and restoreItem round-trip with template validation") {
    auto original = ItemFactory::createWeapon(ecs, 30, Rarity::Rare);
    const auto &origItem = ecs.get<ItemComponent>(original);
    uint32_t expectedBaseId = origItem.baseId;

    auto dto = ItemFactory::serializeItem(ecs, original);
    CHECK(dto.baseId == expectedBaseId);

    // Wipe dynamic name in DTO to test template fallback restoration
    dto.name.clear();

    auto restored = ItemFactory::restoreItem(ecs, dto);
    REQUIRE(ecs.valid(restored));
    const auto &restItem = ecs.get<ItemComponent>(restored);
    CHECK(restItem.baseId == expectedBaseId);
    CHECK(!restItem.name.empty()); // Restored from template
  }
}

TEST_CASE("[Unit] ItemEquipValidationService & LootFilter with Template Registry") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  SUBCASE("ItemEquipValidationService resolves slot from baseId if item.slot is None") {
    EquipmentComponent equipment;
    ItemComponent item;
    item.slot = EquipmentSlot::None;
    item.baseId = 2011; // 破旧法袍 -> Chest

    auto result = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, item, EquipmentSlot::Chest, false);
    CHECK(result.canEquip == true);
    CHECK(result.resolvedSlot == EquipmentSlot::Chest);
  }

  SUBCASE("LootFilter matches item using baseId template resolution") {
    FilterRule rule;
    rule.enabled = true;
    rule.condition.baseName = "锈蚀铁剑";
    rule.action.type = FilterActionType::SHOW;

    ItemComponent item;
    item.baseId = 1001;
    item.name = ""; // Missing name, will resolve from template baseId 1001

    CHECK(rule.matches(item, 1) == true);
  }
}
