#include "TestCommon.hpp"

#include "game/systems/item/LootFilter.hpp"

#include <filesystem>
#include <fstream>

namespace NoMoreDay {

TEST_CASE("[Unit] LootFilter - Rule matching honors condition boundaries") {
  TestSetupScope scope;

  FilterRule rule;
  rule.enabled = true;
  rule.condition.minRarity = Rarity::Rare;
  rule.condition.maxRarity = Rarity::Legendary;
  rule.condition.minLevel = 10;
  rule.condition.maxLevel = 20;
  rule.condition.itemType = ItemType::Weapon;
  rule.condition.baseName = "Blade";

  ItemComponent item;
  item.name = "Sun Blade";
  item.type = ItemType::Weapon;
  item.rarity = Rarity::Epic;

  CHECK(rule.matches(item, 15));
  CHECK_FALSE(rule.matches(item, 9));
  CHECK_FALSE(rule.matches(item, 21));

  item.rarity = Rarity::Magic;
  CHECK_FALSE(rule.matches(item, 15));

  item.rarity = Rarity::Epic;
  item.name = "Sun Axe";
  CHECK_FALSE(rule.matches(item, 15));
}

TEST_CASE("[Unit] LootFilter - Evaluate uses first enabled matching rule") {
  TestSetupScope scope;

  const std::filesystem::path tempPath =
      std::filesystem::temp_directory_path() / "nmd_loot_filter_test.json";

  const char *jsonText = R"JSON(
{
  "name": "unit-test-profile",
  "description": "loot filter unit test",
  "rules": [
    {
      "name": "disabled-hide-weapons",
      "enabled": false,
      "conditions": { "item_type": "weapon" },
      "action": "HIDE"
    },
    {
      "name": "emphasize-weapons",
      "enabled": true,
      "conditions": { "item_type": "weapon" },
      "action": "EMPHASIZE",
      "color": [10, 20, 30, 200],
      "scale": 1.25,
      "minimap_icon": true
    }
  ]
}
)JSON";

  {
    std::ofstream out(tempPath);
    REQUIRE(out.is_open());
    out << jsonText;
  }

  LootFilter::load(tempPath.string());

  ItemComponent weapon;
  weapon.type = ItemType::Weapon;
  auto weaponResult = LootFilter::evaluate(weapon, 1);
  CHECK(weaponResult.type == FilterActionType::EMPHASIZE);
  REQUIRE(weaponResult.colorOverride.has_value());
  CHECK(weaponResult.colorOverride->r == 10);
  CHECK(weaponResult.colorOverride->g == 20);
  CHECK(weaponResult.colorOverride->b == 30);
  CHECK(weaponResult.colorOverride->a == 200);
  CHECK(weaponResult.scale == doctest::Approx(1.25f));
  CHECK(weaponResult.minimapIcon);

  ItemComponent armor;
  armor.type = ItemType::Armor;
  auto armorResult = LootFilter::evaluate(armor, 1);
  CHECK(armorResult.type == FilterActionType::SHOW);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

} // namespace NoMoreDay
