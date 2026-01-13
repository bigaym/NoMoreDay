// tests/TestHeirloomSystem.cpp
// 传家宝系统单元测试
#include "TestCommon.hpp"
#include "game/components/HeirloomComponent.hpp"
#include "game/systems/item/HeirloomScaling.hpp"
#include "game/systems/item/HeirloomVault.hpp"
#include <filesystem>

namespace NoMoreDay {

TEST_CASE("Heirloom Scaling Calculations") {
  LoggerScope scope;

  SUBCASE("Full scale at required level") {
    float scale = HeirloomScaling::calculateScalingFactor(100, 100);
    CHECK(scale == doctest::Approx(1.0f));
  }

  SUBCASE("Full scale when player level exceeds requirement") {
    float scale = HeirloomScaling::calculateScalingFactor(120, 100);
    CHECK(scale == doctest::Approx(1.0f));
  }

  SUBCASE("Minimum scale at level 1 with high requirement") {
    float scale = HeirloomScaling::calculateScalingFactor(1, 100);
    // Should be close to min scale (0.15) but not exactly due to power curve
    CHECK(scale >= HeirloomScaling::kMinScale);
    CHECK(scale < 0.20f); // Should be very low
  }

  SUBCASE("Mid-range scaling") {
    float scale = HeirloomScaling::calculateScalingFactor(50, 100);
    // At 50% of required level with 0.8 exponent:
    // ratio = 0.5, scaled_ratio = 0.5^0.8 ≈ 0.574
    // result = 0.15 + 0.85 * 0.574 ≈ 0.638
    CHECK(scale > 0.5f);
    CHECK(scale < 0.75f);
  }

  SUBCASE("Zero level requirement returns full scale") {
    float scale = HeirloomScaling::calculateScalingFactor(10, 0);
    CHECK(scale == doctest::Approx(1.0f));
  }

  SUBCASE("Integer scaling preserves minimum of 1") {
    int result = HeirloomScaling::applyScalingInt(10, 0.05f);
    CHECK(result >= 1);
  }

  SUBCASE("Float scaling works correctly") {
    float result = HeirloomScaling::applyScalingFloat(100.0f, 0.5f);
    CHECK(result == doctest::Approx(50.0f));
  }

  SUBCASE("Power percent calculation") {
    float percent = HeirloomScaling::calculateEffectivePowerPercent(100, 100);
    CHECK(percent == doctest::Approx(100.0f));

    float lowPercent = HeirloomScaling::calculateEffectivePowerPercent(1, 100);
    CHECK(lowPercent >= 15.0f); // At least min scale
    CHECK(lowPercent < 25.0f);  // But should be low
  }
}

TEST_CASE("Heirloom Component") {
  LoggerScope scope;
  entt::registry registry;

  auto item = registry.create();
  auto &heirloom = registry.emplace<HeirloomComponent>(item);

  SUBCASE("Default initialization") {
    CHECK(heirloom.tier == 1);
    CHECK(heirloom.original_level_requirement == 1);
    CHECK(heirloom.is_active_this_run == false);
    CHECK(heirloom.display_name.empty());
  }

  SUBCASE("Can modify properties") {
    heirloom.tier = 3;
    heirloom.original_level_requirement = 80;
    heirloom.display_name = "Ancient Blade of Kings";
    heirloom.original_rarity = static_cast<uint8_t>(Rarity::Mythic);

    CHECK(registry.get<HeirloomComponent>(item).tier == 3);
    CHECK(registry.get<HeirloomComponent>(item).display_name ==
          "Ancient Blade of Kings");
  }
}

TEST_CASE("Heirloom Vault Persistence") {
  LoggerScope scope;

  // Use a temporary path for testing
  const std::string testPath = "test_heirloom_vault.json";

  // Cleanup before test
  if (std::filesystem::exists(testPath)) {
    std::filesystem::remove(testPath);
  }

  auto &vault = HeirloomVault::Get();

  SUBCASE("Load empty vault succeeds") {
    bool result = vault.load(testPath);
    CHECK(result == true);
    CHECK(vault.size() == 0);
  }

  SUBCASE("Add and save heirloom") {
    // Clear any existing data by loading fresh
    vault.load(testPath);

    ItemComponent item;
    item.name = "Test Sword";
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.attack = 150.0f;
    item.rarity = Rarity::Legendary;

    bool added = vault.addHeirloom(item, 50, Rarity::Legendary);
    CHECK(added == true);
    CHECK(vault.size() >= 1);

    // Save
    bool saved = vault.save(testPath);
    CHECK(saved == true);
    CHECK(std::filesystem::exists(testPath));

    // Verify file was created
    std::ifstream file(testPath);
    CHECK(file.good());
  }

  SUBCASE("Load saved heirloom") {
    // First add and save
    vault.load(testPath);

    if (vault.size() == 0) {
      ItemComponent item;
      item.name = "Persistent Blade";
      item.attack = 200.0f;
      vault.addHeirloom(item, 75, Rarity::Mythic);
      vault.save(testPath);
    }

    // Clear and reload
    size_t countBefore = vault.size();
    vault.load(testPath);

    CHECK(vault.size() == countBefore);

    const auto *loaded = vault.getHeirloom(0);
    CHECK(loaded != nullptr);
    if (loaded) {
      CHECK(loaded->item.attack > 0);
    }
  }

  SUBCASE("Vault max capacity") {
    vault.load(testPath);

    // Fill vault to capacity
    while (!vault.isFull()) {
      ItemComponent item;
      item.name = "Filler Item";
      vault.addHeirloom(item, 1, Rarity::Common);
    }

    CHECK(vault.isFull() == true);
    CHECK(vault.size() == HeirloomVault::kMaxHeirlooms);

    // Try to add one more
    ItemComponent extra;
    extra.name = "Overflow Item";
    bool added = vault.addHeirloom(extra, 1, Rarity::Common);
    CHECK(added == false);
  }

  // Cleanup after all tests
  if (std::filesystem::exists(testPath)) {
    std::filesystem::remove(testPath);
  }
}

} // namespace NoMoreDay
