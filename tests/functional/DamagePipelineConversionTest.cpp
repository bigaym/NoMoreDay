#include "TestCommon.hpp"
#include "doctest.h"
#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <tuple>
#include <vector>

namespace NoMoreDay {
namespace {

// 追加一个伤害修正到攻击者的全局修正组件（首次调用时创建组件）。
void PushGlobalMod(entt::registry &registry, entt::entity attacker,
                   DamageModifier mod) {
  if (auto *existing = registry.try_get<GlobalModifierComponent>(attacker)) {
    existing->modifiers.push_back(mod);
  } else {
    auto &created = registry.emplace<GlobalModifierComponent>(attacker);
    created.modifiers.push_back(mod);
  }
}

// 构造一个转换/额外收益修正。
DamageModifier MakeConversion(Tag src, Tag dst, ModifierType type, float value) {
  DamageModifier mod;
  mod.source_tag = src;
  mod.target_tag = dst;
  mod.type = type;
  mod.value = value;
  return mod;
}

} // namespace

TEST_CASE("[Functional] DamagePipeline - Iterative Conversion Chain") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();

  registry.emplace<CombatStats>(attacker);
  auto &attStats = registry.get<CombatStats>(attacker);
  attStats.min_weapon_damage = 0.0f;
  attStats.max_weapon_damage = 0.0f;
  attStats.crit_chance = 0.0f;

  auto &defStats = registry.emplace<CombatStats>(defender);
  defStats.cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f; // Physical

  SUBCASE("Physical -> Lightning (single-pass, no cascade)") {
    auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    DamageModifier conv1;
    conv1.source_tag = Tag::Physical;
    conv1.target_tag = Tag::Lightning;
    conv1.type = ModifierType::Convert;
    conv1.value = 0.5f;
    mods.modifiers.push_back(conv1);

    DamageModifier conv2;
    conv2.source_tag = Tag::Lightning;
    conv2.target_tag = Tag::Cold;
    conv2.type = ModifierType::Convert;
    conv2.value = 1.0f;
    mods.modifiers.push_back(conv2);

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    // 快照式单遍：规则只作用于转换前的初始元素池。初始 Lightning=0，故
    // Lightning->Cold 规则不参与本次结算，不产生级联。
    //   100 Physical × retained(1-0.5) = 50 Physical
    //   100 Physical × 0.5             = 50 Lightning
    //   Cold                            = 0
    CHECK(res.final_pool.values[0] == doctest::Approx(50.0f));

    CHECK(res.final_pool.values[3] ==
          doctest::Approx(50.0f)); // No cascade: Lightning stays

    CHECK(res.final_pool.values[2] == doctest::Approx(0.0f)); // No Cold
  }

    

      SUBCASE("Bidirectional Conversion (S2)") {

        auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    

        // S2：Cold <-> Fire 双向转换均合法，不再有单向顺序限制。

        DamageModifier conv1;

        conv1.source_tag = Tag::Cold;

        conv1.target_tag = Tag::Fire;

        conv1.type = ModifierType::Convert;

        conv1.value = 1.0f;

        mods.modifiers.push_back(conv1);

    

        // Fire -> Cold 同为合法方向

        DamageModifier conv2;

        conv2.source_tag = Tag::Fire;

        conv2.target_tag = Tag::Cold;

        conv2.type = ModifierType::Convert;

        conv2.value = 1.0f;

        mods.modifiers.push_back(conv2);

    

        DamagePool coldPool;

        coldPool.values[2] = 100.0f; // Cold

    

        auto res = DamagePipeline::Calculate(registry, attacker, defender, 0,

                                             coldPool, Tag::Hit, entt::null, true);

    

        // 初始 Cold 100 → Fire 100（单遍：规则只作用于转换前的初始元素池）

        CHECK(res.final_pool.values[2] == doctest::Approx(0.0f));

        CHECK(res.final_pool.values[1] == doctest::Approx(100.0f)); 

    

        // 初始 Fire 100 → Cold 100：逆向转换不再被丢弃（修复前会被 IsValidConversion 拦截）

    
    DamagePool firePool;
    firePool.values[1] = 100.0f; // Fire
    auto res2 = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                          firePool, Tag::Hit, entt::null, true);
    CHECK(res2.final_pool.values[1] == doctest::Approx(0.0f));
    CHECK(res2.final_pool.values[2] == doctest::Approx(100.0f));
  }
}

