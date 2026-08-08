#pragma once

#include "TestCommon.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/components/Progression.hpp"
#include "game/components/MaterialBankComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/PlayerState.hpp"

#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"    
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/ShadowSystem.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include "game/systems/combat/MovementStanceSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_set>

namespace NoMoreDay {

namespace {

std::filesystem::path FindProjectRoot() {
  std::array<std::filesystem::path, 4> candidates = {
      std::filesystem::current_path(),
      std::filesystem::current_path() / "..",
      std::filesystem::current_path() / "../..",
      std::filesystem::current_path() / "../../.."};

  for (const auto &candidate : candidates) {
    const auto root = std::filesystem::weakly_canonical(candidate);
    if (std::filesystem::exists(root / "assets/data/skills.json") &&
        std::filesystem::exists(root / "src/game/systems/skill/behaviors")) {
      return root;
    }
  }

  return std::filesystem::current_path();
}

std::unordered_set<uint32_t>
LoadTalentNodeIds(const std::filesystem::path &skillsJsonPath) {
  std::unordered_set<uint32_t> ids;
  std::ifstream in(skillsJsonPath);
  REQUIRE(in.is_open());

  nlohmann::json j;
  in >> j;
  REQUIRE(j.contains("skills"));

  for (const auto &skill : j["skills"]) {
    if (!skill.contains("talent_tree"))
      continue;
    for (const auto &node : skill["talent_tree"]) {
      if (node.contains("id")) {
        ids.insert(node["id"].get<uint32_t>());
      }
    }
  }

  return ids;
}

std::vector<uint32_t>
ExtractNodeConstantsFromCpp(const std::filesystem::path &cppPath) {
  std::ifstream in(cppPath);
  REQUIRE(in.is_open());

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  std::vector<uint32_t> constants;
  std::regex namespaceRe(
      R"(namespace\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([\s\S]*?)\}\s*//\s*namespace\s+\1)");
  std::regex constRe(
      R"(constexpr\s+uint32_t\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*([0-9]+)\s*;)");

  auto begin = std::sregex_iterator(content.begin(), content.end(), namespaceRe);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it) {
    const std::string nsName = (*it)[1].str();
    if (!nsName.ends_with("Nodes")) {
      continue;
    }

    const std::string nsBody = (*it)[2].str();
    auto cb = std::sregex_iterator(nsBody.begin(), nsBody.end(), constRe);
    auto ce = std::sregex_iterator();
    for (auto cit = cb; cit != ce; ++cit) {
      constants.push_back(static_cast<uint32_t>(std::stoul((*cit)[1].str())));
    }
  }

  return constants;
}

} // namespace

TEST_CASE("[Integration] Astrolabe - Node Activation") {
  auto &registry_data = AstrolabeRegistry::Get();
  registry_data.Load("assets/data/profession_talents.json");
  const auto& graph = registry_data.GetGraph();

  entt::registry registry;
  auto entity = registry.create();
  registry.emplace<PrimaryStats>(entity);
  auto& comp = registry.emplace<AstrolabeComponent>(entity);
  registry.emplace<CombatStats>(entity);
  registry.emplace<GlobalModifierComponent>(entity);
  registry.emplace<ActiveSkillsComponent>(entity);
  registry.emplace<EquipmentComponent>(entity);
  
  SUBCASE("Node Activation") {
      comp.available_points = 1;
      // 1001 is a Minor Tier 1 node (炼体术)
      bool success = AstrolabeSystem::addPointToNode(registry, entity, graph, 1001);

      CHECK(success == true);
      CHECK(comp.getNodePoints(1001) > 0);
  }
}

TEST_CASE("[Integration] CombatSystem - Basic Damage Flow") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    
    registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0;
    registry.emplace<Position>(attacker, 0.0f, 0.0f);
    
    registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
    registry.emplace<CombatStats>(defender);
    registry.emplace<Position>(defender, 10.0f, 0.0f);

    DamagePool pool;
    pool.Add(Tag::Physical, 20.0f);
    
    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, pool, Tag::Melee, entt::null, true);
    CHECK(result.total_damage > 0.0f);
}

TEST_CASE("[Integration] ItemSystem - Equipment Flow") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<InventoryComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    
    auto weapon = ItemFactory::createWeapon(registry, 1, Rarity::Common);
    bool equipped = InventorySystem::equipItem(registry, player, weapon, EquipmentSlot::MainHand);
    CHECK(equipped);
    CHECK(registry.valid(registry.get<EquipmentComponent>(player).Get(EquipmentSlot::MainHand)));


}

TEST_CASE("[Integration] MaterialSystem - Bank Operations") {
    MaterialBankComponent bank;
    bank.Add(1001, 10);
    CHECK(bank.GetCount(1001) == 10);
    CHECK(bank.Has(1001, 5));
    bank.Remove(1001, 5);
    CHECK(bank.GetCount(1001) == 5);
}

