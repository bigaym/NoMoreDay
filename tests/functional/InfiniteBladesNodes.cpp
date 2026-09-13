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
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 5;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
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

} // namespace

TEST_CASE("[Functional] Skill 5 - Graph Topology & Prerequisite Integrity (C1 / J9 / J10 / J16)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  const auto *tree = SkillRegistry::Get().GetSkillTree(kSkillId);
  REQUIRE(tree != nullptr);
  CHECK(tree->nodes.size() == 28);

  auto findNode = [&](uint32_t id) -> const TalentNode* {
    auto it = tree->nodes.find(id);
    if (it != tree->nodes.end()) return &it->second;
    return nullptr;
  };

  // 验证 500 max_points 为 4
  const auto *node500 = findNode(500);
  REQUIRE(node500 != nullptr);
  CHECK(node500->max_points == 4);

  // 验证 510 神识锁定 max_points 为 1，前置需 500 (2 点)
  const auto *node510 = findNode(510);
  REQUIRE(node510 != nullptr);
  CHECK(node510->max_points == 1);
  REQUIRE(!node510->prerequisites.empty());
  CHECK(node510->prerequisites[0].node_id == 500);
  CHECK(node510->prerequisites[0].required_points == 2);

  // 验证 511 无处遁形 max_points 为 3，前置需 510 (1 点)
  const auto *node511 = findNode(511);
  REQUIRE(node511 != nullptr);
  CHECK(node511->max_points == 3);
  REQUIRE(!node511->prerequisites.empty());
  CHECK(node511->prerequisites[0].node_id == 510);
  CHECK(node511->prerequisites[0].required_points == 1);

  // 验证 512 天降命印 max_points 为 5，前置需 511 (2 点)
  const auto *node512 = findNode(512);
  REQUIRE(node512 != nullptr);
  CHECK(node512->max_points == 5);
  REQUIRE(!node512->prerequisites.empty());
  CHECK(node512->prerequisites[0].node_id == 511);
  CHECK(node512->prerequisites[0].required_points == 2);

  // 验证 513 天诛 max_points 为 1，前置需 512 (4 点)
  const auto *node513 = findNode(513);
  REQUIRE(node513 != nullptr);
  CHECK(node513->max_points == 1);
  REQUIRE(!node513->prerequisites.empty());
  CHECK(node513->prerequisites[0].node_id == 512);
  CHECK(node513->prerequisites[0].required_points == 4);

  // 验证 550 御剑行前置为 500 剑雨绵绵 (3 点)（设计 §3.5 L376: 需剑雨绵绵 3/4）
  const auto *node550 = findNode(550);
  REQUIRE(node550 != nullptr);
  CHECK(node550->max_points == 1);
  REQUIRE(!node550->prerequisites.empty());
  CHECK(node550->prerequisites[0].node_id == 500);
  CHECK(node550->prerequisites[0].required_points == 3);
}

TEST_CASE("[Functional] Skill 5 - Contract Keystones and Transmuter Exclusion (C2 / J6 / J13)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  const auto *contract = SkillRegistry::Get().GetSkillContract(kSkillId);
  REQUIRE(contract != nullptr);
  CHECK(contract->max_transmuters == 1);

  // 验证 Keystone 互斥组: 570 (Fire) 与 572 (Cold) 互斥
  const auto *node570 = SkillRegistry::Get().GetNodeContract(kSkillId, 570);
  const auto *node572 = SkillRegistry::Get().GetNodeContract(kSkillId, 572);
  REQUIRE(node570 != nullptr);
  REQUIRE(node572 != nullptr);
  CHECK(node570->keystone_exclusion_group > 0);
  CHECK(node570->keystone_exclusion_group == node572->keystone_exclusion_group);
  CHECK(contract->transmuter_node_ids[0] == 570);
  CHECK(contract->transmuter_node_ids[1] == 572);

  // 验证 513 为 Trigger 节点且 trigger_skill_id 为 5, effectiveness 为 3.0
  const auto *contract513 = SkillRegistry::Get().GetNodeContract(kSkillId, 513);
  REQUIRE(contract513 != nullptr);
  CHECK(contract513->role == SpecNodeRole::Trigger);
  CHECK(contract513->trigger.trigger_skill_id == 5);
  CHECK(contract513->trigger.effectiveness == doctest::Approx(3.0f));
  CHECK(contract513->trigger.internal_cooldown == doctest::Approx(2.0f));

  // 验证 533 为 Keystone 节点（非 Trigger，彻底消除 C4）
  const auto *contract533 = SkillRegistry::Get().GetNodeContract(kSkillId, 533);
  REQUIRE(contract533 != nullptr);
  CHECK(contract533->role == SpecNodeRole::Keystone);
}

