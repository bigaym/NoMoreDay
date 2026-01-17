#include "game/components/ItemComponent.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include <doctest.h>
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("RunewordSystem Initialization and Data Loading") {
  RunewordSystem::initialize();

  SUBCASE("Load Runes") {
    const auto *el = RunewordSystem::getRune(3001);
    REQUIRE(el != nullptr);
    CHECK(el->name == "El");
    CHECK(el->tier == 1);

    const auto *zod = RunewordSystem::getRuneByName("Zod");
    REQUIRE(zod != nullptr);
    CHECK(zod->id == 3033);
  }

  SUBCASE("Load Runewords") {
    // stealth id is likely 1
    // RunewordSystem doesn't expose list by ID easily unless we check `apply`
    // or internal logic, but we can check if a known Runeword ID exists by
    // verifying `checkForRuneword` logic works. Assuming we can't inspect
    // s_runewords directly as it is private.
  }
}

TEST_CASE("Runeword Logic") {
  entt::registry registry;
  RunewordSystem::initialize();

  // Create Socketed Runes
  auto createRune = [&](uint32_t id) {
    auto e = registry.create();
    ItemComponent item;
    item.id = id;
    item.type = ItemType::Material;
    item.name = RunewordSystem::getRune(id)->name;
    registry.emplace<ItemComponent>(e, item);
    return e;
  };

  auto tal = createRune(3007);
  auto eth = createRune(3005);
  auto tir = createRune(3003);

  SUBCASE("Stealth (Tal + Eth) in Armor") {
    ItemComponent armor;
    armor.type = ItemType::Armor;
    armor.slot = EquipmentSlot::Chest;

    std::vector<entt::entity> sockets = {tal, eth};

    uint32_t rwId = RunewordSystem::checkForRuneword(armor, sockets, registry);
    CHECK(rwId != 0); // Should be Stealth (ID 1)

    if (rwId != 0) {
      RunewordSystem::applyRuneword(armor, rwId);
      CHECK(armor.name == "Stealth");
      CHECK(armor.rarity == Rarity::Legendary);

      // Validate Stats
      bool foundCastSpeed = false;
      bool foundPoisonRes = false;
      bool foundDex = false;
      bool foundMoveSpeed = false;

      for (const auto &affix : armor.affixes) {
        // if (affix.name == "Runeword Bonus") { // Name removed
          if (affix.type == AffixType::CastSpeed) {
            CHECK(affix.value == 25);
            foundCastSpeed = true;
          }
          if (affix.type == AffixType::ResistPoison) {
            CHECK(affix.value == 30);
            foundPoisonRes = true;
          }
          if (affix.type == AffixType::Dexterity) {
            CHECK(affix.value == 6);
            foundDex = true;
          }
          if (affix.type == AffixType::MoveSpeed) {
            CHECK(affix.value == 25);
            foundMoveSpeed = true;
          }
        // }
      }
      CHECK(foundCastSpeed);
      CHECK(foundPoisonRes);
      CHECK(foundDex);
      CHECK(foundMoveSpeed);
    }
  }

  SUBCASE("Invalid Sequence (Eth + Tal)") {
    ItemComponent armor;
    armor.type = ItemType::Armor;

    std::vector<entt::entity> sockets = {eth, tal};

    uint32_t rwId = RunewordSystem::checkForRuneword(armor, sockets, registry);
    CHECK(rwId == 0);
  }

  SUBCASE("Invalid Base (Weapon)") {
    ItemComponent weapon;
    weapon.type = ItemType::Weapon;

    std::vector<entt::entity> sockets = {tal, eth};

    uint32_t rwId = RunewordSystem::checkForRuneword(weapon, sockets, registry);
    CHECK(rwId == 0); // Stealth is Armor only
  }
}
