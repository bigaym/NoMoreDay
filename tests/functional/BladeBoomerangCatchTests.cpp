// 技能8 御剑·回旋 交付系统专项功能测试。
// 范围：单权威状态机（折返/悬停/接刃）、810 滞空切割节拍、832/833/834/835/815
//       接刃结算、850/851 折返路径牵引、870/872 元素路径生成、非法 returnTarget 销毁、
//       以及技能2 回旋语义回归。
// 说明：直接构造 BoomerangComponent 并调用 BoomerangDeliverySystem，隔离行为层版本差异，
//       仅验证交付层契约；数值来源字段由行为层/烘培层写入，本测试直接赋值。

#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BoomerangDeliverySystem.hpp"
#include "game/systems/skill/ElementPathSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <string>

namespace NoMoreDay {
namespace {

constexpr uint32_t kBladeBoomerangSkillId = 8u;
constexpr uint32_t kRendingWaveSkillId = 2u;

// 810 悬停切割计数钩子：注册为技能8的命中函数，统计切割派发次数
int g_hoverCutCount = 0;
void CountBladeBoomerangHit(entt::registry &, entt::entity, entt::entity, Tag,
                            bool) {
  ++g_hoverCutCount;
}

// 命中函数替换守卫：用完例钩子后恢复原实现，避免污染后续用例的 GetHit(8)
// （doctest REQUIRE 提前返回时析构仍会执行）。
struct HitHandlerGuard {
  SkillBehaviorRegistry::HitFunc original;
  ~HitHandlerGuard() {
    SkillBehaviorRegistry::RegisterHit(kBladeBoomerangSkillId, original);
  }
};

// 加载机制/技能数据并注册行为，保证 GetHit(8) 与 Ailment 可用
void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  (void)systems::AilmentRegistry::Get().EnsureLoaded();
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

void RebuildGrid(systems::SpatialHashGrid &grid, entt::registry &registry) {
  grid.rebuild(registry.view<Position>(), registry);
}

entt::entity CreatePlayer(entt::registry &registry, Vector2 pos = {0.0f, 0.0f},
                          float mana = 100.0f, float maxMana = 100.0f,
                          float hp = 1000.0f, float maxHp = 1000.0f) {
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, pos.x, pos.y);
  registry.emplace<HealthComponent>(player, hp, maxHp);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.health = hp;
  stats.max_health = maxHp;
  stats.mana = mana;
  stats.max_mana = maxMana;
  registry.emplace<ActiveSkillsComponent>(player);
  return player;
}

entt::entity CreateEnemy(entt::registry &registry, Vector2 pos, float hp = 1000.0f,
                         bool boss = false) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, pos.x, pos.y);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, hp, hp);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.health = hp;
  stats.max_health = hp;
  if (boss) {
    registry.emplace<BossBattleComponent>(enemy);
  }
  return enemy;
}

// 构造一枚回旋飞剑，字段由调用方按用例继续覆写
entt::entity CreateBlade(entt::registry &registry, entt::entity owner,
                         Vector2 pos, Vector2 vel, float radius,
                         uint32_t skillId = kBladeBoomerangSkillId) {
  auto blade = registry.create();
  registry.emplace<Position>(blade, pos.x, pos.y);
  registry.emplace<Velocity>(blade, vel.x, vel.y);
  auto &proj = registry.emplace<Projectile>(blade);
  proj.owner = owner;
  proj.radius = radius;
  proj.speed = 600.0f;
  proj.lifeTime = 5.0f;
  auto &bc = registry.emplace<BoomerangComponent>(blade);
  bc.owner = owner;
  bc.skill_id = skillId;
  bc.phase = BoomerangPhase::Outward;
  bc.returnSpeed = 600.0f;
  return blade;
}

const BuffEffect *FindEffect(entt::registry &registry, entt::entity e,
                             std::string_view id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (effects == nullptr) {
    return nullptr;
  }
  for (const auto &b : effects->effects) {
    if (b.id == id) {
      return &b;
    }
  }
  return nullptr;
}