TEST_CASE("[Functional] Skill 5 - Channeling Lifecycle and Mana Drain (H1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry);

  CastInfiniteBlades(registry, player);

  // 验证引导元数据仅由 BeamChannelComponent 承载
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  CHECK(beam->max_channel_time == doctest::Approx(5.0f));
  // channel_timer 是技能7 专用的输入保活窗口；技能5 不再写入该字段（保持默认 0），
  // 引导收尾只由 max_channel_time 决定。此处断言默认值，防止死写点回归。
  CHECK(beam->channel_timer == doctest::Approx(0.0f));
  CHECK(beam->tick_interval == doctest::Approx(0.3f));

  auto *stats = registry.try_get<CombatStats>(player);
  REQUIRE(stats != nullptr);
  float initial_mana = stats->mana; // 200

  // 触发 1 次 tick (dt = 0.35s)
  BeamChannelDeliverySystem::Update(registry, grid, 0.35f);

  // 每秒消耗 20 法力，0.3s tick 应扣除 ~6.0 点法力
  CHECK(stats->mana < initial_mana);
  float consumed = initial_mana - stats->mana;
  CHECK(consumed == doctest::Approx(20.0f * 0.3f).epsilon(0.01f));

  // 验证法力耗尽时引导中断且 BeamChannelComponent 被清除
  stats->mana = 1.0f; // 低于下一次 tick 所需法力
  beam->tick_timer = 0.0f;
  BeamChannelDeliverySystem::Update(registry, grid, 0.05f);

  CHECK(!registry.any_of<BeamChannelComponent>(player));
}

TEST_CASE("[Functional] Skill 5 - Colossal Blades 533 Mechanics (H3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{533, 1}});

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, player, kSkillId);
  REQUIRE(profile != nullptr);
  CHECK(profile->more_damage_mult >= 2.5f); // 基础伤害 +150%

  CastInfiniteBlades(registry, player);

  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);

  // 触发发射
  beam->tick_timer = 0.0f;
  BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

  // 巨剑术: 数量减半为 1 枚巨剑，半径为 70
  int projCount = 0;
  float projRadius = 0.0f;
  auto view = registry.view<Projectile>();
  for (auto e : view) {
    if (view.get<Projectile>(e).owner == player) {
      projCount++;
      projRadius = view.get<Projectile>(e).radius;
    }
  }
  CHECK(projCount == 1);
  CHECK(projRadius == doctest::Approx(70.0f));
}

