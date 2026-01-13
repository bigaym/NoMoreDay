// tests/TestEternalNightmare.cpp
// 无尽梦魇系统单元测试
#include "TestCommon.hpp"
#include "game/systems/progression/LeaderboardSystem.hpp"
#include "game/systems/world/CorruptionSystem.hpp"
#include <filesystem>


namespace NoMoreDay {

TEST_CASE("Corruption System - Floor Progression") {
  LoggerScope scope;
  auto &corruption = CorruptionSystem::Get();
  corruption.reset();

  SUBCASE("Initial state") {
    CHECK(corruption.getCorruption() == 0);
    CHECK(corruption.getCurrentFloor() == 0);
  }

  SUBCASE("Advancing floors increases corruption") {
    corruption.advanceFloor();
    CHECK(corruption.getCurrentFloor() == 1);
    CHECK(corruption.getCorruption() == CorruptionSystem::kCorruptionPerFloor);

    corruption.advanceFloor();
    CHECK(corruption.getCurrentFloor() == 2);
    CHECK(corruption.getCorruption() ==
          CorruptionSystem::kCorruptionPerFloor * 2);
  }

  SUBCASE("Boss floor detection") {
    for (int i = 0; i < 10; ++i) {
      corruption.advanceFloor();
    }
    CHECK(corruption.getCurrentFloor() == 10);
    CHECK(corruption.isBossFloor() == true);

    corruption.advanceFloor();
    CHECK(corruption.getCurrentFloor() == 11);
    CHECK(corruption.isBossFloor() == false);
  }

  corruption.reset();
}

TEST_CASE("Corruption System - Stat Multipliers") {
  LoggerScope scope;
  auto &corruption = CorruptionSystem::Get();
  corruption.reset();

  SUBCASE("Base multiplier at floor 0") {
    float mult = corruption.calculateStatMultiplier();
    CHECK(mult == doctest::Approx(1.0f));
  }

  SUBCASE("Multiplier increases with floors") {
    for (int i = 0; i < 10; ++i) {
      corruption.advanceFloor();
    }

    // Floor 10, Corruption 50
    // tierMult = 1.08^10 ≈ 2.159
    // corruptionMult = 1 + 50/100 = 1.5
    // Total ≈ 3.24
    float mult = corruption.calculateStatMultiplier();
    CHECK(mult > 3.0f);
    CHECK(mult < 3.5f);
  }

  SUBCASE("Health multiplier grows faster") {
    for (int i = 0; i < 10; ++i) {
      corruption.advanceFloor();
    }

    float statMult = corruption.calculateStatMultiplier();
    float hpMult = corruption.calculateHealthMultiplier();

    CHECK(hpMult > statMult);
  }

  corruption.reset();
}

TEST_CASE("Corruption System - Loot Bonuses") {
  LoggerScope scope;
  auto &corruption = CorruptionSystem::Get();
  corruption.reset();

  SUBCASE("T7 double chance scales with corruption") {
    CHECK(corruption.calculateDoubleT7Chance() == doctest::Approx(0.0f));

    corruption.addCorruption(100);
    float chance = corruption.calculateDoubleT7Chance();
    CHECK(chance > 0.0f);
    CHECK(chance < 0.2f); // Should be around 10%
  }

  SUBCASE("Drop rate bonus scales with corruption") {
    CHECK(corruption.calculateDropRateBonus() == doctest::Approx(0.0f));

    corruption.addCorruption(100);
    float bonus = corruption.calculateDropRateBonus();
    CHECK(bonus > 0.0f);
  }

  corruption.reset();
}

TEST_CASE("Leaderboard System - Entry Management") {
  LoggerScope scope;
  auto &leaderboard = LeaderboardSystem::Get();
  leaderboard.clear();

  SUBCASE("Adding entries") {
    LeaderboardEntry entry1{
        .player_name = "Player1", .highest_floor = 50, .peak_dps = 10000.0f};
    LeaderboardEntry entry2{
        .player_name = "Player2", .highest_floor = 100, .peak_dps = 5000.0f};

    leaderboard.addEntry(entry1);
    leaderboard.addEntry(entry2);

    CHECK(leaderboard.size() == 2);
  }

  SUBCASE("Sorting by floor") {
    LeaderboardEntry entry1{.player_name = "Low", .highest_floor = 10};
    LeaderboardEntry entry2{.player_name = "High", .highest_floor = 100};
    LeaderboardEntry entry3{.player_name = "Mid", .highest_floor = 50};

    leaderboard.addEntry(entry1);
    leaderboard.addEntry(entry2);
    leaderboard.addEntry(entry3);

    auto top = leaderboard.getTopByFloor(3);
    REQUIRE(top.size() == 3);
    CHECK(top[0].highest_floor == 100);
    CHECK(top[1].highest_floor == 50);
    CHECK(top[2].highest_floor == 10);
  }

  SUBCASE("Sorting by DPS") {
    LeaderboardEntry entry1{
        .player_name = "LowDPS", .highest_floor = 100, .peak_dps = 1000.0f};
    LeaderboardEntry entry2{
        .player_name = "HighDPS", .highest_floor = 50, .peak_dps = 50000.0f};

    leaderboard.addEntry(entry1);
    leaderboard.addEntry(entry2);

    auto top = leaderboard.getTopByDPS(2);
    REQUIRE(top.size() == 2);
    CHECK(top[0].peak_dps == doctest::Approx(50000.0f));
  }

  leaderboard.clear();
}

TEST_CASE("Leaderboard System - Persistence") {
  LoggerScope scope;
  auto &leaderboard = LeaderboardSystem::Get();
  leaderboard.clear();

  const std::string testPath = "test_leaderboard.json";

  // Cleanup
  if (std::filesystem::exists(testPath)) {
    std::filesystem::remove(testPath);
  }

  SUBCASE("Save and load") {
    LeaderboardEntry entry{.player_name = "TestPlayer",
                           .highest_floor = 75,
                           .peak_dps = 25000.0f,
                           .corruption_reached = 100};

    leaderboard.addEntry(entry);
    bool saved = leaderboard.save(testPath);
    CHECK(saved == true);
    CHECK(std::filesystem::exists(testPath));

    // Clear and reload
    size_t countBefore = leaderboard.size();
    leaderboard.clear();
    CHECK(leaderboard.size() == 0);

    bool loaded = leaderboard.load(testPath);
    CHECK(loaded == true);
    CHECK(leaderboard.size() == countBefore);

    auto entries = leaderboard.getAllEntries();
    REQUIRE(!entries.empty());
    CHECK(entries[0].player_name == "TestPlayer");
    CHECK(entries[0].highest_floor == 75);
  }

  // Cleanup
  if (std::filesystem::exists(testPath)) {
    std::filesystem::remove(testPath);
  }
  leaderboard.clear();
}

TEST_CASE("Leaderboard System - Record Tracking") {
  LoggerScope scope;
  auto &leaderboard = LeaderboardSystem::Get();
  leaderboard.clear();

  LeaderboardEntry e1{.highest_floor = 50, .peak_dps = 10000.0f};
  LeaderboardEntry e2{.highest_floor = 100, .peak_dps = 5000.0f};
  LeaderboardEntry e3{.highest_floor = 75, .peak_dps = 30000.0f};

  leaderboard.addEntry(e1);
  leaderboard.addEntry(e2);
  leaderboard.addEntry(e3);

  CHECK(leaderboard.getHighestFloorRecord() == 100);
  CHECK(leaderboard.getHighestDPSRecord() == doctest::Approx(30000.0f));

  leaderboard.clear();
}

} // namespace NoMoreDay
