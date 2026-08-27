#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "doctest.h"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"

#include <chrono>
#include <vector>

namespace NoMoreDay::tests {

static ItemInstance MakeBenchmarkItemProto(uint32_t id, uint32_t baseId) {
  ItemInstance inst;
  inst.instanceId = id;
  inst.baseId = baseId;
  inst.quantity = 1;
  inst.itemLevel = static_cast<uint16_t>(1 + (id % 100));
  inst.rarity = static_cast<uint8_t>((id % 6));
  inst.attack = 10.0f + static_cast<float>(id % 50);
  inst.defense = 5.0f + static_cast<float>(id % 30);
  inst.value = 100.0f + static_cast<float>(id % 500);
  inst.forgingPotential = static_cast<int16_t>(20 + (id % 30));
  inst.affixCount = 4;
  for (uint8_t a = 0; a < 4; ++a) {
    inst.affixes[a].type = static_cast<AffixType>(static_cast<uint16_t>(AffixType::Strength) + a);
    inst.affixes[a].tier = static_cast<uint8_t>(1 + (a % 5));
    inst.affixes[a].isPrefix = (a % 2 == 0);
    inst.affixes[a].value = 10.0f * (a + 1);
  }
  return inst;
}

TEST_CASE("[Performance] ItemStore - Move and Swap (10k items, 1M ops)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService service;
  constexpr size_t kItemCount = 10000;
  std::vector<SlotRef> slotsA;
  std::vector<SlotRef> slotsB;
  slotsA.reserve(kItemCount);
  slotsB.reserve(kItemCount);

  // 在个人仓库中播种 10k 物品
  service.setUnlockedPages(ContainerKind::PersonalStash, 10);
  for (uint32_t i = 0; i < kItemCount; ++i) {
    const uint16_t page = static_cast<uint16_t>((i / ItemStorageService::kStashPageCapacity) % 10);
    const uint16_t index = static_cast<uint16_t>(i % ItemStorageService::kStashPageCapacity);
    const SlotRef slot{ContainerKind::PersonalStash, 0, page, index};
    const ItemInstance proto = MakeBenchmarkItemProto(i + 1, 1001 + (i % 10));
    const ItemHandle h = service.getStoreMutable().create(proto);
    service.setSlotHandle(slot, h);
    slotsA.push_back(slot);
  }

  // 在共享仓库中生成目标槽位
  service.setUnlockedPages(ContainerKind::SharedStash, 10);
  for (uint32_t i = 0; i < kItemCount; ++i) {
    const uint16_t page = static_cast<uint16_t>((i / ItemStorageService::kStashPageCapacity) % 10);
    const uint16_t index = static_cast<uint16_t>(i % ItemStorageService::kStashPageCapacity);
    slotsB.push_back(SlotRef{ContainerKind::SharedStash, 0, page, index});
  }

  std::vector<double> samples;
  samples.reserve(20);

  constexpr int kIterations = 100000; // 每采样 100k 次交换
  for (int iter = 0; iter < 20; ++iter) {
    ScopedTimer timer(samples);
    for (int op = 0; op < kIterations; ++op) {
      const size_t idxA = static_cast<size_t>(op % kItemCount);
      const size_t idxB = static_cast<size_t>((op + 37) % kItemCount);
      service.swapItem(slotsA[idxA], slotsB[idxB]);
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemStore - Swap 100k ops", stats, "< 1.5ms per 100k (< 15ns/swap)");
  // 软性预算: 基准测试仅供参考，CI 门禁保持非性能阻断 (docs/workflows/performance.md)；性能回退作为警告暴露。
  constexpr double kBudgetMs = 1.5;
  if (stats.mean_ms > kBudgetMs) {
    LOG_WARN("[Perf] ItemStore swap over budget: mean {:.4f}ms > {:.3f}ms",
             stats.mean_ms, kBudgetMs);
  }
}

TEST_CASE("[Performance] ItemStore - Move 100k ops") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStorageService service;
  constexpr size_t kItemCount = 10000;
  std::vector<SlotRef> slotsA;
  std::vector<SlotRef> slotsB;
  slotsA.reserve(kItemCount);
  slotsB.reserve(kItemCount);

  // 在个人仓库中播种 10k 物品 (镜像 swap 基准测试)
  service.setUnlockedPages(ContainerKind::PersonalStash, 10);
  for (uint32_t i = 0; i < kItemCount; ++i) {
    const uint16_t page = static_cast<uint16_t>((i / ItemStorageService::kStashPageCapacity) % 10);
    const uint16_t index = static_cast<uint16_t>(i % ItemStorageService::kStashPageCapacity);
    const SlotRef slot{ContainerKind::PersonalStash, 0, page, index};
    const ItemInstance proto = MakeBenchmarkItemProto(i + 1, 1001 + (i % 10));
    const ItemHandle h = service.getStoreMutable().create(proto);
    service.setSlotHandle(slot, h);
    slotsA.push_back(slot);
  }

  // 在共享仓库中生成空目标槽位
  service.setUnlockedPages(ContainerKind::SharedStash, 10);
  for (uint32_t i = 0; i < kItemCount; ++i) {
    const uint16_t page = static_cast<uint16_t>((i / ItemStorageService::kStashPageCapacity) % 10);
    const uint16_t index = static_cast<uint16_t>(i % ItemStorageService::kStashPageCapacity);
    slotsB.push_back(SlotRef{ContainerKind::SharedStash, 0, page, index});
  }

  std::vector<double> samples;
  samples.reserve(20);

  constexpr int kIterations = 100000; // 每采样 200k 次移动 (往返)
  for (int iter = 0; iter < 20; ++iter) {
    ScopedTimer timer(samples);
    for (int op = 0; op < kIterations; ++op) {
      const size_t idxA = static_cast<size_t>(op % kItemCount);
      const size_t idxB = static_cast<size_t>((op + 37) % kItemCount);
      // 成对往返: A->B 然后 B->A 保证每个源槽位均被填充，使所有测量的移动均走成功路径。
      service.moveItem(slotsA[idxA], slotsB[idxB]);
      service.moveItem(slotsB[idxB], slotsA[idxA]);
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemStore - Move 200k ops", stats, "< 2.0ms per 200k (< 10ns/move)");
  constexpr double kBudgetMs = 2.0;
  if (stats.mean_ms > kBudgetMs) {
    LOG_WARN("[Perf] ItemStore move over budget: mean {:.4f}ms > {:.3f}ms",
             stats.mean_ms, kBudgetMs);
  }
}

TEST_CASE("[Performance] ItemStore - Visit 10k Items") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStore store;
  store.reserve(12000);

  constexpr size_t kItemCount = 10000;
  for (uint32_t i = 0; i < kItemCount; ++i) {
    store.create(MakeBenchmarkItemProto(i + 1, 1001 + (i % 10)));
  }
  CHECK(store.activeCount() == kItemCount);

  std::vector<double> samples;
  samples.reserve(50);

  uint64_t checksum = 0;
  for (int iter = 0; iter < 50; ++iter) {
    ScopedTimer timer(samples);
    store.visit([&checksum](ItemHandle, const ItemInstance &inst) {
      checksum += inst.instanceId;
      checksum += static_cast<uint64_t>(inst.rarity);
    });
  }

  (void)checksum;
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemStore - Visit 10k items", stats, "< 0.100ms (100us)");
  constexpr double kBudgetMs = 0.100;
  if (stats.mean_ms > kBudgetMs) {
    LOG_WARN("[Perf] ItemStore visit over budget: mean {:.4f}ms > {:.3f}ms",
             stats.mean_ms, kBudgetMs);
  }
}

TEST_CASE("[Performance] ItemStore - Freeze Snapshot (10k items memcpy)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemStore store;
  store.reserve(12000);

  constexpr size_t kItemCount = 10000;
  for (uint32_t i = 0; i < kItemCount; ++i) {
    store.create(MakeBenchmarkItemProto(i + 1, 1001 + (i % 10)));
  }

  std::vector<double> samples;
  samples.reserve(50);

  for (int iter = 0; iter < 50; ++iter) {
    ScopedTimer timer(samples);
    ItemStore snapshot = store; // 拷贝构造 / freeze 快照
    (void)snapshot.activeCount();
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemStore - Freeze Snapshot 10k", stats, "< 0.200ms (200us)");
  constexpr double kBudgetMs = 0.200;
  if (stats.mean_ms > kBudgetMs) {
    LOG_WARN("[Perf] ItemStore freeze over budget: mean {:.4f}ms > {:.3f}ms",
             stats.mean_ms, kBudgetMs);
  }
}

TEST_CASE("[Performance] ItemStore - Create and Destroy Batch (10k items)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();

  std::vector<double> samples;
  samples.reserve(30);

  constexpr size_t kItemCount = 10000;
  std::vector<ItemHandle> handles;
  handles.resize(kItemCount);

  for (int iter = 0; iter < 30; ++iter) {
    ScopedTimer timer(samples);
    ItemStore store;
    store.reserve(kItemCount + 100);

    for (size_t i = 0; i < kItemCount; ++i) {
      handles[i] = store.create(MakeBenchmarkItemProto(static_cast<uint32_t>(i + 1), 1001));
    }
    for (size_t i = 0; i < kItemCount; i += 2) {
      store.destroy(handles[i]);
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ItemStore - Create 10k & Destroy 5k", stats, "< 1.000ms");
  constexpr double kBudgetMs = 1.000;
  if (stats.mean_ms > kBudgetMs) {
    LOG_WARN("[Perf] ItemStore create/destroy over budget: mean {:.4f}ms > {:.3f}ms",
             stats.mean_ms, kBudgetMs);
  }
}

} // namespace NoMoreDay::tests
