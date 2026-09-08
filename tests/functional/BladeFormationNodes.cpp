#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SummonAISystem.hpp"
#include "game/systems/skill/SummonCombatBridge.hpp"
#include "game/systems/skill/SummonLifecycleSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cmath>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 3;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

entt::entity CreateTestPlayer(entt::registry &registry,
                             const std::unordered_map<uint32_t, int> &allocated_nodes = {}) {
  auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 200.0f;
  stats.mana = 200.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  // Bake profile
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
  active.baked_profiles[0] = profile;

  return player;
}

void CastBladeFormation(entt::registry &registry, entt::entity player) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = {100.0f, 100.0f};

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
}

std::vector<entt::entity> GetSwordsForOwner(entt::registry &registry, entt::entity owner) {
  std::vector<entt::entity> swords;
  auto view = registry.view<SpiritSwordTag, SummonComponent>();
  for (auto e : view) {
    if (view.get<SummonComponent>(e).owner == owner) {
      swords.push_back(e);
    }
  }
  return swords;
}

} // namespace

TEST_CASE("[Functional] Skill 3 - Summon Lifecycle and Frame-1 Destruction Exemption (C8)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry);

  // Cast skill 3
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 3);

  // Check summon lifetime settings
  const auto &summon = registry.get<SummonComponent>(swords[0]);
  CHECK(summon.lifetime < 0.0f);
  CHECK(summon.max_lifetime <= 0.0f);

  // Run SummonLifecycleSystem across several frames (dt = 0.1f, 1.0f, 10.0f)
  systems::SummonLifecycleSystem::Update(registry, 0.1f);
  CHECK(registry.valid(swords[0]));

  systems::SummonLifecycleSystem::Update(registry, 1.0f);
  CHECK(registry.valid(swords[0]));

  systems::SummonLifecycleSystem::Update(registry, 10.0f);
  CHECK(registry.valid(swords[0]));

  // When owner is destroyed, summon MUST be cleaned up on next update
  registry.destroy(player);
  systems::SummonLifecycleSystem::Update(registry, 0.016f);
  CHECK_FALSE(registry.valid(swords[0]));
}

