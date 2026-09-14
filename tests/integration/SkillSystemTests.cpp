#pragma once
#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/systems/physics/PhysicsSystem.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/BoomerangDeliverySystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <entt/entt.hpp>
#include <unordered_set>
#include "raylib.h"


namespace NoMoreDay {

TEST_CASE("[Integration] SkillSystem - Registry Loading") {
  LoggerScope scope;
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  SUBCASE("Load Flowing Thrust") {
    const auto *skill = registry.GetSkill(1);
    REQUIRE(skill != nullptr);
    CHECK(skill->name_key == "流云刺");
    CHECK(skill->mana_cost == 5.0f);
    CHECK(HasTag(skill->tags, Tag::Physical));
  }

  SUBCASE("Load Rending Wave") {
    const auto *skill = registry.GetSkill(2);
    REQUIRE(skill != nullptr);
    CHECK(HasTag(skill->tags, Tag::Projectile));
    CHECK(skill->max_charges == 0);
  }
}

TEST_CASE("[Integration] SkillSystem - Execution Logic") {
  LoggerScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Init();
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 100.0f;
  active.slots[0].id = 1;

  SUBCASE("Charges and Cooldown") {
    active.slots[0].current_charges = 1;
    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(active.slots[0].current_charges == 0);

    SkillSystem::Update(registry, grid, 0.5f);
    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
  }

  SUBCASE("State Machine - Unregistered Cast Takes Warning Path") {
    auto oldCast = SkillBehaviorRegistry::GetCast(1);
    SkillBehaviorRegistry::RegisterCast(1, nullptr);

    active.slots[0].current_charges = 1;
    SkillSystem::TryCast(registry, player, 0);
    SkillSystem::Update(registry, grid, 0.11f);

    SkillBehaviorRegistry::RegisterCast(1, oldCast);
  }
}

TEST_CASE("[Integration] SkillSystem - Seven Star Slash signature gating") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto &combat = registry.emplace<CombatStats>(player);
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 10;
  active.slots[0].current_charges = 1;

  CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::SwordSaint));
  active.slots[0].current_charges = 1;

  CHECK(SkillSystem::TryCast(registry, player, 0));
}

TEST_CASE("[Integration] SkillSystem - Heavenly Sword signature gating") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto &combat = registry.emplace<CombatStats>(player);
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 11;
  active.slots[0].current_charges = 1;

  CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::HeavenlySword));
  active.slots[0].current_charges = 1;

  CHECK(SkillSystem::TryCast(registry, player, 0));
}

TEST_CASE("[Integration] SkillSystem - Blood Sea signature gating and life spend") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto &combat = registry.emplace<CombatStats>(player);
  combat.max_health = 200.0f;
  combat.health = 200.0f;
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 12;
  active.slots[0].current_charges = 1;

  CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::DemonBlade));
  active.slots[0].current_charges = 1;

  CHECK(SkillSystem::TryCast(registry, player, 0));
  CHECK(registry.get<CombatStats>(player).health == doctest::Approx(110.0f));
  CHECK(registry.get<CombatStats>(player).mana == doctest::Approx(100.0f));
  CHECK(registry.get<BladeResourceComponent>(player).current == 1);
}

TEST_CASE("[Integration] Blade Mastery - Heavenly Sword descent spends tiers and creates field") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto &combat = registry.emplace<CombatStats>(player);
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;
  combat.min_weapon_damage = 40.0f;
  combat.max_weapon_damage = 40.0f;

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 11;

  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::HeavenlySword));
  registry.get<BladeMasteryComponent>(player).heavenly_attunement =
      BladeAttunement::Lightning;
  REQUIRE(systems::BladeResourceService::Gain(registry, player, 6, 11u));

  SkillExecution exec;
  exec.skill_id = 11;
  exec.owner = player;
  exec.target_pos = {120.0f, 0.0f};

  auto castFunc = SkillBehaviorRegistry::GetCast(11);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);

  CHECK(registry.get<BladeResourceComponent>(player).current == 1);

  auto view = registry.view<HeavenlySwordFieldComponent, Position>();
  REQUIRE(view.begin() != view.end());
  const auto field = *view.begin();
  const auto &fieldComp = view.get<HeavenlySwordFieldComponent>(field);
  CHECK(fieldComp.header.owner == player);
  CHECK(fieldComp.spent_tiers == 5);
  CHECK(fieldComp.attunement == BladeAttunement::Lightning);
  CHECK(fieldComp.header.duration == doctest::Approx(5.0f));
  CHECK(fieldComp.header.radius > 0.0f);
}

