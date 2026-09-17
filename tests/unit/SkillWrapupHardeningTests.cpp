// UMR 收尾与契约加固验收测试（计划 v1.2：Task 3.0 前置门禁 + Phase 6.3）。
//
// 覆盖：
//   1. 节点 1015 的 stat 作用域前置门禁（Task 3.0）：决定清理数据是否可安全推进；
//   2. 技能 10 施法半径按烘焙档案单源解析（56/68/100，Task 2.1/6.3）；
//   3. 技能 6 超距落点钳制与 owner 缺 Position 的降级（Task 2.2/6.3）；
//   4. 技能 7 引导射程共享解析函数的三分支（Task 2.4/4.2）；
//   5. 节点 1015 保留 SKILL_DURATION_FLAT 无敌时长（Task 3.0/6.3）。
//
// 断言一律落在行为层可观测量上（组件字段/返回值），不复制实现公式，避免与实现同源。

#include "TestCommon.hpp"

#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/behaviors/BeamChannelShared.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cstdint>
#include <unordered_map>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSevenStarSkillId =
    skills::seven_star_shared::kSevenStarSlashSkillId; // 技能 10
constexpr uint32_t kSwordArraySkillId = 6;
constexpr uint32_t kBeamChannelSkillId = 7;
constexpr uint32_t kNodeVoidTread = skills::SevenStarSlashNodes::VoidTread;

// 用例前置：加载权威技能表与机制表。技能行为表为静态注册，Initialize 幂等。
void EnsureGameData() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

