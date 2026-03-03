#include "doctest.h"

#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"

#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("[Unit] Progression - AddExperience levels up and grants unlock budgets") {
    entt::registry registry;
    const entt::entity player = registry.create();

    auto &playerStats = registry.emplace<PlayerStats>(player);
    playerStats.level = 1;
    playerStats.current_xp = 0.0f;
    playerStats.required_xp = ProgressionSystem::CalculateRequiredXP(playerStats.level);

    auto &playerLevel = registry.emplace<PlayerLevel>(player);
    playerLevel.value = playerStats.level;

    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.available_points = 0;

    auto &activeSkills = registry.emplace<ActiveSkillsComponent>(player);
    activeSkills.available_talent_points = 0;

    registry.emplace<PrimaryStats>(player);

    ProgressionSystem::AddExperience(registry, player, playerStats.required_xp + 10.0f);

    CHECK(playerStats.level == 2);
    CHECK(playerLevel.value == 2);
    CHECK(playerStats.current_xp == doctest::Approx(10.0f));
    CHECK(playerStats.required_xp == doctest::Approx(ProgressionSystem::CalculateRequiredXP(2)));
    CHECK(playerStats.available_attribute_points == 5);
    CHECK(playerStats.available_skill_points == 1);
    CHECK(astrolabe.available_points == 1);
    CHECK(activeSkills.available_talent_points == 1);
    CHECK(registry.all_of<StatsDirty>(player));
}

TEST_CASE("[Unit] Progression - Allocation gate failures preserve state") {
    entt::registry registry;
    const entt::entity player = registry.create();

    auto &playerStats = registry.emplace<PlayerStats>(player);
    auto &primaryStats = registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);

    playerStats.available_attribute_points = 1;
    REQUIRE(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength));
    CHECK(primaryStats.strength == doctest::Approx(11.0f));
    CHECK(playerStats.available_attribute_points == 0);

    const float strengthAfterSuccess = primaryStats.strength;
    CHECK_FALSE(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength));
    CHECK(primaryStats.strength == doctest::Approx(strengthAfterSuccess));
    CHECK(playerStats.available_attribute_points == 0);

    playerStats.available_attribute_points = 1;
    CHECK_FALSE(ProgressionSystem::AllocateAttribute(registry, player, StatType::MaxHealth));
    CHECK(primaryStats.strength == doctest::Approx(strengthAfterSuccess));
    CHECK(playerStats.available_attribute_points == 1);

    playerStats.available_skill_points = 1;
    REQUIRE(ProgressionSystem::AllocateSkillPoint(registry, player));
    CHECK(playerStats.available_skill_points == 0);

    CHECK_FALSE(ProgressionSystem::AllocateSkillPoint(registry, player));
    CHECK(playerStats.available_skill_points == 0);
}

TEST_CASE("[Unit] Progression - Max level clamp blocks overflow and rollback") {
    entt::registry registry;
    const entt::entity player = registry.create();

    auto &playerStats = registry.emplace<PlayerStats>(player);
    playerStats.level = ProgressionSystem::MAX_LEVEL - 1;
    playerStats.required_xp = 50.0f;
    playerStats.current_xp = 49.0f;

    ProgressionSystem::AddExperience(registry, player, 5000.0f);

    CHECK(playerStats.level == ProgressionSystem::MAX_LEVEL);
    CHECK(playerStats.current_xp == doctest::Approx(0.0f));
    CHECK(playerStats.required_xp == doctest::Approx(0.0f));

    ProgressionSystem::AddExperience(registry, player, 1234.0f);
    CHECK(playerStats.level == ProgressionSystem::MAX_LEVEL);
    CHECK(playerStats.current_xp == doctest::Approx(0.0f));
    CHECK(playerStats.required_xp == doctest::Approx(0.0f));
}
