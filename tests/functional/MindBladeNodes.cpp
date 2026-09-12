// 技能7 心剑·无影 — 专项功能测试（审查整改回归）
// 覆盖：C3 OnCast 生命周期、C2/M8 转质、M2/M3 tick 伤害范围、H9 持续扣蓝/空蓝打断、
//       M9 松开收尾、711 碎空爆、775 TypeE 抗性上限压制、H6 714 触发规则、契约矩阵烟雾。
// 说明：仅可观测行为断言，不改动 src/assets；每用例独立重置全局单例，避免跨用例污染。

#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/HazardComponents.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/VisualFXSystem.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/ProcEngine.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 7;

// 完整重置技能机制链，保证用例间无残留（含事件派发器与钩子）
void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  (void)systems::AilmentRegistry::Get().EnsureLoaded();
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

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y, float max_hp = 1000.0f) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<HealthComponent>(enemy, max_hp, max_hp);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = max_hp;
  stats.health = max_hp;
  stats.cached_area_level = 1;
  return enemy;
}

// 直调行为层 OnCast：与既有 functional 测试一致，避免依赖完整技能状态机
void CastMindBlade(entt::registry &registry, entt::entity player, Vector2 target_pos) {
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

void RebuildGrid(systems::SpatialHashGrid &grid, entt::registry &registry) {
  grid.rebuild(registry.view<Position>(), registry);
}

// 逐帧推进光束交付，并保持输入保活窗口（模拟玩家持续按住按键）
void StepBeam(entt::registry &registry, systems::SpatialHashGrid &grid,
              entt::entity caster, float dt) {
  if (auto *chan = registry.try_get<ChannelingComponent>(caster)) {
    chan->channel_timer = 0.25f;
  }
  RebuildGrid(grid, registry);
  BeamChannelDeliverySystem::Update(registry, grid, dt);
}

float HpOf(entt::registry &registry, entt::entity e) {
  return registry.get<HealthComponent>(e).current;
}

// 按 id 查找实体身上的 buff 效果；不存在返回 nullptr
const BuffEffect *FindEffect(entt::registry &registry, entt::entity e,
                             const std::string &id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.id == id) return &b;
  }
  return nullptr;
}

} // namespace

// 用例1：OnCast 同时建立 ChannelingComponent 与 BeamChannelComponent；
//        硬性引导上限 max_channel_time=5.0，绝不等于输入保活窗口 0.25（C3 回归）。
TEST_CASE("[Functional] MindBlade - OnCast 生命周期与引导上限 (C3)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});

  CastMindBlade(registry, player, {100.0f, 40.0f});

  auto *chan = registry.try_get<ChannelingComponent>(player);
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(chan != nullptr);
  REQUIRE(beam != nullptr);

  CHECK(chan->skill_id == kSkillId);
  CHECK(chan->channel_timer == doctest::Approx(0.25f));
  CHECK(chan->tick_interval == doctest::Approx(0.3f));

  CHECK(beam->skill_id == kSkillId);
  CHECK(beam->owner == player);
  CHECK(beam->mode == BeamChannelMode::ContinuousLaser);
  // 关键回归：引导上限必须是机制数据 5.0，不能被输入保活窗口 0.25 污染
  CHECK(beam->max_channel_time == doctest::Approx(5.0f));
  CHECK(beam->max_channel_time != doctest::Approx(0.25f));
  CHECK(beam->tick_interval == doctest::Approx(0.3f));
  CHECK(beam->target_pos.x == doctest::Approx(100.0f));
  CHECK(beam->target_pos.y == doctest::Approx(40.0f));
}

// 用例2：HandleSkillInput 保活 — 重置 channel_timer=0.25 且 beam.target_pos 跟随
TEST_CASE("[Functional] MindBlade - 引导输入保活与目标跟随") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});
  CastMindBlade(registry, player, {100.0f, 0.0f});

  auto *chan = registry.try_get<ChannelingComponent>(player);
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(chan != nullptr);
  REQUIRE(beam != nullptr);

  // 先人为消耗保活窗口，验证输入确实重新刷新
  chan->channel_timer = 0.05f;
  SkillSystem::HandleSkillInput(registry, player, 0, {220.0f, 60.0f});

  CHECK(chan->channel_timer == doctest::Approx(0.25f));
  CHECK(chan->target_pos.x == doctest::Approx(220.0f));
  CHECK(beam->target_pos.x == doctest::Approx(220.0f));
  CHECK(beam->target_pos.y == doctest::Approx(60.0f));
}