bool HasEffectType(entt::registry &registry, entt::entity e, BuffType type) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (effects == nullptr) {
    return false;
  }
  for (const auto &b : effects->effects) {
    if (b.type == type) {
      return true;
    }
  }
  return false;
}

} // namespace

// 810 滞空切割：飞抵最大距离后在顶点悬停 0.8s，每 0.2s 派发一次 AoE 命中
TEST_CASE("[Functional] BladeBoomerang - 810 滞空 0.8s 每 0.2s 切割") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  element_path::ClearForTests(registry);
  auto player = CreatePlayer(registry);
  auto enemy = CreateEnemy(registry, {400.0f, 0.0f});
  REQUIRE(registry.valid(enemy));

  g_hoverCutCount = 0;
  HitHandlerGuard hitGuard{SkillBehaviorRegistry::GetHit(kBladeBoomerangSkillId)};
  SkillBehaviorRegistry::RegisterHit(kBladeBoomerangSkillId,
                                     &CountBladeBoomerangHit);

  // 距施法者 400 > max_distance 300，首帧即判定抵达顶点
  auto blade = CreateBlade(registry, player, {400.0f, 0.0f}, {0.0f, 0.0f}, 40.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.max_distance = 300.0f;
  bc.hover_duration = 0.8f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.01f);
  REQUIRE(bc.phase == BoomerangPhase::HoverApex);
  CHECK(bc.hover_timer == doctest::Approx(0.8f));
  CHECK(g_hoverCutCount == 0);

  // 4 个 0.2s 窗口应在 0.8s 悬停内产生 4 次切割
  for (int i = 0; i < 4; ++i) {
    RebuildGrid(grid, registry);
    BoomerangDeliverySystem::Update(registry, grid, 0.2f);
    CHECK(g_hoverCutCount == i + 1);
  }

  // 悬停窗口结束后进入折返，且不再有额外切割
  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.01f);
  CHECK(g_hoverCutCount == 4);
  CHECK(bc.phase == BoomerangPhase::Returning);
}

// 832 接剑：接住任意一把回旋体返还法力
TEST_CASE("[Functional] BladeBoomerang - 832 接刃回蓝") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry, {0.0f, 0.0f}, 10.0f, 100.0f);
  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.catch_mana = 5.0f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK(registry.try_get<CombatStats>(player)->mana == doctest::Approx(15.0f));
  CHECK_FALSE(registry.valid(blade));
}

// 833 连环劲：接剑后施加攻速增益，2s 持续、刷新叠加
TEST_CASE("[Functional] BladeBoomerang - 833 接刃施加连环劲") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.combo_attack_speed = 20.0f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  const std::string comboId(BuffIdToString(BuffId::BladeBoomerangCombo));
  const auto *combo = FindEffect(registry, player, comboId);
  REQUIRE(combo != nullptr);
  CHECK(combo->type == BuffType::AttackUp);
  CHECK(combo->duration == doctest::Approx(2.0f));
  CHECK(combo->remaining == doctest::Approx(2.0f));
  CHECK(combo->stacks >= 1);
  CHECK(combo->is_debuff == false);
  REQUIRE(combo->modifiers.size() == 1);
  CHECK(combo->modifiers[0].type == StatType::AttackSpeed);
}

// 834 御剑接踵：接剑时延长剑步并施加下次投掷免蓝
TEST_CASE("[Functional] BladeBoomerang - 834 延步与免蓝") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  skills::seven_star_shared::GrantSwordStep(registry, player, 2.0f, 30.0f);

  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.step_extend_sec = 0.5f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  const auto *step = skills::seven_star_shared::FindBuff(
      registry, player, BuffIdToString(BuffId::SwordStep));
  REQUIRE(step != nullptr);
  CHECK(step->remaining == doctest::Approx(2.5f));

  const std::string freeCastId(
      BuffIdToString(BuffId::BladeBoomerangFreeCast));
  CHECK(FindEffect(registry, player, freeCastId) != nullptr);
}

