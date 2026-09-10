#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/HazardComponents.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/VisualFXSystem.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SummonAISystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SwordArray.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 6;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  systems::AilmentRegistry::Get().EnsureLoaded();
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

entt::entity CreateTestPlayer(entt::registry &registry,
                             const std::unordered_map<uint32_t, int> &allocated_nodes = {},
                             Vector2 pos = {0.0f, 0.0f}) {
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, pos.x, pos.y);
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 500.0f;
  stats.mana = 500.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 10.0f;
  stats.crit_damage = 1.5f;
  stats.effective_intelligence = 100.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].current_charges = 5;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  return player;
}

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y, float max_hp = 1000.0f, bool is_boss = false) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<HealthComponent>(enemy, max_hp, max_hp);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = max_hp;
  stats.health = max_hp;
  stats.cached_area_level = 1;
  if (is_boss) {
    registry.emplace<BossBattleComponent>(enemy);
  }
  return enemy;
}

void CastSwordArray(entt::registry &registry, entt::entity player, Vector2 target_pos) {
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

} // namespace

TEST_CASE("[Functional] Skill 6 - Base Cast & Array Limit Management") {
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});

  // 验证 TryCast 契约与前置通过
  CHECK(SkillSystem::TryCast(registry, player, 0, {50.0f, 0.0f}));
  registry.remove<SkillExecution>(player);

  // 1. 施放基础剑阵
  CastSwordArray(registry, player, {50.0f, 0.0f});

  auto view = registry.view<SwordArrayComponent, AreaFieldComponent, Position>();
  REQUIRE(view.begin() != view.end());
  const auto arrayEnt = *view.begin();
  const auto &arr = view.get<SwordArrayComponent>(arrayEnt);
  const auto &field = view.get<AreaFieldComponent>(arrayEnt);

  CHECK(arr.duration == doctest::Approx(5.0f));
  CHECK(arr.total_duration == doctest::Approx(5.0f));
  CHECK(arr.radius == doctest::Approx(150.0f));
  CHECK(arr.damage_interval == doctest::Approx(0.5f));
  CHECK(field.remaining_duration == doctest::Approx(5.0f));
  CHECK(field.pulse_interval == doctest::Approx(0.5f));
  CHECK(arr.max_arrays == 1);

  // 2. 默认单阵上限：再次施放会销毁原先的剑阵实体
  CastSwordArray(registry, player, {100.0f, 0.0f});

  int array_count = 0;
  entt::entity newArrayEnt = entt::null;
  for (auto e : registry.view<SwordArrayComponent>()) {
    ++array_count;
    newArrayEnt = e;
  }
  CHECK(array_count == 1);
  CHECK(registry.valid(newArrayEnt));
  CHECK(newArrayEnt != arrayEnt);
}

TEST_CASE("[Functional] Skill 6 - Multi-Array Nodes 610, 611 & Relocate 675") {
  EnsureSkillMechanics();

  SUBCASE("610 Twin Arrays allows 2 arrays") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{610, 1}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    CastSwordArray(registry, player, {100.0f, 0.0f});

    int count = 0;
    for (auto e : registry.view<SwordArrayComponent>()) {
      (void)e;
      ++count;
    }
    CHECK(count == 2);

    // 施放第 3 个会顶替最早的阵
    CastSwordArray(registry, player, {200.0f, 0.0f});
    count = 0;
    for (auto e : registry.view<SwordArrayComponent>()) {
      (void)e;
      ++count;
    }
    CHECK(count == 2);
  }

  SUBCASE("611 Tri Formation allows 3 arrays") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{610, 1}, {611, 1}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    CastSwordArray(registry, player, {100.0f, 0.0f});
    CastSwordArray(registry, player, {200.0f, 0.0f});

    int count = 0;
    for (auto e : registry.view<SwordArrayComponent>()) {
      (void)e;
      ++count;
    }
    CHECK(count == 3);
  }

  SUBCASE("675 Relocate via SkillSystem::TryCast with 0 charges and 50% mana cost") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{670, 1}, {675, 1}});
    SkillSystem::RebakeSkillProfiles(registry, player);

    // 第一次施放剑阵
    CHECK(SkillSystem::TryCast(registry, player, 0, {0.0f, 0.0f}));
    registry.remove<SkillExecution>(player);
    CastSwordArray(registry, player, {0.0f, 0.0f});

    auto view = registry.view<SwordArrayComponent, Position>();
    REQUIRE(view.begin() != view.end());
    const auto initialEnt = *view.begin();

    // 模拟充能耗尽 (0 charges) 且冷却中
    auto &active = registry.get<ActiveSkillsComponent>(player);
    active.slots[0].current_charges = 0;
    active.slots[0].cooldown = 10.0f;
    auto &stats = registry.get<CombatStats>(player);
    const float manaBefore = stats.mana;

    // 此时 TryCast 移形换阵依然允许施放
    CHECK(SkillSystem::TryCast(registry, player, 0, {300.0f, 0.0f}));
    registry.remove<SkillExecution>(player);

    // 验证消耗 50% 法耗，不扣除充能，不增加冷却
    CHECK(stats.mana == doctest::Approx(manaBefore - 15.0f));
    CHECK(active.slots[0].current_charges == 0);
    CHECK(active.slots[0].cooldown == 10.0f);

    // 再次施放到新位置
    CastSwordArray(registry, player, {300.0f, 0.0f});

    int count = 0;
    for (auto e : registry.view<SwordArrayComponent>()) {
      (void)e;
      ++count;
    }
    CHECK(count == 1);
    CHECK(registry.valid(initialEnt));
    const auto &newPos = registry.get<Position>(initialEnt);
    CHECK(newPos.x == doctest::Approx(300.0f));
  }
}