// 用例3：0.3s tick 伤害与范围 — 0.3s 前不结算，0.3s 结算，半径外不受影响（M2/M3）
TEST_CASE("[Functional] MindBlade - 0.3s tick 伤害与范围判定 (M2/M3)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});
  auto enemyNear = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);
  auto enemyFar = CreateTestEnemy(registry, 100.0f, 400.0f, 5000.0f);

  CastMindBlade(registry, player, {100.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  // 从"上一 tick 刚结算"状态开始，验证满 0.3s 才再次结算
  beam->tick_timer = 0.3f;

  const float near0 = HpOf(registry, enemyNear);
  const float far0 = HpOf(registry, enemyFar);

  StepBeam(registry, grid, player, 0.1f);
  StepBeam(registry, grid, player, 0.1f);
  // 0.2s 时未到 tick 间隔
  CHECK(HpOf(registry, enemyNear) == doctest::Approx(near0));

  // 累计到达 0.3s（末帧取 0.15s 规避 0.1f 累加的浮点残差）
  StepBeam(registry, grid, player, 0.15f);
  // 满 0.3s：范围内敌人掉血，范围外不受影响
  CHECK(HpOf(registry, enemyNear) < near0);
  CHECK(HpOf(registry, enemyFar) == doctest::Approx(far0));
}

// 用例4：持续扣蓝与空蓝打断 — 每次 tick 扣 mana_cost_per_sec*tick_interval；
//        mana 不足时移除双组件并派发一次 OnChannelEnd（H9）
TEST_CASE("[Functional] MindBlade - 持续扣蓝与空蓝打断 (H9)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  int channelEndCount = 0;
  const uint32_t handlerId = CombatEventDispatcher::Register(
      CombatEventType::OnChannelEnd,
      [&channelEndCount](entt::registry &, const CombatEvent &) { ++channelEndCount; }, 0);

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});
  CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);

  CastMindBlade(registry, player, {100.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  auto *stats = registry.try_get<CombatStats>(player);
  REQUIRE(beam != nullptr);
  REQUIRE(stats != nullptr);

  beam->tick_timer = 0.0f;
  const float manaBefore = stats->mana;
  StepBeam(registry, grid, player, 0.05f);
  // 法耗 = 15 * 0.3 = 4.5
  CHECK(stats->mana == doctest::Approx(manaBefore - 4.5f));
  CHECK(channelEndCount == 0);

  // 空蓝：低于本次 tick 消耗即打断收尾
  stats->mana = 3.0f;
  beam->tick_timer = 0.0f;
  RebuildGrid(grid, registry);
  BeamChannelDeliverySystem::Update(registry, grid, 0.05f);

  CHECK(channelEndCount == 1);
  CHECK(registry.try_get<ChannelingComponent>(player) == nullptr);
  CHECK(registry.try_get<BeamChannelComponent>(player) == nullptr);

  CombatEventDispatcher::Unregister(CombatEventType::OnChannelEnd, handlerId);
}

// 用例5：松开按键收尾 — channel_timer 归零后 Update 移除双组件且 OnChannelEnd 派发一次（C3/M9）
TEST_CASE("[Functional] MindBlade - 松开按键收尾 (C3/M9)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  int channelEndCount = 0;
  const uint32_t handlerId = CombatEventDispatcher::Register(
      CombatEventType::OnChannelEnd,
      [&channelEndCount](entt::registry &, const CombatEvent &) { ++channelEndCount; }, 0);

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {});
  CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);

  CastMindBlade(registry, player, {100.0f, 0.0f});
  auto *chan = registry.try_get<ChannelingComponent>(player);
  REQUIRE(chan != nullptr);

  chan->channel_timer = 0.0f;
  RebuildGrid(grid, registry);
  BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

  CHECK(channelEndCount == 1);
  CHECK(registry.try_get<ChannelingComponent>(player) == nullptr);
  CHECK(registry.try_get<BeamChannelComponent>(player) == nullptr);

  CombatEventDispatcher::Unregister(CombatEventType::OnChannelEnd, handlerId);
}

