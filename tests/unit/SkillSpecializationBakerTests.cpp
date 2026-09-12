#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/FlowingThrustComponents.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/OrbitingSentinelDeliverySystem.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/FlowingThrust.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "raylib.h"

namespace NoMoreDay {

TEST_CASE("[Unit] SkillSpecializationBaker - Base Profile Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 1, nullptr, profile, nullptr);

  CHECK(profile.skill_id == 1);
  CHECK(profile.effective_level == 1);
  CHECK(profile.projectile_count >= 1);
  CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  CHECK(profile.delivery.primary_archetype == static_cast<uint8_t>(DeliveryArchetype::Mobility));
  CHECK(profile.delivery.secondary_archetype == static_cast<uint8_t>(DeliveryArchetype::DirectStrike));
  CHECK(profile.delivery.speed == doctest::Approx(400.0f));
}

TEST_CASE("[Unit] SkillSpecializationBaker - Talent Modifiers and Archetype Morph") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  // Configure Skill 2 with talent 210 (extra projectiles) and 230 (Boomerang morph)
  SpecializedSkill spec;
  spec.skill_id = 2;
  spec.allocated_points[210] = 3; // +3 projectiles
  spec.allocated_points[211] = 1; // split
  spec.allocated_points[230] = 1; // Boomerang morph

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, &triggers);

  CHECK(profile.skill_id == 2);
  CHECK(profile.projectile_count == 4); // 1 base + 3
  CHECK(profile.delivery.primary_archetype == static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile));
  CHECK(profile.delivery.sub_count == 3);
  CHECK((profile.delivery.feature_flags & 4) != 0); // HasSplit
}

TEST_CASE("[Unit] SkillSpecializationBaker - Trigger Contract Rule Generation") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  // Skill 1 node 134 is a trigger contract (Shadow Blitz)
  SpecializedSkill spec;
  spec.skill_id = 1;
  spec.allocated_points[134] = 1;

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, &triggers);

  const auto *contract = SkillRegistry::Get().GetNodeContract(1, 134);
  if (contract && contract->role == SpecNodeRole::Trigger) {
    CHECK(triggers.HasRule(134));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Idempotence and Equality") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);

  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[100] = 2;

  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *profile1 = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(profile1 != nullptr);
  const BakedSkillProfile savedCopy = *profile1;

  // Rebake again without changes
  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *profile2 = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(profile2 != nullptr);

  // Profile must be strictly identical
  CHECK(*profile2 == savedCopy);
}

TEST_CASE("[Unit] SkillSpecializationBaker - Rebake TriggerRule Idempotency") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[134] = 1; // Trigger node

  SkillSystem::RebakeSkillProfiles(registry, player);
  const size_t count1 = triggers.rule_count;
  CHECK(count1 > 0);
  CHECK(triggers.HasRule(134));

  // Rebake a second time: rule_count must NOT accumulate
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == count1);

  // Rebake a third time
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == count1);

  // Player respecs: clear allocated points and rebake
  active.specialized_slots[0].allocated_points.clear();
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));

  // Player re-allocates and equips
  active.specialized_slots[0].allocated_points[134] = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(134));

  // Player unequips hotbar slot only (specialization tree remains allocated)
  active.slots[0].id = 0;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) == nullptr);

  // Player re-equips hotbar slot
  active.slots[0].id = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(134));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) != nullptr);

  // Player unsets/unequips skill slot completely (both slot and specialization tree)
  active.slots[0].id = 0;
  active.specialized_slots[0].skill_id = 0;
  active.specialized_slots[0].allocated_points.clear();
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));

  // Scenario: Entity does NOT have TriggerRuleComponent initially
  {
    entt::registry freshRegistry;
    const auto player2 = freshRegistry.create();
    auto &active2 = freshRegistry.emplace<ActiveSkillsComponent>(player2);
    active2.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
    active2.specialized_slots[0].skill_id = 1;
    active2.specialized_slots[0].allocated_points[134] = 1;

    CHECK_FALSE(freshRegistry.all_of<TriggerRuleComponent>(player2));
    SkillSystem::RebakeSkillProfiles(freshRegistry, player2);
    CHECK(freshRegistry.all_of<TriggerRuleComponent>(player2));
    auto *trig2 = freshRegistry.try_get<TriggerRuleComponent>(player2);
    REQUIRE(trig2 != nullptr);
    CHECK(trig2->rule_count == 1);
    CHECK(trig2->HasRule(134));

    // Rebake again: rule count unchanged
    SkillSystem::RebakeSkillProfiles(freshRegistry, player2);
    CHECK(trig2->rule_count == 1);
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - BakedDeliveryParams Dedicated Fields") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();

  // Skill 2 node 232 (Pull Trap) sets pull_radius without overwriting ballistic range
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[232] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK(profile.delivery.primary_archetype == static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile));
    CHECK(profile.delivery.pull_radius == 120.0f);
    CHECK((profile.delivery.feature_flags & 16) != 0);
    CHECK(profile.delivery.range == 500.0f); // Default ballistic range preserved
  }

  // Skill 5 nodes 502 (more damage), 555 (crit dmg) & 574 (armor pen)
  {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[502] = 2; // 2 * 10% = 20%
    spec.allocated_points[555] = 2; // 2 * 0.2 = 0.4（分数制暴伤增量）
    spec.allocated_points[574] = 3; // 3 * 6.0 = 18.0
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit_damage == doctest::Approx(0.4f));
    CHECK(profile.delivery.armor_pen == doctest::Approx(18.0f));
    CHECK(profile.delivery.speed == 300.0f); // Default speed NOT clobbered!
    CHECK(profile.delivery.range == 200.0f); // Default range NOT clobbered!
    CHECK(profile.more_damage_mult == doctest::Approx(1.20f));
  }

  // Skill 8 飞行参数唯一事实源为 skills.json params(speed 400 / max_distance 300)
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.delivery.speed == doctest::Approx(400.0f));
    CHECK(profile.delivery.range == doctest::Approx(300.0f));
    CHECK(profile.delivery.sub_count == 0);
  }

  // Skill 8 专职字段：830 侧刃、854 巨剑、800 法耗、801 速度/距离、803 折返、810 悬停、
  //                832 接刃回蓝、833 接刃攻速、834 剑步、851 牵引半径、853 剑意尺度
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    spec.allocated_points[800] = 4; // 8 - 4 = 4 法力
    spec.allocated_points[801] = 4; // +60% 速度/距离
    spec.allocated_points[803] = 4; // 1 + 0.40 折返倍率
    spec.allocated_points[810] = 1; // 悬停 0.8s
    spec.allocated_points[830] = 1; // 两侧刃
    spec.allocated_points[832] = 3; // 接刃回蓝 6
    spec.allocated_points[833] = 3; // 接刃攻速 +45%
    spec.allocated_points[834] = 3; // 剑步 +1.5s
    spec.allocated_points[851] = 4; // 牵引半径 *1.60
    spec.allocated_points[853] = 3; // 剑意尺度 +6%
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(4.0f));
    CHECK(profile.delivery.speed == doctest::Approx(640.0f));
    CHECK(profile.delivery.range == doctest::Approx(480.0f));
    CHECK(profile.delivery.return_damage_mult == doctest::Approx(1.40f));
    CHECK(profile.delivery.duration == doctest::Approx(0.8f));
    CHECK(profile.delivery.sub_count == 2);
    CHECK(profile.delivery.catch_mana == doctest::Approx(6.0f));
    CHECK(profile.delivery.combo_attack_speed == doctest::Approx(45.0f));
    CHECK(profile.delivery.step_extend_sec == doctest::Approx(1.5f));
    CHECK(profile.delivery.pull_radius_mult == doctest::Approx(1.60f));
    CHECK(profile.delivery.intent_scaling == doctest::Approx(0.06f));
  }

  // Skill 8 854 巨剑模式：取消侧刃并开启 5% 护甲转基础物伤
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    spec.allocated_points[854] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.delivery.sub_count == 0);
    CHECK(profile.delivery.giant_armor_scale == doctest::Approx(0.05f));
  }
}

