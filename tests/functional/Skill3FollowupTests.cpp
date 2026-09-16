// 灵剑决 (Skill 3) 后续实现行为测试：限时召唤、紫电紫雷静电场、灵剑充能、
// 弱点锁定溅射、剑阵共鸣攻速。所有机制数值必须从 skill_mechanics.json 读取。
#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SummonAISystem.hpp"
#include "game/systems/skill/SummonLifecycleSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 3;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // 331 等节点的专精数值已迁入 UMR canonical 记录，烘焙结果依赖真实运行时数据。
  // 该 registry 是进程级单例，会被其它用例注入的合成 blob 覆盖，故显式重载以消除顺序依赖。
  REQUIRE(ReloadModifierRuntimeFromAsset());
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

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
  active.baked_profiles[0] = profile;

  return player;
}

void CastBladeFormation(entt::registry &registry, entt::entity player,
                        Vector2 target = {100.0f, 100.0f}) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target;

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

entt::entity CreateEnemy(entt::registry &registry, float x, float y, float hp) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, x, y);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.health = hp;
  stats.max_health = hp;
  registry.emplace<HealthComponent>(enemy, hp, hp);
  return enemy;
}

} // namespace

// B2.1 / D4：灵剑为限时召唤，重复施放刷新时长而非叠加数量。
TEST_CASE("[Functional] Skill 3 - Spirit Sword Limited Lifetime & Recast Refresh (B2.1/D4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry);
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(swords.size() == 3);
  // 默认时长 8s（mechanics 缺失 duration 键时的回退值）
  for (auto s : swords) {
    const auto &summon = registry.get<SummonComponent>(s);
    CHECK(summon.max_lifetime == doctest::Approx(8.0f));
    CHECK(summon.lifetime == doctest::Approx(8.0f));
  }

  // 4 秒后仍存活且时长递减
  systems::SummonLifecycleSystem::Update(registry, 4.0f);
  CHECK(GetSwordsForOwner(registry, player).size() == 3);
  CHECK(registry.get<SummonComponent>(swords[0]).lifetime == doctest::Approx(4.0f));

  // 重复施放：数量不叠加，存活时长刷新至满额
  CastBladeFormation(registry, player);
  CHECK(GetSwordsForOwner(registry, player).size() == 3);
  CHECK(registry.get<SummonComponent>(swords[0]).lifetime == doctest::Approx(8.0f));

  // 超过剩余时长后自动消散
  systems::SummonLifecycleSystem::Update(registry, 5.0f);
  CHECK(GetSwordsForOwner(registry, player).size() == 3);
  systems::SummonLifecycleSystem::Update(registry, 3.5f);
  CHECK(GetSwordsForOwner(registry, player).empty());
}