// 用例6：711 碎空爆 — 引导期间不结算 tick 伤害，结束瞬间在 target_pos 产生范围爆发（含未点 711 对照）
TEST_CASE("[Functional] MindBlade - 711 碎空爆引导期禁 tick 与结束爆发") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  {
    // 对照组：未点 711，tick 正常造成伤害
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {});
    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);
    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *beam = registry.try_get<BeamChannelComponent>(player);
    REQUIRE(beam != nullptr);
    beam->tick_timer = 0.0f;
    const float hp0 = HpOf(registry, enemy);
    StepBeam(registry, grid, player, 0.05f);
    CHECK(HpOf(registry, enemy) < hp0);
  }

  {
    // 711：引导期间 tick 不造成伤害，收尾时范围爆发造成伤害
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{711, 1}});
    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);
    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *beam = registry.try_get<BeamChannelComponent>(player);
    auto *chan = registry.try_get<ChannelingComponent>(player);
    REQUIRE(beam != nullptr);
    REQUIRE(chan != nullptr);

    beam->tick_timer = 0.0f;
    const float hpBefore = HpOf(registry, enemy);
    StepBeam(registry, grid, player, 0.05f);
    CHECK(HpOf(registry, enemy) == doctest::Approx(hpBefore));

    // 松手收尾 → 碎空爆引爆
    chan->channel_timer = 0.0f;
    RebuildGrid(grid, registry);
    BeamChannelDeliverySystem::Update(registry, grid, 0.01f);
    CHECK(HpOf(registry, enemy) < hpBefore);
  }
}

// 用例7：转质互斥 — 仅点 770 → Cold 不含 Lightning；仅点 772 → Lightning 不含 Cold（C2/M8）
TEST_CASE("[Functional] MindBlade - 770/772 转质元素映射 (C2/M8)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{770, 1}});
    auto &spec = registry.get<ActiveSkillsComponent>(player).specialized_slots[0];

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
    CHECK((profile.effective_tags & Tag::Cold) != Tag::None);
    CHECK((profile.effective_tags & Tag::Lightning) == Tag::None);

    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *chan = registry.try_get<ChannelingComponent>(player);
    REQUIRE(chan != nullptr);
    CHECK(chan->conversion_tag == Tag::Cold);
  }

  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{772, 1}});
    auto &spec = registry.get<ActiveSkillsComponent>(player).specialized_slots[0];

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
    CHECK((profile.effective_tags & Tag::Lightning) != Tag::None);
    CHECK((profile.effective_tags & Tag::Cold) == Tag::None);

    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *chan = registry.try_get<ChannelingComponent>(player);
    REQUIRE(chan != nullptr);
    CHECK(chan->conversion_tag == Tag::Lightning);
  }
}

// 用例8：775 心念灭抗（TypeE）— 命中挂 resist_cap_suppression>0 的 debuff，
//        且 DamageMitigationService 有效抗性上限被压低
TEST_CASE("[Functional] MindBlade - 775 TypeE 抗性上限压制") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  // 770 保证元素为 Cold，775 挂压制
  auto player = CreateTestPlayer(registry, {{770, 1}, {775, 4}});
  auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);

  CastMindBlade(registry, player, {100.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);

  beam->tick_timer = 0.0f;
  StepBeam(registry, grid, player, 0.05f);

  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const BuffEffect *suppress = nullptr;
  for (const auto &b : effects->effects) {
    if (b.id == "Skill7MentalSuppression") {
      suppress = &b;
      break;
    }
  }
  REQUIRE(suppress != nullptr);
  CHECK(suppress->is_debuff);
  CHECK(suppress->resist_cap_element == Tag::Cold);
  CHECK(suppress->resist_cap_suppression == doctest::Approx(0.12f)); // 0.03 * 4

  // 基线：一个无压制、冰抗 0.75 的敌人
  auto baseline = CreateTestEnemy(registry, 300.0f, 300.0f, 5000.0f);
  auto *baselineStats = registry.try_get<CombatStats>(baseline);
  REQUIRE(baselineStats != nullptr);
  baselineStats->resistances[static_cast<size_t>(DamageType::Cold)] = 0.75f;

  auto *stats = registry.try_get<CombatStats>(enemy);
  REQUIRE(stats != nullptr);
  stats->resistances[static_cast<size_t>(DamageType::Cold)] = 0.75f;

  const float baselineDamage = DamageMitigationService::Apply(
      registry, entt::null, baseline, kSkillId, Tag::None, Tag::Cold, 100.0f,
      baselineStats, {}, false, false, 1.0f, entt::null);
  const float suppressedDamage = DamageMitigationService::Apply(
      registry, entt::null, enemy, kSkillId, Tag::None, Tag::Cold, 100.0f, stats,
      {}, false, false, 1.0f, entt::null);

  // 0.75 上限被压制 0.12 → 有效上限 0.63；100*(1-0.75)=25 vs 100*(1-0.63)=37
  CHECK(baselineDamage == doctest::Approx(25.0f));
  CHECK(suppressedDamage == doctest::Approx(37.0f));
  CHECK(suppressedDamage > baselineDamage);
}