// 834 反例：未处于御剑步时接刃不获得免蓝
TEST_CASE("[Functional] BladeBoomerang - 834 无剑步不获得免蓝") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);

  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.step_extend_sec = 0.5f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK(skills::seven_star_shared::FindBuff(
            registry, player, BuffIdToString(BuffId::SwordStep)) == nullptr);
  const std::string freeCastId(
      BuffIdToString(BuffId::BladeBoomerangFreeCast));
  CHECK(FindEffect(registry, player, freeCastId) == nullptr);
}

// 835 回旋游步：折返击杀后接剑净化减速/定身并给予移速
TEST_CASE("[Functional] BladeBoomerang - 835 净化与移速") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(player);
  BuffEffect slow;
  slow.id = "chill_slow";
  slow.type = BuffType::SpeedDown;
  slow.duration = 5.0f;
  slow.remaining = 5.0f;
  slow.is_debuff = true;
  effects.AddOrRefresh(slow);
  BuffEffect root;
  root.id = "grasping_root";
  root.type = BuffType::Root;
  root.duration = 5.0f;
  root.remaining = 5.0f;
  root.is_debuff = true;
  effects.AddOrRefresh(root);

  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.returning_kill_occurred = true;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK_FALSE(HasEffectType(registry, player, BuffType::SpeedDown));
  CHECK_FALSE(HasEffectType(registry, player, BuffType::Root));
  const std::string swiftId(BuffIdToString(BuffId::BladeBoomerangSwift));
  const auto *swift = FindEffect(registry, player, swiftId);
  REQUIRE(swift != nullptr);
  CHECK(swift->type == BuffType::SpeedUp);
}

// 815 风眼：接剑把本次飞行累计流血伤害按比例转化为治疗
TEST_CASE("[Functional] BladeBoomerang - 815 治疗") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry, {0.0f, 0.0f}, 100.0f, 100.0f, 500.0f,
                             1000.0f);
  auto blade = CreateBlade(registry, player, {20.0f, 0.0f}, {-10.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.heal_bleed_pct = 0.5f;
  bc.bleed_damage_pool = 200.0f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK(registry.get<HealthComponent>(player).current == doctest::Approx(600.0f));
}

// 850/851 折返路径牵引：轻/中型敌人被拉向施法者，重型豁免，且不只在顶点生效
TEST_CASE("[Functional] BladeBoomerang - 850/851 折返路径牵引") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  auto light = CreateEnemy(registry, {60.0f, 0.0f});
  auto heavy = CreateEnemy(registry, {60.0f, 20.0f}, 1000.0f, /*boss=*/true);

  // 飞剑位于折返路径中段（远离施法者，不会立即接刃）
  auto blade = CreateBlade(registry, player, {100.0f, 0.0f}, {0.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  bc.pull_radius = 200.0f;
  bc.pull_radius_mult = 1.5f; // 有效半径 300
  bc.pull_strength = 300.0f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.1f);

  // 轻型被拉向施法者（-x 方向）；重型速度不变
  CHECK(registry.get<Velocity>(light).vx < 0.0f);
  CHECK(registry.get<Velocity>(light).vy == doctest::Approx(0.0f));
  CHECK(registry.get<Velocity>(heavy).vx == doctest::Approx(0.0f));
  CHECK(registry.get<Velocity>(heavy).vy == doctest::Approx(0.0f));
}

// 870/872 元素路径：带元素标签的飞行轨迹生成路径残留
TEST_CASE("[Functional] BladeBoomerang - 870/872 元素路径生成") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  element_path::ClearForTests(registry);
  auto player = CreatePlayer(registry);

  auto blade = CreateBlade(registry, player, {50.0f, 0.0f}, {100.0f, 0.0f}, 40.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.max_distance = 1000.0f;
  bc.element_path_tag = Tag::Fire;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.05f);

  // 线段 start=(45,0) end=(50,0)，半宽 20
  CHECK(element_path::IsInside(registry, player, Tag::Fire, {47.5f, 0.0f}));
  CHECK_FALSE(element_path::IsInside(registry, player, Tag::Fire, {200.0f, 0.0f}));

  // 闪电路径独立（元素路径组件按施法者单元素存储，用独立施法者验证 872）
  auto caster2 = CreatePlayer(registry);
  auto lightBlade =
      CreateBlade(registry, caster2, {50.0f, 40.0f}, {100.0f, 0.0f}, 40.0f);
  auto &lightBc = registry.get<BoomerangComponent>(lightBlade);
  lightBc.max_distance = 1000.0f;
  lightBc.element_path_tag = Tag::Lightning;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.05f);

  CHECK(element_path::IsInside(registry, caster2, Tag::Lightning,
                               {47.5f, 40.0f}));
}

