#include "TestCommon.hpp"

#include <cmath>
#include <unordered_map>

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {

namespace skills {
// 552 环形落点采样在 BeamChannelDeliverySystem.cpp 中实现；此处声明以在回归测试中
// 直接断言其「严格环形」几何语义（替代旧圆盘随机落点）。
Vector2 SampleFollowingShadowRingPoint(Vector2 center, float radius, float angle_deg);
} // namespace skills

namespace {

constexpr uint32_t kSkillId = 5;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // ModifierRuntimeRegistry 为进程级单例，前置用例可能注入合成 blob；依赖真实
  // 生成数据的烘焙断言前强制重载，避免执行顺序造成跨用例污染。
  REQUIRE(ReloadModifierRuntimeFromAsset());
  SkillBehaviorRegistry::Initialize();
}

entt::entity CreateTestPlayer(entt::registry &registry,
                              const std::unordered_map<uint32_t, int> &allocated_nodes = {}) {
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
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

entt::entity CreateTestEnemy(entt::registry &registry, float x = 50.0f, float y = 0.0f) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<HealthComponent>(enemy, 5000.0f, 5000.0f);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = 5000.0f;
  stats.health = 5000.0f;
  stats.armor = 0.0f;
  registry.emplace<EnemyTag>(enemy);
  return enemy;
}

void CastInfiniteBlades(entt::registry &registry, entt::entity player, Vector2 target_pos = {50.0f, 0.0f}) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target_pos;
  static uint64_t s_cast_id = 1;
  exec.cast_id = ++s_cast_id;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
}

bool HasStun(entt::registry &registry, entt::entity entity) {
  const auto *fx = registry.try_get<ActiveEffectsComponent>(entity);
  if (!fx) return false;
  for (const auto &effect : fx->effects) {
    if (effect.type == BuffType::Stun) return true;
  }
  return false;
}

} // namespace

TEST_CASE("[Functional] Skill 5 - Shockwave 535 Stun Excludes Boss (N12 C3.1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{534, 1}, {535, 3}});
  auto normalEnemy = CreateTestEnemy(registry, 50.0f, 0.0f);
  auto boss = CreateTestEnemy(registry, 55.0f, 0.0f);
  registry.emplace<BossBattleComponent>(boss);
  grid.rebuild(registry.view<Position>(), registry);

  CastInfiniteBlades(registry, player, {50.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->current_channel_time = beam->max_channel_time; // 自然结束，触发 534 终结技

  BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

  // 设计（535 余波）明文仅击晕「普通敌人」，Boss 免疫击晕
  CHECK(HasStun(registry, normalEnemy));
  CHECK_FALSE(HasStun(registry, boss));

  // 收窄仅针对击晕控制，Boss 仍正常结算天剑范围伤害
  const auto *bossHp = registry.try_get<HealthComponent>(boss);
  REQUIRE(bossHp != nullptr);
  CHECK(bossHp->current < 5000.0f);
}

TEST_CASE("[Functional] Skill 5 - Sword God 534 Requires Natural Channel Completion (N12 C3.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{534, 1}});
  auto enemy = CreateTestEnemy(registry, 50.0f, 0.0f);
  grid.rebuild(registry.view<Position>(), registry);

  CastInfiniteBlades(registry, player, {50.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->current_channel_time = 2.5f; // 引导已满足 >= 2s，但尚未到上限
  beam->tick_timer = 0.0f;

  auto *stats = registry.try_get<CombatStats>(player);
  REQUIRE(stats != nullptr);
  stats->mana = 1.0f; // 低于单 tick 法耗，模拟蓝尽中断

  BeamChannelDeliverySystem::Update(registry, grid, 0.05f);

  // 蓝尽属中断：引导终止但不触发 534「结束一段引导」的终结技
  CHECK(registry.try_get<BeamChannelComponent>(player) == nullptr);
  const auto *enemyHp = registry.try_get<HealthComponent>(enemy);
  REQUIRE(enemyHp != nullptr);
  CHECK(enemyHp->current == 5000.0f);
}

TEST_CASE("[Functional] Skill 5 - Fate Mark 512 Splash Ignores SecondaryHit Recursion (N12 C3.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{512, 5}});
  auto enemy = CreateTestEnemy(registry, 50.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // 携带 SecondaryHit 的衍生命中（如满层命印溅射）不得挂载命印，切断递归扩散链
  hitFunc(registry, player, enemy, Tag::Physical | Tag::SecondaryHit, false);
  CHECK(registry.try_get<ActiveEffectsComponent>(enemy) == nullptr);

  // 真实命中：5 点满额几率为 100%，命印正常挂载
  hitFunc(registry, player, enemy, Tag::Physical, false);
  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);
  const auto *mark = fx->GetByKind(BuffKind::FateMark);
  REQUIRE(mark != nullptr);
  CHECK(mark->stacks == 1);
}

TEST_CASE("[Functional] Skill 5 - Following Shadow 552 Ring Landing (N12 C3.2)") {
  // 552 落点原为随机圆盘 (r∈[10,150])，收窄为半径 circle_radius 的严格圆环。
  // 落点半径无法从弹道方向反推，故直接断言交付层采样函数的环形几何语义。
  constexpr float kRadius = 150.0f;
  const Vector2 center{100.0f, 200.0f};
  for (float angle : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f, 359.0f}) {
    const Vector2 point = skills::SampleFollowingShadowRingPoint(center, kRadius, angle);
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    CHECK(std::sqrt(dx * dx + dy * dy) == doctest::Approx(kRadius));
  }
}

TEST_CASE("[Functional] Skill 5 - Base Delivery Uses Only BarrageEmitter (B2.3 / B3.3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  // 形态契约：BeamChannelMode 仅「连续激光」与「弹幕发射」两种，无天降/陨星残留值
  CHECK(static_cast<int>(BeamChannelMode::ContinuousLaser) == 0);
  CHECK(static_cast<int>(BeamChannelMode::BarrageEmitter) == 1);

  entt::registry registry;
  // 叠加影响落点/数量/范围的专精节点，验证基底交付形态仍唯一
  auto player = CreateTestPlayer(registry, {{515, 1}, {533, 1}, {552, 1}, {570, 1}});
  CastInfiniteBlades(registry, player, {50.0f, 0.0f});

  const auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  CHECK(beam->mode == BeamChannelMode::BarrageEmitter);
}

} // namespace NoMoreDay