TEST_CASE("[Functional] Skill 6 - Shape & Synergy Nodes 600, 601, 603, 612, 613, 614, 615") {
  EnsureSkillMechanics();

  SUBCASE("600 Duration & 601 Radius scaling") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{600, 4}, {601, 4}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent>();
    REQUIRE(view.begin() != view.end());
    const auto &arr = view.get<SwordArrayComponent>(*view.begin());

    // 5.0s + 4 * 0.5s = 7.0s
    CHECK(arr.duration == doctest::Approx(7.0f));
    CHECK(arr.total_duration == doctest::Approx(7.0f));
    // 150.0f * (1 + 4 * 0.15f) = 150 * 1.6 = 240.0f
    CHECK(arr.radius == doctest::Approx(240.0f));
  }

  SUBCASE("603 Mana Cost Reduction") {
    entt::registry registry;
    BakedSkillProfile profile;
    SpecializedSkill spec;
    spec.skill_id = kSkillId;
    spec.allocated_points = {{603, 4}};

    auto player = CreateTestPlayer(registry, {{603, 4}});
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);

    // 基础 30 法耗，-20% = 24
    CHECK(profile.effective_mana_cost == doctest::Approx(24.0f));
  }

  SUBCASE("612 Resonance amplifies damage in overlapping sword arrays") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{610, 1}, {612, 3}}); // +60% resonance more

    // 施放两个剑阵：阵 1 在 (0, 0)，阵 2 在 (100, 0)，半径 150
    CastSwordArray(registry, player, {0.0f, 0.0f});
    CastSwordArray(registry, player, {100.0f, 0.0f});

    // 敌方 A 在 (50, 0) (处于双阵重叠区)
    auto enemyA = CreateTestEnemy(registry, 50.0f, 0.0f, 1000.0f);
    // 敌方 B 在 (-100, 0) (仅处于阵 1)
    auto enemyB = CreateTestEnemy(registry, -100.0f, 0.0f, 1000.0f);

    grid.rebuild(registry.view<Position>(), registry);
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    const float dmgA = 1000.0f - registry.get<HealthComponent>(enemyA).current;
    const float dmgB = 1000.0f - registry.get<HealthComponent>(enemyB).current;

    CHECK(dmgA > 0.0f);
    CHECK(dmgB > 0.0f);
    // 敌方 A 处于双阵重叠区，受到两次共鸣脉冲 (每次 1.6x，共 3.2x)，相比单阵 B 为 3.2 倍
    CHECK(dmgA / dmgB == doctest::Approx(3.2f).epsilon(0.05f));
  }

  SUBCASE("613 Thousand Threads line segment damages and slows enemies between arrays") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{610, 1}, {613, 1}});

    // 阵 1 在 (0, 0)，阵 2 在 (200, 0)
    CastSwordArray(registry, player, {0.0f, 0.0f});
    CastSwordArray(registry, player, {200.0f, 0.0f});

    // 缩小半径为 50，以确保 (100, 0) 处不在任何阵内，仅在两阵连线上
    for (auto e : registry.view<SwordArrayComponent>()) {
      registry.get<SwordArrayComponent>(e).radius = 50.0f;
    }

    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);

    // 运行 SwordArray::Update
    for (auto e : registry.view<SwordArrayComponent>()) {
      skills::SwordArray::Update(registry, e, registry.get<SwordArrayComponent>(e), 0.2f, grid);
    }

    // 敌人受到连线切割伤害并减速 50%
    CHECK(registry.get<HealthComponent>(enemy).current < 1000.0f);
    auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(fx != nullptr);
    bool foundSlow = false;
    for (const auto &b : fx->effects) {
      if (b.id == "SwordArrayConnectionSlow") {
        foundSlow = true;
        break;
      }
    }
    CHECK(foundSlow);
  }

  SUBCASE("614 Cloud Piercing detonates array when owner dashes through") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{614, 1}}, {0.0f, 0.0f});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEnt = *view.begin();

    // 阵内放置一名敌人
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);

    // 赋予玩家御剑步 buff
    auto &pEffects = registry.emplace<ActiveEffectsComponent>(player);
    BuffEffect stepBuff;
    stepBuff.id = std::string(BuffIdToString(BuffId::SwordStep));
    stepBuff.duration = 2.0f;
    stepBuff.remaining = 2.0f;
    pEffects.AddOrRefresh(stepBuff);

    // 更新 SwordArray
    skills::SwordArray::Update(registry, arrayEnt, registry.get<SwordArrayComponent>(arrayEnt), 0.1f, grid);

    // 剑阵引爆并销毁，敌人受到引爆伤害
    CHECK_FALSE(registry.valid(arrayEnt));
    CHECK(registry.get<HealthComponent>(enemy).current < 1000.0f);
  }

  SUBCASE("615 Array Step accelerates pulse frequency when in Sword Riding stance") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{615, 3}}, {0.0f, 0.0f}); // +60% frequency
    registry.emplace<MovementStanceComponent>(player).stance = MovementStance::SwordRiding;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<AreaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    auto &field = view.get<AreaFieldComponent>(*view.begin());

    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);

    // 基础 pulse_interval 是 0.5s。给定 dt = 0.35s：
    // 若无 615 加速: 0.35s < 0.5s 不会触发脉冲 (timer 变为 0.35s, enemy 无伤)
    // 若有 615 加速: 0.35s * 1.6 = 0.56s >= 0.5s，立刻触发脉冲并重置 timer 为 0！
    AreaFieldDeliverySystem::Update(registry, grid, 0.35f);

    CHECK(field.timer == doctest::Approx(0.0f));
    CHECK(registry.get<HealthComponent>(enemy).current < 1000.0f);
  }
}

