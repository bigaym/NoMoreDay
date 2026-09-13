// 技能4 剑气护体 追补实现回归测试（计划 §5 C2 / B1-02/03/04）。
// 覆盖：节点数值烘焙、476 元素曝光、472/473 雷电法环、435 暴击加成、
// 470 反击剑气实体、474/475 霜铠冰霜风暴。
#include "TestCommon.hpp"

#include <cmath>
#include <cstddef>
#include <string_view>
#include <unordered_map>

#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/FactionComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/damage/DamageInterceptors.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/BladeWardRuntime.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 4;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

// 注册与生产语义一致的结算钩子：让 472 雷电法环脉冲经真实伤害管线落地。
void RegisterTestResolutionHooks() {
  DamageResolutionHooks hooks;
  hooks.execute = [](entt::registry &registry, const DamageRequest &request,
                     entt::entity target) {
    return DamagePipeline::Execute(registry, request, target, false);
  };
  hooks.calculateBatch = [](entt::registry &registry,
                            const DamageRequest &request) {
    return DamagePipeline::CalculateBatchResults(registry, request);
  };
  RegisterDamageResolutionHooks(hooks);
}

entt::entity CreateTestPlayer(entt::registry &registry,
                              const std::unordered_map<uint32_t, int> &allocated_nodes = {}) {
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(player, 10000.0f, 10000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 10000.0f;
  stats.health = 10000.0f;
  stats.max_mana = 200.0f;
  stats.mana = 200.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 10.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
  active.baked_profiles[0] = profile;

  return player;
}

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, 5000.0f, 5000.0f);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = 5000.0f;
  stats.health = 5000.0f;
  stats.armor = 0.0f;
  // 雷电法环/霜铠风暴以 FactionComponent + EnemyTag 作为敌方筛选标记。
  registry.emplace<FactionComponent>(enemy);
  registry.emplace<EnemyTag>(enemy);
  return enemy;
}

entt::entity CastBladeWard(entt::registry &registry, entt::entity player) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = {0.0f, 0.0f};
  static uint64_t s_cast_id = 100;
  exec.cast_id = ++s_cast_id;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
  return player;
}

const BuffEffect *FindEffectById(const ActiveEffectsComponent *fx,
                                 std::string_view id) {
  if (fx == nullptr) {
    return nullptr;
  }
  for (const auto &effect : fx->effects) {
    if (effect.id == id) {
      return &effect;
    }
  }
  return nullptr;
}

const StatModifier *FindModifier(const BuffEffect *effect, StatType type) {
  if (effect == nullptr) {
    return nullptr;
  }
  for (const auto &modifier : effect->modifiers) {
    if (modifier.type == type) {
      return &modifier;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("[Functional] Skill 4 - Followup Nodes Bake Mechanics Values (C2.3/C2.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  const std::unordered_map<uint32_t, int> nodes = {
      {402, 1}, {403, 1}, {410, 1}, {413, 1}, {415, 1}, {431, 1}, {433, 1},
      {434, 1}, {435, 1}, {453, 1}, {454, 1}, {472, 1}, {473, 1}, {474, 1},
      {475, 1}, {476, 1}};
  auto player = CreateTestPlayer(registry, nodes);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);

  // 402/403
  CHECK(ward->mana_cost_reduction == doctest::Approx(0.15f));
  CHECK(ward->counter_chance_bonus == doctest::Approx(0.10f));
  CHECK(ward->counter_range_bonus == doctest::Approx(0.10f));
  // 410
  CHECK(ward->armor_dr_bonus == doctest::Approx(0.10f));
  CHECK(ward->armor_dr_per_1000 == doctest::Approx(0.01f));
  // 413
  CHECK(ward->last_stand_threshold == doctest::Approx(0.35f));
  CHECK(ward->last_stand_base_dr == doctest::Approx(0.24f));
  CHECK(ward->last_stand_armor_mult == doctest::Approx(2.0f));
  // 415/433/434
  CHECK(ward->missing_hp_regen_pct == doctest::Approx(0.005f));
  CHECK(ward->has_blood_barrier);
  CHECK(ward->perfect_parry_interval == doctest::Approx(5.0f));
  CHECK(ward->perfect_parry_ready);
  // 435/453/454
  CHECK(ward->block_intent_crit == doctest::Approx(0.05f));
  CHECK(ward->block_intent_chance == doctest::Approx(0.15f));
  CHECK(ward->aftermath_heal_pct == doctest::Approx(0.05f));
  CHECK(ward->sword_step_dodge_rating == doctest::Approx(50.0f));
  // 472/473
  CHECK(ward->static_interval == doctest::Approx(0.5f));
  CHECK(ward->static_radius == doctest::Approx(50.0f));
  CHECK(ward->thunder_frequency_bonus == doctest::Approx(0.30f));
  CHECK(ward->shock_slow == doctest::Approx(0.10f));
  CHECK(ward->shock_damage_bonus == doctest::Approx(0.15f));
  // 474/475：半径 = 80 * (1 + 0.20)
  CHECK(ward->frost_knockback == doctest::Approx(100.0f));
  CHECK(ward->frost_radius == doctest::Approx(96.0f));
  CHECK(ward->permafrost_radius_bonus == doctest::Approx(0.20f));
  // 476
  CHECK(ward->exposure_pct == doctest::Approx(0.04f));
  CHECK(ward->exposure_duration == doctest::Approx(3.0f));
}

TEST_CASE("[Functional] Skill 4 - Exposure 476 Shreds Element Resist And Refreshes (C2.1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{476, 3}, {472, 1}});
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->exposure_pct == doctest::Approx(0.12f));

  // 雷电系剑气护体：曝光按雷元素抗性削减结算。
  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);

  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *exposure = FindEffectById(fx, "blade_ward_exposure");
  REQUIRE(exposure != nullptr);
  const StatModifier *resist = FindModifier(exposure, StatType::ResistLightning);
  REQUIRE(resist != nullptr);
  CHECK(resist->value == doctest::Approx(-12.0f));

  // 重复命中只刷新，不叠加：曝光效果数量恒为 1。
  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);
  int exposureCount = 0;
  for (const auto &effect : fx->effects) {
    if (effect.id == "blade_ward_exposure") {
      ++exposureCount;
    }
  }
  CHECK(exposureCount == 1);
}

