#include "TestCommon.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include <entt/entity/entity.hpp>

namespace NoMoreDay {

TEST_CASE("[Unit] Projectile - Pierce Limit Reached for Penetrating Projectile") {
  Projectile proj;
  proj.pierce = true;
  proj.max_pierce = 8;
  CHECK_FALSE(proj.IsUnlimitedPiercing());

  bool limitReached = false;
  // Hit 7 distinct targets: none should reach limit
  for (uint32_t i = 1; i <= 7; ++i) {
    entt::entity target{i};
    CHECK(proj.TryRecordHit(target, limitReached));
    CHECK_FALSE(limitReached);
    CHECK(proj.HasHit(target));
  }

  // 8th target reaches the limit of 8
  entt::entity target8{8};
  CHECK(proj.TryRecordHit(target8, limitReached));
  CHECK(limitReached);
  CHECK(proj.HasHit(target8));

  // Re-hitting already hit target: should return false (not recorded) and limitReached is true
  CHECK_FALSE(proj.TryRecordHit(target8, limitReached));
  CHECK(limitReached);

  // 9th target: limit is reached
  entt::entity target9{9};
  CHECK(proj.TryRecordHit(target9, limitReached));
  CHECK(limitReached);
}

TEST_CASE("[Unit] Projectile - AoE Hitbox Unlimited Pierce (>8 Targets)") {
  Projectile proj;
  proj.pierce = true;
  proj.pierceCount = 999;
  proj.max_pierce = Projectile::kUnlimitedPiercing;
  CHECK(proj.IsUnlimitedPiercing());

  bool limitReached = false;
  // Hit 25 distinct targets: NONE should reach limit
  for (uint32_t i = 1; i <= 25; ++i) {
    entt::entity target{i};
    CHECK(proj.TryRecordHit(target, limitReached));
    CHECK_FALSE(limitReached);
  }

  CHECK(proj.hit_count == 25);
  // All 25 targets must be cached in SBO hit_cache (capacity 32) and HasHit returns true
  for (uint32_t i = 1; i <= 25; ++i) {
    CHECK(proj.HasHit(entt::entity{i}));
    // Re-hitting must return false to prevent multi-hit
    CHECK_FALSE(proj.TryRecordHit(entt::entity{i}, limitReached));
    CHECK_FALSE(limitReached);
  }

  // Test SBO overflow beyond 32 targets (targets 26..40)
  for (uint32_t i = 26; i <= 40; ++i) {
    entt::entity target{i};
    CHECK(proj.TryRecordHit(target, limitReached));
    CHECK_FALSE(limitReached);
    CHECK(proj.HasHit(target));
  }
  CHECK(proj.hit_count == 40);

  // CRITICAL: ALL 40 targets (including early targets 1..25) MUST remain remembered!
  // In a lossy ring buffer, targets 1..8 were overwritten and HasHit returned false,
  // causing multi-hit regression. True SBO guarantees zero multi-hit across all 40 targets.
  for (uint32_t i = 1; i <= 40; ++i) {
    entt::entity target{i};
    CHECK(proj.HasHit(target));
    CHECK_FALSE(proj.TryRecordHit(target, limitReached));
    CHECK_FALSE(limitReached);
  }

  // ClearHits must clear both inline cache and overflow storage
  proj.ClearHits();
  CHECK(proj.hit_count == 0);
  for (uint32_t i = 1; i <= 40; ++i) {
    CHECK_FALSE(proj.HasHit(entt::entity{i}));
  }
}

TEST_CASE("[Unit] Projectile - Static AoE Hitbox Multi-Hit Protection Across Updates") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  DamageResolutionHooks hooks;
  hooks.execute = [](entt::registry &reg, const DamageRequest &req, entt::entity target) {
    return DamagePipeline::Execute(reg, req, target, false);
  };
  RegisterDamageResolutionHooks(hooks);

  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.health = 1000.0f;
  pStats.max_health = 1000.0f;
  pStats.min_weapon_damage = 50.0f;
  pStats.max_weapon_damage = 50.0f;

  // Create 12 enemy entities in range
  std::vector<entt::entity> enemies;
  for (int i = 0; i < 12; ++i) {
    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 5.0f + static_cast<float>(i), 5.0f);
    auto &hp = registry.emplace<HealthComponent>(enemy);
    hp.max = 1000.0f;
    hp.current = 1000.0f;
    auto &cStats = registry.emplace<CombatStats>(enemy);
    cStats.health = 1000.0f;
    cStats.max_health = 1000.0f;
    cStats.armor = 0.0f;
    cStats.damage_reduction = 0.0f;
    enemies.push_back(enemy);
  }

  // Create static AoE hitbox projectile (speed = 0.0f, unlimited pierce, lifetime = 0.1f)
  auto projEnt = registry.create();
  registry.emplace<Position>(projEnt, 10.0f, 5.0f);
  registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projEnt);
  proj.owner = player;
  proj.speed = 0.0f;
  proj.radius = 40.0f;
  proj.lifeTime = 0.1f;
  proj.pierce = true;
  proj.pierceCount = 999;
  proj.max_pierce = Projectile::kUnlimitedPiercing;

  DamagePayloadContext ctx{};
  ctx.base_damage_min = 100.0f;
  ctx.base_damage_max = 100.0f;
  ctx.crit_chance = 0.0f;
  ctx.increased_damage = 0.0f;
  ctx.more_damage = 1.0f;
  ctx.effective_tags = Tag::Physical;
  proj.payload_context = ctx;

  auto &skill = registry.emplace<SkillComponent>(projEnt);
  skill.skill_id = 7;
  skill.owner = player;

  // Frame 1: Hitbox settles hits on all candidates in range
  grid.rebuild(registry.view<Position>(), registry);
  ProjectileSystem::Update(registry, grid, 0.016f);

  // Verify that the static hitbox was marked done in Frame 1
  CHECK(proj.hitLimitReached == true);
  CHECK(proj.lifeTime <= 0.0f);

  // Check how many enemies took damage on Frame 1
  int damagedCount = 0;
  for (auto e : enemies) {
    if (registry.get<HealthComponent>(e).current < 1000.0f) {
      damagedCount++;
    }
  }
  CHECK(damagedCount > 0);

  // Record HP after Frame 1
  std::vector<float> hpAfterFrame1;
  for (auto e : enemies) {
    hpAfterFrame1.push_back(registry.get<HealthComponent>(e).current);
  }

  // Frame 2: Hitbox is destroyed and does NOT cause duplicate multi-hit damage
  grid.rebuild(registry.view<Position>(), registry);
  ProjectileSystem::Update(registry, grid, 0.016f);

  // Proj entity must be destroyed
  CHECK_FALSE(registry.valid(projEnt));

  // Verify that no enemy took additional damage in Frame 2
  for (size_t i = 0; i < enemies.size(); ++i) {
    CHECK(registry.get<HealthComponent>(enemies[i]).current == doctest::Approx(hpAfterFrame1[i]));
  }

  DamageResolutionHooks defaultHooks;
  defaultHooks.execute = [](entt::registry &reg, const DamageRequest &req,
                            entt::entity target) {
    return DamagePipeline::Execute(reg, req, target, true);
  };
  defaultHooks.calculateBatch = [](entt::registry &reg,
                                   const DamageRequest &req) {
    DamagePipeline::CalculateBatch(
        reg, req.attacker, std::vector<entt::entity>{req.defender},
        req.skill_id, req.base_pool, req.additional_tags,
        req.source_entity);
    return std::vector<DamageResult>{};
  };
  RegisterDamageResolutionHooks(defaultHooks);
}

} // namespace NoMoreDay
