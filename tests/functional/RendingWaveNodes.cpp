#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"

#include <cmath>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 2;

void LoadSkillData() {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillBehaviorRegistry::Initialize();
  ProcBudgetManager::Get().ResetForTests();
  CombatEventDispatcher::Init();
  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());
}

void MakeDeterministic(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

entt::entity MakePlayer(entt::registry &registry, float x, float y,
                        const std::unordered_map<uint32_t, int> &nodes) {
  auto player = registry.create();
  registry.emplace<Position>(player, x, y);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 100.0f;
  stats.health = 100.0f;
  stats.max_mana = 100.0f;
  stats.mana = 100.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.effective_dexterity = 50.0f;
  MakeDeterministic(stats);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].current_charges = 1;
  active.specialized_slots[0].skill_id = kSkillId;
  active.specialized_slots[0].allocated_points = nodes;
  return player;
}

entt::entity MakeEnemy(entt::registry &registry, float x, float y, float health = 1000.0f) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<EnemyTag>(enemy);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = health;
  stats.health = health;
  MakeDeterministic(stats);
  registry.emplace<HealthComponent>(enemy, health, health);
  return enemy;
}

void CastRendingWave(entt::registry &registry, entt::entity player, Vector2 target_pos) {
  SkillExecution exec;
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target_pos;
  exec.cast_id = 1;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
}

} // namespace

// ============================================================================
// Base Tier Tests (200 - 203)
// ============================================================================

TEST_CASE("[Unit] RendingWave - 200 WaveExpansion scales area radius and range") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{200, 4}}); // max 4 points

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId,
                                &registry.get<ActiveSkillsComponent>(player).specialized_slots[0],
                                profile, nullptr);

  // Base area_radius = 35.0f * (1 + 0.1 * 4) = 49.0f
  CHECK(profile.area_radius == doctest::Approx(49.0f));
  // Range = 500 * (1 + 0.1 * 4) = 700.0f
  CHECK(profile.delivery.range == doctest::Approx(700.0f));

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());
  CHECK(proj.radius == doctest::Approx(49.0f));
  CHECK(proj.lifeTime == doctest::Approx(700.0f / 300.0f));
}

TEST_CASE("[Unit] RendingWave - 201 Focus reduces mana cost") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{201, 3}}); // 3 points -> -3 mana

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId,
                                &registry.get<ActiveSkillsComponent>(player).specialized_slots[0],
                                profile, nullptr);

  // Base mana = 10.0f - 3.0f = 7.0f
  CHECK(profile.effective_mana_cost == doctest::Approx(7.0f));
}

TEST_CASE("[Unit] RendingWave - 202 Edge grants more physical damage") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{202, 5}}); // max 5 points -> +50%

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId,
                                &registry.get<ActiveSkillsComponent>(player).specialized_slots[0],
                                profile, nullptr);

  CHECK(profile.more_damage_mult == doctest::Approx(1.50f));
}

// ============================================================================
// Branch A Tests: Multi-Wave, Split, Scatter, Orbit, Pursuit (210 - 215)
// ============================================================================

TEST_CASE("[Unit] RendingWave - 210 MultiWave increases projectile count and applies penalty") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{210, 3}}); // +3 projectiles, -15% penalty (0.85x)

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId,
                                &registry.get<ActiveSkillsComponent>(player).specialized_slots[0],
                                profile, nullptr);

  CHECK(profile.projectile_count == 4);
  CHECK(profile.more_damage_mult == doctest::Approx(0.85f));

  CastRendingWave(registry, player, {200.0f, 0.0f});

  int count = 0;
  for (auto ent : registry.view<Projectile>()) {
    (void)ent;
    count++;
  }
  CHECK(count == 4);
}