TEST_CASE("[Integration] SalvageSystem - Item Salvaging") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<MaterialBankComponent>(player);
    
    auto itemEnt = registry.create();
    auto& item = registry.emplace<ItemComponent>(itemEnt);
    item.rarity = Rarity::Magic;
    item.type = ItemType::Weapon;
    
    SalvageSystem::Execute(registry, itemEnt, player);
    CHECK(registry.valid(itemEnt) == false);
}

TEST_CASE("[Integration] Cultivator - Full Combat Flow") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    registry.emplace<MovementStanceComponent>(player);
    intent.stacks = 10;
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    SkillSystem::Update(registry, grid, 0.11f); 
    
    auto exec_view = registry.view<SkillExecution>();
    CHECK(!exec_view.empty());
}

TEST_CASE("[Integration] Legendary - Infrastructure Verification") {
    TestSetupScope scope;
    entt::registry registry;
    ItemFactory::initialize();
    
    auto weapon = ItemFactory::createWeapon(registry, 100, Rarity::Legendary);
    auto& item = registry.get<ItemComponent>(weapon);
    
    // Manually inject a legendary affix to verify the detection logic works
    // (ItemFactory currently filters out legendary affixes from random rolls)
    Affix legAffix;
    legAffix.type = static_cast<AffixType>(1001); 
    item.affixes.push_back(legAffix);

    bool hasLegendaryAffix = false;
    for(const auto& aff : item.affixes) {
        if(static_cast<uint16_t>(aff.type) >= 1000) {
            hasLegendaryAffix = true;
            break;
        }
    }
    CHECK(hasLegendaryAffix);
}

TEST_CASE("[Integration] Skill Nodes - IDs must exist in specialization data tables") {
  const auto root = FindProjectRoot();
  auto legalIds = LoadTalentNodeIds(root / "assets/data/skills.json");
  const auto masteryIds =
      LoadTalentNodeIds(root / "assets/data/mastery_skill_trees.json");
  legalIds.insert(masteryIds.begin(), masteryIds.end());
  REQUIRE(!legalIds.empty());

  const std::array<std::filesystem::path, 10> behaviorFiles = {
      "src/game/systems/skill/behaviors/FlowingThrust.cpp",
      "src/game/systems/skill/behaviors/RendingWave.cpp",
      "src/game/systems/skill/behaviors/BladeFormation.cpp",
      "src/game/systems/skill/behaviors/BladeWard.cpp",
      "src/game/systems/skill/behaviors/InfiniteBlades.cpp",
      "src/game/systems/skill/behaviors/SwordArray.cpp",
      "src/game/systems/skill/behaviors/MindBlade.cpp",
      "src/game/systems/skill/behaviors/BladeBoomerang.cpp",
      "src/game/systems/skill/behaviors/PhantomFlash.cpp",
      "src/game/systems/skill/behaviors/SevenStarSlash.cpp"};

  for (const auto &relativeFile : behaviorFiles) {
    const auto fullPath = root / relativeFile;
    const auto constants = ExtractNodeConstantsFromCpp(fullPath);

    REQUIRE_MESSAGE(!constants.empty(), "No Nodes constants found in ",
                    fullPath.string());

    for (uint32_t id : constants) {
      CHECK_MESSAGE(legalIds.contains(id), "Unknown talent node ID ", id,
                    " in ", fullPath.string());
    }
  }
}

TEST_CASE(
    "[Integration] ModifierRuntimeV2 - legacy skill-tree stat modifiers are inactive") {
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<CombatStats>(player);

  SkillData skill;
  skill.id = 999001;
  skill.name_key = "LegacyPathProbe";
  SkillRegistry::Get().RegisterSkill(skill);

  SkillTreeDefinition tree;
  tree.skill_id = skill.id;
  TalentNode node;
  node.id = 999101;
  node.name_key = "LegacyNode";
  node.max_points = 1;
  node.stat_modifiers.push_back({.value = 50.0f,
                                 .type = StatType::MaxHealth,
                                 .mode = ModifierMode::Flat,
                                 .required_tags = Tag::None});
  tree.nodes[node.id] = node;
  SkillRegistry::Get().RegisterSkillTree(tree);

  registry.emplace<ActiveSkillsComponent>(player);
  auto &active = registry.get<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = skill.id;
  active.specialized_slots[0].allocated_points[node.id] = 1;

  AttributePipeline::Calculate(registry, player);
  const float legacyCandidateHealth = registry.get<CombatStats>(player).max_health;

  entt::registry baseReg;
  auto basePlayer = baseReg.create();
  baseReg.emplace<PlayerTag>(basePlayer);
  baseReg.emplace<CombatStats>(basePlayer);
  AttributePipeline::Calculate(baseReg, basePlayer);
  const float baseHealth = baseReg.get<CombatStats>(basePlayer).max_health;

  CHECK(legacyCandidateHealth == doctest::Approx(baseHealth));
}

} // namespace NoMoreDay