TEST_CASE("[Unit] OrbitingSentinelDeliverySystem - Interception Dice Roll & Cap") {
  systems::SpatialHashGrid grid(100, 100, 50.0f);

  // Subtest 1: 0% chance intercepts nothing
  {
    entt::registry registry;
    auto sentinelEnt = registry.create();
    auto &sentinel = registry.emplace<OrbitingSentinelComponent>(sentinelEnt);
    sentinel.anchor_entity = sentinelEnt;
    sentinel.interception_chance = 0.0f;
    registry.emplace<Position>(sentinelEnt, 0.0f, 0.0f);

    auto proj = registry.create();
    registry.emplace<EnemyTag>(proj);
    registry.emplace<Projectile>(proj);
    registry.emplace<Position>(proj, 10.0f, 10.0f);

    OrbitingSentinelDeliverySystem::Update(registry, grid, 0.016f);
    CHECK(registry.valid(proj)); // NOT intercepted
  }

  // Subtest 2: 100% chance intercepts up to cap (8)
  {
    entt::registry registry;
    auto sentinelEnt = registry.create();
    auto &sentinel = registry.emplace<OrbitingSentinelComponent>(sentinelEnt);
    sentinel.anchor_entity = sentinelEnt;
    sentinel.interception_chance = 1.0f;
    registry.emplace<Position>(sentinelEnt, 0.0f, 0.0f);

    std::vector<entt::entity> projs;
    for (int i = 0; i < 12; ++i) {
      auto proj = registry.create();
      registry.emplace<EnemyTag>(proj);
      registry.emplace<Projectile>(proj);
      registry.emplace<Position>(proj, 5.0f, 5.0f);
      projs.push_back(proj);
    }

    OrbitingSentinelDeliverySystem::Update(registry, grid, 0.016f);
    int destroyedCount = 0;
    for (auto p : projs) {
      if (!registry.valid(p)) ++destroyedCount;
    }
    CHECK(destroyedCount == 8); // Capped at exactly 8 per frame
  }

  // Subtest 3: 50% chance statistical distribution test
  {
    int interceptedTotal = 0;
    constexpr int kTrials = 200;
    for (int i = 0; i < kTrials; ++i) {
      entt::registry registry;
      auto sentinelEnt = registry.create();
      auto &sentinel = registry.emplace<OrbitingSentinelComponent>(sentinelEnt);
      sentinel.anchor_entity = sentinelEnt;
      sentinel.interception_chance = 0.5f;
      registry.emplace<Position>(sentinelEnt, 0.0f, 0.0f);

      auto proj = registry.create();
      registry.emplace<EnemyTag>(proj);
      registry.emplace<Projectile>(proj);
      registry.emplace<Position>(proj, 5.0f, 5.0f);

      OrbitingSentinelDeliverySystem::Update(registry, grid, 0.016f);
      if (!registry.valid(proj)) {
        ++interceptedTotal;
      }
    }
    // With 200 trials and p=0.5, mean=100, stddev=sqrt(50)~7.07. 4 sigma bounds [60, 140].
    CHECK(interceptedTotal > 60);
    CHECK(interceptedTotal < 140);
  }

  // Subtest 4: Projectile owned by enemy is intercepted
  {
    entt::registry registry;
    auto sentinelEnt = registry.create();
    auto &sentinel = registry.emplace<OrbitingSentinelComponent>(sentinelEnt);
    sentinel.anchor_entity = sentinelEnt;
    sentinel.interception_chance = 1.0f;
    registry.emplace<Position>(sentinelEnt, 0.0f, 0.0f);

    auto enemyOwner = registry.create();
    registry.emplace<EnemyTag>(enemyOwner);

    auto proj = registry.create();
    auto &p = registry.emplace<Projectile>(proj);
    p.owner = enemyOwner;
    registry.emplace<Position>(proj, 5.0f, 5.0f);

    OrbitingSentinelDeliverySystem::Update(registry, grid, 0.016f);
    CHECK_FALSE(registry.valid(proj)); // Intercepted via enemy owner
  }

  // Subtest 5: Multiple sentinels do not double-intercept the same projectile
  {
    entt::registry registry;
    auto anchor = registry.create();
    registry.emplace<Position>(anchor, 0.0f, 0.0f);

    auto s1 = registry.create();
    auto &sent1 = registry.emplace<OrbitingSentinelComponent>(s1);
    sent1.anchor_entity = anchor;
    sent1.orbit_radius = 0.0f;
    sent1.interception_chance = 1.0f;
    registry.emplace<Position>(s1, 0.0f, 0.0f);

    auto s2 = registry.create();
    auto &sent2 = registry.emplace<OrbitingSentinelComponent>(s2);
    sent2.anchor_entity = anchor;
    sent2.orbit_radius = 0.0f;
    sent2.interception_chance = 1.0f;
    registry.emplace<Position>(s2, 0.0f, 0.0f);

    // Create 10 enemy projectiles
    std::vector<entt::entity> projs;
    for (int i = 0; i < 10; ++i) {
      auto proj = registry.create();
      registry.emplace<EnemyTag>(proj);
      registry.emplace<Projectile>(proj);
      registry.emplace<Position>(proj, 5.0f, 5.0f);
      projs.push_back(proj);
    }

    OrbitingSentinelDeliverySystem::Update(registry, grid, 0.016f);
    // Sentinel 1 intercepts up to 8, Sentinel 2 can intercept remaining 2
    int destroyedCount = 0;
    for (auto p : projs) {
      if (!registry.valid(p)) ++destroyedCount;
    }
    CHECK(destroyedCount == 10); // Combined interception handles all 10 without duplicate counting
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 1 Flowing Thrust Detailed Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();

  SUBCASE("Skill 1 Base Profile has 2 charges and 4.0s cooldown") {
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, nullptr, profile, nullptr);

    CHECK(profile.skill_id == 1);
    CHECK(profile.effective_charges == 2);
    CHECK(profile.effective_cooldown == doctest::Approx(4.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
    CHECK((profile.effective_tags & Tag::SwordSkill) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) != Tag::None);
    CHECK((profile.effective_tags & Tag::Movement) != Tag::None);
  }

  SUBCASE("Node 101 Energy Flow reduces mana cost without giving illegal damage") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[101] = 3; // 3 * 15% = 45% reduction

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    const auto *data = SkillRegistry::Get().GetSkill(1);
    REQUIRE(data != nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(data->mana_cost * 0.55f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  }

  SUBCASE("Node 102 Keen Edge increases bonus crit") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[102] = 3; // +6% crit

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 推导：node 102 每点 +2%，3 点 = 6% = 0.06（分数制）
    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.06f));
  }

  SUBCASE("Node 110 Thrust Rhythm reduces CD by 1s and damage by 15%") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[110] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_cooldown == doctest::Approx(3.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(0.85f));
  }

  SUBCASE("Node 111 Continuous Thrust increases max charges and CD") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[111] = 2; // +2 charges, +30% cooldown

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_charges == 4);
    CHECK(profile.effective_cooldown == doctest::Approx(4.0f * 1.30f));
  }

  SUBCASE("Node 112 marks momentum allocation (runtime dash range / more scaling)") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[112] = 2;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 112 仅标记分配（feature_flags 位 256）；距离加成与位移 More 在运行时按实际位移结算
    CHECK((profile.delivery.feature_flags & 256) != 0);
    CHECK(profile.delivery.speed == doctest::Approx(400.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  }

  SUBCASE("Node 113 enables Windwalker feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[113] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 4) != 0);
  }

  SUBCASE("Node 114 Riding The Wind is conditional on sword step, not baked unconditionally") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[114] = 3; // 御剑步期间近战暴击 +24%

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 未处于剑步时: 条件暴击不应注入无条件交付参数
    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.0f));
    // 处于剑步时: 由 profile.riding_wind_bonus_crit 记录, 交付构造处检查剑步状态后注入
    // 推导：node 114 每点 +8%，3 点 = 24% = 0.24（分数制）
    CHECK(profile.riding_wind_bonus_crit == doctest::Approx(0.24f));
  }

  SUBCASE("Node 130 sets afterimage sub_count and feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[130] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.delivery.sub_count == 1);
    CHECK((profile.delivery.feature_flags & 2) != 0);
  }

  SUBCASE("Node 133 Swap enables explosion feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[133] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 8) != 0);
  }

  SUBCASE("Node 134 Shadow Blitz generates trigger rule and scales radius/damage") {
    auto &triggers = registry.emplace<TriggerRuleComponent>(player);
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[134] = 2; // +50% dmg, +40% radius

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, &triggers);

    CHECK(profile.more_damage_mult == doctest::Approx(1.50f));
    CHECK(profile.area_radius == doctest::Approx(1.40f));
    CHECK(triggers.HasRule(134));
  }

  SUBCASE("Node 154 All In sets single charge, 8s CD, double mana, double damage, 100% crit") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[154] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    const auto *data = SkillRegistry::Get().GetSkill(1);
    REQUIRE(data != nullptr);
    CHECK(profile.effective_charges == 1);
    CHECK(profile.effective_cooldown == doctest::Approx(8.0f));
    CHECK(profile.effective_mana_cost == doctest::Approx(data->mana_cost * 2.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(2.0f));
    // 推导：node 154 必暴 = 1.0（分数制）
    CHECK(profile.delivery.bonus_crit >= 1.0f);
  }

  SUBCASE("Node 170 Hellfire transmuter converts Physical to Fire") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[170] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.effective_tags & Tag::Fire) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) == Tag::None);
  }

  SUBCASE("Node 172 FreezingWind transmuter converts Physical to Cold") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[172] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.effective_tags & Tag::Cold) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) == Tag::None);
  }

  SUBCASE("Node 133 Swap removes Movement tag and adds Teleport tag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[133] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 8) != 0);
    CHECK((profile.effective_tags & Tag::Teleport) != Tag::None);
    CHECK((profile.effective_tags & Tag::Movement) == Tag::None);
  }

  SUBCASE("ResolveElementalConversion supports node-based transmuters across skills") {
    // Skill 1: 170 Fire, 172 Cold
    auto c170 = ResolveElementalConversion(170, 1);
    CHECK(c170.target_element == Tag::Fire);
    auto c172 = ResolveElementalConversion(172, 1);
    CHECK(c172.target_element == Tag::Cold);

    // Skill 2: 270 Cold, 272 Lightning (verified fix for 250 typo)
    auto c270 = ResolveElementalConversion(270, 1);
    CHECK(c270.target_element == Tag::Cold);
    auto c272 = ResolveElementalConversion(272, 1);
    CHECK(c272.target_element == Tag::Lightning);

    // Skill 3: 370 Fire, 372 Lightning
    auto c370 = ResolveElementalConversion(370, 1);
    CHECK(c370.target_element == Tag::Fire);
    auto c372 = ResolveElementalConversion(372, 1);
    CHECK(c372.target_element == Tag::Lightning);

    // Skill 4: 472 Lightning, 474 Cold
    auto c472 = ResolveElementalConversion(472, 1);
    CHECK(c472.target_element == Tag::Lightning);
    auto c474 = ResolveElementalConversion(474, 1);
    CHECK(c474.target_element == Tag::Cold);

    // Skill 5: 570 Fire, 572 Cold
    auto c570 = ResolveElementalConversion(570, 1);
    CHECK(c570.target_element == Tag::Fire);
    auto c572 = ResolveElementalConversion(572, 1);
    CHECK(c572.target_element == Tag::Cold);
  }
}