TEST_CASE("[Integration] SkillSystem - Heavenly Sword impact and cycle nodes close remaining runtime gaps") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  struct Outcome {
    float distant_health_after_cast = 0.0f;
    float elite_health_after_delayed_window = 0.0f;
    float formation_attack_interval = 0.0f;
    float infinite_blades_damage_mult = 0.0f;
    float afflicted_health = 0.0f;
    float afflicted_remaining = 0.0f;
  };

  const auto exerciseCase = [&](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64);

    const auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);

    auto &stats = registry.emplace<PlayerStats>(player);
    stats.level = 50;

    auto &combat = registry.emplace<CombatStats>(player);
    combat.max_mana = 100.0f;
    combat.mana = 100.0f;
    combat.min_weapon_damage = 40.0f;
    combat.max_weapon_damage = 40.0f;

    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    for (const auto &[nodeId, points] : nodes) {
      active.specialized_slots[0].allocated_points[nodeId] = points;
    }

    systems::BladeMasteryService::RefreshPlayerState(registry, player);
    REQUIRE(systems::BladeMasteryService::SelectMastery(
        registry, player, BladeMasteryId::HeavenlySword));
    registry.get<BladeMasteryComponent>(player).heavenly_attunement =
        BladeAttunement::Fire;
    REQUIRE(systems::BladeResourceService::Gain(registry, player, 3, 11u));

    SkillExecution formationExec;
    formationExec.skill_id = 3;
    formationExec.owner = player;
    auto formationCast = SkillBehaviorRegistry::GetCast(3);
    REQUIRE(formationCast != nullptr);
    formationCast(registry, player, formationExec);

    // 高血量确保目标在完整伤害管线下的技能冲击中存活，从而真正执行场域持续 tick
    // 与节点效果路径（回鞘加成、异常刷新），避免目标提前死亡导致的空目标早退。
    const auto eliteTarget = registry.create();
    registry.emplace<Position>(eliteTarget, 20.0f, 0.0f);
    registry.emplace<EnemyTag>(eliteTarget);
    registry.emplace<HealthComponent>(eliteTarget, 2000000.0f, 2000000.0f);
    registry.emplace<CombatStats>(eliteTarget);
    registry.emplace<EnemyRarityComponent>(eliteTarget, EnemyRarityComponent::BOSS);

    const auto afflictedTarget = registry.create();
    registry.emplace<Position>(afflictedTarget, 24.0f, 0.0f);
    registry.emplace<EnemyTag>(afflictedTarget);
    registry.emplace<HealthComponent>(afflictedTarget, 2000000.0f, 2000000.0f);
    registry.emplace<CombatStats>(afflictedTarget);
    auto &afflictedEffects = registry.emplace<ActiveEffectsComponent>(afflictedTarget);
    BuffEffect ignite;
    ignite.id = "integration_ignite";
    ignite.name = "Integration Ignite";
    ignite.type = BuffType::Burn;
    ignite.duration = 3.0f;
    ignite.remaining = 0.2f;
    ignite.is_debuff = true;
    afflictedEffects.AddOrRefresh(ignite);

    const auto distantTarget = registry.create();
    registry.emplace<Position>(distantTarget, 140.0f, 0.0f);
    registry.emplace<EnemyTag>(distantTarget);
    registry.emplace<HealthComponent>(distantTarget, 2000.0f, 2000.0f);
    registry.emplace<CombatStats>(distantTarget);

    SkillExecution descentExec;
    descentExec.skill_id = 11;
    descentExec.owner = player;
    descentExec.cast_id = 71118001u + static_cast<uint64_t>(nodes.size());
    descentExec.target_pos = {20.0f, 0.0f};
    auto descentCast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(descentCast != nullptr);
    descentCast(registry, player, descentExec);

    Outcome outcome;
    outcome.distant_health_after_cast = registry.get<HealthComponent>(distantTarget).current;

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 1.01f);

    SkillExecution infiniteExec;
    infiniteExec.skill_id = 5;
    infiniteExec.owner = player;
    infiniteExec.target_pos = {20.0f, 0.0f};
    auto infiniteCast = SkillBehaviorRegistry::GetCast(5);
    REQUIRE(infiniteCast != nullptr);
    infiniteCast(registry, player, infiniteExec);

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.26f);

    outcome.elite_health_after_delayed_window =
        registry.get<HealthComponent>(eliteTarget).current;
    outcome.formation_attack_interval =
        registry.get<BladeFormationComponent>(player).attack_interval;
    REQUIRE(registry.all_of<BeamChannelComponent>(player));
    outcome.infinite_blades_damage_mult =
        registry.get<BeamChannelComponent>(player).bonus_damage_mult;
    outcome.afflicted_health = registry.get<HealthComponent>(afflictedTarget).current;
    auto *refreshedIgnite = afflictedEffects.Get("integration_ignite");
    REQUIRE(refreshedIgnite != nullptr);
    outcome.afflicted_remaining = refreshedIgnite->remaining;
    return outcome;
  };

  const auto baseline = exerciseCase({{1113, 1}});
  const auto improved = exerciseCase(
      {{1100, 2}, {1104, 2}, {1105, 2}, {1106, 2}, {1107, 1}, {1108, 2}, {1112, 2}, {1113, 1}, {1114, 2}, {1118, 2}});

  CHECK(improved.distant_health_after_cast < baseline.distant_health_after_cast);
  CHECK(improved.elite_health_after_delayed_window < baseline.elite_health_after_delayed_window);
  CHECK(improved.formation_attack_interval < baseline.formation_attack_interval);
  CHECK(improved.infinite_blades_damage_mult > baseline.infinite_blades_damage_mult);
  CHECK(improved.afflicted_health < baseline.afflicted_health);
  CHECK(improved.afflicted_remaining > baseline.afflicted_remaining);
}

TEST_CASE("[Integration] Blade Mastery - Blood Sea consumes Bloodthirst and creates pursuit field") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 10.0f, 20.0f);

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto &combat = registry.emplace<CombatStats>(player);
  combat.max_health = 200.0f;
  combat.health = 90.0f;
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;
  combat.min_weapon_damage = 40.0f;
  combat.max_weapon_damage = 40.0f;

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 12;
  active.specialized_slots[0].allocated_points = {{1217, 1}, {1224, 2}};

  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::DemonBlade));
  REQUIRE(systems::BladeResourceService::Gain(registry, player, 6, 12u));

  SkillExecution exec;
  exec.skill_id = 12;
  exec.owner = player;
  exec.target_pos = {60.0f, 20.0f};

  auto castFunc = SkillBehaviorRegistry::GetCast(12);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);

  CHECK(registry.get<BladeResourceComponent>(player).current == 0);

  auto view = registry.view<BloodSeaFieldComponent, Position>();
  REQUIRE(view.begin() != view.end());
  const auto field = *view.begin();
  const auto &fieldComp = view.get<BloodSeaFieldComponent>(field);
  const auto &fieldPos = view.get<Position>(field);
  CHECK(fieldComp.header.owner == player);
  CHECK(fieldComp.consumed_bloodthirst == 6);
  CHECK(fieldComp.header.duration == doctest::Approx(6.0f));
  CHECK(fieldComp.leech_ratio > 0.0f);
  CHECK(fieldPos.x == doctest::Approx(10.0f));
  CHECK(fieldPos.y == doctest::Approx(20.0f));
}