TEST_CASE("[Functional] Skill 6 - Control Nodes 630 Slow, 631 Armor Shred, 632 Weaken") {
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {{630, 4}, {631, 2}, {632, 3}});
  auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);

  CastSwordArray(registry, player, {0.0f, 0.0f});
  grid.rebuild(registry.view<Position>(), registry);

  // 脉冲 1 (0.5s 触发一次)
  AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

  auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);

  // 1. 验证 630 缓速
  bool foundSlow = false;
  for (const auto &b : fx->effects) {
    if (b.type == BuffType::SpeedDown || b.managed_ailment || b.id.find("Slow") != std::string::npos || b.id.find("slow") != std::string::npos) {
      foundSlow = true;
      break;
    }
  }
  CHECK(foundSlow);

  // 630 数值断言: 4 点 -> slow_magnitude = 0.10 * 4 = 0.40
  // 施加时 mod.value = -slow_magnitude * 100.0f = -40.0 (PercentAdd 百分数语义)
  auto *slow = fx->Get(BuffId::SwordArraySlow);
  REQUIRE(slow != nullptr);
  CHECK(slow->type == BuffType::SpeedDown);
  CHECK(slow->modifiers.size() == 1);
  CHECK(slow->modifiers[0].type == StatType::MoveSpeed);
  CHECK(slow->modifiers[0].mode == ModifierMode::PercentAdd);
  CHECK(slow->modifiers[0].value == doctest::Approx(-40.0f));

  // 2. 验证 631 破甲剑意 (BuffId::SwordArrayArmorShred) 线性堆叠与数值缩放
  auto *shred = fx->Get(BuffId::SwordArrayArmorShred);
  REQUIRE(shred != nullptr);
  CHECK(shred->stacks == 1);
  CHECK(shred->modifiers[0].value == doctest::Approx(-10.0f));

  // 脉冲 2: 层数增加至 2，护甲减少 -20
  AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
  CHECK(shred->stacks == 2);
  CHECK(shred->modifiers[0].value == doctest::Approx(-20.0f));

  // 脉冲 3: 层数增加至 3，护甲减少 -30
  AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
  CHECK(shred->stacks == 3);
  CHECK(shred->modifiers[0].value == doctest::Approx(-30.0f));

  // 3. 验证 632 虚弱领域 (全属性伤害 Less 18%)
  auto *weaken = fx->Get("SwordArrayWeaken");
  REQUIRE(weaken != nullptr);
  CHECK(weaken->modifiers.size() == 6); // 覆盖全部 6 种伤害类型
  CHECK(weaken->modifiers[0].value == doctest::Approx(-18.0f));
}

