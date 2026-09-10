// 技能8 御剑·回旋 — 元素路径系统功能测试（审查整改回归）
// 覆盖：路径几何/生命周期（Spawn/IsInside/衰减）、871 燎原之势增伤、
//       875 灵根破壁元素穿透、876 元素护体绝对减伤、872/873 电弧与电磁爆发。
// 说明：仅可观测行为断言，不改动 src/assets；抗性节点通过直调
//       DamageMitigationService::Apply 精确验证，电弧通过系统 Update 的
//       伤害数值差异验证，避免依赖尚未烘焙的 skill_mechanics 技能8数据。

#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/ElementPathSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <string>
#include <unordered_map>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 8;

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
  stats.min_weapon_damage = 30.0f;
  stats.max_weapon_damage = 30.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.accuracy = 1.0f;
  stats.effective_intelligence = 100.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].current_charges = 5;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  // 与生产链路一致：专精分配后重烘培，876 等烘焙产物才有数值
  SkillSystem::RebakeSkillProfiles(registry, player);

  return player;
}

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y,
                             float max_hp = 1000.0f) {
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

// 直调减伤入口，元素固定为 Fire，避免依赖技能8已烘焙的转质标签。
float ApplyElementDamage(entt::registry &registry, entt::entity attacker,
                         entt::entity defender, uint32_t skill_id, Tag element,
                         float damage) {
  auto &stats = registry.get<CombatStats>(defender);
  systems::EndgameModifierAggregate endgame{};
  return DamageMitigationService::Apply(
      registry, attacker, defender, skill_id, element, element, damage, &stats,
      endgame, false, false, 1.0f, entt::null);
}

float HpOf(entt::registry &registry, entt::entity e) {
  return registry.get<HealthComponent>(e).current;
}

const BuffEffect *FindEffect(entt::registry &registry, entt::entity e,
                             const std::string &id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.id == id) return &b;
  }
  return nullptr;
}

element_path::SpawnParams MakeSegment(entt::entity owner, Tag element, Vector2 start,
                                      Vector2 end, float amp = 0.0f,
                                      float penetration = 0.0f,
                                      float arc_freq_mult = 1.0f,
                                      float duration = 3.0f,
                                      uint64_t cast_id = 1,
                                      float half_width = 30.0f) {
  element_path::SpawnParams params{};
  params.owner = owner;
  params.cast_id = cast_id;
  params.skill_id = kSkillId;
  params.element_tag = element;
  params.start = start;
  params.end = end;
  params.half_width = half_width;
  params.duration = duration;
  params.amp = amp;
  params.penetration = penetration;
  params.arc_freq_mult = arc_freq_mult;
  return params;
}

} // namespace

// 用例1：Spawn/IsInside 基础几何 — 线段内、宽度边界、端点外、元素不匹配。
TEST_CASE("BladeBoomerangElementPath.GeometryInsideOutside [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto owner = CreateTestPlayer(registry);
  element_path::Spawn(registry,
                      MakeSegment(owner, Tag::Fire, {0.0f, 0.0f}, {100.0f, 0.0f}));

  // 线段中部与宽度内侧命中
  CHECK(element_path::IsInside(registry, owner, Tag::Fire, {50.0f, 0.0f}));
  CHECK(element_path::IsInside(registry, owner, Tag::Fire, {50.0f, 29.0f}));
  // 超出半宽、超出端点、错误元素均不命中
  CHECK_FALSE(element_path::IsInside(registry, owner, Tag::Fire, {50.0f, 31.0f}));
  CHECK_FALSE(element_path::IsInside(registry, owner, Tag::Fire, {150.0f, 0.0f}));
  CHECK_FALSE(element_path::IsInside(registry, owner, Tag::Fire, {-50.0f, 0.0f}));
  CHECK_FALSE(element_path::IsInside(registry, owner, Tag::Lightning, {50.0f, 0.0f}));
}

// 用例2：Update 衰减到过期后路径失效（生命周期）。
TEST_CASE("BladeBoomerangElementPath.DecayAndExpire [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto owner = CreateTestPlayer(registry);
  element_path::Spawn(registry,
                      MakeSegment(owner, Tag::Fire, {0.0f, 0.0f}, {100.0f, 0.0f},
                                  0.0f, 0.0f, 1.0f, /*duration=*/0.5f));

  element_path::Update(registry, 0.2f);
  CHECK(element_path::IsInside(registry, owner, Tag::Fire, {50.0f, 0.0f}));

  element_path::Update(registry, 0.4f); // 累计 0.6s > 0.5s，段失效
  CHECK_FALSE(element_path::IsInside(registry, owner, Tag::Fire, {50.0f, 0.0f}));
}