TEST_CASE("[Functional] Skill 3 - AI Profile and Melee Orbit Assembly (C6 & C7)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("Default Orbit Mode") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry);
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);
    auto sword = swords[0];

    // C7 Verification: SummonAIProfile & SummonRuntimeState must be assembled
    REQUIRE(registry.all_of<SummonAIProfile>(sword));
    REQUIRE(registry.all_of<SummonRuntimeState>(sword));
    REQUIRE(registry.all_of<SpiritSwordAI>(sword));
    REQUIRE(registry.all_of<SummonCombatProfile>(sword));

    const auto &aiProf = registry.get<SummonAIProfile>(sword);
    CHECK(aiProf.role == SummonRole::Orbit);

    const auto &swordAi = registry.get<SpiritSwordAI>(sword);
    CHECK(swordAi.state == SpiritSwordAI::State::Idle);

    const auto &combatProf = registry.get<SummonCombatProfile>(sword);
    CHECK(combatProf.damage_scale == doctest::Approx(0.5f));
  }

  SUBCASE("Node 351 Blade Orbit (Melee Orbit Mode)") {
    entt::registry registry;
    // Allocate Node 351
    auto player = CreateTestPlayer(registry, {{351, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);
    auto sword = swords[0];

    // C6 Verification: melee_orbit role and state
    const auto &aiProf = registry.get<SummonAIProfile>(sword);
    CHECK(aiProf.role == SummonRole::Melee);

    const auto &swordAi = registry.get<SpiritSwordAI>(sword);
    CHECK(swordAi.state == SpiritSwordAI::State::MeleeOrbit);

    const auto *formation = registry.try_get<BladeFormationComponent>(player);
    REQUIRE(formation != nullptr);
    CHECK(formation->melee_orbit == true);
  }
}

TEST_CASE("[Functional] Skill 3 - Baker Determinism and Giant Sword Scaling (C3 & H5)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("C3 Giant Sword (Node 330) Damage Scaling") {
    entt::registry registry;
    // Node 330: Giant Sword
    auto player = CreateTestPlayer(registry, {{330, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 1);
    auto sword = swords[0];

    const auto &combatProf = registry.get<SummonCombatProfile>(sword);
    // Design: base 0.5f * 2.5x = 1.25f (NOT 9x)
    CHECK(combatProf.damage_scale == doctest::Approx(1.25f));
  }

  SUBCASE("H5 Baker Determinism: 300 + 311 + 330 produces projectile_count = 1 regardless of order") {
    entt::registry registry;
    auto player = registry.create();

    // Order 1: 300, 311, 330
    SpecializedSkill spec1;
    spec1.skill_id = kSkillId;
    spec1.allocated_points[300] = 4; // +4 swords
    spec1.allocated_points[311] = 1; // x2 swords
    spec1.allocated_points[330] = 1; // force 1 sword
    BakedSkillProfile prof1{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec1, prof1, nullptr);

    // Order 2: 330, 300, 311
    SpecializedSkill spec2;
    spec2.skill_id = kSkillId;
    spec2.allocated_points[330] = 1;
    spec2.allocated_points[300] = 4;
    spec2.allocated_points[311] = 1;
    BakedSkillProfile prof2{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec2, prof2, nullptr);

    CHECK(prof1.projectile_count == 1);
    CHECK(prof2.projectile_count == 1);
    CHECK(prof1.projectile_count == prof2.projectile_count);
  }

  SUBCASE("Node 311 Infinite Sheath without Giant Sword doubles swords with 40% damage penalty") {
    entt::registry registry;
    // 300 (4 pts) -> 3 + 4 = 7 swords; 311 (1 pt) -> 7 * 2 = 14 swords, 40% penalty (0.6x)
    auto player = CreateTestPlayer(registry, {{300, 4}, {311, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    CHECK(swords.size() == 14);
    for (auto sword : swords) {
      const auto &combatProf = registry.get<SummonCombatProfile>(sword);
      // 0.5f * 0.6f = 0.3f
      CHECK(combatProf.damage_scale == doctest::Approx(0.3f));
    }
  }
}

TEST_CASE("[Functional] Skill 3 - Element Conversion Mutex (C2 & H3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("Node 370 Fire Conversion") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{370, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);

    const auto *mod = registry.try_get<SkillModifierComponent>(swords[0]);
    REQUIRE(mod != nullptr);
    REQUIRE_FALSE(mod->damage_modifiers.empty());
    CHECK(mod->damage_modifiers[0].target_tag == Tag::Fire);
    CHECK(mod->damage_modifiers[0].type == ModifierType::Convert);
    CHECK(mod->damage_modifiers[0].value == doctest::Approx(1.0f));
  }

  SUBCASE("Node 372 Lightning Conversion") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{372, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);

    const auto *mod = registry.try_get<SkillModifierComponent>(swords[0]);
    REQUIRE(mod != nullptr);
    REQUIRE_FALSE(mod->damage_modifiers.empty());
    CHECK(mod->damage_modifiers[0].target_tag == Tag::Lightning);
    CHECK(mod->damage_modifiers[0].type == ModifierType::Convert);
    CHECK(mod->damage_modifiers[0].value == doctest::Approx(1.0f));
  }
}

TEST_CASE("[Functional] Skill 3 - Node 353 Undying Immortality (H1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("Consumes >= 3 swords and prevents death") {
    entt::registry registry;
    // Default 3 swords and 353 (Immortality)
    auto player = CreateTestPlayer(registry, {{353, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);

    auto *formation = registry.try_get<BladeFormationComponent>(player);
    REQUIRE(formation != nullptr);
    CHECK(formation->has_immortality == true);
    CHECK(formation->immortality_ready == true);

    auto &hp = registry.get<HealthComponent>(player);
    // Take fatal damage through CombatSystem
    ::CombatSystem::ApplyDamage(registry, player, 2000.0f);

    // Immortality should have triggered:
    // 3 swords * 4% max hp (1000) = 120 HP restored
    CHECK(hp.current == doctest::Approx(120.0f));
    CHECK(formation->immortality_ready == false);
    CHECK(formation->immortality_cooldown == doctest::Approx(90.0f));

    // All swords should have been destroyed
    auto remainingSwords = GetSwordsForOwner(registry, player);
    CHECK(remainingSwords.empty());
  }

  SUBCASE("Does NOT trigger if swords < 3") {
    entt::registry registry;
    // Default 3 swords, destroy 2 to leave 1 (< 3)
    auto player = CreateTestPlayer(registry, {{353, 1}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 3);
    registry.destroy(swords[1]);
    registry.destroy(swords[2]);
    auto remainingBefore = GetSwordsForOwner(registry, player);
    REQUIRE(remainingBefore.size() == 1);

    auto &hp = registry.get<HealthComponent>(player);
    ::CombatSystem::ApplyDamage(registry, player, 2000.0f);

    // Immortality must NOT trigger
    CHECK(hp.current <= 0.0f);
  }
}

TEST_CASE("[Functional] Skill 3 - Defensive Mitigations (Nodes 350 & 352)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  // Node 300 (4 pts -> 3 + 4 = 7 swords), Node 350 (Ward, 5 pts = 5% DR/sword), Node 352 (Web, 3 pts = 6% block/sword)
  auto player = CreateTestPlayer(registry, {{300, 4}, {350, 5}, {352, 3}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 7);

  const auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->ward_dr_per_sword == doctest::Approx(0.05f));
  CHECK(formation->block_chance_per_sword == doctest::Approx(0.06f));

  // Verify damage mitigation with 7 swords * 5% = 35% DR
  const auto *stats = registry.try_get<CombatStats>(player);
  systems::EndgameModifierAggregate endgame{};
  float mitigated = DamageMitigationService::Apply(
      registry, entt::null, player, 0, Tag::Physical, Tag::Physical, 100.0f,
      stats, endgame, false, false, 0.0f, entt::null);

  // 100 * (1 - 0.35) = 65.0f
  CHECK(mitigated == doctest::Approx(65.0f));
}

TEST_CASE("[Functional] Skill 3 - Node 302 Edged Spirit Damage Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("3 points give +30% damage") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{302, 3}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(!swords.empty());
    const auto &combatProf = registry.get<SummonCombatProfile>(swords[0]);
    // Base 0.5f * 1.30f = 0.65f
    CHECK(combatProf.damage_scale == doctest::Approx(0.65f));
  }

  SUBCASE("5 points give +50% damage combined with Giant Sword") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{330, 1}, {302, 5}});
    CastBladeFormation(registry, player);

    auto swords = GetSwordsForOwner(registry, player);
    REQUIRE(swords.size() == 1);
    const auto &combatProf = registry.get<SummonCombatProfile>(swords[0]);
    // Giant sword base 1.25f * 1.50f = 1.875f
    CHECK(combatProf.damage_scale == doctest::Approx(1.875f));
  }
}

TEST_CASE("[Functional] Skill 3 - Node 312 Mana Network Point Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("1 point gives 3 mana/sec/sword") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{312, 1}});
    CastBladeFormation(registry, player);

    auto &stats = registry.get<CombatStats>(player);
    stats.mana = 50.0f;
    systems::SpatialHashGrid grid(100, 100, 50);

    SkillSystem::Update(registry, grid, 1.0f);
    // 3 swords * 3.0f * 1.0s = +9.0f mana -> 59.0f
    CHECK(stats.mana == doctest::Approx(59.0f));
  }

  SUBCASE("3 points give 9 mana/sec/sword") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{312, 3}});
    CastBladeFormation(registry, player);

    auto &stats = registry.get<CombatStats>(player);
    stats.mana = 50.0f;
    systems::SpatialHashGrid grid(100, 100, 50);

    SkillSystem::Update(registry, grid, 1.0f);
    // 3 swords * 9.0f * 1.0s = +27.0f mana -> 77.0f
    CHECK(stats.mana == doctest::Approx(77.0f));
  }
}