TEST_CASE("[Unit] SkillSystem - Skill 1 Charges and Cooldown Execution") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  systems::SpatialHashGrid grid(1000, 1000, 50);

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 200.0f;
  stats.max_mana = 200.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;

  SkillSystem::RebakeSkillProfiles(registry, player);

  SUBCASE("Base skill consumes charges and recharges with 4.0s cooldown") {
    auto &slot = active.slots[0];
    CHECK(slot.current_charges == 2);

    // First cast
    bool cast1 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast1);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(4.0f));
    registry.remove<SkillExecution>(player);

    // Second cast
    bool cast2 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast2);
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    // Third cast should fail because 0 charges
    bool cast3 = SkillSystem::TryCast(registry, player, 0);
    CHECK_FALSE(cast3);

    // Tick cooldown by 2.0s
    SkillSystem::Update(registry, grid, 2.0f);
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(2.0f));

    // Tick cooldown by another 2.0s (total 4.0s) -> 1 charge restored, next charge cooldown starts
    SkillSystem::Update(registry, grid, 2.0f);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(4.0f));

    // Now can cast again
    bool cast4 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast4);
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    // Tick 8.0s (two 4.0s recharge cycles) -> both charges fully restored
    SkillSystem::Update(registry, grid, 4.0f);
    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 2);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }

  SUBCASE("Node 111 expands charges to 4") {
    active.specialized_slots[0].allocated_points[111] = 2; // +2 charges (total 4)
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 4;
    slot.cooldown = 0.0f;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 3);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 2);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 1);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
  }

  SUBCASE("Node 154 All In restricts to 1 charge and 8s cooldown") {
    active.specialized_slots[0].allocated_points[154] = 1;
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 1;
    slot.cooldown = 0.0f;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(8.0f));
    registry.remove<SkillExecution>(player);

    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));

    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(4.0f));

    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Unit] FlowingThrust - Runtime Node Behaviors (150, 174, 115, 155)") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.crit_damage = 1.5f;
  pStats.crit_chance = 100.0f; // guarantee crit for testing crit mult

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;

  SUBCASE("Node 150 increases critical damage multiplier against >80% HP or controlled targets") {
    active.specialized_slots[0].allocated_points[150] = 4; // +60% crit multiplier

    // 1. Target with >80% HP
    const auto dummyHighHp = registry.create();
    registry.emplace<HealthComponent>(dummyHighHp, 100.0f, 100.0f);
    registry.emplace<Position>(dummyHighHp, 10.0f, 0.0f);

    DamageRequest reqHigh;
    reqHigh.attacker = player;
    reqHigh.defender = dummyHighHp;
    reqHigh.skill_id = 1;
    reqHigh.base_pool.Add(Tag::Physical, 100.0f);
    reqHigh.is_simulation = true;
    DamageResult resHigh = DamagePipeline::Calculate(registry, reqHigh);
    CHECK(resHigh.is_crit);
    // (100.0f base_pool + 10.0f skill_1 base_damage) * (1.5 base crit + 0.6 bonus crit mult) = 231.0f
    CHECK(resHigh.total_damage == doctest::Approx(231.0f));

    // 2. Target with <80% HP and not controlled
    const auto dummyLowHp = registry.create();
    registry.emplace<HealthComponent>(dummyLowHp, 50.0f, 100.0f);
    registry.emplace<Position>(dummyLowHp, 20.0f, 0.0f);

    DamageRequest reqLow;
    reqLow.attacker = player;
    reqLow.defender = dummyLowHp;
    reqLow.skill_id = 1;
    reqLow.base_pool.Add(Tag::Physical, 100.0f);
    reqLow.is_simulation = true;
    DamageResult resLow = DamagePipeline::Calculate(registry, reqLow);
    CHECK(resLow.is_crit);
    // (100.0f base_pool + 10.0f skill_1 base_damage) * 1.5 base crit = 165.0f
    CHECK(resLow.total_damage == doctest::Approx(165.0f));

    // 3. Target with <80% HP BUT controlled (e.g. Slow)
    auto &eff = registry.emplace<ActiveEffectsComponent>(dummyLowHp);
    eff.effects.push_back(BuffEffect{
        .id = "Slow",
        .name = "Slow",
        .type = BuffType::SpeedDown,
        .duration = 2.0f,
        .remaining = 2.0f
    });
    DamageResult resControlled = DamagePipeline::Calculate(registry, reqLow);
    CHECK(resControlled.is_crit);
    // Controlled -> receives +0.6 bonus crit mult -> 231.0f
    CHECK(resControlled.total_damage == doctest::Approx(231.0f));
  }

  SUBCASE("Node 174 applies and stacks Elemental Erosion debuff") {
    active.specialized_slots[0].allocated_points[174] = 4; // -12 resist per stack
    SkillSystem::RebakeSkillProfiles(registry, player);

    const auto target = registry.create();
    registry.emplace<HealthComponent>(target, 100.0f, 100.0f);
    registry.emplace<Position>(target, 10.0f, 0.0f);
    auto &effects = registry.emplace<ActiveEffectsComponent>(target);
    effects.effects.push_back(BuffEffect{
        .id = "Ignite",
        .name = "Ignite",
        .type = BuffType::Burn,
        .duration = 3.0f,
        .remaining = 3.0f
    });

    auto hitFunc = SkillBehaviorRegistry::GetHit(1);
    REQUIRE(hitFunc != nullptr);

    auto findBuff = [&](const std::string &id) -> const BuffEffect * {
      for (const auto &b : effects.effects) {
        if (b.id == id) return &b;
      }
      return nullptr;
    };

    // Hit 1: 1 stack (-12 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    const auto *buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    REQUIRE_FALSE(buff->modifiers.empty());
    CHECK(buff->modifiers[0].value == doctest::Approx(-12.0f));
    CHECK(buff->modifiers[0].type == StatType::ResistFire);

    // Hit 2: 2 stacks (-24 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    CHECK(buff->modifiers[0].value == doctest::Approx(-24.0f));

    // Hit 3, 4, 5, 6: capped at 5 stacks (-60 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    CHECK(buff->modifiers[0].value == doctest::Approx(-60.0f));
  }

  SUBCASE("Node 155 CD and charge reset on kill with All In active") {
    active.specialized_slots[0].allocated_points[154] = 1; // All In
    active.specialized_slots[0].allocated_points[155] = 4; // 100% chance to reset CD on kill
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 0;
    slot.cooldown = 8.0f;

    const auto victim = registry.create();
    registry.emplace<HealthComponent>(victim, 0.0f, 100.0f); // Dead
    registry.emplace<Position>(victim, 10.0f, 0.0f);

    auto hitFunc = SkillBehaviorRegistry::GetHit(1);
    REQUIRE(hitFunc != nullptr);

    hitFunc(registry, player, victim, Tag::Physical, false);

    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Unit] FlowingThrust - 133 Swap Explosion Damage Payload") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // 注册技能行为与 OnSkillHit 事件处理器（DoHit 依赖事件链路）
  SkillSystem::InitHooks();

  systems::SpatialHashGrid grid(1000, 1000, 50);
  entt::registry registry;

  // 玩家位于原点，持有技能 1 并分配 133（移形换位：传送爆炸）
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<PlayerTag>(player);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.mana = 200.0f;
  pStats.max_mana = 200.0f;
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f; // 关闭暴击，保证伤害可精确对比
  pStats.crit_damage = 1.5f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1;

  // 预置契约运行时并压住 134 的触发冷却，隔离触发派生伤害，专注爆炸本体增伤
  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
  runtime.trigger_cooldowns[134] = 999.0f;

  // 敌人位于传送目标点 (300, 0)
  const auto enemy = registry.create();
  registry.emplace<Position>(enemy, 300.0f, 0.0f);
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(enemy);
  registry.emplace<ActiveEffectsComponent>(enemy);

  // 事件探针：捕获爆炸命中的元素 tags（证明 payload_context.effective_tags 进入事件）
  Tag capturedTags = Tag::None;
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [&](entt::registry &, const CombatEvent &evt) { capturedTags = evt.tags; },
      100);

  // 触发一次传送爆炸：Preparing -> Casting 调用 DoCast 生成两个 DirectStrike，
  // 再由 ProjectileSystem 结算命中
  auto castAndResolve = [&]() {
    SkillSystem::RebakeSkillProfiles(registry, player);
    REQUIRE(SkillSystem::TryCast(registry, player, 0, {300.0f, 0.0f}));
    SkillSystem::Update(registry, grid, 0.11f); // 触发 DoCast（Swap 分支）
    registry.remove<SkillExecution>(player);
    // 诊断：检查爆炸实体携带的有效载荷
    auto strikeView = registry.view<Position, DirectStrikeComponent>();
    for (auto ent : strikeView) {
      const auto &d = strikeView.get<DirectStrikeComponent>(ent);
      MESSAGE("strike pos=(", strikeView.get<Position>(ent).x, ",",
              strikeView.get<Position>(ent).y, ") has_payload=", d.has_payload,
              " more=", d.payload_context.more_damage,
              " effTags=", static_cast<uint64_t>(d.payload_context.effective_tags));
    }
    ProjectileSystem::Update(registry, grid, 0.02f); // DirectStrike 伤害结算
  };

  SUBCASE("Node 134 Shadow Blitz more damage multiplies explosion damage") {
    // 基线：仅 133（无 134），记录一次爆炸伤害
    castAndResolve();
    auto &hp = registry.get<HealthComponent>(enemy);
    const float baseline = hp.max - hp.current;
    REQUIRE(baseline > 0.0f);

    // 分配 134（1 点 -> more_damage_mult x1.25）后重测
    active.specialized_slots[0].allocated_points[134] = 1;
    hp.current = hp.max;
    active.slots[0].current_charges = 2;
    // 首 cast 已将玩家传送到目标点，重置回原点使起点/终点两次爆炸分离，
    // 保证再次命中时仅终点爆炸结算（否则两个爆炸叠加在敌人身上）
    auto &ppos = registry.get<Position>(player);
    ppos.x = 0.0f;
    ppos.y = 0.0f;
    castAndResolve();
    const float withBlitz = hp.max - hp.current;
    MESSAGE("baseline=", baseline, " with134=", withBlitz,
            " expected=", baseline * 1.25f);
    CHECK(withBlitz == doctest::Approx(baseline * 1.25f).epsilon(0.02f));
  }

  SUBCASE("Node 170 Hellfire explosion hit applies Ignite (Fire tag via payload)") {
    active.specialized_slots[0].allocated_points[170] = 1;
    castAndResolve();

    MESSAGE("capturedTags=", static_cast<uint64_t>(capturedTags),
            " hasFire=", ((capturedTags & Tag::Fire) != Tag::None));
    CHECK((capturedTags & Tag::Fire) != Tag::None);

    auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(effects != nullptr);
    MESSAGE("effectCount=", effects->effects.size());
    bool hasIgnite = false;
    for (const auto &b : effects->effects) {
      if (b.type == BuffType::Burn && b.name == "Ignite") {
        hasIgnite = true;
        break;
      }
    }
    CHECK(hasIgnite);
  }

  SUBCASE("Node 172 FreezingWind explosion hit carries Cold tag and applies 30% slow") {
    active.specialized_slots[0].allocated_points[172] = 1;
    castAndResolve();

    // 元素 tags 经 payload_context.effective_tags 进入命中事件，
    // DoHit 的 element_tag 即来自该事件
    MESSAGE("capturedTags=", static_cast<uint64_t>(capturedTags),
            " hasCold=", ((capturedTags & Tag::Cold) != Tag::None));
    CHECK((capturedTags & Tag::Cold) != Tag::None);

    // 172 减速: 走 legacy SpeedDown buff（Slow 异常未注册 ailment 契约，
    // 与 HazardSystem::ApplyChillDebuff 的既有减速机制保持一致）
    auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(fx != nullptr);
    const auto *slow = fx->Get("FrostSlow");
    REQUIRE(slow != nullptr);
    REQUIRE(slow->type == BuffType::SpeedDown);
    REQUIRE_FALSE(slow->modifiers.empty());
    CHECK(slow->modifiers[0].value == doctest::Approx(-30.0f));
    CHECK(slow->modifiers[0].type == StatType::MoveSpeed);
  }
}