// 用例9：714 寂灭触发规则 — role=Trigger、OnKill、cast_skill_id=5，且击杀经 ProcEngine 派发技能5（H6）
TEST_CASE("[Functional] MindBlade - 714 击杀触发技能5 (H6)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{714, 1}});
  auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 1000.0f);

  auto &active = registry.get<ActiveSkillsComponent>(player);
  SkillSpecializationBaker::SyncTriggerRules(registry, player, kSkillId,
                                             &active.specialized_slots[0]);

  auto *triggers = registry.try_get<TriggerRuleComponent>(player);
  REQUIRE(triggers != nullptr);
  const TriggerRule *rule714 = nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == 714) {
      rule714 = &triggers->rules[i];
      break;
    }
  }
  REQUIRE(rule714 != nullptr);
  CHECK(rule714->listen_event == CombatEventType::OnKill);
  CHECK(rule714->cast_skill_id == 5u);
  CHECK(rule714->target_mode == TriggerTargetPolicy::Victim);
  CHECK(rule714->required_skill_id == kSkillId);

  const CombatEvent evt = CombatEventFactory::CreateOnKill(player, enemy, 0.0f, kSkillId);
  ProcEngine::DispatchEvent(registry, player, evt);

  int skill5Executions = 0;
  for (auto e : registry.view<SkillExecution>()) {
    if (registry.get<SkillExecution>(e).skill_id == 5u) {
      ++skill5Executions;
    }
  }
  CHECK(skill5Executions == 1);

  // 来源过滤：非技能7 的击杀不应触发寂灭
  const CombatEvent otherKill = CombatEventFactory::CreateOnKill(player, enemy, 0.0f, 4u);
  ProcEngine::DispatchEvent(registry, player, otherKill);
  skill5Executions = 0;
  for (auto e : registry.view<SkillExecution>()) {
    if (registry.get<SkillExecution>(e).skill_id == 5u) {
      ++skill5Executions;
    }
  }
  CHECK(skill5Executions == 1);
}

// 用例10：契约/矩阵烟雾 — 合法转质组合 TryCast 成功且 Runtime 激活转质=770；
//         770+772 双转质被 max_transmuters=1 拦截；714/711/732 节点角色无错位
TEST_CASE("[Functional] MindBlade - 契约矩阵角色与转质互斥烟雾") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 节点角色（数据错位回归）
  const auto *n714 = SkillRegistry::Get().GetNodeContract(kSkillId, 714);
  const auto *n711 = SkillRegistry::Get().GetNodeContract(kSkillId, 711);
  const auto *n732 = SkillRegistry::Get().GetNodeContract(kSkillId, 732);
  REQUIRE(n714 != nullptr);
  REQUIRE(n711 != nullptr);
  REQUIRE(n732 != nullptr);
  CHECK(n714->role == SpecNodeRole::Trigger);
  CHECK(n711->role == SpecNodeRole::Keystone);
  CHECK(n732->role == SpecNodeRole::Keystone);

  // 合法单转质：TryCast 成功，Runtime 激活 770
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{770, 1}});
    CHECK(SkillSystem::TryCast(registry, player, 0, {50.0f, 0.0f}));
    auto *runtime = registry.try_get<SkillContractRuntimeComponent>(player);
    REQUIRE(runtime != nullptr);
    auto it = runtime->active_transmuter_node_by_skill.find(kSkillId);
    REQUIRE(it != runtime->active_transmuter_node_by_skill.end());
    CHECK(it->second == 770u);
  }

  // 双转质互斥：770+772 超过 max_transmuters=1，TryCast 被拦截
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{770, 1}, {772, 1}});
    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0, {50.0f, 0.0f}));
  }
}