// 构造最小七星斩施法者。bakedAreaRadius > 0 时注入手工烘焙档案：
// ResolveBakedProfile 的缓存命中要求 is_baked，否则行为层会即时重烘覆盖本值。
entt::entity CreateSevenStarOwner(
    entt::registry &registry,
    const std::unordered_map<uint32_t, int> &allocated = {},
    float bakedAreaRadius = 0.0f, uint32_t transmuterNode = 0u) {
  const entt::entity owner = registry.create();
  registry.emplace<Position>(owner, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(owner, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(owner);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 200.0f;
  stats.mana = 200.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(owner);
  active.slots[0].id = kSevenStarSkillId;
  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSevenStarSkillId;
  spec.allocated_points = allocated;

  if (bakedAreaRadius > 0.0f) {
    auto &profile = active.baked_profiles[0];
    profile = BakedSkillProfile{};
    profile.skill_id = kSevenStarSkillId;
    profile.is_baked = true;
    profile.area_radius = bakedAreaRadius;
  }

  if (transmuterNode != 0u) {
    auto &runtime =
        registry.get_or_emplace<SkillContractRuntimeComponent>(owner);
    runtime.active_transmuter_node_by_skill[kSevenStarSkillId] = transmuterNode;
  }
  return owner;
}

// 施放七星斩并返回无敌护盾半径（= 施法命中半径 hitRadius 的可观测量）。
float CastSevenStarAndReadShieldRadius(entt::registry &registry,
                                       entt::entity owner) {
  SkillExecution exec{};
  exec.skill_id = kSevenStarSkillId;
  exec.owner = owner;
  exec.target_pos = Vector2{100.0f, 0.0f};

  auto cast = SkillBehaviorRegistry::GetCast(kSevenStarSkillId);
  REQUIRE(cast != nullptr);
  cast(registry, owner, exec);

  const auto *invulnerable = registry.try_get<InvulnerableComponent>(owner);
  REQUIRE(invulnerable != nullptr);
  return invulnerable->shieldRadius;
}

// 构造最小剑垒施法者；withPosition=false 用于验证 owner 缺 Position 的降级路径。
entt::entity CreateSwordArrayOwner(
    entt::registry &registry,
    const std::unordered_map<uint32_t, int> &allocated = {},
    bool withPosition = true) {
  const entt::entity owner = registry.create();
  if (withPosition) {
    registry.emplace<Position>(owner, 0.0f, 0.0f);
  }
  registry.emplace<HealthComponent>(owner, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(owner);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 500.0f;
  stats.mana = 500.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.effective_intelligence = 100.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(owner);
  active.slots[0].id = kSwordArraySkillId;
  active.slots[0].current_charges = 5;
  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSwordArraySkillId;
  spec.allocated_points = allocated;
  return owner;
}

// 施放剑垒并返回生成实体的落点坐标。
Vector2 CastSwordArrayAndReadSpawn(entt::registry &registry, entt::entity owner,
                                   Vector2 targetPos) {
  SkillExecution exec{};
  exec.skill_id = kSwordArraySkillId;
  exec.owner = owner;
  exec.target_pos = targetPos;

  auto cast = SkillBehaviorRegistry::GetCast(kSwordArraySkillId);
  REQUIRE(cast != nullptr);
  cast(registry, owner, exec);

  auto view = registry.view<SwordArrayComponent>();
  REQUIRE(view.begin() != view.end());
  const auto *pos = registry.try_get<Position>(*view.begin());
  REQUIRE(pos != nullptr);
  return Vector2{pos->x, pos->y};
}

// 构造仅含 CombatStats 与专精点、用于 GetStatWithTags 探针的最小角色。
// dodgeChance 注入非零基准，使探针结果可分辨"读到活值"与"读不到任何来源"。
entt::entity CreateScopedCaster(
    entt::registry &registry,
    const std::unordered_map<uint32_t, int> &allocated,
    float dodgeChance = 0.0f) {
  const entt::entity caster = registry.create();
  auto &stats = registry.emplace<CombatStats>(caster);
  stats.dodge_chance = dodgeChance;
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSevenStarSkillId;
  spec.allocated_points = allocated;
  return caster;
}

float ReadDodgeChance(entt::registry &registry, entt::entity caster,
                      uint32_t skillId) {
  return StatsSystem::GetStatWithTags(registry, caster, StatType::DodgeChance,
                                      Tag::None, skillId, entt::null);
}

} // namespace

// Task 3.0 门禁：节点 1015 清理后必须（a）数据契约上确实不再携带 stat_modifiers，
// （b）不产生作用域外的属性污染。基线注入 0.15 闪避使探针结果非平凡，可排除
// "探针读不到任何来源"造成的假阴性。
//
// 实测偏差（相对计划 v1.2 的预期）：计划原以"skill_id=10 时应可观测"作为作用域
// 正对照，但节点 1015 的修饰符未声明 required_tags，StatsSystem.cpp:318 的
// is_baked 快路径（required_tags==Tag::None 视为已由 AttributePipeline 烘焙）
// 会在所有作用域整体跳过它，且 AttributePipeline 并不折叠专精节点修饰符，故该
// 修饰符为双重惰性；计划原先的 skill_id=0 相等断言亦因 ScopePolicy::SkillOnly
// 的结构性约束而恒真，不构成门禁证据。故此处改以"加载后的技能树不再携带该组
// stat_modifiers"作为可证伪的主要证据，作用域探针降级为不变量锁定项。
TEST_CASE("[Unit] SkillWrapup - Node 1015 stat scope gate (Task 3.0)") {
  TestSetupScope setup;
  EnsureGameData();

  entt::registry registry;
  const entt::entity allocated = CreateScopedCaster(registry, {{1015u, 3}}, 0.15f);
  const entt::entity baseline = CreateScopedCaster(registry, {}, 0.15f);

  // (0) 探针灵敏度：基线本身必须被读出（0.15 -> 15.0），否则后续相等断言无意义。
  const float baselineProbe = ReadDodgeChance(registry, baseline, 0u);
  INFO("灵敏度对照：baseline=" << baselineProbe);
  REQUIRE(baselineProbe == doctest::Approx(15.0f));

  // (1) 数据契约（主要证据）：清理后节点 1015 不再携带任何 stat_modifiers。
  // 该断言在数据回写前必然失败，是"清理生效"的直接可观测证明。
  const SkillTreeDefinition *tree =
      SkillRegistry::Get().GetSkillTree(kSevenStarSkillId);
  REQUIRE(tree != nullptr);
  const auto voidTreadIt = tree->nodes.find(kNodeVoidTread);
  REQUIRE(voidTreadIt != tree->nodes.end());
  CHECK(voidTreadIt->second.stat_modifiers.empty());

  // (2) 作用域不变量：skill_id=0 与 skill_id=10 查询均与基准一致。该结论由
  // ScopePolicy::SkillOnly 与 is_baked 快路径结构性保证，此处仅作"不得回归"的
  // 锁定项，不作为门禁的证伪证据。
  const auto scopedEqual = [&](uint32_t skillId) {
    const float allocatedValue = ReadDodgeChance(registry, allocated, skillId);
    const float baselineValue = ReadDodgeChance(registry, baseline, skillId);
    INFO("skill_id=" << skillId << "：allocated=" << allocatedValue
                     << " baseline=" << baselineValue);
    CHECK(allocatedValue == doctest::Approx(15.0f));
    CHECK(baselineValue == doctest::Approx(15.0f));
  };
  scopedEqual(0u);
  scopedEqual(kSevenStarSkillId);
}


// Phase 6.3 Task 2.1：施法半径必须由烘焙档案 area_radius 单源解析。
// 改动前 baseRadius 被强制 96，三形态为 26.88/32.64/48，本条必然失败（可证伪）。
TEST_CASE("[Unit] SkillWrapup - Skill 10 radius resolves from baked profile") {
  TestSetupScope setup;
  EnsureGameData();

  entt::registry registry;
  // 基础形态：200 * 0.28 = 56。
  CHECK(CastSevenStarAndReadShieldRadius(
            registry, CreateSevenStarOwner(registry, {}, 200.0f)) ==
        doctest::Approx(56.0f));
  // 极星轨迹（node 1021）：200 * 0.34 = 68。
  CHECK(CastSevenStarAndReadShieldRadius(
            registry, CreateSevenStarOwner(
                          registry, {}, 200.0f,
                          skills::SevenStarSlashNodes::PoleStarOrbit)) ==
        doctest::Approx(68.0f));
  // 星落（node 1022）：200 * 0.50 = 100。
  CHECK(CastSevenStarAndReadShieldRadius(
            registry, CreateSevenStarOwner(registry, {}, 200.0f,
                                           skills::SevenStarSlashNodes::Starfall)) ==
        doctest::Approx(100.0f));
}

// Phase 6.3 Task 2.2：剑垒超距落点沿施法方向钳制到 delivery.range 圆周。
TEST_CASE("[Unit] SkillWrapup - Skill 6 cast offset clamps to delivery range") {
  TestSetupScope setup;
  EnsureGameData();
  // RANGE_MULT 由生成的 UMR 运行时提供，显式重载以隔离用例执行顺序。
  REQUIRE(ReloadModifierRuntimeFromAsset());

  SUBCASE("base range clamps 10000 to 400") {
    entt::registry registry;
    const entt::entity owner = CreateSwordArrayOwner(registry, {}, true);
    const Vector2 spawn =
        CastSwordArrayAndReadSpawn(registry, owner, Vector2{10000.0f, 0.0f});
    CHECK(spawn.x == doctest::Approx(400.0f));
    CHECK(spawn.y == doctest::Approx(0.0f));
  }

  SUBCASE("node 603 range clamps 10000 to 560") {
    entt::registry registry;
    const entt::entity owner =
        CreateSwordArrayOwner(registry, {{603u, 4}}, true);
    const Vector2 spawn =
        CastSwordArrayAndReadSpawn(registry, owner, Vector2{10000.0f, 0.0f});
    // 400 * (1 + 0.10 * 4) = 560。
    CHECK(spawn.x == doctest::Approx(560.0f));
    CHECK(spawn.y == doctest::Approx(0.0f));
  }

  SUBCASE("owner without Position degrades to target point without crash") {
    entt::registry registry;
    const entt::entity owner = CreateSwordArrayOwner(registry, {}, false);
    const Vector2 spawn =
        CastSwordArrayAndReadSpawn(registry, owner, Vector2{100.0f, 0.0f});
    CHECK(spawn.x == doctest::Approx(100.0f));
    CHECK(spawn.y == doctest::Approx(0.0f));
  }
}

// Phase 6.3 Task 2.4/4.2：技能 7 引导射程共享解析函数三分支。
TEST_CASE("[Unit] SkillWrapup - ResolveBeamChannelMaxRange branches") {
  TestSetupScope setup;
  EnsureGameData();

  // 默认实参引用生产常量，期望值不再重复字面量；该断言同时校验机制表口径与常量一致。
  const float fallback = data::SkillMechanicsRegistry::Get().GetFloat(
      kBeamChannelSkillId, 0u, "base_range", kBeamChannelBaseRangeDefault);
  CHECK(fallback == doctest::Approx(kBeamChannelBaseRangeDefault));

  BakedSkillProfile baked{};
  baked.skill_id = kBeamChannelSkillId;
  baked.is_baked = true;
  baked.delivery.range = 420.0f;
  CHECK(ResolveBeamChannelMaxRange(&baked, kBeamChannelSkillId) ==
        doctest::Approx(420.0f));

  CHECK(ResolveBeamChannelMaxRange(nullptr, kBeamChannelSkillId) ==
        doctest::Approx(fallback));

  // 哨兵档案（is_baked=false）不得被当作有效射程来源。
  BakedSkillProfile sentinel{};
  sentinel.skill_id = kBeamChannelSkillId;
  sentinel.is_baked = false;
  sentinel.delivery.range = 420.0f;
  CHECK(ResolveBeamChannelMaxRange(&sentinel, kBeamChannelSkillId) ==
        doctest::Approx(fallback));
}

// Phase 6.3 Task 3.0：清理 stat_modifiers 后节点 1015 仍须授予无敌时长。
TEST_CASE("[Unit] SkillWrapup - Node 1015 keeps flat invulnerability duration") {
  TestSetupScope setup;
  EnsureGameData();
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;
  const entt::entity owner =
      CreateSevenStarOwner(registry, {{kNodeVoidTread, 3}});

  SkillExecution exec{};
  exec.skill_id = kSevenStarSkillId;
  exec.owner = owner;
  exec.target_pos = Vector2{100.0f, 0.0f};
  auto cast = SkillBehaviorRegistry::GetCast(kSevenStarSkillId);
  REQUIRE(cast != nullptr);
  cast(registry, owner, exec);

  const auto *invulnerable = registry.try_get<InvulnerableComponent>(owner);
  REQUIRE(invulnerable != nullptr);
  // 基础 0.5s + SKILL_DURATION_FLAT 每点 0.03s * 3 = 0.59s
  //（与 SkillSpecializationBakerTests 对 1015 的断言同值）。
  CHECK(invulnerable->duration == doctest::Approx(0.59f));
}

} // namespace NoMoreDay