TEST_CASE("[Unit] FlowingThrust - H9d removed: hit does NOT reduce skill 2 cooldown") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);
  auto &blade = registry.emplace<BladeResourceComponent>(player);
  blade.kind = BladeResourceKind::SwordFlow;
  blade.current = 5;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[1] = SkillSlot{.id = 2, .cooldown = 5.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);

  const auto victim = registry.create();
  registry.emplace<HealthComponent>(victim, 100.0f, 100.0f);
  registry.emplace<Position>(victim, 10.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Physical, false);

  // 负向断言：旧私有特例"命中削减技能 2 CD 0.75s"已删除，命中后技能 2 CD 保持不变
  CHECK(active.slots[1].cooldown == doctest::Approx(5.0f));
}

TEST_CASE("[Unit] FlowingThrust - 170 Hellfire ember trail ignites enemies") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1; // 移形换位
  active.specialized_slots[0].allocated_points[170] = 1; // 劫火
  SkillSystem::RebakeSkillProfiles(registry, player);

  // 敌人位于突进路径线段 (0,0)->(100,0) 内
  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 30.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(enemy);

  // 直接调用 DoCast（Swap 分支）：沿起点→终点铺设余烬带
  auto castFunc = SkillBehaviorRegistry::GetCast(1);
  REQUIRE(castFunc != nullptr);
  SkillExecution exec;
  exec.skill_id = 1;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(33); // 133 Swap
  exec.active_nodes.set(70); // 170 Hellfire
  castFunc(registry, player, exec);

  // 余烬带参数从 skill_mechanics.json 读取（未分配 171 → 基础值）
  auto zoneView = registry.view<FlowingEmberZoneComponent>();
  REQUIRE_FALSE(zoneView.empty());
  for (auto ent : zoneView) {
    const auto &zone = zoneView.get<FlowingEmberZoneComponent>(ent);
    CHECK(zone.width == doctest::Approx(60.0f));
    CHECK(zone.duration == doctest::Approx(2.0f));
    CHECK(zone.remaining == doctest::Approx(2.0f));
    CHECK(zone.owner == player);
  }

  // 推进 0.5s：敌人处于余烬内被点燃（同一余烬去重）
  skills::UpdateFlowingThrustEmbers(registry, 0.5f);
  auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);
  bool hasIgnite = false;
  for (const auto &b : fx->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) {
      hasIgnite = true;
    }
  }
  CHECK(hasIgnite);

  // 余烬到期后销毁
  skills::UpdateFlowingThrustEmbers(registry, 2.0f);
  CHECK(registry.view<FlowingEmberZoneComponent>().empty());
}