TEST_CASE("[Functional] Skill 3 - Node 314 Concentrate Command Targeting Gate") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  CombatEventDispatcher::Init();
  SkillSystem::InitHooks();

  auto player = CreateTestPlayer(registry);
  CastBladeFormation(registry, player);

  auto enemyA = registry.create();
  registry.emplace<EnemyTag>(enemyA);
  registry.emplace<Position>(enemyA, 110.0f, 100.0f); // 10 units away

  auto enemyB = registry.create();
  registry.emplace<EnemyTag>(enemyB);
  registry.emplace<Position>(enemyB, 150.0f, 100.0f); // 50 units away

  // Without Node 314: hit enemy B with skill 1
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(player, enemyB, 1, Tag::Hit, false));

  auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->has_concentrate == false);
  CHECK(formation->last_skill_hit_target == entt::null);

  // Now allocate Node 314 and re-cast
  registry.destroy(player);
  auto playerWith314 = CreateTestPlayer(registry, {{314, 1}});
  CastBladeFormation(registry, playerWith314);

  auto *formationWith314 = registry.try_get<BladeFormationComponent>(playerWith314);
  REQUIRE(formationWith314 != nullptr);
  CHECK(formationWith314->has_concentrate == true);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(playerWith314, enemyB, 1, Tag::Hit, false));
  CHECK(formationWith314->last_skill_hit_target == enemyB);
}