TEST_CASE("[Integration] Blade Mastery - Sword Saint combat loop") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  auto target = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<Position>(target, 32.0f, 0.0f);

  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;

  auto& combat = registry.emplace<CombatStats>(player);
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;
  combat.min_weapon_damage = 30.0f;
  combat.max_weapon_damage = 30.0f;
  registry.emplace<CombatStats>(target);

  auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 1;
  active.slots[0].cooldown = 2.0f;
  active.slots[1].id = 2;
  active.slots[1].cooldown = 3.0f;
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[1].skill_id = 2;
  active.specialized_slots[1].allocated_points[251] = 1;

  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::SwordSaint));

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(
                    player, target, 1, Tag::Melee | Tag::Physical | Tag::Hit,
                    false));
  CHECK(registry.get<BladeResourceComponent>(player).current >= 2);
  // 负向断言：旧私有特例"命中削减技能 2 CD 0.75s"已删除，命中后技能 2 CD 保持不变
  CHECK(active.slots[1].cooldown == doctest::Approx(3.0f));

  // 获得足够剑意达到 251 剑意爆发阈值 (>= 5)
  REQUIRE(systems::BladeResourceService::Gain(registry, player, 3, 10u));
  CHECK(registry.get<BladeResourceComponent>(player).current >= 5);

  SkillExecution rendingExec;
  rendingExec.skill_id = 2;
  rendingExec.owner = player;
  rendingExec.target_pos = {32.0f, 0.0f};
  auto rendingCast = SkillBehaviorRegistry::GetCast(2);
  REQUIRE(rendingCast != nullptr);
  rendingCast(registry, player, rendingExec);
  CHECK(rendingExec.is_empowered);
  CHECK(registry.get<BladeResourceComponent>(player).current == 0);
  // 负向断言：旧私有特例"消耗剑意削减技能 1 CD 1.5s"已删除，技能 1 CD 保持不变
  CHECK(active.slots[0].cooldown == doctest::Approx(2.0f));

  REQUIRE(systems::BladeResourceService::Gain(registry, player, 3, 10u));
  SkillExecution sevenStarExec;
  sevenStarExec.skill_id = 10;
  sevenStarExec.owner = player;
  sevenStarExec.target_pos = {32.0f, 0.0f};
  auto sevenStarCast = SkillBehaviorRegistry::GetCast(10);
  REQUIRE(sevenStarCast != nullptr);
  sevenStarCast(registry, player, sevenStarExec);
  CHECK(registry.get<BladeResourceComponent>(player).current == 0);
}

TEST_CASE("[Integration] Blade Mastery - Sword Saint restart window") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  auto target = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<Position>(target, 32.0f, 0.0f);

  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;
  registry.emplace<CombatStats>(player);
  registry.emplace<CombatStats>(target);

  auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::SwordSaint));

  REQUIRE(systems::BladeResourceService::Gain(registry, player, 10, 10u));
  auto sevenStarCast = SkillBehaviorRegistry::GetCast(10);
  REQUIRE(sevenStarCast != nullptr);
  SkillExecution sevenStarExec;
  sevenStarExec.skill_id = 10;
  sevenStarExec.owner = player;
  sevenStarExec.target_pos = {32.0f, 0.0f};
  sevenStarCast(registry, player, sevenStarExec);

  const auto& afterSpend = registry.get<BladeResourceComponent>(player);
  CHECK(afterSpend.current == 0);
  CHECK(afterSpend.restart_window_ready);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(
                    player, target, 1, Tag::Melee | Tag::Physical | Tag::Hit,
                    false));

  const auto& afterRestart = registry.get<BladeResourceComponent>(player);
  CHECK(afterRestart.current >= 4);
  CHECK_FALSE(afterRestart.restart_window_ready);
}

TEST_CASE("[Integration] Blade Mastery - full-flow release resets Flowing Thrust") {
  TestSetupScope testScope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 50;
  auto& combat = registry.emplace<CombatStats>(player);
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;

  auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 1;
  active.slots[0].cooldown = 4.0f;
  active.specialized_slots[0].skill_id = 2;
  active.specialized_slots[0].allocated_points[251] = 1;

  systems::BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(systems::BladeMasteryService::SelectMastery(
      registry, player, BladeMasteryId::SwordSaint));

  REQUIRE(systems::BladeResourceService::Gain(registry, player, 10, 2u));
  auto rendingCast = SkillBehaviorRegistry::GetCast(2);
  REQUIRE(rendingCast != nullptr);
  SkillExecution rendingExec;
  rendingExec.skill_id = 2;
  rendingExec.owner = player;
  rendingExec.target_pos = {120.0f, 0.0f};
  rendingCast(registry, player, rendingExec);
  CHECK(active.slots[0].cooldown == doctest::Approx(0.0f));

  active.slots[0].cooldown = 4.0f;
  REQUIRE(systems::BladeResourceService::Gain(registry, player, 10, 10u));
  auto sevenStarCast = SkillBehaviorRegistry::GetCast(10);
  REQUIRE(sevenStarCast != nullptr);
  SkillExecution sevenStarExec;
  sevenStarExec.skill_id = 10;
  sevenStarExec.owner = player;
  sevenStarExec.target_pos = {120.0f, 0.0f};
  sevenStarCast(registry, player, sevenStarExec);
  CHECK(active.slots[0].cooldown == doctest::Approx(0.0f));
}

TEST_CASE("[Integration] SkillSystem - Sword Intent Logic") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Init();
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.decay_interval = 1.0f;
  intent.grace_period = 0.1f;

  SUBCASE("Accumulation") {
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, entt::null, 1,
                      Tag::Melee | Tag::Physical | Tag::Hit, false));
    CHECK(intent.stacks == 1);
  }

  SUBCASE("Decay Logic") {
    intent.stacks = 5;
    intent.grace_period = 1.0f;
    intent.time_since_last_gain = 0.0f;

    // Update roughly 0.5s - shouldn't decay
    // Need to pass executor pointer as nullptr since it's now required in
    // Update signature? Let's check SkillSystem::Update signature. void
    // Update(entt::registry &registry, systems::SpatialHashGrid &grid, float
    // dt, tf::Executor *executor = nullptr); It seems it usually takes
    // executor.
    SkillSystem::Update(registry, grid, 0.5f);
    CHECK(intent.stacks == 5);

    // Update another 0.6s -> total 1.1s > 1.0s grace -> clear all
    SkillSystem::Update(registry, grid, 0.6f);
    CHECK(intent.stacks == 0);
  }

  SUBCASE("No Passive Gain") {
    intent.stacks = 0;
    intent.gain_rate = 1.0f;

    SkillSystem::Update(registry, grid, 2.0f); // 2 seconds
    CHECK(intent.stacks == 0);
  }

  SUBCASE("Empowered Cast") {
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;
    intent.stacks = 10;

    SkillSystem::TryCast(registry, player, 0);
    SkillSystem::Update(registry, grid, 0.11f);

    CHECK(intent.stacks == 0);
    // SkillExecution might be finished (removed) by now, so we can't reliably check it.
    // But stacks == 0 implies the hook ran.
  }

  SUBCASE("Blade resource bridge gain uses mastery-owned runtime") {
    data::BladeMasteryRegistry::Get().LoadFromJson("assets/data/blade_masteries.json");
    auto &stats = registry.emplace<PlayerStats>(player);
    stats.level = 50;
    auto &combat = registry.emplace<CombatStats>(player);
    combat.max_mana = 100.0f;
    combat.mana = 100.0f;
    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

    systems::BladeMasteryService::RefreshPlayerState(registry, player);
    REQUIRE(systems::BladeMasteryService::SelectMastery(
        registry, player, BladeMasteryId::SwordSaint));

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, entt::null, 1,
                      Tag::Melee | Tag::Physical | Tag::Hit, false));

    REQUIRE(registry.all_of<BladeResourceComponent, SwordIntentComponent>(player));
    CHECK(registry.get<BladeResourceComponent>(player).kind ==
          BladeResourceKind::SwordFlow);
    CHECK(registry.get<BladeResourceComponent>(player).current == 2);
    CHECK(registry.get<SwordIntentComponent>(player).stacks == 2);
  }

  SUBCASE("Sword Flow full resource empowers base cast") {
    data::BladeMasteryRegistry::Get().LoadFromJson("assets/data/blade_masteries.json");
    auto &stats = registry.emplace<PlayerStats>(player);
    stats.level = 50;
    auto &combat = registry.emplace<CombatStats>(player);
    combat.max_mana = 100.0f;
    combat.mana = 100.0f;
    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;

    systems::BladeMasteryService::RefreshPlayerState(registry, player);
    REQUIRE(systems::BladeMasteryService::SelectMastery(
        registry, player, BladeMasteryId::SwordSaint));
    REQUIRE(systems::BladeResourceService::Gain(
        registry, player, SkillConstants::DEFAULT_MAX_SWORD_INTENT, 1u));

    REQUIRE(SkillSystem::TryCast(registry, player, 0));
    SkillSystem::Update(registry, grid, 0.11f);

    CHECK(registry.get<BladeResourceComponent>(player).current == 0);
    CHECK(registry.get<SwordIntentComponent>(player).stacks == 0);
  }
}