TEST_CASE("[Functional] Skill 6 - Execution Field 633 & Trigger Echo 635") {
  EnsureSkillMechanics();

  SUBCASE("633 executes non-boss enemy under 12% max HP") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{633, 1}});

    // 创建生命值低于 12% (100 / 1000 = 10%) 的普通敌人
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f, false);
    registry.get<HealthComponent>(enemy).current = 100.0f;
    registry.get<CombatStats>(enemy).health = 100.0f;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    // 触发脉冲
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    // 敌人应当被斩杀
    CHECK(registry.any_of<KilledTag>(enemy));
    CHECK(registry.get<HealthComponent>(enemy).current <= 0.0f);
  }

  SUBCASE("633 does not execute Boss enemy") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{633, 1}});

    // 创建生命值低于 12% 的 Boss
    auto boss = CreateTestEnemy(registry, 20.0f, 0.0f, 10000.0f, true);
    registry.get<HealthComponent>(boss).current = 500.0f;
    registry.get<CombatStats>(boss).health = 500.0f;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    // Boss 不应当直接被斩杀
    CHECK_FALSE(registry.any_of<KilledTag>(boss));
  }

  SUBCASE("635 Trigger Echo casts Skill 2 on execution") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{633, 1}, {635, 1}});

    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f, false);
    registry.get<HealthComponent>(enemy).current = 100.0f;
    registry.get<CombatStats>(enemy).health = 100.0f;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    const auto beforeExecs = registry.storage<SkillExecution>().size();
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
    const auto afterExecs = registry.storage<SkillExecution>().size();

    // 斩杀触发了 635 裂空斩 (Skill 2)
    CHECK(afterExecs > beforeExecs);
    bool foundSkill2 = false;
    for (auto execEnt : registry.view<SkillExecution>()) {
      if (registry.get<SkillExecution>(execEnt).skill_id == 2) {
        foundSkill2 = true;
        break;
      }
    }
    CHECK(foundSkill2);
  }

  SUBCASE("635 Trigger Echo does NOT cast Skill 2 on normal damage kill without execution") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    // 仅分配 635，未分配 633 绝命法场处决
    auto player = CreateTestPlayer(registry, {{635, 1}});

    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f, false);
    // 设置极低生命值，使其死于领域基础脉冲伤害而非处决
    registry.get<HealthComponent>(enemy).current = 5.0f;
    registry.get<CombatStats>(enemy).health = 5.0f;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    const auto beforeExecs = registry.storage<SkillExecution>().size();
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
    const auto afterExecs = registry.storage<SkillExecution>().size();

    // 未经处决击杀，绝不触发 635 裂空斩
    CHECK(afterExecs == beforeExecs);
  }

  SUBCASE("633 executes enemy when damage pushes HP below 12% threshold") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{633, 1}});

    // 创建初始血量高于 12% (130 / 1000 = 13%) 的普通敌人
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f, false);
    registry.get<HealthComponent>(enemy).current = 130.0f;
    registry.get<CombatStats>(enemy).health = 130.0f;

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    // 脉冲造成约 20 点伤害，将血量从 130 削减到 120 以下，触发二阶段斩杀
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    CHECK(registry.any_of<KilledTag>(enemy));
    // ExecutedTag 在事件派发后同步清除 (闭合标记生命周期, 不跨帧残留), 故 Update 返回后不可断言
    CHECK(registry.get<HealthComponent>(enemy).current <= 0.0f);
  }

  SUBCASE("633 Boss More Damage isolation: +20% only against Boss and only with 633") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);

    // 1. 点出 633 的玩家
    auto pWith633 = CreateTestPlayer(registry, {{633, 1}});
    CastSwordArray(registry, pWith633, {0.0f, 0.0f});

    auto normEnemy = CreateTestEnemy(registry, 20.0f, 0.0f, 10000.0f, false);
    auto bossEnemy = CreateTestEnemy(registry, -20.0f, 0.0f, 10000.0f, true);
    grid.rebuild(registry.view<Position>(), registry);

    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    const float dmgNorm = 10000.0f - registry.get<HealthComponent>(normEnemy).current;
    const float dmgBoss = 10000.0f - registry.get<HealthComponent>(bossEnemy).current;

    CHECK(dmgNorm > 0.0f);
    CHECK(dmgBoss > 0.0f);
    CHECK(dmgBoss / dmgNorm == doctest::Approx(1.20f).epsilon(0.01f));

    // 2. 未点 633 的玩家：Boss 与小怪伤害完全一致
    entt::registry reg2;
    systems::SpatialHashGrid grid2(1024, 1024, 64.0f);
    auto pNo633 = CreateTestPlayer(reg2, {});
    CastSwordArray(reg2, pNo633, {0.0f, 0.0f});

    auto normEnemy2 = CreateTestEnemy(reg2, 20.0f, 0.0f, 10000.0f, false);
    auto bossEnemy2 = CreateTestEnemy(reg2, -20.0f, 0.0f, 10000.0f, true);
    grid2.rebuild(reg2.view<Position>(), reg2);

    AreaFieldDeliverySystem::Update(reg2, grid2, 0.55f);

    const float dmgNorm2 = 10000.0f - reg2.get<HealthComponent>(normEnemy2).current;
    const float dmgBoss2 = 10000.0f - reg2.get<HealthComponent>(bossEnemy2).current;

    CHECK(dmgNorm2 > 0.0f);
    CHECK(dmgBoss2 > 0.0f);
    CHECK(dmgBoss2 / dmgNorm2 == doctest::Approx(1.0f).epsilon(0.01f));
  }
}

