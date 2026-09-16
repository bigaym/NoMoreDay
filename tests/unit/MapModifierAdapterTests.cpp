#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/WorldState.hpp"
#include "game/foundation/data/MapAffix.hpp"
#include "game/foundation/stats/AttributePipeline.hpp"
#include "game/systems/modifier/MapModifierAdapter.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"

#include <cstdint>

// 同进程内其它单测会向全局 ModifierRuntimeRegistry 注入合成数据，
// 这里用共享助手从构建产物强制重新加载，保证本文件用例消费真实生成数据。

TEST_CASE("[Unit] MapModifierAdapter - active map affixes produce expected enemy deltas") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 2.0f;
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 30.0f, 5, "test"});
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraDamage,
       NoMoreDay::MapAffixCategory::Debuff, 50.0f, 5, "test"});
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_Fast,
       NoMoreDay::MapAffixCategory::Debuff, 20.0f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(143.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            10.0f, static_cast<uint32_t>(NoMoreDay::StatType::PhysicalDamage),
            delta) == doctest::Approx(15.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            40.0f, static_cast<uint32_t>(NoMoreDay::StatType::MoveSpeed),
            delta) == doctest::Approx(48.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - inactive map state yields empty delta") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::ActiveDimensionalState state;
  state.isActive = false;
  state.resonance.totalEnemyDensity = 99.0f;
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 30.0f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(100.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - runtime affix value overrides json template intensity") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 0.0f;
  // JSON 模板中的 valT1 为 20（百分比刻度）；运行时滚值故意取 50，
  // 若实现误用模板值则结果会是 120 而非 150。
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 50.0f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(150.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - rolled value drives intensity across tiers") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 按生产路径（MapAffixCalculator）用 CalculateValue 由 tier 得到滚值，
  // 再喂给适配器，验证 tier 确实通过滚值影响强度。
  constexpr NoMoreDay::MapAffixType kType = NoMoreDay::MapAffixType::Enemy_ExtraDamage;
  const float t1Value = NoMoreDay::MapAffixRegistry::CalculateValue(kType, 1);
  const float t10Value = NoMoreDay::MapAffixRegistry::CalculateValue(kType, 10);
  // 滚值是 0~100 刻度的百分比，适配器归一化为乘算系数 (0.01) 后注入求值器。
  const float t1Ratio = t1Value * 0.01f;
  const float t10Ratio = t10Value * 0.01f;
  REQUIRE(t10Value > t1Value);

  auto makeState = [](const float rolledValue, const int tier) {
    NoMoreDay::ActiveDimensionalState state;
    state.isActive = true;
    state.resonance.totalEnemyDensity = 0.0f;
    state.explicitAffixes.push_back(
        {kType, NoMoreDay::MapAffixCategory::Debuff, rolledValue, tier, "test"});
    return state;
  };

  const auto stat = static_cast<uint32_t>(NoMoreDay::StatType::PhysicalDamage);
  const auto lowTier =
      NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(makeState(t1Value, 1));
  const auto highTier =
      NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(makeState(t10Value, 10));

  // 适配器把归一化后的滚值作为 ADD_STAT_PERCENT_MULT 的乘算系数，
  // 因此结果必须与 CalculateValue 输出的真实强度严格一致。
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, stat, lowTier) ==
        doctest::Approx(100.0f * (1.0f + t1Ratio)));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, stat, highTier) ==
        doctest::Approx(100.0f * (1.0f + t10Ratio)));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, stat, highTier) >
        NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, stat, lowTier));

  // 适配器只消费 value、不读 tier：同一滚值配不同 tier 应得到同样强度。
  const auto sameValueOtherTier =
      NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(makeState(t1Value, 9));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(100.0f, stat, sameValueOtherTier) ==
        doctest::Approx(100.0f * (1.0f + t1Ratio)));
}

TEST_CASE("[Unit] MapModifierAdapter - resonance density multiplier scales enemy health") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 4.0f;

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  // 4.0 * 0.05 = 0.2 -> 敌方最大生命 *1.2
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(120.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - affixes without combat mapping are ignored") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 0.0f;
  // DropRarity 的 combatStat 为 StatType::Count 哨兵，不应产生敌方属性变化。
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::DropRarity,
       NoMoreDay::MapAffixCategory::Buff, 50.0f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  CHECK(delta.flat.empty());
  CHECK(delta.percent_add.empty());
  CHECK(delta.percent_mult.empty());
}

TEST_CASE("[Unit] MapModifierAdapter - duplicate affix type stacks independently rolled values") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 地图碎片各产一条词缀，同一 MapAffixType 可合法出现多次且滚值不同。
  // 旧实现按 record_id 只取首条 override，会得到 (1+v_first)^N 的静默错误；
  // 正确语义为各次滚值依次连乘。
  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 0.0f;
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 30.0f, 5, "shard-a"});
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 50.0f, 7, "shard-b"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  // (1 + 0.30) * (1 + 0.50) = 1.95，而非只取首条时的 1.30^2 = 1.69
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(195.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - repeated affix occurrences apply once per occurrence") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 同一词缀出现 3 次、滚值一致 0.20 -> 1.2^3 = 1.728，
  // 锁死“N 次出现就施加 N 次”的请求模型。
  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 0.0f;
  for (int i = 0; i < 3; ++i) {
    state.explicitAffixes.push_back(
        {NoMoreDay::MapAffixType::Enemy_Fast,
         NoMoreDay::MapAffixCategory::Debuff, 20.0f, 5, "shard"});
  }

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            40.0f, static_cast<uint32_t>(NoMoreDay::StatType::MoveSpeed),
            delta) == doctest::Approx(40.0f * 1.2f * 1.2f * 1.2f));
}