TEST_CASE("[Integration] SkillSpecialization - Talent Allocation Logic") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 10;
  active.specialized_slots[0].skill_id = 1;

  SUBCASE("Point Allocation and Reset") {
    CHECK(SkillSystem::AddTalentPoint(registry, player, 1, 100));
    CHECK(active.available_talent_points == 9);
    CHECK(SkillSystem::ResetTalents(registry, player, 1));
    CHECK(active.available_talent_points == 10);
  }
}

TEST_CASE("[Integration] SkillSpecialization - Runtime state cleanup on reset/clear") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 0;

  SUBCASE("ResetTalents clears target skill runtime state only") {
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[134] = 1;
    active.specialized_slots[0].allocated_points[170] = 1;
    active.specialized_slots[1].skill_id = 2;
    active.specialized_slots[1].allocated_points[233] = 1;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[1] = 170;
    runtime.active_transmuter_node_by_skill[2] = 270;
    runtime.trigger_cooldowns[134] = 1.5f;
    runtime.trigger_cooldowns[233] = 2.0f;

    CHECK(SkillSystem::ResetTalents(registry, player, 1));
    CHECK(active.available_talent_points == 2);
    CHECK_FALSE(runtime.active_transmuter_node_by_skill.contains(1));
    CHECK(runtime.active_transmuter_node_by_skill.contains(2));
    CHECK_FALSE(runtime.trigger_cooldowns.contains(134));
    CHECK(runtime.trigger_cooldowns.contains(233));
  }

  SUBCASE("ClearAllTalents clears all specialization runtime state") {
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[134] = 1;
    active.specialized_slots[1].skill_id = 8;
    active.specialized_slots[1].allocated_points[870] = 1;
    active.specialized_slots[1].allocated_points[871] = 1;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[1] = 170;
    runtime.active_transmuter_node_by_skill[8] = 870;
    runtime.trigger_cooldowns[134] = 1.0f;
    runtime.trigger_cooldowns[831] = 2.0f;

    CHECK(SkillSystem::ClearAllTalents(registry, player));
    CHECK(active.available_talent_points == 3);
    CHECK(runtime.active_transmuter_node_by_skill.empty());
    CHECK(runtime.trigger_cooldowns.empty());
  }
}

TEST_CASE("[Integration] SkillLogic - Specialized Behaviors") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(1000, 1000, 50);
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);

  SUBCASE("Boomerang") {
    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 100.0f, 0.0f);
    registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f);
    registry.emplace<Projectile>(proj_ent).owner = player;
    auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
    bc.phase = BoomerangPhase::Outward;
    bc.returnTimer = 0.5f;

    // Total time needed: 0.5s (timer) + hover window = 0.8s.
    // Two updates to ensure state transitions (Outward -> HoverApex -> Returning)
    // 单权威状态机：由交付系统推进，ProjectileSystem 不再处理回旋阶段
    BoomerangDeliverySystem::Update(registry, grid, 0.6f); // Outward -> HoverApex
    BoomerangDeliverySystem::Update(registry, grid, 0.3f); // HoverApex -> Returning
    CHECK(bc.phase == BoomerangPhase::Returning);
  }

  SUBCASE("Channeling - Infinite Blades") {
    auto &beam = registry.emplace<BeamChannelComponent>(player);
    beam.owner = player;
    beam.skill_id = 5;
    beam.mode = BeamChannelMode::BarrageEmitter;
    beam.channel_timer = 1.0f;
    beam.tick_interval = 0.1f;
    beam.tick_timer = 0.0f;

    SkillSystem::Update(registry, grid, 0.15f);
    CHECK(!registry.view<Projectile>().empty());
  }

  SUBCASE("Blade Formation - Talent 321") {
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 3;
    active.specialized_slots[0].allocated_points[321] = 1;

    auto &formation = registry.emplace<BladeFormationComponent>(player);
    formation.mana_on_hit = true; // Manually set flag normally set in DoCast

    registry.get<CombatStats>(player).mana = 10.0f;
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks(); // Re-register behavior dispatcher
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(player, entt::null, 3,
                                                     Tag::Hit, false));
    CHECK(registry.get<CombatStats>(player).mana == 12.0f);
  }
}

TEST_CASE("[Integration] Projectile - Snapshotting Logic") {
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();
  auto proj_ent = registry.create();

  auto &a_stats = registry.emplace<CombatStats>(attacker);
  a_stats.damage_multipliers[0] = 1.5f;

  auto &proj = registry.emplace<Projectile>(proj_ent);
  proj.owner = attacker;
  proj.snapshot = a_stats;
  registry.emplace<CombatStats>(proj_ent, proj.snapshot);

  a_stats.damage_multipliers[0] = 0.1f; // Attacker weakened

  CHECK(registry.get<CombatStats>(proj_ent).damage_multipliers[0] ==
        doctest::Approx(1.5f));
}