TEST_CASE("[Unit] RendingWave - 211 & 212 Fracture & ChainReaction set homing split") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{211, 1}, {212, 2}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  auto ent = *projView.begin();
  const auto &proj = projView.get<Projectile>(ent);

  // Fracture: pierce is false so it splits on first hit!
  CHECK_FALSE(proj.pierce);
  CHECK(proj.on_death == Projectile::OnDeathBehavior::Split);
  CHECK(proj.split_count == 3);
  CHECK(registry.any_of<HomingTag>(ent));
  REQUIRE(registry.any_of<SeekerComponent>(ent));

  const auto &seeker = registry.get<SeekerComponent>(ent);
  // Chain reaction: turn_rate reinforced by 15% * 2 = 30%
  CHECK(seeker.turn_rate == doctest::Approx(5.0f * 1.30f));
  // 211 分裂参数自 skill_mechanics.json (节点211) 接线，不再依赖默认值巧合；
  // 212 追踪速度提升：0.8 * (1 + 0.10 * 2) = 0.96
  CHECK(proj.split_damage_mult == doctest::Approx(0.5f));
  CHECK(proj.split_speed_mult == doctest::Approx(0.96f));
}

TEST_CASE("[Unit] RendingWave - 213 Scatter explodes into 8 piercing shards") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{213, 1}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());

  CHECK_FALSE(proj.pierce);
  CHECK(proj.on_death == Projectile::OnDeathBehavior::Explode);
  CHECK(proj.explode_count == 8);
}

TEST_CASE("[Unit] RendingWave - 214 Orbit creates OrbitingSentinel with 3s lifetime") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{214, 1}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto sentinelView = registry.view<OrbitingSentinelComponent, Projectile>();
  REQUIRE(sentinelView.begin() != sentinelView.end());

  const auto &sentinel = sentinelView.get<OrbitingSentinelComponent>(*sentinelView.begin());
  const auto &proj = sentinelView.get<Projectile>(*sentinelView.begin());

  CHECK(sentinel.anchor_entity == player);
  CHECK(sentinel.orbit_radius == doctest::Approx(60.0f));
  CHECK(proj.lifeTime == doctest::Approx(3.0f));
}

TEST_CASE("[Unit] RendingWave - 215 SpiritPursuit spawns companion spirit blade") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{215, 1}});
  registry.emplace<BladeFormationComponent>(player); // Active Blade Formation

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto spiritView = registry.view<SpiritSwordTag, Projectile>();
  REQUIRE(spiritView.begin() != spiritView.end());
  const auto &spProj = spiritView.get<Projectile>(*spiritView.begin());
  CHECK(spProj.owner == player);
  CHECK(spProj.payload_context.has_value());
  CHECK(spProj.payload_context->more_damage == doctest::Approx(0.20f));
}

// ============================================================================
// Branch B Tests: Boomerang, DoubleHit, Gravity, TimeLock (230 - 235)
// ============================================================================

TEST_CASE("[Unit] RendingWave - 230 & 231 Boomerang has sufficient lifetime and DoubleHit More scaling") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{230, 1}, {231, 4}}); // max 4 points -> +60%

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto boomView = registry.view<BoomerangComponent, Projectile>();
  REQUIRE(boomView.begin() != boomView.end());

  const auto &bc = boomView.get<BoomerangComponent>(*boomView.begin());
  const auto &proj = boomView.get<Projectile>(*boomView.begin());

  // Lifetime must be >= 3.0s (not truncated at 1.2s!)
  CHECK(proj.lifeTime >= 3.0f);
  CHECK(bc.phase == BoomerangPhase::Outward);
  CHECK_FALSE(bc.catch_by_owner); // Skill 2 has no catch refund
  // Returning damage multiplier = 0.70 * (1 + 0.15 * 4) = 0.70 * 1.60 = 1.12
  CHECK(bc.returning_damage_mult == doctest::Approx(1.12f));
}

TEST_CASE("[Unit] RendingWave - 232 & 233 GravityWell & AbyssEdge pull and stun on dissipate") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{230, 1}, {232, 1}, {233, 3}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto boomView = registry.view<BoomerangComponent>();
  REQUIRE(boomView.begin() != boomView.end());
  const auto &bc = boomView.get<BoomerangComponent>(*boomView.begin());

  // pull_radius = 120 * (1 + 0.20 * 3) = 192.0f
  CHECK(bc.pull_radius == doctest::Approx(192.0f));
  CHECK(bc.stun_on_apex_end == true);
}

TEST_CASE("[Unit] RendingWave - 234 TimeLock creates hovering storm and does not return") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{234, 1}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  // Must not have BoomerangComponent
  auto boomView = registry.view<BoomerangComponent>();
  CHECK(boomView.empty());

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());

  CHECK(proj.on_death == Projectile::OnDeathBehavior::Hover);
  CHECK(proj.hover_duration == doctest::Approx(2.0f));
}