TEST_CASE("[Functional] DamagePipeline - Unified More Multipliers") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();

  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender).cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f;

  SUBCASE("Multiple More multipliers application") {
    auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    // 20% More Global
    DamageModifier more1;
    more1.type = ModifierType::More;
    more1.value = 0.2f;
    more1.source_tag = Tag::None;
    mods.modifiers.push_back(more1);

    // 50% More Physical
    DamageModifier more2;
    more2.type = ModifierType::More;
    more2.value = 0.5f;
    more2.source_tag = Tag::Physical;
    mods.modifiers.push_back(more2);

    // Calculation: 100 * 1.2 * 1.5 = 180
    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);
    CHECK(res.total_damage == doctest::Approx(180.0f));
  }
}

// P2-1 转换守恒：部分转换后，各元素之和必须等于转换前总量。
TEST_CASE("[Functional] DamagePipeline P2 - Conversion Conservation") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender).cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f; // Physical

  SUBCASE("Partial conversion conserves total") {
    // 30% Physical -> Fire（合法：位序 Physical=0 < Fire=3）。
    // 推导：retained = max(0, 1-0.3) = 0.7 → Physical 100*0.7 = 70；
    //       Fire = 100*0.3 = 30；总和仍为 100。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 0.3f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(70.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(30.0f));
    CHECK(res.total_damage == doctest::Approx(100.0f));
  }

  SUBCASE("Convert sum over 100% scales proportionally") {
    // 80% Physical -> Fire 与 80% Physical -> Cold 同时存在。
    // 推导：sum = 1.6 > 1 → scale = 1/1.6 = 0.625；
    //       retained = max(0, 1-1.6) = 0；
    //       Fire = 100*0.8*0.625 = 50，Cold = 100*0.8*0.625 = 50，总和 100。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 0.8f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Cold, ModifierType::Convert,
                                 0.8f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(0.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[2] == doctest::Approx(50.0f));
    CHECK(res.total_damage == doctest::Approx(100.0f));
  }

  SUBCASE("GainExtra is uncapped and does not deduct source") {
    // 20% Convert Physical -> Fire，另外 50% GainExtra Physical -> Cold。
    // 推导：Physical 保留 100*(1-0.2) = 80（GainExtra 不从源扣除）；
    //       Fire = 100*0.2 = 20；Cold = 100*0.5 = 50；总和 150。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 0.2f));
    PushGlobalMod(
        registry, attacker,
        MakeConversion(Tag::Physical, Tag::Cold, ModifierType::GainExtra, 0.5f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(80.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(20.0f));
    CHECK(res.final_pool.values[2] == doctest::Approx(50.0f));
    CHECK(res.total_damage == doctest::Approx(150.0f));
  }
}

// P2-2 标签剥离：转换产物只保留目标元素标签，不再享受源元素 More 加成。
TEST_CASE("[Functional] DamagePipeline P2 - Tag Strip Prevents Double Dip") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender).cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f; // Physical

  SUBCASE("Source-element More no longer applies after conversion") {
    // 100% Physical -> Fire；同时存在 Physical +100% More。
    // 推导：产物实例标签为 Fire|Hit，已剥离 Physical，故 Physical More 不匹配，
    //       最终 Fire = 100（若未剥离则会被放大到 200，即双重收益）。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 1.0f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::None, ModifierType::More,
                                 1.0f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(0.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(100.0f));
    CHECK(res.total_damage == doctest::Approx(100.0f));
  }

  SUBCASE("Target-element More applies to converted product") {
    // 对照用例：Fire +100% More 应作用于转换后的 Fire 产物。
    // 推导：Fire = 100 * 2 = 200。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 1.0f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Fire, Tag::None, ModifierType::More, 1.0f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[1] == doctest::Approx(200.0f));
    CHECK(res.total_damage == doctest::Approx(200.0f));
  }
}

// P2-3 多元素分池：同一技能内不同源元素各自独立转换，互不影响。
TEST_CASE("[Functional] DamagePipeline P2 - Multi Element Independent Pools") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender).cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f; // Physical
  base.values[3] = 100.0f; // Lightning

  SUBCASE("Two source pools convert independently") {
    // Physical -> Fire 50%（位序 0<3 合法）；Lightning -> Cold 50%（位序 1<2 合法）。
    // 推导：Physical 保留 50、Fire 50；Lightning 保留 50、Cold 50；总和 200。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 0.5f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Lightning, Tag::Cold, ModifierType::Convert,
                                 0.5f));

    // 使用不带元素标签的技能 ID，避免技能自身标签干扰元素池独立性的验证。
    auto res = DamagePipeline::Calculate(registry, attacker, defender, 999999,
                                         base, Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[2] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[3] == doctest::Approx(50.0f));
    CHECK(res.total_damage == doctest::Approx(200.0f));
  }

  SUBCASE("Physical More does not leak into other element pools") {
    // 追加 Physical +100% More：仅作用于保留的 Physical（含 Physical 标签），
    // 不得作用于 Fire/Cold/Lightning 产物。
    // 推导：Physical 50*2 = 100；Fire 50；Lightning 50；Cold 50；总和 250。
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire, ModifierType::Convert,
                                 0.5f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Lightning, Tag::Cold, ModifierType::Convert,
                                 0.5f));
    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::None, ModifierType::More,
                                 1.0f));

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 999999,
                                         base, Tag::Hit, entt::null, true);

    CHECK(res.final_pool.values[0] == doctest::Approx(100.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[2] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[3] == doctest::Approx(50.0f));
    CHECK(res.total_damage == doctest::Approx(250.0f));
  }
}