TEST_CASE("[Functional] Skill 6 - Cage 634 & Mobile Fortress 653") {
  EnsureSkillMechanics();

  SUBCASE("634 Cage flag and radius reduction") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{634, 1}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent>();
    REQUIRE(view.begin() != view.end());
    const auto &arr = view.get<SwordArrayComponent>(*view.begin());

    CHECK(arr.has_cage);
    // 150 * 0.70 = 105.0f
    CHECK(arr.radius == doctest::Approx(105.0f));
  }

  SUBCASE("634 Cage physically repels enemies crossing border") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{634, 1}}, {0.0f, 0.0f});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent, Position>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEnt = *view.begin();
    auto &arr = view.get<SwordArrayComponent>(arrayEnt);

    // 634 半径 105.0f。
    // 1. 外部敌人位于 115.0f (试图侵入)，被屏障推回外侧 (105 + 15 = 120.0f)
    auto enemyOutside = CreateTestEnemy(registry, 115.0f, 0.0f, 1000.0f);
    // 2. 内部敌人位于 100.0f (试图越狱)，被屏障推回内侧 (105 - 15 = 90.0f)
    auto enemyInside = CreateTestEnemy(registry, 100.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);

    skills::SwordArray::Update(registry, arrayEnt, arr, 0.1f, grid);

    CHECK(registry.get<Position>(enemyOutside).x == doctest::Approx(120.0f));
    CHECK(registry.get<Position>(enemyInside).x == doctest::Approx(90.0f));
  }

  SUBCASE("653 Mobile Fortress aura follows player") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{653, 1}}, {0.0f, 0.0f});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent, Position>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEnt = *view.begin();
    auto &arr = view.get<SwordArrayComponent>(arrayEnt);
    CHECK(arr.is_mobile_aura);

    // 移动玩家
    registry.get<Position>(player) = {120.0f, 80.0f};

    // SwordArray tick
    skills::SwordArray::Update(registry, arrayEnt, arr, 0.1f, grid);

    const auto &arrPos = registry.get<Position>(arrayEnt);
    CHECK(arrPos.x == doctest::Approx(120.0f));
    CHECK(arrPos.y == doctest::Approx(80.0f));
  }
}

TEST_CASE("[Functional] Skill 6 - Support Nodes 650, 651, 652, 654, 655") {
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {{650, 4}, {651, 4}, {652, 3}, {654, 3}, {655, 3}}, {0.0f, 0.0f});
  registry.emplace<SwordIntentComponent>(player).stacks = 0;
  registry.get<CombatStats>(player).mana = 100.0f;

  CastSwordArray(registry, player, {0.0f, 0.0f});
  auto view = registry.view<SwordArrayComponent>();
  REQUIRE(view.begin() != view.end());
  const auto arrayEnt = *view.begin();
  auto &arr = view.get<SwordArrayComponent>(arrayEnt);

  // 1 秒 tick
  skills::SwordArray::Update(registry, arrayEnt, arr, 1.0f, grid);

  // 1. 650 Core buff 伤害加成
  auto *fx = registry.try_get<ActiveEffectsComponent>(player);
  REQUIRE(fx != nullptr);
  bool foundCoreBuff = false;
  for (const auto &b : fx->effects) {
    if (b.id == "SwordArrayCore" && b.type == BuffType::AttackUp) {
      foundCoreBuff = true;
      break;
    }
  }
  CHECK(foundCoreBuff);

  // 2. 651 法力回复 (4 点 = +8/s)
  CHECK(registry.get<CombatStats>(player).mana >= 108.0f);

  // 3. 652 意念合一 (+1 剑意)
  CHECK(registry.get<SwordIntentComponent>(player).stacks >= 1);

  // 4. 654 CDR Buff
  bool foundCdrBuff = false;
  for (const auto &b : fx->effects) {
    if (b.id == "SwordArrayCDR") {
      foundCdrBuff = true;
      break;
    }
  }
  CHECK(foundCdrBuff);

  // 5. 655 护盾获取 (智力 100 * 150% = 150 Barrier)
  CHECK(registry.get<CombatStats>(player).barrier >= 150.0f);

  SUBCASE("Multiple overlapping arrays do not duplicate 1s interval owner buffs") {
    entt::registry reg;
    systems::SpatialHashGrid g(1024, 1024, 64.0f);
    auto p = CreateTestPlayer(reg, {{610, 1}, {611, 1}, {651, 4}, {652, 3}, {655, 3}}, {0.0f, 0.0f});
    reg.emplace<SwordIntentComponent>(p).stacks = 0;
    reg.get<CombatStats>(p).mana = 100.0f;
    reg.get<CombatStats>(p).barrier = 0.0f;

    // 3 个剑阵完全重叠在玩家脚下
    CastSwordArray(reg, p, {0.0f, 0.0f});
    CastSwordArray(reg, p, {0.0f, 0.0f});
    CastSwordArray(reg, p, {0.0f, 0.0f});

    for (auto e : reg.view<SwordArrayComponent>()) {
      skills::SwordArray::Update(reg, e, reg.get<SwordArrayComponent>(e), 1.0f, g);
    }

    // 即使处于 3 重剑阵内，每秒回蓝依然是 8 (非 24)，剑意是 1 (非 3)，护盾是 150 (非 450)
    CHECK(reg.get<CombatStats>(p).mana == doctest::Approx(108.0f));
    CHECK(reg.get<SwordIntentComponent>(p).stacks == 1);
    CHECK(reg.get<CombatStats>(p).barrier == doctest::Approx(150.0f));
  }
}