// 非法 returnTarget 统一销毁，避免无限飞行
TEST_CASE("[Functional] BladeBoomerang - 非法 returnTarget 销毁") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  auto blade = CreateBlade(registry, player, {100.0f, 0.0f}, {0.0f, 0.0f}, 12.0f);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;

  const entt::entity dead = registry.create();
  registry.destroy(dead);
  bc.returnTarget = dead;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK_FALSE(registry.valid(blade));
}

// 单状态机：一趟 Update 只衰减一次 returnTimer，杜绝双实现重复推进
TEST_CASE("[Functional] BladeBoomerang - 单阈值无重复衰减") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);

  auto blade =
      CreateBlade(registry, player, {100.0f, 0.0f}, {0.0f, 0.0f}, 12.0f,
                  kRendingWaveSkillId);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.returnTimer = 1.0f;
  bc.hover_duration = 0.0f;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.4f);

  // 若存在两处衰减，此处会得到 0.2
  CHECK(bc.returnTimer == doctest::Approx(0.6f));
  CHECK(bc.phase == BoomerangPhase::Outward);
}

// 折返击杀标记：折返阶段命中已击杀目标时置位，供 835 判定
TEST_CASE("[Functional] BladeBoomerang - 折返击杀标记") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  auto victim = CreateEnemy(registry, {80.0f, 0.0f});
  registry.emplace<KilledTag>(victim, player);

  auto blade = CreateBlade(registry, player, {200.0f, 0.0f}, {0.0f, 0.0f}, 12.0f);
  auto &proj = registry.get<Projectile>(blade);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::Returning;
  proj.hit_cache[0] = victim;
  proj.hit_count = 1;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.016f);

  CHECK(bc.returning_kill_occurred);
}

// 技能2 回归：顶点悬停结束仍按 stun_on_apex_end 施加眩晕
TEST_CASE("[Functional] BladeBoomerang - 技能2 顶点眩晕回归") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreatePlayer(registry);
  auto enemy = CreateEnemy(registry, {40.0f, 0.0f});

  auto blade =
      CreateBlade(registry, player, {60.0f, 0.0f}, {0.0f, 0.0f}, 12.0f,
                  kRendingWaveSkillId);
  auto &bc = registry.get<BoomerangComponent>(blade);
  bc.phase = BoomerangPhase::HoverApex;
  bc.hover_timer = 0.05f;
  bc.hover_duration = 0.05f;
  bc.pull_radius = 100.0f;
  bc.pull_strength = 0.0f;
  bc.stun_on_apex_end = true;

  RebuildGrid(grid, registry);
  BoomerangDeliverySystem::Update(registry, grid, 0.1f);

  CHECK(HasEffectType(registry, enemy, BuffType::Stun));
  CHECK(bc.phase == BoomerangPhase::Returning);
}

} // namespace NoMoreDay