TEST_CASE("[Functional] Skill 5 - Fate Mark 512 & Execution 513 Trigger (C4 / H2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{512, 5}, {513, 1}});
  auto enemy = CreateTestEnemy(registry);

  // 测试 DoHit 附加命印 (BuffKind::FateMark)
  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // 模拟命中 5 次，确保命印堆叠至 5 层
  for (int i = 0; i < 5; ++i) {
    hitFunc(registry, player, enemy, Tag::Physical, false);
  }

  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const auto *mark = effects->GetByKind(BuffKind::FateMark);
  REQUIRE(mark != nullptr);
  CHECK(mark->stacks == 5);

  // 验证 DamagePipeline 计算下 5 层命印增加 15% 伤害
  DamageRequest req{};
  req.attacker = player;
  req.defender = enemy;
  req.skill_id = kSkillId;
  req.source_entity = player;
  req.base_pool.Add(Tag::Physical, 100.0f);
  req.is_simulation = true;

  auto res = DamagePipeline::Calculate(registry, req);
  CHECK(res.total_damage > 100.0f);

  // 测试天诛 513 触发: 当 trigger_depth > 0 时派发主剑，不打断引导
  CastInfiniteBlades(registry, player);
  REQUIRE(registry.any_of<BeamChannelComponent>(player));

  SkillExecution exec513{};
  exec513.skill_id = kSkillId;
  exec513.owner = player;
  exec513.target_pos = {50.0f, 0.0f};
  exec513.trigger_depth = 1; // 触发调用
  exec513.trigger_effectiveness = 3.0f;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  castFunc(registry, player, exec513);

  // 玩家自身的引导元数据仍然存在且未被破坏
  CHECK(registry.any_of<BeamChannelComponent>(player));

  // 验证生成了天诛主剑 (半径 70, 必爆)
  bool foundExecutionBlade = false;
  auto projView = registry.view<Projectile>();
  for (auto e : projView) {
    const auto &proj = projView.get<Projectile>(e);
    if (proj.radius == 70.0f && proj.payload_context && proj.payload_context->crit_chance >= 1.0f) {
      foundExecutionBlade = true;
      CHECK(proj.payload_context->more_damage == doctest::Approx(3.0f));
    }
  }
  CHECK(foundExecutionBlade);
}

TEST_CASE("[Functional] Skill 5 - Intent Burst 554 & Multiplier 555 (H4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{554, 1}, {555, 4}});

  // 1. 满层 (10层) 剑意施放: 消耗 10 层并获得 100% 暴击率
  registry.emplace<SwordIntentComponent>(player).stacks = 10;

  CastInfiniteBlades(registry, player);

  auto *intent = registry.try_get<SwordIntentComponent>(player);
  REQUIRE(intent != nullptr);
  CHECK(intent->stacks == 0); // 消耗完毕

  const auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  CHECK(beam->bonus_crit_chance >= 1.0f); // 必暴归一化 1.0 = 100%
  CHECK(beam->bonus_damage_mult > 1.0f); // 555 暴伤增幅
}

TEST_CASE("[Functional] Skill 5 - Intent Siphon 553 (H4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{553, 2}}); // 每10连击回复 2 层剑意
  auto enemy = CreateTestEnemy(registry);
  registry.emplace<SwordIntentComponent>(player).stacks = 0;

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  for (int i = 0; i < 9; ++i) {
    hitFunc(registry, player, enemy, Tag::Physical, false);
  }
  CHECK(registry.get<SwordIntentComponent>(player).stacks == 0);

  // 第 10 次命中，触发回能
  hitFunc(registry, player, enemy, Tag::Physical, false);
  CHECK(registry.get<SwordIntentComponent>(player).stacks == 2);
}

TEST_CASE("[Functional] Skill 5 - Heavenly Sword Descent Follow-Up Linkage (C7)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry);

  // 模拟天剑降世回鞘状态准备就绪
  auto fieldEntity = registry.create();
  auto &field = registry.emplace<HeavenlySwordFieldComponent>(fieldEntity);
  registry.emplace<PersistentFieldTag>(fieldEntity); // 夹具同步：持久场标记
  field.header.owner = player;
  field.return_to_sheath_timer = 2.0f;
  field.return_to_sheath_bonus_mult = 0.5f;
  field.return_to_sheath_ready = true;

  // 施放万剑归宗（经 RegisterHeavenlySwordDescent 包装后将自动调用 CastInfiniteBladesWithHeavenlyFollowUp）
  CastInfiniteBlades(registry, player);

  auto *beam = registry.try_get<BeamChannelComponent>(player);

  REQUIRE(beam != nullptr);

  // 核心纠偏验证: 交付系统消费的 BeamChannelComponent 被赋予 is_empowered 与增伤倍率，确保回鞘加成不丢失
  CHECK(beam->is_empowered == true);
  CHECK(beam->bonus_damage_mult >= 1.5f);
}