TEST_CASE("[Functional] Skill 3 - Node 315 Sword Step Resonance Haste & Proc") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{315, 3}});
  CastBladeFormation(registry, player);

  auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->sword_step_haste == doctest::Approx(0.6f));
  CHECK(formation->sword_step_intent_chance == doctest::Approx(0.3f));
}

TEST_CASE("[Functional] Skill 3 - Nodes 331 & 332 Crit Inheritance") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  // Node 330 Giant Sword, 331 (5 pts -> +0.25 crit, 归一化), 332 (4 pts -> +100% crit dmg)
  auto player = CreateTestPlayer(registry, {{330, 1}, {331, 5}, {332, 4}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 1);
  auto sword = swords[0];

  const auto &combatProf = registry.get<SummonCombatProfile>(sword);
  CHECK(combatProf.bonus_crit == doctest::Approx(0.25f));
  CHECK(combatProf.bonus_crit_damage == doctest::Approx(1.0f));

  CombatStats inherited = systems::SummonCombatBridge::ResolveInheritedStats(registry, sword);
  CHECK(inherited.crit_chance == doctest::Approx(0.25f)); // Owner 0 + 0.25 (归一化)
  CHECK(inherited.crit_damage == doctest::Approx(2.5f));  // Owner 1.5 + 1.0
}

TEST_CASE("[Functional] Skill 3 - Node 301 Swift Intent Attack Interval Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  SUBCASE("5 points reduce attack interval by 40% haste") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{301, 5}});
    CastBladeFormation(registry, player);

    const auto *formation = registry.try_get<BladeFormationComponent>(player);
    REQUIRE(formation != nullptr);
    // Base 1.0s / (1.0 + 0.40) = 0.7142857s
    CHECK(formation->attack_interval == doctest::Approx(1.0f / 1.40f));
  }

  SUBCASE("Swift Intent with Giant Sword") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{330, 1}, {301, 5}});
    CastBladeFormation(registry, player);

    const auto *formation = registry.try_get<BladeFormationComponent>(player);
    REQUIRE(formation != nullptr);
    // (1.0s / 1.40) * 2.0 = 1.42857s
    CHECK(formation->attack_interval == doctest::Approx(2.0f / 1.40f));
  }
}

TEST_CASE("[Functional] Skill 3 - Node 303 Elemental Core Conversion Ratio Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{303, 4}});
  auto &mastery = registry.emplace<BladeMasteryComponent>(player);
  mastery.selected = BladeMasteryId::HeavenlySword;
  mastery.heavenly_attunement = BladeAttunement::Fire;

  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(!swords.empty());
  const auto *mod = registry.try_get<SkillModifierComponent>(swords[0]);
  REQUIRE(mod != nullptr);
  REQUIRE(!mod->damage_modifiers.empty());
  CHECK(mod->damage_modifiers[0].target_tag == Tag::Fire);
  // Base 0.50 + 4 * 0.10 = 0.90 (90% conversion)
  CHECK(mod->damage_modifiers[0].value == doctest::Approx(0.90f));
}

TEST_CASE("[Functional] Skill 3 - Node 310 Search Radius Leash Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{310, 3}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(!swords.empty());
  const auto &aiProf = registry.get<SummonAIProfile>(swords[0]);
  // 200 * (1 + 0.60) = 320.0f (基准 200 与 BladeFormation fallback 一致)
  CHECK(aiProf.leash_radius == doctest::Approx(320.0f));

  const auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->search_radius == doctest::Approx(320.0f));
}

TEST_CASE("[Functional] Skill 3 - Node 312 Baker Mana Cost Reduction") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = registry.create();
  SpecializedSkill spec;
  spec.skill_id = kSkillId;
  spec.allocated_points[312] = 3; // 3 pts = -15% mana cost

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);

  // Base mana cost 25.0f * (1 - 0.15) = 21.25f
  CHECK(profile.effective_mana_cost == doctest::Approx(21.25f));
}

TEST_CASE("[Functional] Skill 3 - Node 313 Godspeed Attack Speed Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{313, 1}});
  auto &stats = registry.get<CombatStats>(player);
  stats.attack_speed = 140.0f; // +40% bonus attack speed

  CastBladeFormation(registry, player);

  const auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  // Bonus: (140 - 100) * 0.75 / 100 = +30% haste -> 1.0 / 1.30 = 0.76923s
  CHECK(formation->attack_interval == doctest::Approx(1.0f / 1.30f));
}