// 用例11：703 心念映射追踪范围 — 基准 350，每点 +10% 正向放大（修复 M1）
// 对比点：旧实现基础值缺失/回退会让加点后 range 不升反降（负收益）；修复后必须严格大于基准。
TEST_CASE("[Functional] MindBlade - 703 追踪范围按点正向放大 (M1)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  auto bakeRange = [](int points) {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{703, points}});
    auto &spec = registry.get<ActiveSkillsComponent>(player).specialized_slots[0];
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
    return profile.delivery.range;
  };

  // 0 点：保持 350 基准
  CHECK(bakeRange(0) == doctest::Approx(350.0f));
  // 4 点：350 * (1 + 0.10*4) = 490，严格高于基准 —— 加点是正收益
  const float range4 = bakeRange(4);
  CHECK(range4 == doctest::Approx(490.0f));
  CHECK(range4 > 350.0f);
}

// 用例12：775 心念灭抗中心门控 — 仅"撕裂中心"范围内命中挂压制，外圈命中不挂（修复 M2）
// 对比点：旧实现 AoE 路径硬编码 inCenter=true，爆炸范围内所有敌人都会吃到抗性上限压制。
// 场景一（原设计）：711 引爆 —— 敌 A 置于 target_pos（距中心 0），敌 B 距中心 100
//                   （> center_radius 80 且 < detonation_radius 约 120）。
// 场景二（补充）：主 tick 路径，用 702 将半径扩到 60*(1+0.1*4)=84 构造 (80,84] 外圈。
TEST_CASE("[Functional] MindBlade - 775 中心门控仅压制中心敌人 (M2)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 场景一：711 碎空爆引爆，中心/外圈由到 cutPos 的真实距离判定
  {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    // 770 提供 Cold 元素条件，775 提供压制，711 提供收尾引爆
    auto player = CreateTestPlayer(registry, {{711, 1}, {770, 1}, {775, 4}});
    auto centerEnemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f); // 距中心 0
    auto outerEnemy = CreateTestEnemy(registry, 200.0f, 0.0f, 5000.0f);  // 距中心 100

    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *beam = registry.try_get<BeamChannelComponent>(player);
    auto *chan = registry.try_get<ChannelingComponent>(player);
    REQUIRE(beam != nullptr);
    REQUIRE(chan != nullptr);

    const float centerHp0 = HpOf(registry, centerEnemy);
    const float outerHp0 = HpOf(registry, outerEnemy);

    // 松手收尾 → 碎空爆引爆
    chan->channel_timer = 0.0f;
    RebuildGrid(grid, registry);
    BeamChannelDeliverySystem::Update(registry, grid, 0.01f);

    // 两敌都在爆炸半径内、确实都被引爆伤害命中（否则"外圈不挂"断言无区分力）
    CHECK(HpOf(registry, centerEnemy) < centerHp0);
    CHECK(HpOf(registry, outerEnemy) < outerHp0);

    // 中心敌（距离 0 <= center_radius 80）挂压制，压制值 = 0.03 * 4
    const BuffEffect *centerSuppress =
        FindEffect(registry, centerEnemy, "Skill7MentalSuppression");
    REQUIRE(centerSuppress != nullptr);
    CHECK(centerSuppress->resist_cap_suppression == doctest::Approx(0.12f));

    // 外圈敌（100 > 80）不挂压制 —— 旧实现硬编码 inCenter=true 时两者都会挂
    CHECK(FindEffect(registry, outerEnemy, "Skill7MentalSuppression") == nullptr);
  }

  // 场景二：主 tick 路径 —— 702 扩半径越过中心门控，外圈敌置于区间中点
  {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64.0f);
    auto player = CreateTestPlayer(registry, {{770, 1}, {775, 4}, {702, 4}});

    // 依据实际烘焙半径构造 (centerRadius, area_radius) 区间中点，避免写死 84
    auto &spec = registry.get<ActiveSkillsComponent>(player).specialized_slots[0];
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
    const float centerRadius = 80.0f;
    const float areaRadius = (profile.area_radius > 1.0f) ? profile.area_radius : 60.0f;
    REQUIRE(areaRadius > centerRadius);
    const float outerDist = centerRadius + (areaRadius - centerRadius) * 0.5f;

    auto centerEnemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f); // 距落点 0
    auto outerEnemy = CreateTestEnemy(registry, 100.0f + outerDist, 0.0f, 5000.0f);

    CastMindBlade(registry, player, {100.0f, 0.0f});
    auto *beam = registry.try_get<BeamChannelComponent>(player);
    REQUIRE(beam != nullptr);
    beam->tick_timer = 0.0f;

    const float outerHp0 = HpOf(registry, outerEnemy);
    StepBeam(registry, grid, player, 0.05f);

    // 外圈敌确实在烘焙半径内被主 tick 命中
    CHECK(HpOf(registry, outerEnemy) < outerHp0);

    const BuffEffect *centerSuppress =
        FindEffect(registry, centerEnemy, "Skill7MentalSuppression");
    REQUIRE(centerSuppress != nullptr);
    CHECK(centerSuppress->resist_cap_suppression == doctest::Approx(0.12f));
    CHECK(FindEffect(registry, outerEnemy, "Skill7MentalSuppression") == nullptr);
  }
}