TEST_CASE("[Functional] Skill 5 - Elemental Transmutation Tags & Penetration (C5 / C6 / H5)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);

  // 测试 570 天火流星 + 574 灵根感应 (3点)
  auto firePlayer = CreateTestPlayer(registry, {{570, 1}, {574, 3}});
  const auto *fireProfile = SkillSystem::GetBakedSkillProfile(registry, firePlayer, kSkillId);
  REQUIRE(fireProfile != nullptr);
  CHECK(HasTag(fireProfile->effective_tags, Tag::Fire));
  CHECK(!HasTag(fireProfile->effective_tags, Tag::Physical));
  CHECK(fireProfile->delivery.armor_pen == doctest::Approx(18.0f));

  CastInfiniteBlades(registry, firePlayer);

  auto *beam = registry.try_get<BeamChannelComponent>(firePlayer);
  REQUIRE(beam != nullptr);
  beam->tick_timer = 0.0f;
  BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

  // 验证发射的投射物 Payload 包含 Tag::Fire 且不是硬编码的 Physical
  bool foundFireProj = false;
  auto projView = registry.view<Projectile>();
  for (auto e : projView) {
    if (projView.get<Projectile>(e).owner == firePlayer) {
      const auto &proj = projView.get<Projectile>(e);
      REQUIRE(proj.payload_context.has_value());
      CHECK(HasTag(proj.payload_context->effective_tags, Tag::Fire));
      CHECK(!HasTag(proj.payload_context->effective_tags, Tag::Physical));
      CHECK(proj.snapshot.armor_pen == doctest::Approx(18.0f));
      foundFireProj = true;
      break;
    }
  }
  CHECK(foundFireProj);
}

TEST_CASE("[Functional] Skill 5 - Composure 530, Steeled Body 531 & Abundant Qi 532 (H3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{530, 4}, {531, 3}, {532, 3}});

  CastInfiniteBlades(registry, player);

  // 1. 530 气定神闲: 引导时受到的所有伤害降低 24%
  auto *stats = registry.try_get<CombatStats>(player);
  REQUIRE(stats != nullptr);
  float incoming_damage = 100.0f;
  float mitigated = DamageMitigationService::Apply(
      registry, entt::null, player, 0, Tag::Physical, Tag::Physical, incoming_damage,
      stats, {}, false, false, 1.0f, entt::null);
  CHECK(mitigated == doctest::Approx(100.0f * (1.0f - 0.24f)));
  REQUIRE(stats != nullptr);
  stats->barrier = 0.0f;
  stats->max_barrier = 500.0f;

  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->tick_timer = 0.0f;
  BeamChannelDeliverySystem::Update(registry, grid, 0.35f);

  auto *fx = registry.try_get<ActiveEffectsComponent>(player);
  REQUIRE(fx != nullptr);
  bool foundArmor = false;
  for (const auto &b : fx->effects) {
    if (b.id == "SteeledBodyArmor" && b.type == BuffType::DefenseUp) {
      foundArmor = true;
      CHECK(b.stacks >= 1);
      // 每层护甲 = armor_per_sec_per_point(15.0) × 3 点 = 45，modifiers 按当前层数线性累加
      CHECK(!b.modifiers.empty());
      if (!b.modifiers.empty()) {
        CHECK(b.modifiers[0].type == StatType::Armor);
        CHECK(b.modifiers[0].mode == ModifierMode::Flat);
        CHECK(b.modifiers[0].value ==
              doctest::Approx(45.0f * static_cast<float>(b.stacks)));
      }
      break;
    }
  }
  CHECK(foundArmor);
  CHECK(stats->barrier > 0.0f); // 532 回盾生效
}

TEST_CASE("[Functional] Skill 5 - Sword God Finisher 534 & Shockwave 535 (H3)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{534, 1}, {535, 3}});
  auto enemy = CreateTestEnemy(registry, 50.0f, 0.0f);
  grid.rebuild(registry.view<Position>(), registry);

  CastInfiniteBlades(registry, player, {50.0f, 0.0f});

  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->current_channel_time = 2.5f; // 满足引导 >= 2s 条件

  // 结束引导触发天剑降世终结技
  beam->current_channel_time = beam->max_channel_time;
  BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

  const auto *enemyHp = registry.try_get<HealthComponent>(enemy);
  REQUIRE(enemyHp != nullptr);
  // 800% 物理范围伤害 (base 50 * 8 = 400 伤害)
  CHECK(enemyHp->current < 5000.0f);
  CHECK(enemyHp->current <= 4600.0f);

  // 535 余波: 普通怪必定击晕
  auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);
  bool foundStun = false;
  for (const auto &b : fx->effects) {
    if (b.type == BuffType::Stun) {
      foundStun = true;
      break;
    }
  }
  CHECK(foundStun);
}