TEST_CASE("[Integration] SkillSystem - 935 逆命反噬 trigger chain in DeathSeal window") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  auto caster = registry.create();
  registry.emplace<PlayerTag>(caster);
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster);
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  active.slots[0].id = 9;
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].allocated_points[935] = 1;
  SkillSystem::RebakeSkillProfiles(registry, caster);

  auto *triggers = registry.try_get<TriggerRuleComponent>(caster);
  REQUIRE(triggers != nullptr);
  REQUIRE(triggers->HasRule(935));

  const TriggerRule *rule = nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == 935) {
      rule = &triggers->rules[i];
    }
  }
  REQUIRE(rule != nullptr);
  // 契约映射: 20% 基础概率 / 近战限定 / 逆脉窗口 / 0.5s 内置冷却
  CHECK(rule->base_chance == doctest::Approx(0.2f));
  CHECK(rule->requires_melee_hit);
  CHECK(rule->required_window == TriggerWindow::DeathSeal);
  CHECK(rule->internal_cooldown == doctest::Approx(0.5f));

  auto target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 100000.0f, 100000.0f);
  registry.emplace<CombatStats>(target);

  // 手动进入逆脉窗口: params.death_seal=true 且 remaining>0
  auto &pt = registry.emplace<PhantomTranceComponent>(caster);
  pt.remaining = 1.0f;
  pt.params.death_seal = true;

  auto resetCooldowns = [&]() {
    for (uint8_t i = 0; i < triggers->rule_count; ++i) {
      triggers->rules[i].current_cooldown = 0.0f;
    }
  };
  auto countTriggersAfter = [&](Tag tags, int samples) {
    int fired = 0;
    for (int i = 0; i < samples; ++i) {
      resetCooldowns();
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateSkillHit(caster, target, 8, tags,
                                                       false, 0));
      for (uint8_t r = 0; r < triggers->rule_count; ++r) {
        if (triggers->rules[r].rule_id == 935 &&
            triggers->rules[r].current_cooldown > 0.0f) {
          ++fired;
        }
      }
    }
    return fired;
  };

  constexpr int kSamples = 400;
  const int meleeFired = countTriggersAfter(Tag::Melee, kSamples);
  // 20% 期望约 80 次; 二项分布 2σ 约 ±9, 放宽到 [40,130] 仍能拒绝“不触发/必触发”
  CHECK(meleeFired > 40);
  CHECK(meleeFired < 130);

  // 非近战命中不得触发
  CHECK(countTriggersAfter(Tag::Projectile, kSamples) == 0);

  // 非逆脉窗口(仅形态窗口)不得触发
  pt.params.death_seal = false;
  CHECK(countTriggersAfter(Tag::Melee, kSamples) == 0);

  // 窗口彻底消失(remaining<=0)也不得触发
  pt.params.death_seal = true;
  pt.remaining = 0.0f;
  CHECK(countTriggersAfter(Tag::Melee, kSamples) == 0);
}

TEST_CASE(
    "[Integration] SkillBehaviorRegistry - Blade Ascendant key branches run") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player).mana = 500.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 2;
  active.specialized_slots[0].allocated_points[230] = 1; // Rending boomerang
  active.specialized_slots[1].skill_id = 8;
  active.specialized_slots[1].allocated_points[830] = 1; // Boomerang side blades

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);
    CHECK(SkillBehaviorRegistry::HasBehavior(skill_id));
  }

  SUBCASE("Rending Wave boomerang branch creates boomerang projectiles") {
    SkillExecution exec;
    exec.skill_id = 2;
    exec.owner = player;
    exec.cast_id = 501;
    exec.target_pos = {120.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(2);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto boomerangs = registry.view<Projectile, BoomerangComponent>();
    CHECK(boomerangs.begin() != boomerangs.end());
  }

  SUBCASE("Blade Formation giant sword branch toggles giant state") {
    SkillExecution exec;
    exec.skill_id = 3;
    exec.owner = player;
    exec.cast_id = 502;
    exec.target_pos = {40.0f, 0.0f};
    exec.active_nodes.set(330 % 100);

    auto cast = SkillBehaviorRegistry::GetCast(3);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    REQUIRE(registry.all_of<BladeFormationComponent>(player));
    CHECK(registry.get<BladeFormationComponent>(player).has_giant_sword);
  }

  SUBCASE("Blade Boomerang side-blade branch emits multi projectiles") {
    SkillExecution exec;
    exec.skill_id = 8;
    exec.owner = player;
    exec.cast_id = 503;
    exec.target_pos = {150.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(8);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto projectiles = registry.view<Projectile, BoomerangComponent>();
    int count = 0;
    for (auto entity : projectiles) {
      (void)entity;
      ++count;
    }
    CHECK(count >= 3);
  }

  SUBCASE("Phantom Trance main branch enters form") {
    SkillExecution exec;
    exec.skill_id = 9;
    exec.owner = player;
    exec.cast_id = 504;
    exec.target_pos = {80.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(9);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    REQUIRE(registry.all_of<PhantomTranceComponent>(player));
    const auto &pt = registry.get<PhantomTranceComponent>(player);
    CHECK(pt.remaining > 0.0f);
    CHECK(pt.params.duration_sec == doctest::Approx(3.0f));
  }
}

TEST_CASE("[Integration] SkillSystem - Boomerang Catch node restores resources") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 10.0f;
  stats.max_mana = 100.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 8;
  active.slots[0].cooldown = 2.0f;
  active.specialized_slots[0].skill_id = 8;
  active.specialized_slots[0].allocated_points[832] = 2;

  auto projEnt = registry.create();
  registry.emplace<Position>(projEnt, 0.5f, 0.0f);
  registry.emplace<Velocity>(projEnt, -10.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projEnt);
  proj.owner = player;
  proj.radius = 12.0f;
  proj.speed = 400.0f;
  proj.lifeTime = 2.0f;
  registry.emplace<SkillComponent>(projEnt, 8u, player);
  auto &boom = registry.emplace<BoomerangComponent>(projEnt);
  boom.owner = player;
  boom.phase = BoomerangPhase::Returning;
  boom.returnSpeed = 400.0f;
  // 832 接剑回蓝：数值由烘培层写入，交付层只读组件字段
  boom.catch_mana = 5.0f;

  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK(stats.mana > 10.0f);
  CHECK_FALSE(registry.valid(projEnt));
}

TEST_CASE("[Integration] SkillSystem - BladeWard interception counter loop") {
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50);

  auto defender = registry.create();
  registry.emplace<PlayerTag>(defender);
  registry.emplace<Position>(defender, 0.0f, 0.0f);
  registry.emplace<CombatStats>(defender);
  registry.emplace<SwordIntentComponent>(defender).stacks = 0;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.sword_count = 2;
  ward.interception_chance = 1.0f;
  ward.trigger_counter = true;
  ward.is_lightning_ward = true;
  ward.counter_damage_more = 0.4f;

  auto attacker = registry.create();
  registry.emplace<EnemyTag>(attacker);
  registry.emplace<Position>(attacker, 2.0f, 0.0f);
  registry.emplace<CombatStats>(attacker);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

  auto projEnt = registry.create();
  registry.emplace<Position>(projEnt, 0.0f, 0.0f);
  registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projEnt);
  proj.owner = attacker;
  proj.radius = 20.0f;
  proj.speed = 0.0f;
  proj.lifeTime = 1.0f;
  registry.emplace<SkillComponent>(projEnt, 2u, attacker);

  ProjectileSystem::Update(registry, grid, 0.016f);

  CHECK(registry.get<HealthComponent>(attacker).current < 100.0f);
}