TEST_CASE("[Unit] FlowingThrust - 171 Infernal Path fire damage while standing on embers") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1;
  active.specialized_slots[0].allocated_points[170] = 1;
  active.specialized_slots[0].allocated_points[171] = 3; // 业火焚途 3 点
  SkillSystem::RebakeSkillProfiles(registry, player);

  auto castFunc = SkillBehaviorRegistry::GetCast(1);
  REQUIRE(castFunc != nullptr);
  SkillExecution exec;
  exec.skill_id = 1;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(33);
  exec.active_nodes.set(70);
  castFunc(registry, player, exec);

  // 171 每点: 余烬宽度 +25% (60 → 105)、持续 +0.5s (2.0 → 3.5)
  auto zoneView = registry.view<FlowingEmberZoneComponent>();
  REQUIRE_FALSE(zoneView.empty());
  for (auto ent : zoneView) {
    const auto &zone = zoneView.get<FlowingEmberZoneComponent>(ent);
    CHECK(zone.width == doctest::Approx(105.0f));
    CHECK(zone.duration == doctest::Approx(3.5f));
    CHECK(zone.infernal_points == 3);
  }

  // 玩家被传送到终点 (100,0)，站在自己余烬上 → 火焰伤害加成 6%×3 = 18%
  skills::UpdateFlowingThrustEmbers(registry, 0.25f);
  auto &fx = registry.get_or_emplace<ActiveEffectsComponent>(player);
  const auto *buff = fx.Get("InfernalPath");
  REQUIRE(buff != nullptr);
  REQUIRE_FALSE(buff->modifiers.empty());
  CHECK(buff->modifiers[0].value == doctest::Approx(18.0f));
  CHECK(buff->modifiers[0].type == StatType::FireDamage);

  // 离开余烬：不再刷新，短时 buff 自然过期
  registry.get<Position>(player).x = 500.0f;
  skills::UpdateFlowingThrustEmbers(registry, 0.25f);
  fx.Update(0.3f);
  CHECK(fx.Get("InfernalPath") == nullptr);
}

