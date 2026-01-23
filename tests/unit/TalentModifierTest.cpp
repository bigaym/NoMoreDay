#include "doctest.h"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_SUITE("TalentModifierTest") {

  TEST_CASE("TalentStatModifier_MaxHealth_Applied") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player); // Mark as player so defaults apply

    // Setup: Create SkillTree with MaxHealth modifier
    SkillData skill;
    skill.id = 999;
    skill.name_key = "TestSkill";
    SkillRegistry::Get().RegisterSkill(skill);

    SkillTreeDefinition tree;
    tree.skill_id = 999;
    TalentNode node;
    node.id = 99901;
    node.name_key = "Test Node";
    node.max_points = 3;
    node.stat_modifiers.push_back({.value = 50.0f,
                                   .type = StatType::MaxHealth,
                                   .mode = ModifierMode::Flat,
                                   .required_tags = Tag::None});
    tree.nodes[99901] = node;
    SkillRegistry::Get().RegisterSkillTree(tree);

    // Setup player
    registry.emplace<ActiveSkillsComponent>(player);
    auto &active = registry.get<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 999;
    active.specialized_slots[0].allocated_points[99901] = 2; // 2 points

    // Act
    AttributePipeline::Calculate(registry, player);

    // Assert
    auto &stats = registry.get<CombatStats>(player);
    // Base 100 (player default) + 50 * 2 = 200.
    // Assuming DEFAULT_MAX_HEALTH is 100. Even if not, it should be base + 100.
    // We can check if it's significantly higher than base.
    // Or create a base player to compare.

    entt::registry baseReg;
    auto basePlayer = baseReg.create();
    baseReg.emplace<PlayerTag>(basePlayer);
    AttributePipeline::Calculate(baseReg, basePlayer);
    float baseHealth = baseReg.get<CombatStats>(basePlayer).max_health;

    CHECK(stats.max_health == doctest::Approx(baseHealth + 100.0f));
  }

  TEST_CASE("BehaviorInjection_ShadowCaster_Applied") {
    entt::registry registry;
    auto player = registry.create();

    // Ensure Init is called
    BehaviorInjectionRegistry::Init();

    // Act
    BehaviorInjectionRegistry::Apply("shadow_caster", registry, player);

    // Assert
    CHECK(registry.all_of<ShadowKillArrayReady>(player));
  }

  TEST_CASE("AstrolabeModifier_MoveSpeed_Applied") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);

    // Register Astrolabe Node
    AstrolabeNode node;
    node.id = 88801;
    node.modifiers.push_back({.value = 10.0f,
                              .type = StatType::MoveSpeed,
                              .mode = ModifierMode::PercentAdd,
                              .required_tags = Tag::None});
    AstrolabeRegistry::Get().RegisterNode(node);

    // Setup Player Astrolabe
    auto &astro = registry.emplace<AstrolabeComponent>(player);
    astro.activated_nodes.insert(88801);

    // Act
    AttributePipeline::Calculate(registry, player);

    // Assert
    entt::registry baseReg;
    auto basePlayer = baseReg.create();
    baseReg.emplace<PlayerTag>(basePlayer);
    AttributePipeline::Calculate(baseReg, basePlayer);
    float baseSpeed = baseReg.get<CombatStats>(basePlayer).raw_move_speed;

    auto &stats = registry.get<CombatStats>(player);
    // 10% increase means base * 1.1 + flat (0)
    // Or (base + flat) * (1 + 0.1)

    CHECK(stats.raw_move_speed > baseSpeed);
  }
}