// 用例13：772 神雷天罡感电区域 — 多区域重叠只施加感电(1层)，不重复结算全额落雷伤害（修复 M3）
// 对比点：旧实现每个区域每 0.5s 各自结算全额落雷伤害，两点重叠约为 4 倍全额。
TEST_CASE("[Functional] MindBlade - 772 感电区域不叠加全额伤害 (M3)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  auto player = CreateTestPlayer(registry, {{772, 1}});
  auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);

  CastMindBlade(registry, player, {100.0f, 0.0f});

  // 引导出 >=2 个感电区域（落雷间隔 0.5s，区域持续 2s）；
  // dt 必须小于输入保活窗口 0.25s，否则引导会在落雷前被判定为松手收尾
  for (int i = 0; i < 10 && registry.view<ShockFieldComponent>().size() < 2; ++i) {
    StepBeam(registry, grid, player, 0.2f);
  }
  REQUIRE(registry.view<ShockFieldComponent>().size() >= 2);

  // 停掉引导本体，隔离出感电区域自身的结算行为
  if (registry.any_of<BeamChannelComponent>(player)) {
    registry.remove<BeamChannelComponent>(player);
  }
  if (registry.any_of<ChannelingComponent>(player)) {
    registry.remove<ChannelingComponent>(player);
  }

  const float hpBefore = HpOf(registry, enemy);
  for (int i = 0; i < 4; ++i) {
    RebuildGrid(grid, registry);
    BeamChannelDeliverySystem::Update(registry, grid, 0.5f);
  }
  const float fieldLoss = hpBefore - HpOf(registry, enemy);

  // 单体落雷全额 = baseDamage(30) * pillar_damage_mult(2.0) = 60；
  // 新实现区域零直接伤害：4 帧后损失必须低于一个全额（旧实现为多区域叠加，远超此上界）
  const float pillarDamage = 60.0f;
  CHECK(fieldLoss < pillarDamage);

  // 感电状态存在且恰好 1 层
  auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const BuffEffect *shock = nullptr;
  for (const auto &b : effects->effects) {
    if (b.managed_ailment && b.ailment_type == static_cast<uint8_t>(AilmentType::Shock)) {
      shock = &b;
      break;
    }
  }
  REQUIRE(shock != nullptr);
  CHECK(shock->stacks == 1);
}