// 用例3：分段合并与上限 — 共线相邻段合并为一段，非共线/超限时受 32 段上限约束。
TEST_CASE("BladeBoomerangElementPath.SegmentMergeAndCap [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto owner = CreateTestPlayer(registry);
  // 共线相接的两段应合并，count 保持 1（合并后总长 100 未超单段上限）
  element_path::Spawn(registry,
                      MakeSegment(owner, Tag::Fire, {0.0f, 0.0f}, {50.0f, 0.0f}));
  element_path::Spawn(registry,
                      MakeSegment(owner, Tag::Fire, {50.0f, 0.0f}, {100.0f, 0.0f}));
  const auto &comp = registry.get<element_path::ElementPathComponent>(owner);
  CHECK(comp.count == 1);
  CHECK(element_path::IsInside(registry, owner, Tag::Fire, {75.0f, 0.0f}));

  // 大量垂直方向（不共线）的段：受 kMaxSegmentsPerOwner 上限约束
  for (int i = 0; i < 80; ++i) {
    const float y = static_cast<float>(i) * 50.0f;
    element_path::Spawn(
        registry,
        MakeSegment(owner, Tag::Fire, {0.0f, y}, {0.0f, y + 10.0f}, 0.0f, 0.0f,
                    1.0f, 3.0f, static_cast<uint64_t>(100 + i)));
  }
  CHECK(comp.count <= element_path::kMaxSegmentsPerOwner);
}

// 用例4：871 燎原之势 — 目标处于火路径内吃火伤 More 增幅，路径外不吃；其他元素不吃。
TEST_CASE("BladeBoomerangElementPath.Node871WildfireAmp [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto player = CreateTestPlayer(registry);
  auto inside = CreateTestEnemy(registry, 50.0f, 0.0f);
  auto outside = CreateTestEnemy(registry, 50.0f, 500.0f); // 远离路径
  // 火抗为 0，便于观察纯 More 乘算
  registry.get<CombatStats>(inside).resistances[1] = 0.0f;
  registry.get<CombatStats>(outside).resistances[1] = 0.0f;

  constexpr float kBase = 100.0f;
  constexpr float kAmp = 0.45f;
  element_path::Spawn(registry,
                      MakeSegment(player, Tag::Fire, {0.0f, 0.0f}, {100.0f, 0.0f},
                                  kAmp));

  const float dmg_inside =
      ApplyElementDamage(registry, player, inside, kSkillId, Tag::Fire, kBase);
  const float dmg_outside =
      ApplyElementDamage(registry, player, outside, kSkillId, Tag::Fire, kBase);

  CHECK(dmg_inside == doctest::Approx(kBase * (1.0f + kAmp)));
  CHECK(dmg_outside == doctest::Approx(kBase));

  // 路径内但伤害元素为闪电：871 只加成对应元素，不应增幅
  auto lightning_inside = CreateTestEnemy(registry, 50.0f, 10.0f);
  registry.get<CombatStats>(lightning_inside).resistances[3] = 0.0f;
  const float dmg_lightning = ApplyElementDamage(registry, player, lightning_inside,
                                                 kSkillId, Tag::Lightning, kBase);
  CHECK(dmg_lightning == doctest::Approx(kBase));
}

// 用例5：875 灵根破壁 — 路径内目标吃元素穿透，路径外不吃，其他技能不吃。
TEST_CASE("BladeBoomerangElementPath.Node875Penetration [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto player = CreateTestPlayer(registry);
  auto inside = CreateTestEnemy(registry, 50.0f, 0.0f);
  auto outside = CreateTestEnemy(registry, 50.0f, 500.0f);
  registry.get<CombatStats>(inside).resistances[1] = 0.30f;
  registry.get<CombatStats>(outside).resistances[1] = 0.30f;

  constexpr float kBase = 100.0f;
  constexpr float kPen = 0.24f;
  element_path::Spawn(registry,
                      MakeSegment(player, Tag::Fire, {0.0f, 0.0f}, {100.0f, 0.0f},
                                  0.0f, kPen));

  const float dmg_inside =
      ApplyElementDamage(registry, player, inside, kSkillId, Tag::Fire, kBase);
  const float dmg_outside =
      ApplyElementDamage(registry, player, outside, kSkillId, Tag::Fire, kBase);

  // 路径内：0.30 - 0.24 = 0.06 抗性 → 94；路径外：0.30 → 70
  CHECK(dmg_inside == doctest::Approx(kBase * (1.0f - (0.30f - kPen))));
  CHECK(dmg_outside == doctest::Approx(kBase * (1.0f - 0.30f)));

  // 其他技能（技能7）即使站在同一路径内也不享受技能8穿透
  const float dmg_other_skill =
      ApplyElementDamage(registry, player, inside, 7u, Tag::Fire, kBase);
  CHECK(dmg_other_skill == doctest::Approx(kBase * (1.0f - 0.30f)));
}