TEST_CASE("[Unit] FlowingThrust - 172 FreezingWind +50% more damage vs frozen") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &pStats = registry.emplace<CombatStats>(player);
  // 武器伤害置 0：仅剩技能固有基础伤害 base_damage=10（skills.json 技能1），
  // 使期望数值可精确断言
  pStats.min_weapon_damage = 0.0f;
  pStats.max_weapon_damage = 0.0f;
  pStats.crit_chance = 0.0f;
  pStats.crit_damage = 1.5f;

  const auto makeDefender = [&](bool frozen) {
    const auto e = registry.create();
    registry.emplace<HealthComponent>(e, 100000.0f, 100000.0f);
    registry.emplace<CombatStats>(e);
    if (frozen) {
      auto &fx = registry.emplace<ActiveEffectsComponent>(e);
      BuffEffect frozenBuff{.id = "Frozen",
                            .name = "Frozen",
                            .type = BuffType::Freeze,
                            .duration = 2.5f,
                            .remaining = 2.5f,
                            .is_debuff = true};
      fx.AddOrRefresh(frozenBuff);
    }
    return e;
  };

  const auto normal = makeDefender(false);
  const auto frozen = makeDefender(true);

  DamageRequest req;
  req.attacker = player;
  req.defender = normal;
  req.skill_id = 1;
  req.base_pool.Add(Tag::Physical, 100.0f);
  // is_simulation=true：跳过防御判定与拦截器，锁定伤害乘区，便于精确断言冻结增伤
  req.is_simulation = true;
  auto normalResult = DamagePipeline::Execute(registry, req, player, false);
  // 100 基础 + 技能固有基础伤害 10 = 110
  CHECK(normalResult.damage.total_damage == doctest::Approx(110.0f));

  req.defender = frozen;
  auto frozenResult = DamagePipeline::Execute(registry, req, player, false);
  // 110 × 1.5 冻结增伤 = 165
  CHECK(frozenResult.damage.total_damage == doctest::Approx(165.0f));
}

