#include "game/systems/stats/AttributePipeline.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/Progression.hpp"
#include "game/components/WorldState.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/MapAffix.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "TestCommon.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] AttributePipeline - Tag Filter") {
  // Setup
  StatModifier mod;
  mod.required_tags = Tag::Melee;

  // Case 1: No Context Tags -> Inactive
  CHECK_FALSE(mod.IsActive(Tag::None));
  CHECK_FALSE(mod.IsActive(Tag::Fire));

  // Case 2: Matching Tags -> Active
  CHECK(mod.IsActive(Tag::Melee));
  CHECK(mod.IsActive(Tag::Melee | Tag::Fire));

  // Case 3: Tag::None requirement -> Always Active
  mod.required_tags = Tag::None;
  CHECK(mod.IsActive(Tag::None));
  CHECK(mod.IsActive(Tag::Melee));
}

TEST_CASE("[Unit] AttributePipeline - Struct Layout") {
  // Verify StatModifier is 24 bytes (or close/aligned)
  CHECK(sizeof(StatModifier) <= 32);
  CHECK(alignof(StatModifier) == 8);
}

TEST_CASE("[Unit] AttributePipeline - Calculation Logic") {
  // Setup Registry & entity
  entt::registry registry;
  auto entity = registry.create();

  // Components
  auto &stats = registry.emplace<CombatStats>(entity);
  auto &primary = registry.emplace<PrimaryStats>(entity);
  auto &mods = registry.emplace<ModifierList>(entity);

  // Set Base values
  primary.strength = 10.0f;
  stats.armor = 100.0f; // Base Armor (Note: Calculate will reset this, base
                        // should come from equipment or scaling)
  stats.max_health = 1000.0f;

  // To simulate base armor from gear in the current Calculate logic,
  // we either need an ItemComponent or rely on PrimaryStats conversion.
  // In Calculate(), armor base is set to 0 then added from items and STR.

  // 1. +5 Strength (Flat)
  mods.modifiers.push_back(
      {.value = 5.0f, .type = StatType::Strength, .mode = ModifierMode::Flat});

  // 2. +10% Strength (PercentAdd)
  mods.modifiers.push_back({.value = 10.0f,
                            .type = StatType::Strength,
                            .mode = ModifierMode::PercentAdd});

  // 3. +200 Armor (Flat)
  mods.modifiers.push_back(
      {.value = 200.0f, .type = StatType::Armor, .mode = ModifierMode::Flat});

  // Run Calculation
  AttributePipeline::Calculate(registry, entity);

  // Verification
  // 1. Strength: (10 + 5) * (1 + 0.1) = 16.5
  CHECK(stats.effective_strength == doctest::Approx(16.5f));

  // 2. Armor:
  // Base from STR: 16.5 * 2.0 = 33.0
  // Flat Mod: 200.0
  // Total: 233.0
  // (Note: stats.armor = 100 was reset to 0 in Calculate)
  CHECK(stats.armor == doctest::Approx(233.0f));

  // 3. Effective Armor DR:
  // Level 1.
  // LF = 10 + 0.5*1 + 0.05*1 = 10.55
  // DR = 10.55 / (233 + 10.55) = 0.0433 (Multiplier) -> 1 - 0.0433 = 0.9567
  CHECK(stats.effective_armor_dr == doctest::Approx(0.956682f).epsilon(0.001f));
}

TEST_CASE("[Unit] AttributePipeline - activated_nodes legacy fallback is inactive") {
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<CombatStats>(player);
  registry.emplace<GlobalModifierComponent>(player);
  registry.emplace<ActiveSkillsComponent>(player);
  registry.emplace<EquipmentComponent>(player);
  registry.emplace<PrimaryStats>(player);

  TalentGraph graph;
  AstrolabeTalentNode node;
  node.id = 88123;
  node.modifiers.push_back({.value = 10.0f,
                            .type = StatType::MoveSpeed,
                            .mode = ModifierMode::PercentAdd,
                            .required_tags = Tag::None});
  node.profession = ProfessionID::BladeAscendant;
  node.tier = 1;
  graph.nodes[node.id] = node;
  AstrolabeRegistry::Get().SetGraph(graph);

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.activated_nodes.insert(node.id);

  AttributePipeline::Calculate(registry, player);
  const float legacyOnlySpeed = registry.get<CombatStats>(player).raw_move_speed;

  entt::registry baseReg;
  auto basePlayer = baseReg.create();
  baseReg.emplace<PlayerTag>(basePlayer);
  baseReg.emplace<CombatStats>(basePlayer);
  baseReg.emplace<GlobalModifierComponent>(basePlayer);
  baseReg.emplace<ActiveSkillsComponent>(basePlayer);
  baseReg.emplace<EquipmentComponent>(basePlayer);
  baseReg.emplace<PrimaryStats>(basePlayer);

  AttributePipeline::Calculate(baseReg, basePlayer);
  const float baseSpeed = baseReg.get<CombatStats>(basePlayer).raw_move_speed;

  CHECK(legacyOnlySpeed == doctest::Approx(baseSpeed));
}

TEST_CASE("[Unit] AttributePipeline - enemy map and monster stat mods follow adapter evaluator path") {
  auto buildEnemy = [](entt::registry &registry, const bool withModifiers) {
    const auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<CombatStats>(enemy);
    registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                          EnemyArchetype::FODDER);
    registry.get<EnemyStateComponent>(enemy).level = 1;

    if (withModifiers) {
      auto &mapState = registry.ctx().emplace<ActiveDimensionalState>();
      mapState.isActive = true;
      mapState.resonance.totalEnemyDensity = 1.0f;
      mapState.explicitAffixes.push_back(
          {MapAffixType::Enemy_ExtraHealth, MapAffixCategory::Debuff, 0.30f, 5,
           "test"});
      mapState.explicitAffixes.push_back(
          {MapAffixType::Enemy_ExtraDamage, MapAffixCategory::Debuff, 0.50f, 5,
           "test"});
      mapState.explicitAffixes.push_back(
          {MapAffixType::Enemy_Fast, MapAffixCategory::Debuff, 0.20f, 5,
           "test"});

      auto &monsterAffix = registry.emplace<MonsterAffixComponent>(enemy);
      monsterAffix.AddAffix(MonsterAffixType::Fast);
      monsterAffix.AddAffix(MonsterAffixType::Berserker);
      monsterAffix.isBerserk = true;
    }

    AttributePipeline::Calculate(registry, enemy);
    return enemy;
  };

  entt::registry baseRegistry;
  const auto baseEnemy = buildEnemy(baseRegistry, false);
  const auto &baseStats = baseRegistry.get<CombatStats>(baseEnemy);

  entt::registry modifiedRegistry;
  const auto modifiedEnemy = buildEnemy(modifiedRegistry, true);
  const auto &modifiedStats = modifiedRegistry.get<CombatStats>(modifiedEnemy);

  CHECK(modifiedStats.max_health ==
        doctest::Approx(baseStats.max_health * 1.05f * 1.30f));
  CHECK(modifiedStats.move_speed ==
        doctest::Approx(baseStats.move_speed * 1.20f * 1.50f));
  CHECK(modifiedStats.min_weapon_damage ==
        doctest::Approx(baseStats.min_weapon_damage * 1.50f * 4.0f));
}

} // namespace NoMoreDay