TEST_CASE("[Integration] SkillSystem - BladeWard 412 is_solidified prevents sword consumption") {
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50);

  // Case A: is_solidified = false -> sword_count decrements from 2 to 1
  {
    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    registry.emplace<Position>(defender, 0.0f, 0.0f);
    registry.emplace<CombatStats>(defender);
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 2;
    ward.interception_chance = 1.0f;
    ward.is_solidified = false;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    registry.emplace<Position>(attacker, 2.0f, 0.0f);
    registry.emplace<CombatStats>(attacker);

    auto projEnt = registry.create();
    registry.emplace<Position>(projEnt, 0.0f, 0.0f);
    registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
    auto &proj = registry.emplace<Projectile>(projEnt);
    proj.owner = attacker;
    proj.radius = 20.0f;
    proj.speed = 0.0f;
    proj.lifeTime = 1.0f;
    registry.emplace<SkillComponent>(projEnt, 2u, attacker);

    ProjectileSystem::Update(registry, grid, 0.016f);
    CHECK(ward.sword_count == 1);
  }

  // Case B: is_solidified = true -> sword_count stays 2
  {
    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    registry.emplace<Position>(defender, 0.0f, 0.0f);
    registry.emplace<CombatStats>(defender);
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 2;
    ward.interception_chance = 1.0f;
    ward.is_solidified = true;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    registry.emplace<Position>(attacker, 2.0f, 0.0f);
    registry.emplace<CombatStats>(attacker);

    auto projEnt = registry.create();
    registry.emplace<Position>(projEnt, 0.0f, 0.0f);
    registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
    auto &proj = registry.emplace<Projectile>(projEnt);
    proj.owner = attacker;
    proj.radius = 20.0f;
    proj.speed = 0.0f;
    proj.lifeTime = 1.0f;
    registry.emplace<SkillComponent>(projEnt, 2u, attacker);

    ProjectileSystem::Update(registry, grid, 0.016f);
    CHECK(ward.sword_count == 2);
  }
}

TEST_CASE("[Integration] SkillSystem - BladeWard interception_chance 0.5 honors dice") {
  TestSetupScope scope;
  // 固定随机种子驱动判定，避免真实随机造成抖动；ProjectileSystem 的 Blade Ward
  // 拦截掷骰走 raylib GetRandomValue(0, 1000)，SetRandomSeed 可复现该序列。
  SetRandomSeed(20260913u);

  constexpr int kTrials = 400;
  int interceptedCount = 0;

  for (int i = 0; i < kTrials; ++i) {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    registry.emplace<Position>(defender, 0.0f, 0.0f);
    registry.emplace<CombatStats>(defender);
    registry.emplace<SwordIntentComponent>(defender).stacks = 0;
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 1; // 单剑 × 0.5 = 恰好 0.5 触发概率，覆盖掷骰分支
    ward.interception_chance = 0.5f;
    ward.is_solidified = false;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    registry.emplace<Position>(attacker, 2.0f, 0.0f);
    registry.emplace<CombatStats>(attacker);
    registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

    auto projEnt = registry.create();
    registry.emplace<Position>(projEnt, 0.0f, 0.0f);
    registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
    auto &proj = registry.emplace<Projectile>(projEnt);
    proj.owner = attacker;
    proj.radius = 20.0f;
    proj.speed = 0.0f;
    proj.lifeTime = 1.0f;
    registry.emplace<SkillComponent>(projEnt, 2u, attacker);

    ProjectileSystem::Update(registry, grid, 0.016f);

    // 拦截成功会消耗唯一一柄飞剑，据此统计掷骰结果。
    if (ward.sword_count == 0) {
      ++interceptedCount;
    }
  }

  // 0.5 概率应双向覆盖：既非恒拦截也非恒穿透，且比例应接近一半。
  CHECK(interceptedCount > 0);
  CHECK(interceptedCount < kTrials);
  CHECK(interceptedCount >= kTrials * 35 / 100);
  CHECK(interceptedCount <= kTrials * 65 / 100);
}

TEST_CASE("[Integration] SkillSystem - BladeWard interception_chance 1.0 never misses") {
  TestSetupScope scope;
  // 回归护栏：GetRandomValue(0, 1000) 为闭区间，roll/1000 可达 1.0。
  // 旧判定 `roll/1000.0f < chance` 在 chance==1.0 且 roll==1000 时判负，
  // 导致 100% 拦截概率漏拦；修复后 chance>=1.0 直接判定生效。
  // 4000 次独立 trial 下，旧代码出现至少一次漏拦的概率约 98%，
  // 可有效捕获该边界回归；新代码路径不掷骰，恒定 100% 拦截。
  SetRandomSeed(20260914u);

  constexpr int kTrials = 4000;
  int interceptedCount = 0;

  for (int i = 0; i < kTrials; ++i) {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    registry.emplace<Position>(defender, 0.0f, 0.0f);
    registry.emplace<CombatStats>(defender);
    registry.emplace<SwordIntentComponent>(defender).stacks = 0;
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 1;
    ward.interception_chance = 1.0f;
    ward.is_solidified = false;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    registry.emplace<Position>(attacker, 2.0f, 0.0f);
    registry.emplace<CombatStats>(attacker);
    registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

    auto projEnt = registry.create();
    registry.emplace<Position>(projEnt, 0.0f, 0.0f);
    registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
    auto &proj = registry.emplace<Projectile>(projEnt);
    proj.owner = attacker;
    proj.radius = 20.0f;
    proj.speed = 0.0f;
    proj.lifeTime = 1.0f;
    registry.emplace<SkillComponent>(projEnt, 2u, attacker);

    ProjectileSystem::Update(registry, grid, 0.016f);

    // 拦截成功会消耗唯一一柄飞剑，据此统计判定结果。
    if (ward.sword_count == 0) {
      ++interceptedCount;
    }
  }

  // 100% 拦截概率必须每次生效，不允许任何一次掷骰边界漏拦。
  CHECK(interceptedCount == kTrials);
}