// P2-4 暴击单位归一：GetStatWithTags 返回分数制，单目标与批量路径一致。
TEST_CASE("[Functional] DamagePipeline P2 - Crit Unit Fraction Consistency") {
  TestSetupScope scope;

  SUBCASE("GetStatWithTags returns fraction for CritChance") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<Position>(entity, 0.0f, 0.0f);
    registry.emplace<CombatStats>(entity).crit_chance = 0.25f; // 25%

    // 边界归一：CombatStats 以分数存储，GetStatWithTags 必须返回分数 0.25。
    const float crit = StatsSystem::GetStatWithTags(
        registry, entity, StatType::CritChance, Tag::None, 0, entt::null);
    CHECK(crit == doctest::Approx(0.25f));
  }

  SUBCASE("Single simulation matches expected crit multiplier") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    auto &stats = registry.emplace<CombatStats>(attacker);
    stats.crit_chance = 0.5f;  // 分数制 50%
    stats.crit_damage = 2.0f;  // 暴击倍率 2.0
    registry.emplace<CombatStats>(defender).cached_area_level = 1;

    DamagePool base;
    base.values[0] = 10.0f;

    // 推导：期望暴击系数 E = 1 + 0.5*(2.0-1) = 1.5 → 10*1.5 = 15。
    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit, entt::null, true);
    CHECK(res.total_damage == doctest::Approx(15.0f));
  }

  SUBCASE("Single and batch agree under guaranteed crit") {
    entt::registry registry;
    auto attacker = registry.create();
    registry.emplace<Position>(attacker, 0.0f, 0.0f);
    auto &stats = registry.emplace<CombatStats>(attacker);
    stats.crit_chance = 1.0f; // 分数制 100%，批量路径必然暴击，结果确定
    stats.crit_damage = 2.0f;
    stats.cached_area_level = 1;

    auto defender = registry.create();
    registry.emplace<Position>(defender, 10.0f, 0.0f);
    registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);
    registry.emplace<CombatStats>(defender).cached_area_level = 1;

    DamagePool base;
    base.values[0] = 10.0f;

    // 推导：分数制 1.0 = 必暴；单目标模拟 E = 1 + 1.0*(2.0-1) = 2.0 → 20。
    auto single = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                            Tag::Hit, entt::null, true);
    CHECK(single.total_damage == doctest::Approx(20.0f));

    // 批量路径：快照先除回期望暴击系数，再按目标掷暴击；必暴下结果与单目标一致。
    const float hp_before = registry.get<HealthComponent>(defender).current;
    const std::vector<entt::entity> defenders = {defender};
    DamagePipeline::CalculateBatch(registry, attacker, defenders, 0, base,
                                   Tag::Hit, entt::null);
    const float batch_damage =
        hp_before - registry.get<HealthComponent>(defender).current;

    CHECK(batch_damage == doctest::Approx(single.total_damage));
  }
}

