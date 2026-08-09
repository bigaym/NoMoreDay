#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include <vector>

namespace NoMoreDay::tests {
namespace item_factory_benchmark_detail {

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

} // namespace item_factory_benchmark_detail

TEST_CASE("[Performance] ItemFactory - Batch Weapon Creation (1000)") {
  TestSetupScope scope;
  std::vector<double> samples;
  samples.reserve(25);

  for (int iter = 0; iter < 25; ++iter) {
    entt::registry registry;
    ScopedTimer timer(samples);
    for (int i = 0; i < 1000; ++i) {
      (void)ItemFactory::createWeapon(registry, 65, Rarity::Rare);
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemFactory batch createWeapon 1000", stats, "< 5.0ms total");
  item_factory_benchmark_detail::LogThresholdWarn(
      "ItemFactory batch createWeapon 1000", stats, 5.0, 8.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] ItemFactory - Batch Armor Creation (1000)") {
  TestSetupScope scope;
  std::vector<double> samples;
  samples.reserve(25);

  for (int iter = 0; iter < 25; ++iter) {
    entt::registry registry;
    ScopedTimer timer(samples);
    for (int i = 0; i < 1000; ++i) {
      (void)ItemFactory::createArmor(registry, 65, Rarity::Rare,
                                     EquipmentSlot::Chest);
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemFactory batch createArmor 1000", stats, "< 5.0ms total");
  item_factory_benchmark_detail::LogThresholdWarn(
      "ItemFactory batch createArmor 1000", stats, 5.0, 8.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] ItemFactory - Legendary Affix Stress") {
  TestSetupScope scope;
  std::vector<double> samples;
  samples.reserve(40);
  float affixValueSink = 0.0f;

  for (int iter = 0; iter < 40; ++iter) {
    ScopedTimer timer(samples);
    for (int i = 0; i < 1000; ++i) {
      for (int a = 0; a < 6; ++a) {
        const Affix affix = ItemFactory::generateRandomAffix(
            75, (a % 2) == 0, EquipmentSlot::MainHand);
        affixValueSink += affix.value;
      }
    }
  }

  CHECK(affixValueSink >= 0.0f);
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemFactory legendary affix stress", stats, "1000 x 6 rolls");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