TEST_CASE("[Functional] Skill 4 - Lightning Aura 472/473 Pulses Shock (C2.2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{472, 1}, {473, 3}, {476, 1}});
  auto enemy = CreateTestEnemy(registry, 30.0f, 0.0f);
  CastBladeWard(registry, player);
  grid.rebuild(registry.view<Position>(), registry);

  auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->is_lightning_ward);

  const auto *enemyHealth = registry.try_get<HealthComponent>(enemy);
  REQUIRE(enemyHealth != nullptr);
  const float before = enemyHealth->current;

  // 单帧 1.0s 足以触发 0.5/(1+0.9) ≈ 0.263s 的雷击脉冲。
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 1.0f);

  CHECK(enemyHealth->current < before);

  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *shock = FindEffectById(fx, "blade_ward_thunder_shock");
  REQUIRE(shock != nullptr);
  const StatModifier *slow = FindModifier(shock, StatType::MoveSpeed);
  REQUIRE(slow != nullptr);
  CHECK(slow->value < 0.0f);
  const StatModifier *vuln = FindModifier(shock, StatType::ResistLightning);
  REQUIRE(vuln != nullptr);
  CHECK(vuln->value < 0.0f);

  // 476 元素曝光在同一脉冲上生效
  CHECK(FindEffectById(fx, "blade_ward_exposure") != nullptr);
  // 473 频率提升后计时器已归零，下一脉冲需重新累积
  CHECK(ward->static_timer == doctest::Approx(0.0f));
}

TEST_CASE("[Functional] Skill 4 - Crit Bonus 435 Baked As Crit Chance (C2.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{435, 3}});
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  // crit_bonus 0.05/点 × 3 = 0.15
  CHECK(ward->block_intent_crit == doctest::Approx(0.15f));
  CHECK(ward->block_intent_chance == doctest::Approx(0.45f));
}

TEST_CASE("[Functional] Skill 4 - Counter Swords Spawn From 470 (C2.5)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{470, 1}});
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);

  std::size_t before = 0;
  for (auto entity : registry.view<Projectile, SkillComponent>()) {
    (void)entity;
    ++before;
  }

  const DamageRequest request =
      damage::ResolveSkill4CounterEffects(registry, player, enemy, *ward, true);
  CHECK(request.skill_id == kSkillId);
  CHECK(request.base_pool.Get(Tag::Physical) > 0.0f);

  std::size_t counterSwords = 0;
  bool allVisualOnly = true;
  for (auto entity : registry.view<Projectile, SkillComponent>()) {
    const auto &projectile = registry.get<Projectile>(entity);
    const auto &skill = registry.get<SkillComponent>(entity);
    if (projectile.owner == player && skill.skill_id == kSkillId) {
      ++counterSwords;
      allVisualOnly = allVisualOnly && projectile.visual_only;
    }
  }
  CHECK(counterSwords == 5u);
  CHECK(counterSwords > before);
  // 反击剑气为纯表现实体：命中不得二次结算伤害（单源由 ResolveSkill4Counter 承担）。
  CHECK(allVisualOnly);
}

TEST_CASE("[Functional] Skill 4 - Cold Counter 474/475 Frost Storm And Exposure (C2.2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{474, 1}, {475, 1}, {476, 1}});
  auto enemy = CreateTestEnemy(registry, 30.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->is_cold_ward);
  CHECK(ward->frost_radius == doctest::Approx(96.0f));

  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);

  // 击退：目标获得远离施放者的速度分量
  const auto *velocity = registry.try_get<Velocity>(enemy);
  REQUIRE(velocity != nullptr);
  CHECK(velocity->vx > 0.0f);

  // 476：曝光按冰元素抗性削减
  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *exposure = FindEffectById(fx, "blade_ward_exposure");
  REQUIRE(exposure != nullptr);
  const StatModifier *resist = FindModifier(exposure, StatType::ResistCold);
  REQUIRE(resist != nullptr);
  CHECK(resist->value == doctest::Approx(-4.0f));
}

TEST_CASE("[Functional] Skill 4 - Perfect Parry 434 Charge Gates Interception") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{434, 1}});
  CastBladeWard(registry, player);

  auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  REQUIRE(ward->perfect_parry_ready);
  CHECK(ward->perfect_parry_interval == doctest::Approx(5.0f));

  // 充能就绪：投射物命中被完全招架，充能被消费。
  const auto intercepted = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK(intercepted.intercepted);
  CHECK_FALSE(ward->perfect_parry_ready);

  // 间隔内不再拦截：剑数归零以排除几率拦截路径，仅验证充能门控。
  ward->sword_count = 0;
  const auto gated = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK_FALSE(gated.intercepted);

  // 间隔尚未填满（1s < 5s）：充能不恢复，继续不拦截。
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 1.0f);
  CHECK_FALSE(ward->perfect_parry_ready);

  // 间隔填满（累计 6s ≥ 5s）：充能恢复，重新具备完全招架能力。
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 5.0f);
  CHECK(ward->perfect_parry_ready);
  const auto recharged = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK(recharged.intercepted);
}

} // namespace NoMoreDay
