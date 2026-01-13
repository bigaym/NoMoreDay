#pragma once
#include "TestCommon.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/Progression.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/ui/UIAstrolabe.hpp"
#include <filesystem>


namespace NoMoreDay {

TEST_CASE("AstrolabeRegistry: Loading and Node Retrieval") {
  auto &registry = AstrolabeRegistry::Get();

  SUBCASE("Load astrolabe.json") {
    bool success = registry.Load("assets/data/astrolabe.json");
    CHECK(success == true);

    auto &all_nodes = registry.GetAllNodes();
    CHECK(all_nodes.size() >= 10);
  }

  SUBCASE("Retrieve specific nodes") {
    // Node 0: 起源
    const AstrolabeNode *node0 = registry.GetNode(0);
    REQUIRE(node0 != nullptr);
    CHECK(node0->id == 0);
    CHECK(node0->name_key == "起源");
    CHECK(node0->type == AstrolabeNodeType::Minor);
    CHECK(node0->modifiers.size() == 4); // All Attributes +1

    // Node 1: 剑术修炼 I
    const AstrolabeNode *node1 = registry.GetNode(1);
    REQUIRE(node1 != nullptr);
    CHECK(node1->id == 1);
    CHECK(node1->name_key == "剑术修炼 I");
    CHECK(node1->modifiers[0].type == StatType::Dexterity);
    CHECK(node1->modifiers[0].value == 5.0f);
    CHECK(node1->prerequisites[0] == 0);

    // Node 4: 剑心通明 (Keystone)
    const AstrolabeNode *keystone = registry.GetNode(4);
    REQUIRE(keystone != nullptr);
    CHECK(keystone->type == AstrolabeNodeType::Keystone);
    CHECK(keystone->prerequisites[0] == 3);
  }

  SUBCASE("Retrieve non-existent node") {
    const AstrolabeNode *invalid = registry.GetNode(9999);
    CHECK(invalid == nullptr);
  }
}

TEST_CASE("AstrolabeSystem: Activation Logic") {
  entt::registry registry;
  auto entity = registry.create();

  // Setup scope for Registry and Logger
  TestSetupScope setup;
  AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

  auto &astrolabe = registry.emplace<AstrolabeComponent>(entity);
  astrolabe.available_points = 10;

  SUBCASE("Can activate root node") {
    CHECK(AstrolabeSystem::can_activate(registry, entity, 0) == true);
    CHECK(AstrolabeSystem::activate_node(registry, entity, 0) == true);
    CHECK(astrolabe.activated_nodes.contains(0));
    CHECK(astrolabe.available_points == 9);
    CHECK(registry.all_of<StatsDirty>(entity));
  }

  SUBCASE("Cannot activate without prerequisites") {
    // Node 1 depends on 0
    CHECK(AstrolabeSystem::can_activate(registry, entity, 1) == false);
    CHECK(AstrolabeSystem::activate_node(registry, entity, 1) == false);
  }

  SUBCASE("Can activate after prerequisites are met") {
    AstrolabeSystem::activate_node(registry, entity, 0);
    CHECK(AstrolabeSystem::can_activate(registry, entity, 1) == true);
    CHECK(AstrolabeSystem::activate_node(registry, entity, 1) == true);
    CHECK(astrolabe.available_points == 8);
  }

  SUBCASE("Effect Integration: Activating Keystone grants Component") {
    // Path: 0 -> 1 -> 2 -> 3 -> 4 (Sword Heart)
    AstrolabeSystem::activate_node(registry, entity, 0);
    AstrolabeSystem::activate_node(registry, entity, 1);
    AstrolabeSystem::activate_node(registry, entity, 2);
    AstrolabeSystem::activate_node(registry, entity, 3);

    CHECK_FALSE(registry.all_of<SwordHeartComponent>(entity));

    AstrolabeSystem::activate_node(registry, entity, 4);

    CHECK(registry.all_of<SwordHeartComponent>(entity));
  }

  SUBCASE("Stat Integration: Activating node changes stats") {
    auto &combat = registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity); // Source of base stats

    // Recalculate once to get baseline
    StatsSystem::Recalculate(registry, entity);
    float baseline_dex = combat.effective_dexterity;

    // Path to node 1 (+5 Dex)
    AstrolabeSystem::activate_node(registry, entity, 0); // +1 all
    AstrolabeSystem::activate_node(registry, entity, 1); // +5 dex

    // Stats should be dirty, but we call Recalculate directly for testing
    StatsSystem::Recalculate(registry, entity);

    // Total should be baseline + 1 (from 0) + 5 (from 1) = +6
    CHECK(combat.effective_dexterity == baseline_dex + 6.0f);
  }