// 用例6：876 元素护体 — 施法者站在自身对应元素路径内时受对应元素绝对减伤。
TEST_CASE("BladeBoomerangElementPath.Node876ElementShield [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto player = CreateTestPlayer(registry, {{876, 1}});
  auto attacker = CreateTestEnemy(registry, 500.0f, 0.0f);
  registry.get<CombatStats>(player).resistances[1] = 0.0f;
  registry.get<CombatStats>(player).resistances[3] = 0.0f;

  constexpr float kBase = 100.0f;
  // 路径经过自身位置
  element_path::Spawn(registry,
                      MakeSegment(player, Tag::Fire, {-50.0f, 0.0f}, {50.0f, 0.0f}));

  const float dmg_shielded =
      ApplyElementDamage(registry, attacker, player, 1u, Tag::Fire, kBase);
  CHECK(dmg_shielded == doctest::Approx(kBase * (1.0f - 0.15f)));

  // 元素不匹配（火路径受闪电伤害）：不享受护体
  const float dmg_lightning =
      ApplyElementDamage(registry, attacker, player, 1u, Tag::Lightning, kBase);
  CHECK(dmg_lightning == doctest::Approx(kBase));

  // 未站在路径上：施法者移到路径外后不享受护体
  registry.get<Position>(player).x = 1000.0f;
  const float dmg_outside =
      ApplyElementDamage(registry, attacker, player, 1u, Tag::Fire, kBase);
  CHECK(dmg_outside == doctest::Approx(kBase));
}

// 用例7：872/873 电弧 — 电弧伤害随 arc_freq_mult 提升而增加；Detonate 触发范围爆发+眩晕。
TEST_CASE("BladeBoomerangElementPath.Node872ArcFrequency [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  const auto run_arcs = [](float arc_freq_mult) {
    entt::registry registry;
    auto player = CreateTestPlayer(registry);
    auto enemy = CreateTestEnemy(registry, 100.0f, 0.0f, 100000.0f);
    element_path::Spawn(
        registry, MakeSegment(player, Tag::Lightning, {0.0f, 0.0f}, {200.0f, 0.0f},
                              0.0f, 0.0f, arc_freq_mult));
    const float before = HpOf(registry, enemy);
    for (int i = 0; i < 180; ++i) { // 3 秒
      element_path::Update(registry, 1.0f / 60.0f);
    }
    return before - HpOf(registry, enemy);
  };

  const float loss_slow = run_arcs(1.0f);
  const float loss_fast = run_arcs(3.0f);

  CHECK(loss_slow > 0.0f);                  // 电弧确实造成伤害
  CHECK(loss_fast > loss_slow * 1.5f);      // 电弧频率越高，总伤害越高

  // Detonate（873 接刃电磁爆发）：范围内敌人受伤并获得短眩晕
  entt::registry registry;
  auto player = CreateTestPlayer(registry);
  auto enemy = CreateTestEnemy(registry, 50.0f, 0.0f, 100000.0f);
  const float before = HpOf(registry, enemy);
  element_path::Detonate(registry, player, Tag::Lightning, {50.0f, 0.0f}, 100.0f);
  CHECK(HpOf(registry, enemy) < before);
  CHECK(FindEffect(registry, enemy, "ElementPathBurstStun") != nullptr);
}

// 用例8：伤害管道冒烟 — payload_context 空 base_pool 走旧结算路径且数值可观测，
//        同时确保战斗域伤害钩子被链接、注册（电弧用例依赖该注册）。
TEST_CASE("BladeBoomerangElementPath.PipelineSmoke [Functional]") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  entt::registry registry;

  auto player = CreateTestPlayer(registry);
  auto enemy = CreateTestEnemy(registry, 50.0f, 0.0f);

  DamageRequest req{};
  req.attacker = player;
  req.defender = enemy;
  req.skill_id = kSkillId;
  req.additional_tags = Tag::Hit;
  DamagePayloadContext ctx{};
  ctx.base_damage_min = 100.0f;
  ctx.base_damage_max = 100.0f;
  ctx.crit_chance = 0.0f;
  ctx.crit_multiplier = 1.0f;
  ctx.more_damage = 1.0f;
  ctx.effective_tags = Tag::Fire;
  ctx.source_skill_id = kSkillId;
  req.payload_context = ctx;

  const DamageResult result = DamagePipeline::Calculate(registry, req);
  CHECK(result.total_damage > 0.0f);
}

} // namespace NoMoreDay
