#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/OrbitingSentinelDeliverySystem.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"

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

  // Skill 1 node 114 is a trigger contract
  SpecializedSkill spec;
  spec.skill_id = 1;
  spec.allocated_points[114] = 1;

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, &triggers);

  const auto *contract = SkillRegistry::Get().GetNodeContract(1, 114);
  if (contract && contract->role == SpecNodeRole::Trigger) {
    CHECK(triggers.HasRule(114));
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
  active.specialized_slots[0].allocated_points[114] = 1; // Trigger node

  SkillSystem::RebakeSkillProfiles(registry, player);
  const size_t count1 = triggers.rule_count;
  CHECK(count1 > 0);
  CHECK(triggers.HasRule(114));

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
  CHECK(!triggers.HasRule(114));

  // Player re-allocates and equips
  active.specialized_slots[0].allocated_points[114] = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(114));

  // Player unequips hotbar slot only (specialization tree remains allocated)
  active.slots[0].id = 0;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(114));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) == nullptr);

  // Player re-equips hotbar slot
  active.slots[0].id = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(114));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) != nullptr);

  // Player unsets/unequips skill slot completely (both slot and specialization tree)
  active.slots[0].id = 0;
  active.specialized_slots[0].skill_id = 0;
  active.specialized_slots[0].allocated_points.clear();
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(114));

  // Scenario: Entity does NOT have TriggerRuleComponent initially
  {
    entt::registry freshRegistry;
    const auto player2 = freshRegistry.create();
    auto &active2 = freshRegistry.emplace<ActiveSkillsComponent>(player2);
    active2.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
    active2.specialized_slots[0].skill_id = 1;
    active2.specialized_slots[0].allocated_points[114] = 1;

    CHECK_FALSE(freshRegistry.all_of<TriggerRuleComponent>(player2));
    SkillSystem::RebakeSkillProfiles(freshRegistry, player2);
    CHECK(freshRegistry.all_of<TriggerRuleComponent>(player2));
    auto *trig2 = freshRegistry.try_get<TriggerRuleComponent>(player2);
    REQUIRE(trig2 != nullptr);
    CHECK(trig2->rule_count == 1);
    CHECK(trig2->HasRule(114));

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

  // Skill 5 nodes 552 (bonus crit) & 571 (armor pen)
  {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[552] = 2; // 2 * 1.5% = 3.0%
    spec.allocated_points[571] = 3; // 3 * 6.0 = 18.0
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit == doctest::Approx(3.0f));
    CHECK(profile.delivery.armor_pen == doctest::Approx(18.0f));
    CHECK(profile.delivery.speed == 300.0f); // Default speed NOT clobbered!
    CHECK(profile.delivery.range == 200.0f); // Default range NOT clobbered!
    CHECK(profile.more_damage_mult > 1.0f);
  }

  // Skill 8 nodes 830 pull_radius without clobbering range 300
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    spec.allocated_points[830] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.delivery.pull_radius == 100.0f);
    CHECK(profile.delivery.range == 300.0f); // Default flight range preserved
    CHECK((profile.delivery.feature_flags & 4) != 0);
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

} // namespace NoMoreDay
