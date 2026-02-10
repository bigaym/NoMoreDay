#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <vector>

namespace NoMoreDay::tests {
namespace skill_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 300;

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

void PrepareSkillRuntime() {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Init();
  SkillSystem::InitHooks();
}

} // namespace skill_benchmark_detail

TEST_CASE("[Performance] SkillSystem - Update 100 Active Skills") {
  TestSetupScope scope;
  skill_benchmark_detail::PrepareSkillRuntime();

  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  for (int i = 0; i < 100; ++i) {
    const entt::entity e = registry.create();
    auto &active = registry.emplace<ActiveSkillsComponent>(e);
    registry.emplace<CombatStats>(e);

    for (int s = 0; s < 5; ++s) {
      active.slots[s].id = static_cast<uint32_t>((s % 3) + 1);
      active.slots[s].current_charges = 0;
      active.slots[s].cooldown = 1.0f + 0.1f * static_cast<float>(s);
    }
  }

  for (int i = 0; i < skill_benchmark_detail::kWarmupFrames; ++i) {
    SkillSystem::Update(registry, grid, skill_benchmark_detail::kDt);
  }

  std::vector<double> samples;
  samples.reserve(skill_benchmark_detail::kBenchFrames);

  for (int i = 0; i < skill_benchmark_detail::kBenchFrames; ++i) {
    ScopedTimer timer(samples);
    SkillSystem::Update(registry, grid, skill_benchmark_detail::kDt);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("SkillSystem update 100", stats, "< 0.5ms / < 1.0ms");
  skill_benchmark_detail::LogThresholdWarn("SkillSystem update 100", stats, 0.5,
                                           1.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] SkillSystem - UpdateCooldowns Batch") {
  TestSetupScope scope;
  skill_benchmark_detail::PrepareSkillRuntime();

  entt::registry registry;

  for (int i = 0; i < 5000; ++i) {
    const entt::entity e = registry.create();
    auto &active = registry.emplace<ActiveSkillsComponent>(e);
    registry.emplace<CombatStats>(e);

    for (int s = 0; s < 5; ++s) {
      active.slots[s].id = static_cast<uint32_t>((s % 3) + 1);
      active.slots[s].current_charges = 0;
      active.slots[s].cooldown = 0.4f + 0.05f * static_cast<float>(s);
    }
  }

  for (int i = 0; i < skill_benchmark_detail::kWarmupFrames; ++i) {
    SkillSystem::UpdateCooldowns(registry, skill_benchmark_detail::kDt);
  }

  std::vector<double> samples;
  samples.reserve(200);
  for (int i = 0; i < 200; ++i) {
    ScopedTimer timer(samples);
    SkillSystem::UpdateCooldowns(registry, skill_benchmark_detail::kDt);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("SkillSystem UpdateCooldowns batch", stats, "< 0.3ms");
  skill_benchmark_detail::LogThresholdWarn(
      "SkillSystem UpdateCooldowns batch", stats, 0.3, 0.8);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] SkillSystem - GetEffectiveSkillTags") {
  TestSetupScope scope;
  skill_benchmark_detail::PrepareSkillRuntime();

  entt::registry registry;
  const entt::entity player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[100] = 1;
  active.specialized_slots[0].allocated_points[101] = 1;

  std::vector<double> samples;
  samples.reserve(10000);
  uint64_t tagSink = 0;

  for (int i = 0; i < 10000; ++i) {
    ScopedTimer timer(samples);
    const Tag tags = SkillSystem::GetEffectiveSkillTags(registry, player, 1);
    tagSink ^= static_cast<uint64_t>(tags);
  }

  CHECK(tagSink >= 0);

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("SkillSystem GetEffectiveSkillTags", stats, "query");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
