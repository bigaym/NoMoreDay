#include "doctest.h"

#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"
#include "game/systems/combat/XPAwardingSystem.hpp"

#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("[Integration] Progression - XP awarding drives level-up and unlock budgets") {
    XPAwardingSystem::Reset();

    entt::registry registry;

    const entt::entity player = registry.create();
    registry.emplace<PlayerTag>(player);

    auto &playerStats = registry.emplace<PlayerStats>(player);
    playerStats.level = 1;
    playerStats.current_xp = 0.0f;
    playerStats.required_xp = 1.0f;

    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.available_points = 0;

    auto &activeSkills = registry.emplace<ActiveSkillsComponent>(player);
    activeSkills.available_talent_points = 0;

    registry.emplace<CombatStats>(player);

    const entt::entity killedEnemy = registry.create();
    auto &enemyState = registry.emplace<EnemyStateComponent>(killedEnemy);
    enemyState.level = 1;
    registry.emplace<KilledTag>(killedEnemy, player);

    XPAwardingSystem::update(registry);

    CHECK(playerStats.current_map_kills == 1);
    CHECK(playerStats.level == 2);
    CHECK(playerStats.available_attribute_points == 5);
    CHECK(playerStats.available_skill_points == 1);
    CHECK(astrolabe.available_points == 1);
    CHECK(activeSkills.available_talent_points == 1);
    CHECK_FALSE(registry.valid(killedEnemy));

    CHECK(ProgressionSystem::AllocateSkillPoint(registry, player));
    CHECK(playerStats.available_skill_points == 0);

    CHECK_FALSE(ProgressionSystem::AllocateSkillPoint(registry, player));
    CHECK(playerStats.available_skill_points == 0);

    XPAwardingSystem::Reset();
}