TEST_CASE("[Unit] MapModifierAdapter - combatStat to record mapping stays consistent") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 守护常量漂移：凡 combatStat != Count 的词缀，其 record_id 必须存在，
  // 且记录中 ADD_STAT_PERCENT_MULT 的目标属性必须等于 combatStat。
  // 同时覆盖设计中提到的“手改 header + 陈旧 binary”目标属性错配隐患。
  auto &registry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const uint32_t affixCount =
      static_cast<uint32_t>(NoMoreDay::MapAffixType::Count);
  bool anyMapped = false;

  for (uint32_t typeIndex = 0; typeIndex < affixCount; ++typeIndex) {
    const auto type = static_cast<NoMoreDay::MapAffixType>(typeIndex);
    const NoMoreDay::StatType combatStat =
        NoMoreDay::MapAffixRegistry::GetDef(type).combatStat;
    if (combatStat == NoMoreDay::StatType::Count) {
      continue;
    }
    anyMapped = true;

    const uint32_t recordId =
        NoMoreDay::MapModifierIds::kMapRecordIdBase + typeIndex;
    const NoMoreDay::ModifierRuntimeRecord *record =
        registry.FindRecordById(recordId);
    REQUIRE_MESSAGE(record != nullptr, "missing map record "
                                           << recordId << " for affix index "
                                           << typeIndex);

    bool foundMultOp = false;
    for (const auto &op : registry.GetOps(*record)) {
      if (static_cast<NoMoreDay::ModifierOpCode>(op.opcode) ==
          NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT) {
        foundMultOp = true;
        CHECK_MESSAGE(
            op.param_u32 == static_cast<uint32_t>(combatStat),
            "record " << recordId << " op target "
                      << static_cast<int>(op.param_u32) << " != combatStat "
                      << static_cast<int>(combatStat));
      }
    }
    CHECK_MESSAGE(foundMultOp,
                  "record " << recordId << " has no ADD_STAT_PERCENT_MULT op");
  }
  CHECK(anyMapped);
}

TEST_CASE("[Unit] MapModifierAdapter - resonance record exists with MaxHealth multiplier") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 共鸣记录不属于任何 MapAffixType 枚举，需单独守护其存在性与目标属性，
  // 避免常量漂移导致共鸣生命加成静默失效。
  auto &registry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const NoMoreDay::ModifierRuntimeRecord *record =
      registry.FindRecordById(NoMoreDay::MapModifierIds::kMapResonanceRecordId);
  REQUIRE(record != nullptr);

  bool foundMultOp = false;
  for (const auto &op : registry.GetOps(*record)) {
    if (static_cast<NoMoreDay::ModifierOpCode>(op.opcode) ==
        NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT) {
      foundMultOp = true;
      CHECK(op.param_u32 ==
            static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth));
    }
  }
  CHECK(foundMultOp);
}

TEST_CASE("[Unit] MapModifierAdapter - crit and resistance affixes reach enemy "
          "CombatStats") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 用 roll 出的 T1 滚值（百分比刻度）施加词缀，验证 AttributePipeline
  // 的百分点路径真实落到 CombatStats，而不只是产生 Delta。
  auto buildEnemy = [](entt::registry &registry, const bool withModifiers) {
    const auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<NoMoreDay::CombatStats>(enemy);
    registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                          EnemyArchetype::FODDER);
    registry.get<EnemyStateComponent>(enemy).level = 1;

    if (withModifiers) {
      auto &mapState =
          registry.ctx().emplace<NoMoreDay::ActiveDimensionalState>();
      mapState.isActive = true;
      mapState.resonance.totalEnemyDensity = 0.0f;
      mapState.explicitAffixes.push_back(
          {NoMoreDay::MapAffixType::Enemy_ResistPhys,
           NoMoreDay::MapAffixCategory::Debuff,
           NoMoreDay::MapAffixRegistry::CalculateValue(
               NoMoreDay::MapAffixType::Enemy_ResistPhys, 1),
           1, "test"});
      mapState.explicitAffixes.push_back(
          {NoMoreDay::MapAffixType::Enemy_CritChance,
           NoMoreDay::MapAffixCategory::Debuff,
           NoMoreDay::MapAffixRegistry::CalculateValue(
               NoMoreDay::MapAffixType::Enemy_CritChance, 1),
           1, "test"});
    }

    NoMoreDay::AttributePipeline::Calculate(registry, enemy);
    return enemy;
  };

  entt::registry baseRegistry;
  const auto baseEnemy = buildEnemy(baseRegistry, false);
  const auto &baseStats = baseRegistry.get<NoMoreDay::CombatStats>(baseEnemy);

  entt::registry modifiedRegistry;
  const auto modifiedEnemy = buildEnemy(modifiedRegistry, true);
  const auto &modifiedStats =
      modifiedRegistry.get<NoMoreDay::CombatStats>(modifiedEnemy);

  constexpr int kPhysical = static_cast<int>(NoMoreDay::DamageType::Physical);
  // UNDEAD 无原生物理抗性，T1=25 -> +0.25 百分点（Cap::RESISTANCE=0.75 内）。
  CHECK(modifiedStats.resistances[kPhysical] -
            baseStats.resistances[kPhysical] ==
        doctest::Approx(0.25f));
  // 暴击率：基础 0.05，T1=20 -> +20 百分点 = 0.25。
  CHECK(baseStats.crit_chance == doctest::Approx(0.05f));
  CHECK(modifiedStats.crit_chance == doctest::Approx(0.25f));
}