// C1.1：紫电紫雷 (372) 施放瞬移并发起攻击，命中后结算落雷并留下静电场（裁决②）；
// 落点无敌人命中时不放场，场存续期零伤害、仅周期性施加感电（裁决①）。
TEST_CASE("[Functional] Skill 3 - Node 372 Static Field DoHit Timing & Zero Damage (C1.1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{372, 1}});
  // 施法者初始不在目标点，用于验证瞬移
  {
    auto &pPos = registry.get<Position>(player);
    pPos.x = 10.0f;
    pPos.y = 10.0f;
  }

  // 半径内 (距落点 50 < static_field_radius 60) / 半径外 (距落点 180 > 60)
  auto near = CreateEnemy(registry, 150.0f, 100.0f, 5000.0f);
  auto far = CreateEnemy(registry, 280.0f, 100.0f, 5000.0f);

  CastBladeFormation(registry, player, {100.0f, 100.0f});

  // 瞬移：施法者移动到目标点
  const auto &pPos = registry.get<Position>(player);
  CHECK(pPos.x == doctest::Approx(100.0f));
  CHECK(pPos.y == doctest::Approx(100.0f));

  // 裁决②：仅施放未命中 -> 不产生静电场
  CHECK(registry.view<ShockFieldComponent>().size() == 0);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  const float nearBefore = registry.get<HealthComponent>(near).current;
  const float farBefore = registry.get<HealthComponent>(far).current;
  hitFunc(registry, player, near, Tag::Lightning, false);

  // 命中结算落雷本体伤害：命中者受伤，半径外敌人不受影响
  const float nearAfterHit = registry.get<HealthComponent>(near).current;
  CHECK(nearAfterHit < nearBefore);
  CHECK(registry.get<HealthComponent>(far).current == doctest::Approx(farBefore));

  // 命中后释放静电场：半径/持续来自 mechanics，中心为瞬移落点，damage 为 0
  int fieldCount = 0;
  auto fieldView = registry.view<ShockFieldComponent, Position>();
  for (auto fieldEnt : fieldView) {
    const auto &field = fieldView.get<ShockFieldComponent>(fieldEnt);
    if (field.owner != player) {
      continue;
    }
    ++fieldCount;
    CHECK(field.skill_id == kSkillId);
    CHECK(field.radius == doctest::Approx(60.0f));
    CHECK(field.remaining == doctest::Approx(2.0f));
    CHECK(field.center.x == doctest::Approx(100.0f));
    CHECK(field.center.y == doctest::Approx(100.0f));
    CHECK(field.damage == doctest::Approx(0.0f)); // 裁决①：静电场零伤害
  }
  CHECK(fieldCount == 1);

  // 存续期：场周期性施加零强度感电，目标 HP 不再下降（无 DoT 追加）
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  for (int i = 0; i < 4; ++i) {
    grid.rebuild(registry.view<Position>(), registry);
    BeamChannelDeliverySystem::Update(registry, grid, 0.5f);
  }
  CHECK(registry.get<HealthComponent>(near).current == doctest::Approx(nearAfterHit));
  CHECK(registry.get<HealthComponent>(far).current == doctest::Approx(farBefore));

  // 落点约束：目标点超出索敌半径 (默认 200) 时，瞬移落点收敛到半径边界（防越界）
  entt::registry registry2;
  auto player2 = CreateTestPlayer(registry2, {{372, 1}});
  {
    auto &p2 = registry2.get<Position>(player2);
    p2.x = 0.0f;
    p2.y = 0.0f;
  }
  CastBladeFormation(registry2, player2, {1000.0f, 0.0f});
  const auto &p2 = registry2.get<Position>(player2);
  CHECK(p2.x == doctest::Approx(200.0f));
  CHECK(p2.y == doctest::Approx(0.0f));
}

// C1.2：灵剑充能 (375) 阈值与引爆半径来自 mechanics (hits_required_base /
// hits_reduction_per_point / detonate_radius)。
TEST_CASE("[Functional] Skill 3 - Node 375 Data-Driven Threshold & Detonate Radius (C1.2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{370, 1}, {375, 3}});
  CastBladeFormation(registry, player);

  auto victim = CreateEnemy(registry, 120.0f, 100.0f, 5000.0f);
  auto near = CreateEnemy(registry, 150.0f, 100.0f, 5000.0f); // 距主目标 30 < 80
  auto far = CreateEnemy(registry, 220.0f, 100.0f, 5000.0f);  // 距主目标 100 > 80

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  auto *formation = registry.try_get<BladeFormationComponent>(player);
  REQUIRE(formation != nullptr);

  // hits_required_base=5、hits_reduction_per_point=1、pts375=3 -> 阈值 2
  // 辅助判定目标身上是否仍有火焰异常（点燃），用于验证引爆消费
  auto hasIgnite = [&](entt::entity e) {
    const auto *fx = registry.try_get<ActiveEffectsComponent>(e);
    if (fx == nullptr) {
      return false;
    }
    for (const auto &b : fx->effects) {
      if (b.type == BuffType::Burn || b.kind == BuffKind::Ignite) {
        return true;
      }
    }
    return false;
  };

  hitFunc(registry, player, victim, Tag::Fire, false);
  CHECK(formation->charge_attack_counter == 1);
  CHECK(registry.get<HealthComponent>(near).current == doctest::Approx(5000.0f));
  REQUIRE(hasIgnite(victim)); // 命中1附着点燃，供命中2引爆消费

  hitFunc(registry, player, victim, Tag::Fire, false);
  CHECK(formation->charge_attack_counter == 0);
  CHECK(registry.get<HealthComponent>(victim).current < 5000.0f);
  CHECK(registry.get<HealthComponent>(near).current < 5000.0f);
  CHECK(registry.get<HealthComponent>(far).current == doctest::Approx(5000.0f));
  // 裁决③：引爆后目标身上的对应元素异常已被消费，不应残留
  CHECK_FALSE(hasIgnite(victim));

  // hits_reduction_per_point：pts375=1 时阈值回升为 4 次
  entt::registry registry2;
  auto player2 = CreateTestPlayer(registry2, {{370, 1}, {375, 1}});
  CastBladeFormation(registry2, player2);
  auto enemy2 = CreateEnemy(registry2, 120.0f, 100.0f, 5000.0f);
  auto *formation2 = registry2.try_get<BladeFormationComponent>(player2);
  REQUIRE(formation2 != nullptr);
  for (int i = 0; i < 3; ++i) {
    hitFunc(registry2, player2, enemy2, Tag::Fire, false);
  }
  CHECK(formation2->charge_attack_counter == 3); // 尚未达到阈值 4
  hitFunc(registry2, player2, enemy2, Tag::Fire, false);
  CHECK(formation2->charge_attack_counter == 0);
}