TEST_CASE("[Functional] Skill 6 - Transmutation 670 Fire & 672 Lightning & 674 Corrosion") {
  EnsureSkillMechanics();

  SUBCASE("670 Infernal Array: Fire Conversion, Visual Color, & 671 Burning Ailment") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{670, 1}, {671, 3}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent, SkillModifierComponent>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEnt = *view.begin();
    const auto &arr = view.get<SwordArrayComponent>(arrayEnt);
    const auto &mods = view.get<SkillModifierComponent>(arrayEnt);

    CHECK(arr.is_fire_field);
    CHECK(arr.core_color.r == 255);
    CHECK(arr.core_color.g == 80);
    CHECK(arr.core_color.b == 20);

    bool hasFireConvert = false;
    for (const auto &m : mods.damage_modifiers) {
      if (m.type == ModifierType::Convert && m.source_tag == Tag::Physical &&
          m.target_tag == Tag::Fire && m.value == doctest::Approx(1.0f)) {
        hasFireConvert = true;
        break;
      }
    }
    CHECK(hasFireConvert);

    // 验证火阵脉冲对阵内敌人施加点燃异常
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(fx != nullptr);
    bool hasIgnite = false;
    for (const auto &b : fx->effects) {
      if (b.type == BuffType::Burn || (b.managed_ailment && b.ailment_type == static_cast<uint8_t>(AilmentType::Ignite))) {
        hasIgnite = true;
        break;
      }
    }
    CHECK(hasIgnite);
  }

  SUBCASE("672 Nether Thunder Pool: Lightning Conversion & 673 Lightning Strike") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{672, 1}, {673, 2}});

    CastSwordArray(registry, player, {0.0f, 0.0f});
    auto view = registry.view<SwordArrayComponent, SkillModifierComponent>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEnt = *view.begin();
    const auto &arr = view.get<SwordArrayComponent>(arrayEnt);
    const auto &mods = view.get<SkillModifierComponent>(arrayEnt);

    CHECK(arr.is_lightning_pool);
    CHECK(arr.core_color.r == 80);
    CHECK(arr.core_color.g == 180);
    CHECK(arr.core_color.b == 255);
    // 2 base + 随机 0..1 + 2 from 673 = 4..5 targets
    CHECK(arr.lightning_targets >= 4);
    CHECK(arr.lightning_targets <= 5);

    bool hasLightningConvert = false;
    for (const auto &m : mods.damage_modifiers) {
      if (m.type == ModifierType::Convert && m.source_tag == Tag::Physical &&
          m.target_tag == Tag::Lightning && m.value == doctest::Approx(1.0f)) {
        hasLightningConvert = true;
        break;
      }
    }
    CHECK(hasLightningConvert);

    // 验证 1 秒雷击脉冲对阵内敌人造成雷电伤害
    auto enemy1 = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);
    auto enemy2 = CreateTestEnemy(registry, 40.0f, 0.0f, 1000.0f);
    grid.rebuild(registry.view<Position>(), registry);

    AreaFieldDeliverySystem::Update(registry, grid, 1.05f);
    CHECK(registry.get<HealthComponent>(enemy1).current < 1000.0f);
    CHECK(registry.get<HealthComponent>(enemy2).current < 1000.0f);
  }

  SUBCASE("674 Array Corrosion applies shred stacks and scales resist reduction with SkillOnly scope") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{670, 1}, {674, 4}});
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f);

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    // 脉冲 1: 施加 4 层 (4 点 = 4 层/s)
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(fx != nullptr);
    auto *corrosion = fx->Get("SwordArrayCorrosion");
    REQUIRE(corrosion != nullptr);
    CHECK(corrosion->source_skill_id == 6);
    CHECK(corrosion->stacks == 4);
    CHECK(corrosion->modifiers[0].value == doctest::Approx(-8.0f)); // -2.0 * 4

    // 脉冲 2: 增加至 8 层
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
    CHECK(corrosion->stacks == 8);
    CHECK(corrosion->modifiers[0].value == doctest::Approx(-16.0f));

    // 脉冲 3: 上限封顶 10 层
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);
    CHECK(corrosion->stacks == 10);
    CHECK(corrosion->modifiers[0].value == doctest::Approx(-20.0f));
  }

  SUBCASE("670 and 672 Mutual Exclusion: Baker selects single transmuter") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{670, 1}, {672, 1}});

    BakedSkillProfile profile;
    SpecializedSkill spec;
    spec.skill_id = kSkillId;
    spec.allocated_points = {{670, 1}, {672, 1}};

    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);

    // 只能保留一个转换质变器，不可同时生效
    CHECK(HasTag(profile.effective_tags, Tag::Fire));
    CHECK_FALSE(HasTag(profile.effective_tags, Tag::Lightning));
  }

  SUBCASE("672 Chain Thunder isolation: Chain lightning requires node 673") {
    entt::registry registry;
    auto playerNo673 = CreateTestPlayer(registry, {{672, 1}});
    CastSwordArray(registry, playerNo673, {0.0f, 0.0f});
    auto view1 = registry.view<SwordArrayComponent>();
    REQUIRE(view1.begin() != view1.end());
    const auto &arr1 = view1.get<SwordArrayComponent>(*view1.begin());
    CHECK(arr1.is_lightning_pool);
    CHECK_FALSE(arr1.has_chain_lightning);
    // 基线 2 + 随机 0..1 (无 673 点数加成)
    CHECK(arr1.lightning_targets >= 2);
    CHECK(arr1.lightning_targets <= 3);

    entt::registry reg2;
    auto playerWith673 = CreateTestPlayer(reg2, {{672, 1}, {673, 2}});
    CastSwordArray(reg2, playerWith673, {0.0f, 0.0f});
    auto view2 = reg2.view<SwordArrayComponent>();
    REQUIRE(view2.begin() != view2.end());
    const auto &arr2 = view2.get<SwordArrayComponent>(*view2.begin());
    CHECK(arr2.is_lightning_pool);
    CHECK(arr2.has_chain_lightning);
    // 2 base + 随机 0..1 + 2 from 673 = 4..5 targets
    CHECK(arr2.lightning_targets >= 4);
    CHECK(arr2.lightning_targets <= 5);
  }

  SUBCASE("670 Fire and 672 Lightning convert 613 Threads and 614 Dash Detonation damage tags") {
    entt::registry regFire;
    auto pFire = CreateTestPlayer(regFire, {{670, 1}});
    CastSwordArray(regFire, pFire, {0.0f, 0.0f});
    auto vFire = regFire.view<SwordArrayComponent>();
    REQUIRE(vFire.begin() != vFire.end());
    CHECK(vFire.get<SwordArrayComponent>(*vFire.begin()).effective_tag == Tag::Fire);

    entt::registry regLight;
    auto pLight = CreateTestPlayer(regLight, {{672, 1}});
    CastSwordArray(regLight, pLight, {0.0f, 0.0f});
    auto vLight = regLight.view<SwordArrayComponent>();
    REQUIRE(vLight.begin() != vLight.end());
    CHECK(vLight.get<SwordArrayComponent>(*vLight.begin()).effective_tag == Tag::Lightning);
  }
}

