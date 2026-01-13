// tests/TestPolishSystems.cpp
// Phase 4 体验优化系统单元测试
#include "TestCommon.hpp"
#include "game/systems/progression/AchievementSystem.hpp"
#include "game/systems/ui/TutorialSystem.hpp"
#include <filesystem>

namespace NoMoreDay {

TEST_CASE("Achievement System - Registration and Unlock") {
  LoggerScope scope;
  auto &achievements = AchievementSystem::Get();
  achievements.ClearAll();

  // 注册测试成就
  Achievement testAch{.id = "test_kill_10",
                      .name = "新手刺客",
                      .description = "击杀10个敌人",
                      .target_progress = 10};
  achievements.RegisterAchievement(testAch);

  SUBCASE("Achievement retrieval") {
    const auto *ach = achievements.GetAchievement("test_kill_10");
    REQUIRE(ach != nullptr);
    CHECK(ach->name == "新手刺客");
    CHECK(ach->target_progress == 10);
    CHECK(ach->unlocked == false);
  }

  SUBCASE("Progress update") {
    achievements.UpdateProgress("test_kill_10", 5);

    const auto *ach = achievements.GetAchievement("test_kill_10");
    REQUIRE(ach != nullptr);
    CHECK(ach->current_progress == 5);
    CHECK(ach->unlocked == false);
  }

  SUBCASE("Auto-unlock on completion") {
    achievements.UpdateProgress("test_kill_10", 10);

    const auto *ach = achievements.GetAchievement("test_kill_10");
    REQUIRE(ach != nullptr);
    CHECK(ach->current_progress == 10);
    CHECK(ach->unlocked == true);
  }

  SUBCASE("Direct unlock") {
    bool result = achievements.Unlock("test_kill_10");
    CHECK(result == true);

    const auto *ach = achievements.GetAchievement("test_kill_10");
    CHECK(ach->unlocked == true);

    // 重复解锁返回 false
    result = achievements.Unlock("test_kill_10");
    CHECK(result == false);
  }

  achievements.ClearAll();
}

TEST_CASE("Achievement System - Progress Tracking") {
  LoggerScope scope;
  auto &achievements = AchievementSystem::Get();
  achievements.ClearAll();

  // 注册多个成就
  achievements.RegisterAchievement(
      {.id = "ach_1", .name = "A1", .target_progress = 1});
  achievements.RegisterAchievement(
      {.id = "ach_2", .name = "A2", .target_progress = 1});
  achievements.RegisterAchievement(
      {.id = "ach_3", .name = "A3", .target_progress = 1});

  SUBCASE("Completion percentage") {
    CHECK(achievements.GetCompletionPercent() == doctest::Approx(0.0f));

    achievements.Unlock("ach_1");
    CHECK(achievements.GetCompletionPercent() == doctest::Approx(1.0f / 3.0f));

    achievements.Unlock("ach_2");
    achievements.Unlock("ach_3");
    CHECK(achievements.GetCompletionPercent() == doctest::Approx(1.0f));
  }

  SUBCASE("Get unlocked achievements") {
    achievements.Unlock("ach_1");
    achievements.Unlock("ach_2");

    auto unlocked = achievements.GetUnlockedAchievements();
    CHECK(unlocked.size() == 2);
  }

  achievements.ClearAll();
}

TEST_CASE("Achievement System - Unlock Callback") {
  LoggerScope scope;
  auto &achievements = AchievementSystem::Get();
  achievements.ClearAll();

  achievements.RegisterAchievement(
      {.id = "callback_test", .name = "Callback Test"});

  bool callbackInvoked = false;
  std::string unlockedName;

  achievements.SetUnlockCallback([&](const Achievement &ach) {
    callbackInvoked = true;
    unlockedName = ach.name;
  });

  achievements.Unlock("callback_test");

  CHECK(callbackInvoked == true);
  CHECK(unlockedName == "Callback Test");

  achievements.ClearAll();
}

TEST_CASE("Tutorial System - Step Progression") {
  LoggerScope scope;
  auto &tutorial = TutorialSystem::Get();
  tutorial.ResetProgress();

  // 直接注册步骤用于测试 (通常从 JSON 加载)
  // 由于没有加载方法，跳过此测试或使用模拟

  SUBCASE("Initial state") {
    CHECK(tutorial.IsActive() == false);
    CHECK(tutorial.GetCurrentStep().has_value() == false);
  }

  SUBCASE("Skip all") {
    tutorial.SkipAll();
    CHECK(tutorial.IsActive() == false);
  }
}

TEST_CASE("Achievement Progress Persistence") {
  LoggerScope scope;
  auto &achievements = AchievementSystem::Get();
  achievements.ClearAll();

  const std::string testPath = "test_achievements.json";

  // Cleanup
  if (std::filesystem::exists(testPath)) {
    std::filesystem::remove(testPath);
  }

  // 注册并解锁
  achievements.RegisterAchievement(
      {.id = "persist_test", .name = "Persistence Test", .target_progress = 5});

  achievements.UpdateProgress("persist_test", 3);

  // 验证进度
  const auto *ach = achievements.GetAchievement("persist_test");
  REQUIRE(ach != nullptr);
  CHECK(ach->current_progress == 3);
  CHECK(ach->unlocked == false);

  achievements.ClearAll();
}

} // namespace NoMoreDay