  SUBCASE("Persistence: JSON Serialization") {
    AstrolabeComponent c;
    c.available_points = 10;
    c.activated_nodes = {1, 2, 100};

    nlohmann::json j = c;

    CHECK(j["available_points"] == 10);
    CHECK(j["activated_nodes"].is_array());
    CHECK(j["activated_nodes"].size() == 3);

    AstrolabeComponent c2 = j.get<AstrolabeComponent>();
    CHECK(c2.available_points == 10);
    CHECK(c2.activated_nodes.size() == 3);
    CHECK(c2.activated_nodes.contains(1));
    CHECK(c2.activated_nodes.contains(100));
  }
}

TEST_CASE("Astrolabe Keystone Balance Check") {
  // Load Registry
  AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

  entt::registry registry;
  auto entity = registry.create();

  // Initialize Stats
  registry.emplace<PrimaryStats>(entity);
  registry.emplace<CombatStats>(entity);
  registry.emplace<AstrolabeComponent>(entity);
  registry.emplace<StatsDirty>(entity);

  // Setup Sword Heart conditions (Main Hand Weapon, Empty Offhand)
  auto &equip = registry.emplace<EquipmentComponent>(entity);

  // Mock Main Hand
  auto weapon = registry.create();
  auto &item = registry.emplace<ItemComponent>(weapon);
  item.type = ItemType::Weapon;
  item.slot = EquipmentSlot::MainHand;
  item.attack = 100.0f;
  item.id = 123; // Valid ID

  equip.Set(EquipmentSlot::MainHand, weapon);

  // Force Stats Recalculate to establish baseline
  StatsSystem::Recalculate(registry, entity);
  float baseline_min = registry.get<CombatStats>(entity).min_weapon_damage;

  // Let's try to inject the component directly first to verify StatsSystem
  // logic.
  registry.emplace<SwordHeartComponent>(entity);
  StatsSystem::Recalculate(registry, entity);

  float buffed_min = registry.get<CombatStats>(entity).min_weapon_damage;

  // Balanced: 15% More -> 1.15x
  CHECK(buffed_min == doctest::Approx(baseline_min * 1.15f));

  // Now check Node 41 (IntToCritMult)
  registry.remove<SwordHeartComponent>(entity);

  // Manually simulate Node 41 activation in the component
  auto &astro = registry.get<AstrolabeComponent>(entity);
  astro.activated_nodes.insert(41);

  // Set 100 Intelligence -> should give 15% Crit Mult (0.15 ratio)
  registry.get<PrimaryStats>(entity).intelligence = 100.0f;

  StatsSystem::Recalculate(registry, entity);
  float crit_mult = registry.get<CombatStats>(entity).crit_damage;

  CHECK(crit_mult == doctest::Approx(1.65f));

  SUBCASE("Stat Truncation (Clamping) Check") {
    // Test Resistance Clamp (Base 0% + 150% = 150%, should be 75% effective)
    auto &list = registry.get_or_emplace<ModifierList>(entity);
    list.modifiers.push_back(
        {StatType::ResistFire, ModifierMode::Flat, 150.0f});

    StatsSystem::Recalculate(registry, entity);
    const auto &c = registry.get<CombatStats>(entity);

    CHECK(c.resistances[(int)DamageType::Fire] == doctest::Approx(0.75f));
    CHECK(c.raw_resistances[(int)DamageType::Fire] == doctest::Approx(1.50f));

    // Test Attack Speed Clamp (10.0 cap)
    list.modifiers.clear();
    list.modifiers.push_back({StatType::AttackSpeed, ModifierMode::Flat,
                              1500.0f}); // +1500% -> 16.0 total

    StatsSystem::Recalculate(registry, entity);
    const auto &c2 = registry.get<CombatStats>(entity);

    CHECK(c2.attack_speed == doctest::Approx(10.0f));
    CHECK(c2.raw_attack_speed == doctest::Approx(16.0f));
  }
}

TEST_CASE("Astrolabe UI System Logic") {
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);

  SUBCASE("Toggle Logic") {
    // Initial state: No component
    CHECK(registry.try_get<AstrolabeUIComponent>(player) == nullptr);
    CHECK_FALSE(UIAstrolabe::IsVisible(registry, player));

    // Toggle On
    UIAstrolabe::Toggle(registry, player);
    auto *ui = registry.try_get<AstrolabeUIComponent>(player);
    REQUIRE(ui != nullptr);
    CHECK(ui->isOpen);
    CHECK(UIAstrolabe::IsVisible(registry, player));
    CHECK(ui->zoom == 1.0f);
    CHECK(ui->offset.x == 0.0f);
    CHECK(ui->offset.y == 0.0f);

    // Toggle Off
    UIAstrolabe::Toggle(registry, player);
    CHECK_FALSE(ui->isOpen);
    CHECK_FALSE(UIAstrolabe::IsVisible(registry, player));
  }
}

} // namespace NoMoreDay