TEST_CASE("[Functional] Skill 3 - Node 333 Sword Pressure Physical Vulnerability Debuff") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{330, 1}, {333, 3}});
  CastBladeFormation(registry, player);

  // 缓存字段一致性：DoCast 预烘焙 taken_phys_pct = 5% * 3 = 15%
  const auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->taken_phys_pct == doctest::Approx(15.0f));

  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 120.0f, 100.0f);
  registry.emplace<CombatStats>(enemy);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Physical, false);

  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const auto *debuff = effects->Get("SwordPressure");
  REQUIRE(debuff != nullptr);
  CHECK(debuff->is_debuff == true);
  REQUIRE(!debuff->modifiers.empty());
  CHECK(debuff->modifiers[0].type == StatType::ResistPhysical);
  // 3 pts * 5% = -15%
  CHECK(debuff->modifiers[0].value == doctest::Approx(-15.0f));
}

TEST_CASE("[Functional] Skill 3 - Node 334 Crush Stun and Armor Shred") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{330, 1}, {334, 3}});
  CastBladeFormation(registry, player);

  // 缓存字段一致性：3 点碎岩击晕几率封顶 100%
  const auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);
  CHECK(formation->crush_stun_chance == doctest::Approx(1.0f));

  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 120.0f, 100.0f);
  registry.emplace<CombatStats>(enemy);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Physical, false);

  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const auto *shred = effects->Get("ArmorShred");
  REQUIRE(shred != nullptr);
  CHECK(shred->is_debuff == true);
  REQUIRE(!shred->modifiers.empty());
  CHECK(shred->modifiers[0].type == StatType::Armor);
  CHECK(shred->modifiers[0].value == doctest::Approx(-10.0f));
}

TEST_CASE("[Functional] Skill 3 - Node 354 Spell Echo Simultaneous Firing") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{354, 1}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 3);

  // Cast non-skill-3 skill (e.g. skill 1)
  auto execEnt = registry.create();
  registry.emplace<LocalLevelTag>(execEnt);
  auto &exec = registry.emplace<SkillExecution>(execEnt);
  exec.skill_id = 1;
  exec.owner = player;
  exec.state = SkillState::Preparing;
  exec.timer = 0.01f;
  exec.target_pos = {200.0f, 100.0f};

  // Run UpdateStates to trigger Spell Echo
  SkillSystem::UpdateStates(registry, 0.05f);

  // Verify that shadows were spawned as echoes
  auto shadowView = registry.view<ShadowComponent, Position>();
  int shadowCount = 0;
  for (auto sEnt : shadowView) {
    const auto &sc = shadowView.get<ShadowComponent>(sEnt);
    // Echo damage scale should be 0.20f
    if (std::abs(sc.damage_scale - 0.20f) < 0.01f) {
      shadowCount++;
    }
  }
  CHECK(shadowCount == 3);
}

TEST_CASE("[Functional] Skill 3 - Node 355 Sword Array Resonance Haste") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{355, 1}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(!swords.empty());
  auto sword = swords[0];
  auto &ai = registry.get<SpiritSwordAI>(sword);
  ai.attack_timer = 2.0f;

  // Create an active SwordArrayComponent owned by player
  auto arrEnt = registry.create();
  registry.emplace<Position>(arrEnt, 100.0f, 100.0f);
  auto &arr = registry.emplace<SwordArrayComponent>(arrEnt);
  arr.owner = player;
  arr.radius = 150.0f;

  systems::SpatialHashGrid grid(100, 100, 50);
  // Update 1.0s -> haste boosts effective dt by 1.5x -> timer decreases by 1.5s
  systems::SummonAISystem::Update(registry, 1.0f, grid);

  // 2.0 - 1.5 = 0.5f
  CHECK(ai.attack_timer == doctest::Approx(0.5f));
}

TEST_CASE("[Functional] Skill 3 - Nodes 370 & 371 Ignite Magnitude & Duration Scaling") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  // 300 (4 pts -> 7 swords), 370 (Fire), 371 (3 pts -> +60% ignite duration)
  auto player = CreateTestPlayer(registry, {{300, 4}, {370, 1}, {371, 3}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 7);

  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 120.0f, 100.0f);
  registry.emplace<CombatStats>(enemy);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Fire, false);

  const auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  // Find ignite buff (type == Burn)
  const BuffEffect *ignite = nullptr;
  for (const auto &b : effects->effects) {
    if (b.type == BuffType::Burn || b.id.find("ignite") != std::string::npos) {
      ignite = &b;
      break;
    }
  }
  REQUIRE(ignite != nullptr);
  // Base 3.0s * (1 + 0.60) = 4.80s
  CHECK(ignite->duration == doctest::Approx(4.80f));
  // Magnitude: base 15.0f + 7 swords * 10.0f = 85.0f
  CHECK(ignite->tick_damage == doctest::Approx(85.0f));
}