TEST_CASE("[Integration] SkillSystem - BladeWard 470 counter on Melee and Block") {
  TestSetupScope scope;
  entt::registry registry;
  // 与后续用例一致的夹具隔离：重置全局 Hook/单例，避免跨用例污染导致偶发失败。
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  // Melee hit triggers counter
  {
    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    // accuracy=1.0：默认命中率 0.97 会经 effective_dodge = dodge - (accuracy-1)
    // 产生 3% 有效闪避，可能让反击命中被闪避，导致用例偶发失败；此处固定必中。
    auto &defStats = registry.emplace<CombatStats>(defender);
    defStats.accuracy = 1.0f;
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.trigger_counter = true;
    ward.counter_damage_more = 0.2f;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    auto &atkStats = registry.emplace<CombatStats>(attacker);
    atkStats.accuracy = 1.0f;
    registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

    DamagePool pool;
    pool.Add(Tag::Physical, 10.0f);
    DamageRequest req;
    req.attacker = attacker;
    req.defender = defender;
    req.skill_id = 1;
    req.base_pool = pool;
    req.additional_tags = Tag::Hit | Tag::Melee;
    req.source_entity = attacker;

    DamagePipeline::CalculateBatch(registry, attacker, {defender}, 1, pool, req.additional_tags, attacker);
    CHECK(registry.get<HealthComponent>(attacker).current < 100.0f);
  }

  // Blocked hit triggers counter
  {
    auto defender = registry.create();
    registry.emplace<PlayerTag>(defender);
    auto &defStats = registry.emplace<CombatStats>(defender);
    defStats.block_chance = 1.0f; // 100% block
    // accuracy=1.0：默认命中率 0.97 会转化为 3% 有效闪避，闪避判定先于格挡，
    // 偶发跳过格挡/反击路径；固定必中以确保稳定命中 100% 格挡分支。
    defStats.accuracy = 1.0f;
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.trigger_counter = true;

    auto attacker = registry.create();
    registry.emplace<EnemyTag>(attacker);
    auto &atkStats = registry.emplace<CombatStats>(attacker);
    atkStats.accuracy = 1.0f;
    registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

    DamagePool pool;
    pool.Add(Tag::Physical, 10.0f);
    DamageRequest req;
    req.attacker = attacker;
    req.defender = defender;
    req.skill_id = 1;
    req.base_pool = pool;
    req.additional_tags = Tag::Hit; // Not melee, but blocked
    req.source_entity = attacker;

    DamagePipeline::CalculateBatch(registry, attacker, {defender}, 1, pool, req.additional_tags, attacker);
    CHECK(registry.get<HealthComponent>(attacker).current < 100.0f);
  }
}

TEST_CASE("[Integration] SkillSystem - BladeWard 451 & 455 OnDodge triggers") {
  entt::registry registry;
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  auto defender = registry.create();
  registry.emplace<PlayerTag>(defender);
  registry.emplace<CombatStats>(defender);
  auto &intent = registry.emplace<SwordIntentComponent>(defender);
  intent.stacks = 0;

  auto &active = registry.emplace<ActiveSkillsComponent>(defender);
  SpecializedSkill spec;
  spec.skill_id = 4u;
  spec.allocated_points[451u] = 3;
  spec.allocated_points[455u] = 1;
  active.specialized_slots[0] = spec;

  auto attacker = registry.create();
  registry.emplace<EnemyTag>(attacker);
  registry.emplace<CombatStats>(attacker);

  CombatEvent dodgeEvt = CombatEventFactory::CreateOnDodge(defender, attacker);
  CombatEventDispatcher::Dispatch(registry, dodgeEvt);

  const auto *effects = registry.try_get<ActiveEffectsComponent>(defender);
  REQUIRE(effects != nullptr);
  const auto *speedBuff = effects->Get("blade_ward_dodge_speed");
  REQUIRE(speedBuff != nullptr);
  CHECK(speedBuff->type == BuffType::SpeedUp);

  const auto *powerBuff = effects->Get("blade_ward_dodge_power");
  REQUIRE(powerBuff != nullptr);
  CHECK(powerBuff->type == BuffType::PowerBoost);

  CHECK(registry.get<SwordIntentComponent>(defender).stacks == 1);
}

TEST_CASE("[Integration] SkillSystem - BladeWard 432 & 435 OnBlock triggers") {
  entt::registry registry;
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  auto blocker = registry.create();
  registry.emplace<PlayerTag>(blocker);
  auto &stats = registry.emplace<CombatStats>(blocker);
  stats.barrier = 0.0f;
  auto &intent = registry.emplace<SwordIntentComponent>(blocker);
  intent.stacks = 0;

  auto &active = registry.emplace<ActiveSkillsComponent>(blocker);
  SpecializedSkill spec;
  spec.skill_id = 4u;
  spec.allocated_points[432u] = 3; // 30 barrier
  spec.allocated_points[435u] = 3; // intent chance
  active.specialized_slots[0] = spec;

  auto attacker = registry.create();
  registry.emplace<EnemyTag>(attacker);
  registry.emplace<CombatStats>(attacker);

  CombatEvent blockEvt = CombatEventFactory::CreateOnBlock(blocker, attacker, 50.0f);
  CombatEventDispatcher::Dispatch(registry, blockEvt);

  CHECK(registry.get<CombatStats>(blocker).barrier >= 30.0f);

  // 节点 435 剑意格御：3 点 = 45% 几率每次格挡回 1 层剑意
  // （intent_chance_per_point 0.15，见 skill_mechanics.json）。概率触发，
  // 重复派发以可靠观察到至少一次回复（128 次全不触发概率 ~0.55^128）。
  for (int i = 0; i < 128; ++i) {
    CombatEvent repeatEvt =
        CombatEventFactory::CreateOnBlock(blocker, attacker, 50.0f);
    CombatEventDispatcher::Dispatch(registry, repeatEvt);
  }
  const int intentStacks = registry.get<SwordIntentComponent>(blocker).stacks;
  CHECK(intentStacks >= 1);
  CHECK(intentStacks <= registry.get<SwordIntentComponent>(blocker).max_stacks);
}

