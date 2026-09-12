#include "TestCommon.hpp"

#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

#include <taskflow/taskflow.hpp>

#include <array>
#include <cmath>
#include <vector>

namespace NoMoreDay {
namespace {

// P3-3 分流对照的确定性场景：守方属性逐目标变化，保证标量/SIMD 内核若存在
// 数值漂移一定会被目标间的异质性放大暴露。
void BuildScene(entt::registry &registry, entt::entity &attacker,
                std::vector<entt::entity> &defenders, size_t count,
                bool with_conditions, float crit_chance) {
  attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  auto &attacker_stats = registry.emplace<CombatStats>(attacker);
  attacker_stats.crit_chance = crit_chance;
  attacker_stats.crit_damage = 1.5f;
  attacker_stats.armor_pen = 25.0f;
  attacker_stats.cached_area_level = 1;

  defenders.clear();
  defenders.reserve(count);
  for (size_t k = 0; k < count; ++k) {
    const auto defender = registry.create();
    defenders.push_back(defender);
    registry.emplace<Position>(defender, static_cast<float>(k) * 10.0f, 0.0f);
    registry.emplace<HealthComponent>(defender, 1.0e6f, 1.0e6f);
    auto &stats = registry.emplace<CombatStats>(defender);
    stats.cached_area_level = 1 + static_cast<int>(k % 3);
    stats.armor = 40.0f + 37.0f * static_cast<float>(k % 7);
    stats.damage_reduction = 0.02f * static_cast<float>(k % 4);
    stats.resistances[0] = 0.05f * static_cast<float>(k % 5); // Physical
    stats.resistances[1] = 0.03f * static_cast<float>(k % 6); // Fire
    stats.crit_chance = 0.0f;
    stats.crit_damage = 1.5f;

    if (with_conditions) {
      auto &effects = registry.emplace<ActiveEffectsComponent>(defender);
      BuffEffect fate;
      fate.id = "FateMark";
      fate.kind = BuffKind::FateMark;
      fate.stacks = static_cast<int>(k % 4);
      effects.effects.push_back(fate);

      BuffEffect qi;
      qi.id = "QiBrand";
      qi.kind = BuffKind::QiBrand;
      qi.stacks = static_cast<int>(k % 3);
      effects.effects.push_back(qi);

      if (k % 2 == 0) {
        BuffEffect freeze;
        freeze.id = "Freeze";
        freeze.type = BuffType::Freeze;
        effects.effects.push_back(freeze);
      }
    }
  }
}

struct RunOutcome {
  std::vector<float> losses;
  DamagePipeline::BatchKernelStats stats;
};

// 用同一套确定性场景分别触发标量/自动（SIMD）内核，返回逐目标扣血量与
// 内核选择计数。executor 为空时走串行派发，非空时可驱动内部并行派发。
RunOutcome RunBatch(size_t count, bool force_scalar, tf::Executor *executor,
                    uint32_t skill_id, bool with_conditions, float crit_chance) {
  DamagePipeline::ResetBatchKernelStats();
  DamagePipeline::SetForceScalarKernelForTests(force_scalar);

  entt::registry registry;
  entt::entity attacker = entt::null;
  std::vector<entt::entity> defenders;
  BuildScene(registry, attacker, defenders, count, with_conditions, crit_chance);

  DamagePool pool;
  pool.Add(Tag::Physical, 120.0f);
  pool.Add(Tag::Fire, 45.0f);

  std::vector<float> before;
  before.reserve(count);
  for (const auto defender : defenders) {
    before.push_back(registry.get<HealthComponent>(defender).current);
  }

  DamagePipeline::CalculateBatch(registry, attacker, defenders, skill_id, pool,
                                 Tag::Melee, entt::null, executor);

  RunOutcome outcome;
  outcome.stats = DamagePipeline::GetBatchKernelStats();
  outcome.losses.reserve(count);
  for (size_t i = 0; i < defenders.size(); ++i) {
    outcome.losses.push_back(before[i] -
                             registry.get<HealthComponent>(defenders[i]).current);
  }

  DamagePipeline::SetForceScalarKernelForTests(false);
  return outcome;
}

// 保证测试失败中断时也不会把“强制标量”开关泄漏给后续用例。
class KernelModeGuard {
public:
  KernelModeGuard() {
    DamagePipeline::SetForceScalarKernelForTests(false);
    DamagePipeline::ResetBatchKernelStats();
  }
  ~KernelModeGuard() {
    DamagePipeline::SetForceScalarKernelForTests(false);
    DamagePipeline::ResetBatchKernelStats();
  }
};

void CheckLossesEqual(const std::vector<float> &scalar,
                      const std::vector<float> &simd) {
  REQUIRE(scalar.size() == simd.size());
  for (size_t i = 0; i < scalar.size(); ++i) {
    // xsimd 与标量使用同一组公式常量，运算顺序一致，仅允许 float 舍入容差。
    const bool close =
        std::fabs(simd[i] - scalar[i]) <=
        1e-3f + 1e-5f * std::fabs(scalar[i]);
    CHECK_MESSAGE(close, "target ", i, " scalar=", scalar[i],
                  " simd=", simd[i]);
  }
}

} // namespace

// P3-3：N=1..20（含原生批宽 ±1）标量内核与 xsimd 内核逐目标数值一致，
// 同时用内核计数确认路径选择阈值（N<4 标量，N>=4 SIMD）。
TEST_CASE("[Unit] DamagePipeline P3b - adaptive dispatch scalar vs simd parity") {
  KernelModeGuard guard;
  LoggerScope scope;

  for (size_t n = 1; n <= 20; ++n) {
    CAPTURE(n);
    const RunOutcome scalar = RunBatch(n, true, nullptr, 0, false, 0.0f);
    const RunOutcome simd = RunBatch(n, false, nullptr, 0, false, 0.0f);

    CheckLossesEqual(scalar.losses, simd.losses);
    for (float loss : scalar.losses) {
      CHECK(loss > 0.0f);
    }

    // 强制标量：全部目标走标量内核，且不产生任何 SIMD 块。
    CHECK(scalar.stats.scalar_targets == n);
    CHECK(scalar.stats.simd_blocks == 0u);
    CHECK(scalar.stats.simd_targets == 0u);

    // 自动分流：N<4 走标量；N>=4 走 SIMD，且余数目标由标量补齐。
    if (n < 4) {
      CHECK(simd.stats.scalar_targets == n);
      CHECK(simd.stats.simd_blocks == 0u);
    } else {
      CHECK(simd.stats.simd_blocks > 0u);
      CHECK(simd.stats.simd_targets + simd.stats.scalar_targets == n);
    }
  }
}

// P3-3：带条件掩码 / 快照目标（命印 More、气印 CritDamage、冻结掩码）
// 在标量/SIMD 两条内核下逐目标一致，覆盖条件乘区分支。
TEST_CASE("[Unit] DamagePipeline P3b - conditional masks parity across kernels") {
  KernelModeGuard guard;
  LoggerScope scope;

  // skill_id=5 会注入“命印每层 More”与“气印每层暴击伤害增量”条件规则；
  // crit_chance=1 保证暴击确定发生，使 cond_crit 分支必被求值。
  for (size_t n = 4; n <= 20; n += 4) {
    CAPTURE(n);
    const RunOutcome scalar = RunBatch(n, true, nullptr, 5, true, 1.0f);
    const RunOutcome simd = RunBatch(n, false, nullptr, 5, true, 1.0f);

    CheckLossesEqual(scalar.losses, simd.losses);
    for (float loss : scalar.losses) {
      CHECK(loss > 0.0f);
    }
    CHECK(simd.stats.simd_blocks > 0u);
  }
}

// P3-3 边界：批宽 ±1 与“单块内混入无效目标”时，SIMD 路径必须跳过失效实体，
// 不向 entt::null 落地伤害，且与标量路径结果一致。
TEST_CASE("[Unit] DamagePipeline P3b - invalid targets skipped in simd block") {
  KernelModeGuard guard;
  LoggerScope scope;

  constexpr size_t kCount = 9; // 8（原生批宽）+ 1 余数
  entt::registry scalar_registry;
  entt::entity scalar_attacker = entt::null;
  std::vector<entt::entity> scalar_defenders;
  BuildScene(scalar_registry, scalar_attacker, scalar_defenders, kCount, false,
             0.0f);

  entt::registry simd_registry;
  entt::entity simd_attacker = entt::null;
  std::vector<entt::entity> simd_defenders;
  BuildScene(simd_registry, simd_attacker, simd_defenders, kCount, false, 0.0f);

  // 在两套场景的同一位置挖空一个目标（破坏实体），两条内核都应跳过它。
  const size_t hole = 3;
  scalar_registry.destroy(scalar_defenders[hole]);
  simd_registry.destroy(simd_defenders[hole]);

  DamagePool pool;
  pool.Add(Tag::Physical, 120.0f);

  auto run = [&](entt::registry &registry, entt::entity attacker,
                 std::vector<entt::entity> &defenders, bool force_scalar) {
    DamagePipeline::ResetBatchKernelStats();
    DamagePipeline::SetForceScalarKernelForTests(force_scalar);
    std::vector<float> before;
    for (size_t i = 0; i < defenders.size(); ++i) {
      before.push_back(i == hole ? 0.0f
                                 : registry.get<HealthComponent>(defenders[i])
                                       .current);
    }
    DamagePipeline::CalculateBatch(registry, attacker, defenders, 0, pool,
                                   Tag::Melee);
    std::vector<float> losses;
    for (size_t i = 0; i < defenders.size(); ++i) {
      losses.push_back(i == hole
                           ? 0.0f
                           : before[i] - registry.get<HealthComponent>(
                                             defenders[i])
                                             .current);
    }
    DamagePipeline::SetForceScalarKernelForTests(false);
    return losses;
  };

  const std::vector<float> scalar = run(scalar_registry, scalar_attacker,
                                        scalar_defenders, true);
  const std::vector<float> simd =
      run(simd_registry, simd_attacker, simd_defenders, false);

  CHECK(simd[hole] == 0.0f);
  CheckLossesEqual(scalar, simd);
}

// P3-4：tf::Executor 内部多线程派发与串行参考结果逐目标一致，重复多轮以
// 暴露 results 写入 / 随机数 / 注册表读取的潜在竞争。
TEST_CASE("[Unit] DamagePipeline P3b - concurrent batch matches serial reference") {
  KernelModeGuard guard;
  LoggerScope scope;

  constexpr size_t kTargets = 64; // >= BATCH_GRAIN_SIZE，触发并行派发
  constexpr int kRounds = 5;

  const RunOutcome reference = RunBatch(kTargets, false, nullptr, 0, false,
                                        0.0f);
  CHECK(reference.stats.simd_blocks > 0u);

  for (int threads : {4, 8}) {
    tf::Executor executor(threads);
    for (int round = 0; round < kRounds; ++round) {
      CAPTURE(threads, round);
      const RunOutcome parallel = RunBatch(kTargets, false, &executor, 0, false,
                                           0.0f);
      // 并行派发下计数器跨任务累加，故只校验总量与参考一致。
      CHECK(parallel.stats.simd_targets + parallel.stats.scalar_targets ==
            kTargets);
      CheckLossesEqual(reference.losses, parallel.losses);
    }
  }
}

// P3-4：带条件掩码的多线程批量结算，条件求值在并行区间内只读守方状态，
// 与串行结果一致。
TEST_CASE("[Unit] DamagePipeline P3b - concurrent conditional batch consistency") {
  KernelModeGuard guard;
  LoggerScope scope;

  constexpr size_t kTargets = 96;
  const RunOutcome reference = RunBatch(kTargets, false, nullptr, 5, true, 1.0f);

  tf::Executor executor(6);
  for (int round = 0; round < 3; ++round) {
    CAPTURE(round);
    const RunOutcome parallel =
        RunBatch(kTargets, false, &executor, 5, true, 1.0f);
    CheckLossesEqual(reference.losses, parallel.losses);
  }
}

// P3-4 较重压测：更大规模 + 更多轮次，验证并行结算的稳定性（不进 ci 常规集）。
TEST_CASE("[Performance] DamagePipeline P3b - parallel stress") {
  KernelModeGuard guard;
  LoggerScope scope;

  constexpr size_t kTargets = 200;
  const RunOutcome reference = RunBatch(kTargets, false, nullptr, 0, false,
                                        0.0f);

  tf::Executor executor(8);
  for (int round = 0; round < 20; ++round) {
    CAPTURE(round);
    const RunOutcome parallel =
        RunBatch(kTargets, false, &executor, 0, false, 0.0f);
    CheckLossesEqual(reference.losses, parallel.losses);
  }
}

} // namespace NoMoreDay