// ============================================================================
// Branch C Tests: QiBrand, IntentBurst, Obliteration, Echo, Recovery (250 - 255)
// ============================================================================

TEST_CASE("[Unit] RendingWave - 250 QiBrand applies debuff on hit") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{250, 4}}); // 100% chance
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Physical, false);

  auto *vicEffects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(vicEffects != nullptr);
  // 热路径契约：烙印按数值类别查找 (GetByKind)，kind 与 id 同步写入
  const auto *brand = vicEffects->GetByKind(BuffKind::QiBrand);
  REQUIRE(brand != nullptr);
  CHECK(brand->kind == BuffKind::QiBrand);
  CHECK(brand->type == BuffType::DefenseDown);
  CHECK(brand->stacks == 1);
}

TEST_CASE("[Unit] RendingWave - 251 IntentBurst consumes intent when >= 5") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{251, 1}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 6;
  intent.max_stacks = 10;

  CastRendingWave(registry, player, {200.0f, 0.0f});

  // Intent consumed to 0
  CHECK(intent.stacks == 0);

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());
  // Base radius 35.0 * (1 + 0.08 * 6) = 35.0 * 1.48 = 51.8f
  CHECK(proj.radius == doctest::Approx(51.8f));
}

TEST_CASE("[Unit] RendingWave - 251 IntentBurst does NOT consume intent when < 5") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{251, 1}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 4;
  intent.max_stacks = 10;

  CastRendingWave(registry, player, {200.0f, 0.0f});

  // Intent preserved
  CHECK(intent.stacks == 4);
}

TEST_CASE("[Unit] RendingWave - 253 ObliterationWave triggers at 10 intent") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{251, 1}, {253, 1}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 10;

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());

  // More damage 1.8x (全局乘区默认 1.0 不改变数值)；253 湮灭波以显式布尔
  // ignore_resist 标志替代旧 armor_pen+=500 哨兵值
  CHECK(proj.payload_context->more_damage == doctest::Approx(1.80f));
  CHECK(proj.ignore_resist);
}

TEST_CASE("[Unit] RendingWave - 254 EchoedSlash triggers backward wave") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{251, 1}, {254, 1}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 5;

  CastRendingWave(registry, player, {200.0f, 0.0f});

  int count = 0;
  bool foundBackward = false;
  for (auto ent : registry.view<Projectile, Velocity>()) {
    const auto &vel = registry.get<Velocity>(ent);
    if (vel.vx < 0.0f) {
      foundBackward = true;
    }
    count++;
  }
  // 1 forward wave + 1 backward echoed slash
  CHECK(count == 2);
  CHECK(foundBackward);
}

TEST_CASE("[Unit] RendingWave - 255 IntentRecovery only applies to intent-consumed casts") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{251, 1}, {254, 1}, {255, 3}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 6;

  CastRendingWave(registry, player, {200.0f, 0.0f});

  // 消耗剑意施放：主波携带 IntentConsumedCastTag (255 roll 前提)，
  // 254 回响斩衍生波 (向后) 不带标记，命中不触发回剑意
  int taggedForward = 0;
  int untaggedBackward = 0;
  for (auto ent : registry.view<Projectile, Velocity>()) {
    const auto &vel = registry.get<Velocity>(ent);
    if (vel.vx < 0.0f) {
      CHECK_FALSE(registry.any_of<IntentConsumedCastTag>(ent));
      ++untaggedBackward;
    } else {
      CHECK(registry.any_of<IntentConsumedCastTag>(ent));
      ++taggedForward;
    }
  }
  CHECK(taggedForward == 1);
  CHECK(untaggedBackward == 1);

  // 普通施放 (剑意低于 251 门槛，未消耗)：主波同样不带标记
  entt::registry registry2;
  auto player2 = MakePlayer(registry2, 0.0f, 0.0f, {{251, 1}, {255, 3}});
  auto &intent2 = registry2.emplace<SwordIntentComponent>(player2);
  intent2.stacks = 4;

  CastRendingWave(registry2, player2, {200.0f, 0.0f});
  CHECK(intent2.stacks == 4);
  for (auto ent : registry2.view<Projectile>()) {
    CHECK_FALSE(registry2.any_of<IntentConsumedCastTag>(ent));
  }
}