TEST_CASE("[Integration] SkillSystem - BladeWard 452 defensive trigger on dodge") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  auto dodger = registry.create();
  registry.emplace<PlayerTag>(dodger);
  registry.emplace<Position>(dodger, 0.0f, 0.0f);
  registry.emplace<CombatStats>(dodger);

  auto &active = registry.emplace<ActiveSkillsComponent>(dodger);
  SpecializedSkill spec;
  spec.skill_id = 4u;
  spec.allocated_points[452u] = 1;
  active.specialized_slots[0] = spec;

  SkillSpecializationBaker::SyncTriggerRules(registry, dodger, 4u, &spec);

  auto attacker = registry.create();
  registry.emplace<EnemyTag>(attacker);
  registry.emplace<Position>(attacker, 50.0f, 0.0f);
  registry.emplace<CombatStats>(attacker);

  CombatEvent dodgeEvt = CombatEventFactory::CreateOnDodge(dodger, attacker);
  CombatEventDispatcher::Dispatch(registry, dodgeEvt);

  auto execView = registry.view<SkillExecution>();
  bool foundTriggeredCast = false;
  for (auto ent : execView) {
    const auto &exec = execView.get<SkillExecution>(ent);
    if (exec.skill_id == 1u && exec.owner == dodger) {
      foundTriggeredCast = true;
      break;
    }
  }
  CHECK(foundTriggeredCast);
}

TEST_CASE("[Integration] SkillSystem - BladeWard B2 expiration cleanup") {
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50);

  auto entity = registry.create();
  registry.emplace<PlayerTag>(entity);
  registry.emplace<Position>(entity, 0.0f, 0.0f);

  auto &ward = registry.emplace<BladeWardComponent>(entity);
  ward.duration = 10.0f;
  ward.remaining = 0.0f;

  auto &sent = registry.emplace<OrbitingSentinelComponent>(entity);
  sent.skill_id = 4u;

  SkillSystem::Update(registry, grid, 0.016f);

  CHECK_FALSE(registry.all_of<BladeWardComponent>(entity));
  CHECK_FALSE(registry.all_of<OrbitingSentinelComponent>(entity));
}

TEST_CASE("[Integration] SkillSystem - Key-node cast smoke matrix") {
  const std::array<uint32_t, 9> skills = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const auto key_nodes = test::skill_keynode_matrix::ExpectedKeyNodesBySkill();

  for (const uint32_t skill_id : skills) {
    CAPTURE(skill_id);
    REQUIRE(key_nodes.contains(skill_id));

    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();
    CombatEventDispatcher::Clear();
    SkillSystem::ShutdownHooks();
    SkillSystem::InitHooks();

    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster =
        test::skill_keynode_matrix::CreateCaster(registry, 1200.0f);
    test::skill_keynode_matrix::ConfigureSkillSlot(registry, caster, skill_id, 0,
                                                   2);
    test::skill_keynode_matrix::ConfigureSpecialization(
        registry, caster, skill_id,
        test::skill_keynode_matrix::AsAllocatedPoints(
            test::skill_keynode_matrix::CastSmokeNodes(skill_id,
                                                       key_nodes.at(skill_id)),
            1));

    CHECK(SkillSystem::TryCast(registry, caster, 0, {80.0f, 0.0f}));
    for (int i = 0; i < 3; ++i) {
      SkillSystem::Update(registry, grid, 0.08f);
    }

    if (skill_id == 6) {
      auto view = registry.view<SwordArrayComponent>();
      CHECK(view.begin() != view.end());
    } else if (skill_id == 9) {
      CHECK(registry.all_of<PhantomTranceComponent>(caster));
    } else if (skill_id == 5 || skill_id == 7) {
      CHECK(registry.all_of<BeamChannelComponent>(caster));
    } else if (skill_id == 8) {
      auto view = registry.view<Projectile, BoomerangComponent>();
      CHECK(view.begin() != view.end());
    } else if (skill_id == 2) {
      auto view = registry.view<Projectile>();
      CHECK(view.begin() != view.end());
    } else {
      CHECK(true);
    }
  }
}

} // namespace NoMoreDay

TEST_CASE("[Bugfix] SkillSystem - UAF Reproduction / Reallocation Safety") {
  // This test ensures that if a hook adds many new entities with SkillExecution
  // (triggering a pool reallocation), the main loop in UpdateStates doesn't
  // crash or access invalid memory.

  entt::registry registry;
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(100, 100, 50);

  // 1. Setup a single preparing skill
  auto entity = registry.create();
  auto &exec = registry.emplace<SkillExecution>(entity);
  exec.skill_id = 999; // Dummy ID
  exec.state = SkillState::Preparing;
  exec.timer = 0.0f;
  exec.owner = entity;

  // 2. Add a malicious hook that forces reallocation
  SkillSystem::ClearHooks();
  SkillSystem::AddPreCastHook([](entt::registry &r, entt::entity e,
                                 SkillExecution &ex) {
    // Force reallocation of SkillExecution pool by adding MANY new entities
    // Default page size for EnTT is often 4096 or similar, so adding enough
    // should trigger it. Or just `reserve` significantly more if possible, but
    // creating is surer.
    for (int i = 0; i < 2000; ++i) {
      auto newEnt = r.create();
      auto &newExec = r.emplace<SkillExecution>(newEnt);
      newExec.skill_id = 1;
      newExec.timer = 100.0f; // Don't trigger recursively immediately
    }

    // Also modify the original execution to verify write access still works (if
    // we have reference) If UAF occurs, 'ex' might be dangling here if it was a
    // reference to the old pool! Wait, 'ex' is passed by reference from
    // UpdateStates. If UpdateStates holds a reference to an element in the
    // pool, and the pool moves, 'ex' is invalid. We can't easily detect the
    // crash inside the hook (it happens when we access ex OR when the loop
    // continues).

    ex.timer =
        5.0f; // Write to potentially dangling reference?
              // Actually, EnTT reference stability depends on storage type.
              // Standard component view invalidates references on reallocation.
  });

  // 3. Trigger Update
  // If UAF is present, this might crash or corrupt memory (detected by
  // ASAN/valgrind, or crash)
  SkillSystem::UpdateStates(registry, 0.1f);
}