// 用例14：714 寂灭真实击杀链路 — ApplyDamage(skill_id=7) 致命击杀经 KillEnemy→OnKill 触发技能5（修复 M4）
// 对比点：走真实伤害管线而非手工 DispatchEvent；skill_id=4 的致命击杀必须被来源过滤拦截。
TEST_CASE("[Functional] MindBlade - 714 真实击杀触发技能5 (M4)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  auto countSkill5Executions = [](entt::registry &registry) {
    int n = 0;
    for (auto e : registry.view<SkillExecution>()) {
      if (registry.get<SkillExecution>(e).skill_id == 5u) ++n;
    }
    return n;
  };

  // 场景 A：技能7 来源致命击杀 → 触发一次技能5
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{714, 1}});
    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 1000.0f);
    auto &active = registry.get<ActiveSkillsComponent>(player);
    SkillSpecializationBaker::SyncTriggerRules(registry, player, kSkillId,
                                               &active.specialized_slots[0]);
    registry.get<HealthComponent>(enemy).current = 10.0f;

    const bool died = CombatSystem::ApplyDamage(registry, enemy, 100.0f, player,
                                                /*isCrit=*/false, /*showVFX=*/false,
                                                nullptr, kSkillId);
    CHECK(died);
    CHECK(countSkill5Executions(registry) == 1);
  }

  // 场景 B：非技能7 来源致命击杀 → 不触发（required_skill_id 来源过滤）
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{714, 1}});
    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 1000.0f);
    auto &active = registry.get<ActiveSkillsComponent>(player);
    SkillSpecializationBaker::SyncTriggerRules(registry, player, kSkillId,
                                               &active.specialized_slots[0]);
    registry.get<HealthComponent>(enemy).current = 10.0f;

    const bool died = CombatSystem::ApplyDamage(registry, enemy, 100.0f, player,
                                                /*isCrit=*/false, /*showVFX=*/false,
                                                nullptr, 4u);
    CHECK(died);
    CHECK(countSkill5Executions(registry) == 0);
  }
}

// 用例15：735 精准切割孤立增伤 — 按真实命中敌数判定，多敌不再吃满 735 增伤（修复 M5）
// 对比点：旧实现暗杀计数硬编码 nearbyCount=1，多敌场景仍按孤立结算。
TEST_CASE("[Functional] MindBlade - 735 孤立增伤按真实敌数判定 (M5)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 主 tick 对比：单敌（孤立）伤害 > 双敌（非孤立）对同一目标的伤害
  {
    auto tickDamage = [](bool twoEnemies) {
      entt::registry registry;
      systems::SpatialHashGrid grid(1024, 1024, 64.0f);
      auto player = CreateTestPlayer(registry, {{735, 4}});
      registry.get<CombatStats>(player).crit_chance = 0.0f; // 消除随机暴击，保证可比
      auto target = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);
      if (twoEnemies) CreateTestEnemy(registry, 140.0f, 0.0f, 5000.0f);
      CastMindBlade(registry, player, {100.0f, 0.0f});
      auto *beam = registry.try_get<BeamChannelComponent>(player);
      REQUIRE(beam != nullptr);
      beam->tick_timer = 0.0f;
      const float hp0 = HpOf(registry, target);
      StepBeam(registry, grid, player, 0.05f);
      return hp0 - HpOf(registry, target);
    };

    const float single = tickDamage(false);
    const float multi = tickDamage(true);
    CHECK(single > multi);
  }

  // 711 引爆对比：单敌吃满 735，多敌不再享受（旧实现硬编码 nearbyCount=1 两者一致）
  {
    auto detonationDamage = [](bool twoEnemies) {
      entt::registry registry;
      systems::SpatialHashGrid grid(1024, 1024, 64.0f);
      auto player = CreateTestPlayer(registry, {{711, 1}, {735, 4}});
      registry.get<CombatStats>(player).crit_chance = 0.0f;
      auto target = CreateTestEnemy(registry, 100.0f, 0.0f, 5000.0f);
      if (twoEnemies) CreateTestEnemy(registry, 140.0f, 0.0f, 5000.0f);
      CastMindBlade(registry, player, {100.0f, 0.0f});
      auto *chan = registry.try_get<ChannelingComponent>(player);
      REQUIRE(chan != nullptr);
      chan->channel_timer = 0.0f;
      const float hp0 = HpOf(registry, target);
      RebuildGrid(grid, registry);
      BeamChannelDeliverySystem::Update(registry, grid, 0.01f);
      return hp0 - HpOf(registry, target);
    };

    const float single = detonationDamage(false);
    const float multi = detonationDamage(true);
    CHECK(single > multi);
  }
}