// ============================================================================
// Branch D Tests: FrostForm, Shatter, LightningForm, Conduction (270 - 275)
// ============================================================================

TEST_CASE("[Unit] RendingWave - 270 FrostForm converts to Cold, chills and freezes full health") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{270, 1}});
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f, 100.0f); // 100% full health

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());
  CHECK(proj.payload_context->effective_tags == Tag::Cold);

  auto *mod = registry.try_get<SkillModifierComponent>(*projView.begin());
  REQUIRE(mod != nullptr);
  REQUIRE_FALSE(mod->damage_modifiers.empty());
  CHECK(mod->damage_modifiers.front().target_tag == Tag::Cold);

  // Trigger hit
  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Cold, false);

  auto *vicEffects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(vicEffects != nullptr);
  bool hasChill = false;
  bool hasFreeze = false;
  for (const auto &fx : vicEffects->effects) {
    if (fx.type == BuffType::SpeedDown || fx.id.find("Chill") != std::string::npos) hasChill = true;
    if (fx.type == BuffType::Freeze || fx.id.find("Freeze") != std::string::npos) hasFreeze = true;
  }
  CHECK(hasChill);
  CHECK(hasFreeze);
}

TEST_CASE("[Unit] RendingWave - 272 LightningForm doubles speed and converts to Lightning") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{272, 1}});

  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());
  CHECK(proj.speed == doctest::Approx(600.0f)); // 300 * 2
  CHECK(proj.payload_context->effective_tags == Tag::Lightning);

  auto *mod = registry.try_get<SkillModifierComponent>(*projView.begin());
  REQUIRE(mod != nullptr);
  REQUIRE_FALSE(mod->damage_modifiers.empty());
  CHECK(mod->damage_modifiers.front().target_tag == Tag::Lightning);
}

TEST_CASE("[Unit] RendingWave - 274 ElementalAffinity bakes penetration") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{270, 1}, {274, 4}}); // max 4 points -> 20% pen

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId,
                                &registry.get<ActiveSkillsComponent>(player).specialized_slots[0],
                                profile, nullptr);

  CHECK(profile.delivery.armor_pen == doctest::Approx(20.0f));
  CHECK((profile.delivery.feature_flags & (1 << 22)) != 0);
}

TEST_CASE("[Unit] RendingWave - 274 ElementalAffinity applies penetration in DamageMitigationService") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{270, 1}, {274, 4}}); // 20% pen
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f, 1000.0f);
  auto &defender_stats = registry.get<CombatStats>(enemy);
  defender_stats.resistances[static_cast<int>(DamageType::Cold)] = 0.50f; // 50% cold resist

  DamageRequest req;
  req.attacker = player;
  req.defender = enemy;
  req.skill_id = kSkillId;
  req.base_pool.Add(Tag::Cold, 100.0f);
  req.additional_tags = Tag::Hit | Tag::Cold;
  req.is_simulation = true;

  auto res = DamagePipeline::Calculate(registry, req);
  // Base 100 dmg with Dexterity scaling (200 raw). Without pen: 50% resist -> 100 dmg. With 20% pen: 30% resist -> 140 dmg.
  CHECK(res.total_damage == doctest::Approx(140.0f));
}

// 235 御剑引力回归：护甲击碎与流云刺 152 (FlowingThrust) 共用运行时 id
// "ArmorShred"，语义为 DefenseDown + 每层 -10 护甲 Flat；RendingWave 侧额外把
// 投掷层数写入 .stacks 用于展示。
TEST_CASE("[Unit] RendingWave - 235 ArmorShred shares id and DefenseDown semantics") {
  TestSetupScope scope;
  LoadSkillData();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{235, 2}}); // 100% 触发，1 层
  registry.emplace<PhaseTag>(player);                          // 御剑步状态
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Physical, false);

  auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);
  const auto *shred = fx->Get("ArmorShred");
  REQUIRE(shred != nullptr);
  CHECK(shred->type == BuffType::DefenseDown);
  CHECK(shred->stacks == 1);
  REQUIRE(!shred->modifiers.empty());
  CHECK(shred->modifiers[0].type == StatType::Armor);
  CHECK(shred->modifiers[0].value == doctest::Approx(-10.0f));
}

} // namespace NoMoreDay