// P4 审查修复：元素转换后，战斗事件标签必须反映实际结算元素（resolved），
// 同时保留 Melee/Projectile/Hit 等动作与机制位。此前事件直接使用转换前的
// combined_hit_tags，导致 100% 物转火后事件仍带 Physical 而不带 Fire。
TEST_CASE("[Functional] DamagePipeline P4 - event tags follow resolved element") {
  TestSetupScope scope;

  auto run_case = [](float convert_pct) {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
    registry.emplace<CombatStats>(defender).cached_area_level = 1;
    registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);

    DamagePool base;
    base.values[0] = 100.0f; // Physical

    PushGlobalMod(registry, attacker,
                  MakeConversion(Tag::Physical, Tag::Fire,
                                 ModifierType::Convert, convert_pct));

    CombatEvent skill_hit{};
    bool has_skill_hit = false;
    const uint32_t handler = CombatEventDispatcher::Register(
        CombatEventType::OnSkillHit,
        [&](entt::registry &, const CombatEvent &evt) {
          skill_hit = evt;
          has_skill_hit = true;
        },
        3000);

    DamageRequest request;
    request.attacker = attacker;
    request.defender = defender;
    request.skill_id = 0;
    request.base_pool = base;
    request.additional_tags = Tag::Hit | Tag::Melee;
    const DamageResult result = DamagePipeline::Calculate(registry, request);

    CombatEventDispatcher::Unregister(CombatEventType::OnSkillHit, handler);
    return std::make_tuple(result, skill_hit, has_skill_hit);
  };

  SUBCASE("100% conversion replaces the element bits only") {
    const auto [result, evt, has_evt] = run_case(1.0f);
    REQUIRE(has_evt);
    CHECK(result.resolved_element_tags == Tag::Fire);
    CHECK(HasTag(evt.tags, Tag::Fire));
    CHECK_FALSE(HasTag(evt.tags, Tag::Physical));
    // 动作/机制位必须保留，否则事件类型与 Proc 判定会失效。
    CHECK(HasTag(evt.tags, Tag::Hit));
    CHECK(HasTag(evt.tags, Tag::Melee));
  }

  SUBCASE("partial conversion keeps both elements") {
    const auto [result, evt, has_evt] = run_case(0.5f);
    REQUIRE(has_evt);
    CHECK(result.resolved_element_tags == (Tag::Physical | Tag::Fire));
    CHECK(HasTag(evt.tags, Tag::Physical));
    CHECK(HasTag(evt.tags, Tag::Fire));
    CHECK(HasTag(evt.tags, Tag::Hit));
    CHECK(HasTag(evt.tags, Tag::Melee));
  }
}

// P4 审查修复：终态倍率必须覆盖整个伤害池（含 Void 位），否则
// total_damage 被缩放而 final_pool 未同步，破坏 total == sum(final_pool) 守恒。
TEST_CASE("[Functional] DamagePipeline P4 - final pool conservation under suppressor") {
  TestSetupScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  auto defender = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<Position>(defender, 100.0f, 0.0f);
  auto &def_stats = registry.emplace<CombatStats>(defender);
  def_stats.cached_area_level = 1;
  def_stats.dodge_chance = 0.0f;
  def_stats.block_chance = 0.0f;
  auto &suppressor = registry.emplace<SuppressorComponent>(defender);
  suppressor.threshold = 10.0f;
  suppressor.damageReduction = 0.5f;

  DamagePool base;
  base.values[6] = 100.0f; // Void（位 6，修复前 i<6 的循环不会缩放该位）

  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool = base;
  request.additional_tags = Tag::Hit;
  request.skip_mitigation = true;

  const DamageResult result = DamagePipeline::Calculate(registry, request);

  float pool_sum = 0.0f;
  for (float value : result.final_pool.values) {
    pool_sum += value;
  }
  CHECK(result.total_damage == doctest::Approx(50.0f));
  CHECK(result.final_pool.values[6] == doctest::Approx(result.total_damage));
  CHECK(pool_sum == doctest::Approx(result.total_damage));
}

// P4 审查修复补充：payload 显式声明的元素（如专精 170 火焰/172 冰霜）与伤害元素
// 语义正交，重映时不能把 payload 元素位当作"待替换的转换前元素"抹掉，否则
// DoHit 的元素异常（Ignite/FrostSlow）失效。回归 SkillSpecializationBakerTests。
TEST_CASE("[Functional] DamagePipeline P4 - explicit payload element tags survive remap") {
  TestSetupScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  auto defender = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender).cached_area_level = 1;
  registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);

  DamagePool base;
  base.values[0] = 100.0f; // Physical 伤害本体

  CombatEvent skill_hit{};
  bool has_skill_hit = false;
  const uint32_t handler = CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [&](entt::registry &, const CombatEvent &evt) {
        skill_hit = evt;
        has_skill_hit = true;
      },
      3000);

  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool = base;
  request.additional_tags = Tag::Hit | Tag::Melee;
  request.payload_context = DamagePayloadContext{};
  request.payload_context->effective_tags = Tag::Fire;

  const DamageResult result = DamagePipeline::Calculate(registry, request);
  CombatEventDispatcher::Unregister(CombatEventType::OnSkillHit, handler);

  REQUIRE(has_skill_hit);
  CHECK(result.resolved_element_tags == Tag::Physical);
  CHECK(HasTag(skill_hit.tags, Tag::Physical)); // 实际伤害元素
  CHECK(HasTag(skill_hit.tags, Tag::Fire));     // payload 显式元素必须保留
  CHECK(HasTag(skill_hit.tags, Tag::Hit));
  CHECK(HasTag(skill_hit.tags, Tag::Melee));
}

} // namespace NoMoreDay