TEST_CASE("[Functional] Skill 5 - Blades To Array Synergy 515 (H2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{515, 1}});

  // 创建归属于 player 的剑阵
  auto arrayEnt = registry.create();
  registry.emplace<Position>(arrayEnt, 60.0f, 0.0f);
  auto &arr = registry.emplace<SwordArrayComponent>(arrayEnt);
  arr.owner = player;
  arr.duration = 5.0f;
  arr.radius = 150.0f;

  CastInfiniteBlades(registry, player, {60.0f, 0.0f});

  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->tick_timer = 0.0f;

  // 触发 1 次 tick
  BeamChannelDeliverySystem::Update(registry, grid, 0.35f);

  // 验证剑阵持续时间被暂停判定 (duration 延长补足了消耗)
  CHECK(arr.duration > 5.0f);
}

TEST_CASE("[Functional] Skill 5 - Blade Storm 514 Kill Split (H2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{514, 3}});
  auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f);
  registry.emplace<KilledTag>(enemy); // 模拟敌人被击杀

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, enemy, Tag::Physical, false);

  // 验证击杀分裂产生 3 枚小剑气碎屑
  int shardCount = 0;
  auto projView = registry.view<Projectile>();
  for (auto e : projView) {
    if (projView.get<Projectile>(e).owner == player && projView.get<Projectile>(e).radius == 20.0f) {
      shardCount++;
      CHECK(projView.get<Projectile>(e).payload_context->base_damage_min == doctest::Approx(50.0f * 0.90f));
    }
  }
  CHECK(shardCount == 3);
}

TEST_CASE("[Functional] Skill 5 - Doomsday Ash 571 & Absolute Zero 573 (H5)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{571, 3}, {573, 3}});
  auto enemy = CreateTestEnemy(registry);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // 1. 571 末日余烬: 火焰命中生成燃烧地表
  hitFunc(registry, player, enemy, Tag::Fire, false);
  bool foundBurnField = false;
  auto fieldView = registry.view<AreaFieldComponent>();
  for (auto e : fieldView) {
    if (fieldView.get<AreaFieldComponent>(e).source_skill_id == kSkillId) {
      foundBurnField = true;
      CHECK(HasTag(fieldView.get<AreaFieldComponent>(e).payloads[0].damage_tags, Tag::Fire));
      break;
    }
  }
  CHECK(foundBurnField);

  // 2. 573 绝对零度: 冰霜命中延长冻结时间
  auto &fx = registry.get_or_emplace<ActiveEffectsComponent>(enemy);
  BuffEffect freeze{
    .id = "Freeze",
    .name = "Freeze",
    .type = BuffType::Freeze,
    .duration = 1.0f,
    .remaining = 1.0f,
    .stacks = 1,
    .max_stacks = 1,
    .is_debuff = true
  };
  fx.AddOrRefresh(freeze);

  hitFunc(registry, player, enemy, Tag::Cold, false);

  const auto *updatedFreeze = fx.Get("Freeze");
  REQUIRE(updatedFreeze != nullptr);
  CHECK(updatedFreeze->remaining >= 1.3f); // 延长了 0.3s

  // 验证对冻结敌人的击碎伤害增加 45%
  DamageRequest req{};
  req.attacker = player;
  req.defender = enemy;
  req.skill_id = kSkillId;
  req.source_entity = player;
  req.base_pool.Add(Tag::Cold, 100.0f);
  req.is_simulation = true;

  auto res = DamagePipeline::Calculate(registry, req);
  CHECK(res.total_damage >= 145.0f);
}

} // namespace NoMoreDay
