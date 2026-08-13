#include "doctest.h"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/foundation/stats/AttributePipeline.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_SUITE("TalentModifierTest") {

  TEST_CASE("[Unit] TalentModifier - Legacy skill-tree stat path disabled") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player); // Mark as player so defaults apply
    registry.emplace<CombatStats>(player);

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
    // Legacy skill-tree stat_modifiers path is inactive, so health must stay at
    // the baseline player value.
    entt::registry baseReg;
    auto basePlayer = baseReg.create();
    baseReg.emplace<PlayerTag>(basePlayer);
    baseReg.emplace<CombatStats>(basePlayer);
    AttributePipeline::Calculate(baseReg, basePlayer);
    float baseHealth = baseReg.get<CombatStats>(basePlayer).max_health;

    CHECK(stats.max_health == doctest::Approx(baseHealth));
  }

  TEST_CASE("[Unit] TalentModifier - Behavior Injection") {
    entt::registry registry;
    auto player = registry.create();

    // Ensure Init is called
    BehaviorInjectionRegistry::Init();

    // Act
    BehaviorInjectionRegistry::Apply(SkillBehaviorId::ShadowCaster, registry,
                                     player);

    // Assert
    CHECK(registry.all_of<ShadowKillArrayReady>(player));
  }

  TEST_CASE("[Unit] TalentModifier - Astrolabe Modifiers") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<GlobalModifierComponent>(player);
    registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<PrimaryStats>(player);

    // Register Astrolabe Node
    TalentGraph graph;
    AstrolabeTalentNode node;
    node.id = 88801;
    node.modifiers.push_back({.value = 10.0f,
                              .type = StatType::MoveSpeed,
                              .mode = ModifierMode::PercentAdd,
                              .required_tags = Tag::None});
    node.profession = ProfessionID::BladeAscendant;
    node.tier = 1;
    graph.nodes[88801] = node;
    AstrolabeRegistry::Get().SetGraph(graph);

    // Setup Player Astrolabe
    auto &astro = registry.emplace<AstrolabeComponent>(player);
    astro.nodePoints[88801] = 1;

    // Act
    AttributePipeline::Calculate(registry, player);

    // Assert
    entt::registry baseReg;
    auto basePlayer = baseReg.create();
    baseReg.emplace<PlayerTag>(basePlayer);
    baseReg.emplace<CombatStats>(basePlayer);
    baseReg.emplace<GlobalModifierComponent>(basePlayer);
    baseReg.emplace<ActiveSkillsComponent>(basePlayer);
    baseReg.emplace<EquipmentComponent>(basePlayer);
    baseReg.emplace<PrimaryStats>(basePlayer);
    
    AttributePipeline::Calculate(baseReg, basePlayer);
    float baseSpeed = baseReg.get<CombatStats>(basePlayer).raw_move_speed;

    auto &stats = registry.get<CombatStats>(player);
    // 10% increase means base * 1.1 + flat (0)
    // Or (base + flat) * (1 + 0.1)

    CHECK(stats.raw_move_speed > baseSpeed);
  }
}