TEST_CASE("[Functional] Skill 3 - Nodes 372 & 373 Shock Application & Arc Chain Lightning") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{372, 1}, {373, 3}});
  CastBladeFormation(registry, player);

  auto enemyA = registry.create();
  registry.emplace<EnemyTag>(enemyA);
  registry.emplace<Position>(enemyA, 120.0f, 100.0f);
  auto &statsA = registry.emplace<CombatStats>(enemyA);
  statsA.health = 500.0f;
  statsA.max_health = 500.0f;
  registry.emplace<HealthComponent>(enemyA, 500.0f, 500.0f);

  auto enemyB = registry.create();
  registry.emplace<EnemyTag>(enemyB);
  registry.emplace<Position>(enemyB, 150.0f, 100.0f); // 30 units from enemyA
  auto &statsB = registry.emplace<CombatStats>(enemyB);
  statsB.health = 500.0f;
  statsB.max_health = 500.0f;
  registry.emplace<HealthComponent>(enemyB, 500.0f, 500.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemyA, Tag::Lightning, false);

  // Enemy A should have Shock
  const auto *effectsA = registry.try_get<ActiveEffectsComponent>(enemyA);
  REQUIRE(effectsA != nullptr);
  bool hasShock = false;
  for (const auto &b : effectsA->effects) {
    if (b.type == BuffType::Shock || b.id.find("shock") != std::string::npos) {
      hasShock = true;
      break;
    }
  }
  CHECK(hasShock == true);

  // Enemy B should have taken chain lightning damage
  const auto &hpB = registry.get<HealthComponent>(enemyB);
  CHECK(hpB.current < 500.0f);
}

TEST_CASE("[Functional] Skill 3 - Node 374 Spirit Corrosion Elemental Resistance Shred") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  // Node 370 (Fire), Node 374 (4 pts -> -8 shred/stack, max 8)
  auto player = CreateTestPlayer(registry, {{370, 1}, {374, 4}});
  CastBladeFormation(registry, player);

  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 120.0f, 100.0f);
  registry.emplace<CombatStats>(enemy);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // Hit 1: applies Ignite. Ailment is applied in hit 1, so subsequent check finds ailment and stacks shred
  hitFunc(registry, player, enemy, Tag::Fire, false);

  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  auto *shred = effects->Get("SpiritCorrosion_Fire");
  REQUIRE(shred != nullptr);
  CHECK(shred->stacks == 1);
  CHECK(shred->modifiers[0].value == doctest::Approx(-8.0f));

  // Hit 2
  hitFunc(registry, player, enemy, Tag::Fire, false);
  shred = effects->Get("SpiritCorrosion_Fire");
  CHECK(shred->stacks == 2);
  CHECK(shred->modifiers[0].value == doctest::Approx(-16.0f));

  // Stack up to 8 times
  for (int i = 0; i < 10; ++i) {
    hitFunc(registry, player, enemy, Tag::Fire, false);
  }
  shred = effects->Get("SpiritCorrosion_Fire");
  CHECK(shred->stacks == 8); // Capped at 8
  CHECK(shred->modifiers[0].value == doctest::Approx(-64.0f));
}

TEST_CASE("[Functional] Skill 3 - Node 375 Charge Counter & Elemental Burst") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  // Node 370 (Fire), Node 375 (3 pts -> hitsRequired = 5 - 3 = 2)
  auto player = CreateTestPlayer(registry, {{370, 1}, {375, 3}});
  CastBladeFormation(registry, player);

  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 120.0f, 100.0f);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.health = 1000.0f;
  stats.max_health = 1000.0f;
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);

  // Hit 1: counter becomes 1
  hitFunc(registry, player, enemy, Tag::Fire, false);
  CHECK(formation->charge_attack_counter == 1);

  // Hit 2: counter reaches 2 -> triggers burst and resets counter to 0
  hitFunc(registry, player, enemy, Tag::Fire, false);
  CHECK(formation->charge_attack_counter == 0);

  // Enemy should have taken burst damage
  const auto &hp = registry.get<HealthComponent>(enemy);
  CHECK(hp.current < 1000.0f);
}

} // namespace NoMoreDay