TEST_CASE("[Functional] Skill 6 - Cross-Skill Linkage 515, 355 & VisualFX") {
  EnsureSkillMechanics();

  SUBCASE("515 Blades to Array extends both SwordArray and AreaField duration") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);
    auto &stats = registry.emplace<CombatStats>(player);
    stats.max_health = 1000.0f;
    stats.health = 1000.0f;
    stats.max_mana = 500.0f;
    stats.mana = 500.0f;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.slots[0].id = 5;
    active.slots[0].current_charges = 5;
    active.slots[0].cooldown = 0.0f;
    active.specialized_slots[0].skill_id = 5;
    active.specialized_slots[0].allocated_points = {{515, 1}};
    SkillSystem::RebakeSkillProfiles(registry, player);

    // 创建剑阵实体
    auto arrayEnt = registry.create();
    registry.emplace<Position>(arrayEnt, 60.0f, 0.0f);
    auto &arr = registry.emplace<SwordArrayComponent>(arrayEnt);
    arr.owner = player;
    arr.duration = 5.0f;
    arr.total_duration = 5.0f;
    arr.radius = 150.0f;

    auto &field = registry.emplace<AreaFieldComponent>(arrayEnt);
    field.owner = player;
    field.remaining_duration = 5.0f;
    field.pulse_interval = 0.5f;

    // 施放技能 5 引导
    SkillExecution exec{};
    exec.skill_id = 5;
    exec.owner = player;
    exec.target_pos = {60.0f, 0.0f};
    auto castFunc5 = SkillBehaviorRegistry::GetCast(5);
    REQUIRE(castFunc5 != nullptr);
    castFunc5(registry, player, exec);

    auto *beam = registry.try_get<BeamChannelComponent>(player);
    REQUIRE(beam != nullptr);
    beam->tick_timer = 0.0f;

    // 1 次 tick 轰击剑阵
    BeamChannelDeliverySystem::Update(registry, grid, 0.35f);

    CHECK(arr.duration > 5.0f);
    CHECK(field.remaining_duration > 5.0f);
  }

  SUBCASE("355 Blade Formation synergy truly accelerates SpiritSword attack timer inside array") {
    entt::registry registry;

    // 玩家在千里之外 (1000, 1000)
    // 剑阵在原点 (0, 0)，半径 150
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto &formation = registry.emplace<BladeFormationComponent>(player);
    formation.has_array_resonance = true;

    auto arrayEnt = registry.create();
    registry.emplace<Position>(arrayEnt, 0.0f, 0.0f);
    auto &arr = registry.emplace<SwordArrayComponent>(arrayEnt);
    arr.owner = player;
    arr.radius = 150.0f;
    arr.duration = 5.0f;

    // 灵剑 A 归属 player，在阵内 (20, 20)
    auto swordIn = registry.create();
    registry.emplace<SpiritSwordTag>(swordIn);
    registry.emplace<SummonComponent>(swordIn, player);
    registry.emplace<SummonAIProfile>(swordIn);
    registry.emplace<SummonRuntimeState>(swordIn);
    registry.emplace<Position>(swordIn, 20.0f, 20.0f);
    auto &aiIn = registry.emplace<SpiritSwordAI>(swordIn);
    aiIn.attack_timer = 1.0f;
    aiIn.attack_interval = 1.0f;

    // 玩家 2 在千里之外 (1000, 1000)，灵剑 B 归属玩家 2 在阵外
    auto player2 = registry.create();
    registry.emplace<PlayerTag>(player2);
    registry.emplace<Position>(player2, 1000.0f, 1000.0f);
    auto &formation2 = registry.emplace<BladeFormationComponent>(player2);
    formation2.has_array_resonance = true;

    auto swordOut = registry.create();
    registry.emplace<SpiritSwordTag>(swordOut);
    registry.emplace<SummonComponent>(swordOut, player2);
    registry.emplace<SummonAIProfile>(swordOut);
    registry.emplace<SummonRuntimeState>(swordOut);
    registry.emplace<Position>(swordOut, 1000.0f, 1000.0f);
    auto &aiOut = registry.emplace<SpiritSwordAI>(swordOut);
    aiOut.attack_timer = 1.0f;
    aiOut.attack_interval = 1.0f;

    // 运行 SummonAISystem: 参数为 (registry, dt, grid)
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    systems::SummonAISystem::Update(registry, 0.1f, grid);

    // 阵内灵剑获得 50% 攻速加速，计时器前进 0.1 * 1.5 = 0.15s -> 剩余 0.85s
    CHECK(aiIn.attack_timer == doctest::Approx(0.85f));
    // 阵外灵剑正常前进 0.1s -> 剩余 0.90s
    CHECK(aiOut.attack_timer == doctest::Approx(0.90f));
  }

  SUBCASE("VisualFX visibility calculation does not clamp to zero when duration is extended") {
    SwordArrayComponent arrInfo;
    arrInfo.duration = 6.8f;
    arrInfo.total_duration = 7.0f; // 由 515 扩展到 7.0s，且当前剩余 6.8s (运行了 0.2s)

    const float remaining = arrInfo.duration;
    const float total = (arrInfo.total_duration > 0.0f) ? std::max(arrInfo.total_duration, remaining) : std::max(5.0f, remaining);
    const float elapsed = std::max(0.0f, total - remaining);
    const float fadeIn = std::clamp(elapsed / 0.3f, 0.0f, 1.0f);
    const float fadeOut = std::clamp(remaining / 0.4f, 0.0f, 1.0f);
    const float visibility = std::min(fadeIn, fadeOut);

    // 0.2s / 0.3s = 0.6667f, 不会产生负数 clamp 到 0
    CHECK(visibility == doctest::Approx(0.666667f).epsilon(0.01f));
    CHECK(visibility > 0.0f);
  }
}