// 用例16：730 次级撕裂以自身中心判定 775 — 次级撕裂中心取 secPos 而非主 cutPos（修复 Major）
// 对比点：旧实现 applyHit 一律用主 cutPos 算中心距离，位于主落点外圈、却恰好是次级撕裂中心的
//         敌人会因距 cutPos 远而不挂压制；构造唯一候选保证 730 随机抽取确定性。
TEST_CASE("[Functional] MindBlade - 730 次级撕裂以自身中心判定 775") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  // 730 生成次级撕裂，770 提供 Cold 满足 775 元素门控，775 施加抗性上限压制；
  // 不点 711，否则主 tick 与 730 都会被蓄力分支短路
  auto player = CreateTestPlayer(registry, {{730, 1}, {770, 1}, {775, 4}});

  // caster 原点、cutPos=(350,0)：B 距 cutPos ≈ 320 > centerRadius(80)，但在 700 候选范围内，
  // 因而不会被主 tick 命中，只能成为唯一的次级撕裂目标；旧实现按 cutPos 判定会漏挂压制。
  auto enemy = CreateTestEnemy(registry, 100.0f, 200.0f, 5000.0f);

  CastMindBlade(registry, player, {350.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->tick_timer = 0.0f;

  const float hp0 = HpOf(registry, enemy);
  StepBeam(registry, grid, player, 0.05f);

  // 次级撕裂确实命中 B（候选唯一，随机抽取不会偏向其他目标）
  CHECK(HpOf(registry, enemy) < hp0);

  // B 位于自身次级撕裂中心（距离 0 <= 80），必须挂压制
  const BuffEffect *suppress = FindEffect(registry, enemy, "Skill7MentalSuppression");
  REQUIRE(suppress != nullptr);
  CHECK(suppress->resist_cap_suppression == doctest::Approx(0.12f));
  CHECK(suppress->resist_cap_element == Tag::Cold);
}

// 用例17：775 Lightning×730 组合 — 772 提供闪电元素时，730 次级撕裂仍以自身 secPos
//         为中心判定并挂 Lightning 抗性上限压制（补齐 skill7 复核 §5-5 未覆盖组合）。
// 对比点：既有用例 16 只覆盖 770(Cold)×730；本用例验证元素口径随转质切换为闪电。
TEST_CASE("[Functional] MindBlade - 772 Lightning × 730 次级撕裂挂 775 压制") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  systems::SpatialHashGrid grid(1024, 1024, 64.0f);
  // 730 生成次级撕裂，772 提供 Lightning 满足 775 元素门控，775 施加抗性上限压制；
  // 不点 770/711，避免冰转质与蓄力引爆分支干扰闪电口径的独立验证。
  auto player = CreateTestPlayer(registry, {{730, 1}, {772, 1}, {775, 4}});

  // 唯一敌人：位于 700 候选范围内，但距主中心远，只能成为 730 次级撕裂目标，
  // 从而保证次级中心取 secPos（而非主 cutPos）且元素判定为 Lightning。
  auto enemy = CreateTestEnemy(registry, 100.0f, 200.0f, 5000.0f);

  CastMindBlade(registry, player, {350.0f, 0.0f});
  auto *beam = registry.try_get<BeamChannelComponent>(player);
  REQUIRE(beam != nullptr);
  beam->tick_timer = 0.0f;

  const float hp0 = HpOf(registry, enemy);
  StepBeam(registry, grid, player, 0.05f);

  // 次级撕裂确实命中该敌（否则压制断言无区分力）
  CHECK(HpOf(registry, enemy) < hp0);

  // 775 压制按 Lightning 元素记录，压制值 = 0.03 * 4
  const BuffEffect *suppress = FindEffect(registry, enemy, "Skill7MentalSuppression");
  REQUIRE(suppress != nullptr);
  CHECK(suppress->resist_cap_element == Tag::Lightning);
  CHECK(suppress->resist_cap_suppression == doctest::Approx(0.12f));
}

} // namespace NoMoreDay