TEST_CASE("[Unit] FlowingThrust - 173 BoneDeepFrost shatter on frozen victim") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  // 武器伤害置 0：碎裂溅射数值取自 LastCritDamageComponent，与武器无关；
  // 置 0 后仅剩技能固有基础伤害 base_damage=10，便于精确断言设计值
  pStats.min_weapon_damage = 0.0f;
  pStats.max_weapon_damage = 0.0f;
  pStats.crit_chance = 0.0f; // 关闭暴击，保证溅射伤害可精确对比
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[173] = 7; // 碎裂几率 15%×7=105% 必然触发
  SkillSystem::RebakeSkillProfiles(registry, player);

  // 冻结目标: 拥有 Freeze buff，且记录了来自玩家的上次暴击 1000 点
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 10000.0f, 10000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  BuffEffect frozen{.id = "Frozen",
                    .name = "Frozen",
                    .type = BuffType::Freeze,
                    .duration = 2.5f,
                    .remaining = 2.5f,
                    .is_debuff = true};
  vfx.AddOrRefresh(frozen);
  registry.emplace<LastCritDamageComponent>(victim, 1000.0f, player);

  // 溅射目标位于碎裂半径 200 码内
  const auto splashTarget = registry.create();
  registry.emplace<EnemyTag>(splashTarget);
  registry.emplace<Position>(splashTarget, 100.0f, 0.0f);
  registry.emplace<HealthComponent>(splashTarget, 10000.0f, 10000.0f);
  registry.emplace<CombatStats>(splashTarget);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Cold, true);

  // 溅射 = 1000 × 30% = 300 冰霜伤害
  const auto &shp = registry.get<HealthComponent>(splashTarget);
  // 实路径按完整管线结算，无额外合成系数。
  // 设计值 300 由下方 is_simulation 请求交叉验证。
  CHECK(shp.current == doctest::Approx(9700.0f));

  // 复现 DoHit 溅射请求构造（skill_id=1，Cold 300，防递归标签），交叉验证设计值
  DamageRequest splashReq;
  splashReq.attacker = player;
  splashReq.defender = splashTarget;
  splashReq.skill_id = 1;
  splashReq.base_pool.Add(Tag::Cold, 300.0f);
  splashReq.additional_tags = Tag::Cold | Tag::DamageOverTime | Tag::Area;
  splashReq.is_simulation = true;
  auto splashResult = DamagePipeline::Execute(registry, splashReq, player, false);
  // 300 溅射 + 技能固有基础伤害 10（skills.json base_damage=10，武器已置 0）
  CHECK(splashResult.damage.total_damage == doctest::Approx(310.0f));
}

