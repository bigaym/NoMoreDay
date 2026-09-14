#include "TestCommon.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/damage/DamageTypes.hpp"

#include <vector>

namespace NoMoreDay {
namespace {

// 伤害元素存在两条位序，仅 4/5 位 (Shadow/Poison) 互换：
//   - DamagePool / Tag 位序：池索引 4 = Shadow、5 = Poison；
//   - DamageType / CombatStats.resistances[] (StatType 序)：索引 4 = Poison、
//     5 = Shadow。
// 本文件锁定“以池索引读取 DamageType 序抗性数组”这一映射缺陷的回归。
constexpr size_t kResistPoison = static_cast<size_t>(DamageType::Poison); // 4
constexpr size_t kResistShadow = static_cast<size_t>(DamageType::Shadow); // 5

// 攻方：关闭暴击随机性，只保留乘区基线，避免干扰抗性结算断言。
entt::entity MakeAttacker(entt::registry &registry) {
  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.armor_pen = 0.0f;
  return attacker;
}

// 守方：Poison/Shadow 抗性按 StatType 序显式设置，护甲/减伤置零。
entt::entity MakeDefender(entt::registry &registry, float poison_res,
                          float shadow_res) {
  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 1.0e6f, 1.0e6f);
  auto &stats = registry.emplace<CombatStats>(defender);
  stats.resistances[kResistPoison] = poison_res;
  stats.resistances[kResistShadow] = shadow_res;
  stats.armor = 0.0f;
  stats.damage_reduction = 0.0f;
  stats.crit_chance = 0.0f;
  return defender;
}

DamagePool ElementPool(Tag element, float amount) {
  DamagePool pool;
  pool.Add(element, amount);
  return pool;
}

} // namespace

// 回归：单目标生产路径 (Calculate -> DamageMitigationService::Apply) 必须以
// 池索引映射后的 DamageType 读取 resistances[]。旧实现直取 resistances[j]，
// 纯 Shadow 会读到 Poison 抗性、纯 Poison 读到 Shadow 抗性，两者对调。
TEST_CASE("[Unit] DamageElementIndex - single target Shadow/Poison resist "
          "mapping") {
  LoggerScope scope;
  entt::registry registry;
  auto attacker = MakeAttacker(registry);
  // Poison 0.8 高于 RESISTANCE_MAX(0.75)，结算时被钳制为 0.75。
  auto defender = MakeDefender(registry, /*poison=*/0.8f, /*shadow=*/0.1f);

  const DamageResult shadow = DamagePipeline::Calculate(
      registry, attacker, defender, 0, ElementPool(Tag::Shadow, 100.0f),
      Tag::Melee, entt::null);
  const DamageResult poison = DamagePipeline::Calculate(
      registry, attacker, defender, 0, ElementPool(Tag::Poison, 100.0f),
      Tag::Melee, entt::null);

  // 纯 Shadow 只吃 Shadow 抗性 (0.1) => 100 * 0.9 = 90。
  CHECK(shadow.total_damage == doctest::Approx(90.0f));
  // 纯 Poison 吃 Poison 抗性 (0.8 -> 上限 0.75) => 100 * 0.25 = 25。
  CHECK(poison.total_damage == doctest::Approx(25.0f));
}

// 回归：批量标量/SIMD 两内核都必须做同一池索引映射。N=8 触发 SIMD，强制标量
// 路径作为对照。旧实现两内核均直取 resistances[j]，纯 Shadow 得 25、纯 Poison
// 得 90（与正确值对调），本用例会失败。
TEST_CASE("[Unit] DamageElementIndex - batch kernels Shadow/Poison resist "
          "mapping") {
  LoggerScope scope;

  constexpr size_t kCount = 8; // >= 4，自动分流走 SIMD
  for (const bool is_shadow : {true, false}) {
    const Tag element = is_shadow ? Tag::Shadow : Tag::Poison;
    const float expected = is_shadow ? 90.0f : 25.0f;
    for (const bool force_scalar : {true, false}) {
      CAPTURE(is_shadow);
      CAPTURE(force_scalar);
      entt::registry registry;
      auto attacker = MakeAttacker(registry);
      std::vector<entt::entity> defenders;
      defenders.reserve(kCount);
      for (size_t i = 0; i < kCount; ++i) {
        defenders.push_back(MakeDefender(registry, 0.8f, 0.1f));
      }

      std::vector<float> before;
      before.reserve(kCount);
      for (const auto defender : defenders) {
        before.push_back(registry.get<HealthComponent>(defender).current);
      }

      DamagePipeline::ResetBatchKernelStats();
      DamagePipeline::SetForceScalarKernelForTests(force_scalar);
      DamagePipeline::CalculateBatch(registry, attacker, defenders, 0,
                                     ElementPool(element, 100.0f), Tag::Melee,
                                     entt::null);
      const DamagePipeline::BatchKernelStats stats =
          DamagePipeline::GetBatchKernelStats();
      DamagePipeline::SetForceScalarKernelForTests(false);

      // 对照组必须真正覆盖对应内核，避免用例空跑。
      if (force_scalar) {
        CHECK(stats.scalar_targets == kCount);
        CHECK(stats.simd_blocks == 0u);
      } else {
        CHECK(stats.simd_blocks > 0u);
      }

      for (size_t i = 0; i < kCount; ++i) {
        const float loss =
            before[i] - registry.get<HealthComponent>(defenders[i]).current;
        CHECK(loss == doctest::Approx(expected));
      }
    }
  }
}

// m1 回归：crit_chance 快照为 1.0 时批量两内核必须无条件暴击。旧实现依赖
// GetFloat01() < 1.0，存在极小概率不暴；现以 >= 1.0 短路保证确定性。
TEST_CASE("[Unit] DamageElementIndex - batch guaranteed crit at chance 1.0") {
  LoggerScope scope;

  constexpr size_t kCount = 8;
  const float base = 100.0f;
  const float crit_damage = 2.0f; // 暴击后 200

  for (const bool force_scalar : {true, false}) {
    CAPTURE(force_scalar);
    entt::registry registry;
    auto attacker = registry.create();
    registry.emplace<Position>(attacker, 0.0f, 0.0f);
    auto &attacker_stats = registry.emplace<CombatStats>(attacker);
    attacker_stats.crit_chance = 1.0f;
    attacker_stats.crit_damage = crit_damage;
    attacker_stats.armor_pen = 0.0f;

    std::vector<entt::entity> defenders;
    defenders.reserve(kCount);
    for (size_t i = 0; i < kCount; ++i) {
      // 无任何抗性/护甲/减伤，暴击结果应恰为 base * crit_damage。
      defenders.push_back(MakeDefender(registry, 0.0f, 0.0f));
    }

    std::vector<float> before;
    before.reserve(kCount);
    for (const auto defender : defenders) {
      before.push_back(registry.get<HealthComponent>(defender).current);
    }

    DamagePipeline::ResetBatchKernelStats();
    DamagePipeline::SetForceScalarKernelForTests(force_scalar);
    DamagePipeline::CalculateBatch(registry, attacker, defenders, 0,
                                   ElementPool(Tag::Physical, base), Tag::Melee,
                                   entt::null);
    DamagePipeline::SetForceScalarKernelForTests(false);

    for (size_t i = 0; i < kCount; ++i) {
      const float loss =
          before[i] - registry.get<HealthComponent>(defenders[i]).current;
      CHECK(loss == doctest::Approx(base * crit_damage));
    }
  }
}

} // namespace NoMoreDay