TEST_CASE("[Functional] Skill 6 - 633 Execute routes through shared kill path") {
  EnsureSkillMechanics();

  // 假定：633 处决 (AreaFieldDeliverySystem tryExecute) 走统一击杀链
  // CombatSystem::KillEnemy，从而触发 MonsterAffixSystem::OnEnemyDeath 的
  // 死亡词缀效果并累计玩家击杀数 (PlayerStats::killCount)。
  // 若该链被绕回旧实现，毒球与击杀统计断言将失败——这正是本条护栏的目的。
  SUBCASE("633 execution triggers Toxic affix on-death orbs and increments killCount") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{633, 1}});
    // 击杀统计挂载在 PlayerStats 组件 (killCount 字段)
    registry.emplace<PlayerStats>(player);

    // 生命值低于 12% (100 / 1000 = 10%) 的普通敌人，装配 Toxic 死亡词缀
    auto enemy = CreateTestEnemy(registry, 20.0f, 0.0f, 1000.0f, false);
    registry.get<HealthComponent>(enemy).current = 100.0f;
    registry.get<CombatStats>(enemy).health = 100.0f;
    auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
    affix.AddAffix(MonsterAffixType::Toxic);

    CastSwordArray(registry, player, {0.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    // 触发脉冲 -> 处决
    AreaFieldDeliverySystem::Update(registry, grid, 0.55f);

    // 1. 敌人被处决 (KilledTag 由公共击杀链写入; ExecutedTag 在派发后同步清除, 不做断言)
    CHECK(registry.any_of<KilledTag>(enemy));

    // 2. 玩家击杀统计 +1
    CHECK(registry.get<PlayerStats>(player).killCount == 1);

    // 3. Toxic 死亡词缀生成 3 个追踪毒球 (ApplyToxicOnDeath 确定性循环, 仅坐标随机)
    auto orbView = registry.view<VolatileOrbTag>();
    CHECK(orbView.size() == 3);
    int ownedOrbs = 0;
    for (auto orb : orbView) {
      const auto &orbComp = registry.get<VolatileOrbComponent>(orb);
      if (orbComp.owner == enemy) {
        ++ownedOrbs;
      }
    }
    CHECK(ownedOrbs == 3);
  }
}

} // namespace NoMoreDay