TEST_CASE("[Unit] FlowingThrust - 175 Residual Elements spread respects 1s ICD") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[175] = 5; // 传染几率 20%×5=100%

  // 携带点燃的命中目标
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  BuffEffect ignite{.id = "Ignite",
                    .name = "Ignite",
                    .type = BuffType::Burn,
                    .duration = 3.0f,
                    .remaining = 3.0f,
                    .is_debuff = true};
  vfx.AddOrRefresh(ignite);

  // 附近两名敌人：A 在 50 码、B 在 80 码
  const auto targetA = registry.create();
  registry.emplace<EnemyTag>(targetA);
  registry.emplace<Position>(targetA, 50.0f, 0.0f);
  registry.emplace<HealthComponent>(targetA, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetA);
  const auto targetB = registry.create();
  registry.emplace<EnemyTag>(targetB);
  registry.emplace<Position>(targetB, 80.0f, 0.0f);
  registry.emplace<HealthComponent>(targetB, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetB);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);

  // 第一次命中: 传染给最近的 A
  hitFunc(registry, player, victim, Tag::Fire, false);
  auto *fxA = registry.try_get<ActiveEffectsComponent>(targetA);
  REQUIRE(fxA != nullptr);
  bool aIgnited = false;
  for (const auto &b : fxA->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) aIgnited = true;
  }
  CHECK(aIgnited);
  CHECK(registry.try_get<ActiveEffectsComponent>(targetB) == nullptr);

  // 第二次命中（同一帧，ICD 1s 内）: 不传染
  hitFunc(registry, player, victim, Tag::Fire, false);
  CHECK(registry.try_get<ActiveEffectsComponent>(targetB) == nullptr);

  // 冷却结束后: A 移出范围，传染给 B
  auto &ftState = registry.get_or_emplace<FlowingThrustStateComponent>(player);
  ftState.last_infect_time = static_cast<float>(GetTime()) - 2.0f;
  registry.get<Position>(targetA).x = 500.0f;
  hitFunc(registry, player, victim, Tag::Fire, false);
  auto *fxB = registry.try_get<ActiveEffectsComponent>(targetB);
  REQUIRE(fxB != nullptr);
  bool bIgnited = false;
  for (const auto &b : fxB->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) bIgnited = true;
  }
  CHECK(bIgnited);
}

// F7 回归：传染减速与 172 直击减速共用 id "FrostSlow"，刷新时 AddOrRefresh 会
// 用新效果的 type/kind 覆盖旧值；若传染创建点漏写 type=SpeedDown，会把目标身上
// 共享的减速清成 type=None，导致 275 扩散/835 净化/UI 等 type 消费方失效。
TEST_CASE("[Unit] FlowingThrust - 175 spread slow keeps SpeedDown type on refresh") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[175] = 5; // 传染几率 20%×5=100%

  // 主目标携带寒冷减速（172 已施加的 FrostSlow：type=SpeedDown / kind=Slow）
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  vfx.AddOrRefresh(BuffEffect{.id = "FrostSlow",
                              .name = "Frost Slow",
                              .type = BuffType::SpeedDown,
                              .kind = BuffKind::Slow,
                              .duration = 2.5f,
                              .remaining = 2.5f,
                              .is_debuff = true});

  // 附近目标已带同一 FrostSlow，传染刷新后其 type/kind 必须保持不变
  const auto targetA = registry.create();
  registry.emplace<EnemyTag>(targetA);
  registry.emplace<Position>(targetA, 50.0f, 0.0f);
  registry.emplace<HealthComponent>(targetA, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetA);
  auto &afx = registry.emplace<ActiveEffectsComponent>(targetA);
  afx.AddOrRefresh(BuffEffect{.id = "FrostSlow",
                              .name = "Frost Slow",
                              .type = BuffType::SpeedDown,
                              .kind = BuffKind::Slow,
                              .duration = 2.5f,
                              .remaining = 2.5f,
                              .is_debuff = true});

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Cold, false);

  auto *fxA = registry.try_get<ActiveEffectsComponent>(targetA);
  REQUIRE(fxA != nullptr);
  const BuffEffect *spread = fxA->GetByKind(BuffKind::Slow);
  REQUIRE(spread != nullptr);
  CHECK(spread->id == "FrostSlow");
  CHECK(spread->type == BuffType::SpeedDown);
  CHECK(spread->kind == BuffKind::Slow);
}

} // namespace NoMoreDay