// C1.3：弱点锁定 (331) 巨剑暴击时按 splash_radius 对周围造成范围伤害。
TEST_CASE("[Functional] Skill 3 - Node 331 Crit Splash Radius Consumed (C1.3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{330, 1}, {331, 5}});
  CastBladeFormation(registry, player);

  auto victim = CreateEnemy(registry, 120.0f, 100.0f, 5000.0f);
  auto near = CreateEnemy(registry, 150.0f, 100.0f, 5000.0f); // 30 < 60
  auto far = CreateEnemy(registry, 220.0f, 100.0f, 5000.0f);  // 100 > 60

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // 非暴击不触发溅射
  hitFunc(registry, player, victim, Tag::Physical, false);
  CHECK(registry.get<HealthComponent>(near).current == doctest::Approx(5000.0f));

  // 暴击触发溅射：半径内受伤，半径外不受影响
  hitFunc(registry, player, victim, Tag::Physical, true);
  CHECK(registry.get<HealthComponent>(near).current < 5000.0f);
  CHECK(registry.get<HealthComponent>(far).current == doctest::Approx(5000.0f));

  // 负例 (M-1)：仅点亮 331 未点亮巨剑 (330) 时，暴击不产生溅射（GDD L257 巨剑限定）
  entt::registry registry2;
  auto player2 = CreateTestPlayer(registry2, {{331, 5}});
  CastBladeFormation(registry2, player2);

  const auto *formation2 = registry2.try_get<BladeFormationComponent>(player2);
  REQUIRE(formation2 != nullptr);
  REQUIRE_FALSE(formation2->has_giant_sword);

  auto victim2 = CreateEnemy(registry2, 120.0f, 100.0f, 5000.0f);
  auto near2 = CreateEnemy(registry2, 150.0f, 100.0f, 5000.0f); // 30 < 60
  hitFunc(registry2, player2, victim2, Tag::Physical, true);
  CHECK(registry2.get<HealthComponent>(near2).current == doctest::Approx(5000.0f));
}

// C1.4：剑阵共鸣 (355) 攻速加成来自 array_haste_pct 而非硬编码 1.50f。
TEST_CASE("[Functional] Skill 3 - Node 355 Array Haste Percent Consumed (C1.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{355, 1}});
  CastBladeFormation(registry, player);

  auto swords = GetSwordsForOwner(registry, player);
  REQUIRE(!swords.empty());
  auto &ai = registry.get<SpiritSwordAI>(swords[0]);
  ai.attack_timer = 2.0f;

  auto arrEnt = registry.create();
  registry.emplace<Position>(arrEnt, 100.0f, 100.0f);
  auto &arr = registry.emplace<SwordArrayComponent>(arrEnt);
  arr.owner = player;
  arr.radius = 150.0f;

  systems::SpatialHashGrid grid(100, 100, 50);
  // array_haste_pct=50 -> effectiveDt = 1.5x，2.0 - 1.5 = 0.5
  systems::SummonAISystem::Update(registry, 1.0f, grid);

  CHECK(ai.attack_timer == doctest::Approx(0.5f));
}

} // namespace NoMoreDay
