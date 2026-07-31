#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/persistence/SaveManager.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/PlayerProfile.hpp"
#include "game/components/Progression.hpp"
#include "game/components/StashComponent.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include <array>
#include <vector>

namespace NoMoreDay::tests {
namespace save_manager_benchmark_detail {

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

entt::entity CreateSimpleItem(entt::registry &registry, uint32_t id, int level,
                              ItemType type, EquipmentSlot slot) {
  const entt::entity itemEntity = registry.create();
  ItemComponent item;
  item.id = id;
  item.itemLevel = level;
  item.name = "BenchItem_" + std::to_string(id);
  item.type = type;
  item.slot = slot;
  item.rarity = Rarity::Rare;
  item.quantity = 1;
  item.value = 100.0f + static_cast<float>(id % 50);
  item.attack = (type == ItemType::Weapon) ? 20.0f + static_cast<float>(id % 7)
                                           : 0.0f;
  item.defense = (type == ItemType::Armor) ? 15.0f + static_cast<float>(id % 5)
                                           : 0.0f;
  registry.emplace<ItemComponent>(itemEntity, item);
  return itemEntity;
}

void SetupPlayerSnapshotFixture(entt::registry &registry, int totalItems) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerName>(player, "PerfPlayer");
  registry.emplace<PlayerPlaytime>(player, 3600,
                                   static_cast<double>(GetTime()) - 120.0);
  registry.emplace<Position>(player, 512.0f, 256.0f);
  registry.emplace<PrimaryStats>(player, 40.0f, 35.0f, 30.0f, 25.0f);
  registry.emplace<ActiveSkillsComponent>(player);
  registry.emplace<AstrolabeComponent>(player);
  registry.emplace<PlayerCombatHistory>(player);

  auto &inventory = registry.emplace<InventoryComponent>(player);
  inventory.capacity = 560;
  inventory.items.assign(static_cast<size_t>(inventory.capacity), entt::null);
  inventory.gold = 999999;

  auto &equipment = registry.emplace<EquipmentComponent>(player);
  const std::array<EquipmentSlot, 11> equipSlots = {
      EquipmentSlot::MainHand, EquipmentSlot::OffHand, EquipmentSlot::Head,
      EquipmentSlot::Shoulder, EquipmentSlot::Chest, EquipmentSlot::Hands,
      EquipmentSlot::Legs,     EquipmentSlot::Feet,    EquipmentSlot::Neck,
      EquipmentSlot::Ring1,    EquipmentSlot::Ring2};

  auto &stash = registry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 3;
  stash.tabs.resize(3);
  stash.tabs[0].name = "Stash A";
  stash.tabs[1].name = "Stash B";
  stash.tabs[2].name = "Stash C";

  int created = 0;
  uint32_t id = 10000;

  // Equipment items.
  for (EquipmentSlot slot : equipSlots) {
    if (created >= totalItems) {
      break;
    }
    const ItemType type = (slot == EquipmentSlot::MainHand)
                              ? ItemType::Weapon
                              : ItemType::Armor;
    const entt::entity item =
        CreateSimpleItem(registry, id++, 70, type, slot);
    equipment.set(slot, item);
    ++created;
  }

  // Inventory items.
  for (size_t i = 0; i < inventory.items.size() && created < totalItems; ++i) {
    const ItemType type = (i % 5 == 0) ? ItemType::Weapon : ItemType::Armor;
    const EquipmentSlot slot =
        (type == ItemType::Weapon) ? EquipmentSlot::MainHand : EquipmentSlot::Chest;
    inventory.items[i] = CreateSimpleItem(registry, id++, 65, type, slot);
    ++created;
  }

  // Stash items.
  for (StashTab &tab : stash.tabs) {
    for (int i = 0; i < StashTab::CAPACITY && created < totalItems; ++i) {
      const ItemType type = (i % 7 == 0) ? ItemType::Weapon : ItemType::Armor;
      const EquipmentSlot slot =
          (type == ItemType::Weapon) ? EquipmentSlot::MainHand : EquipmentSlot::Legs;
      tab.items[i] = CreateSimpleItem(registry, id++, 60, type, slot);
      ++created;
    }
  }
}

} // namespace save_manager_benchmark_detail

TEST_CASE("[Performance] SaveManager - createSnapshot (1000 items)") {
  TestSetupScope scope;
  entt::registry registry;
  save_manager_benchmark_detail::SetupPlayerSnapshotFixture(registry, 1000);

  // Warmup
  for (int i = 0; i < 5; ++i) {
    auto warmData = SaveManager::Get().createSnapshot(registry);
    (void)warmData;
  }

  std::vector<double> samples;
  samples.reserve(50);
  size_t sink = 0;
  for (int i = 0; i < 50; ++i) {
    ScopedTimer timer(samples);
    CharacterSaveData data = SaveManager::Get().createSnapshot(registry);
    sink += data.inventory.size();
    sink += data.equipment.size();
    if (data.personalStash.has_value()) {
      sink += data.personalStash->tabs.size();
    }
  }

  CHECK(sink > 0);
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("SaveManager createSnapshot 1000", stats, "< 10.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "SaveManager createSnapshot 1000", stats, 10.0, 20.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] SaveManager - restoreFromSnapshot") {
  TestSetupScope scope;
  entt::registry sourceRegistry;
  save_manager_benchmark_detail::SetupPlayerSnapshotFixture(sourceRegistry, 1000);

  const CharacterSaveData snapshot = SaveManager::Get().createSnapshot(sourceRegistry);

  // Warmup
  for (int i = 0; i < 3; ++i) {
    entt::registry warmRegistry;
    SaveManager::Get().restoreFromSnapshot(warmRegistry, snapshot);
  }

  std::vector<double> samples;
  samples.reserve(40);
  size_t playerCountSink = 0;
  for (int i = 0; i < 40; ++i) {
    entt::registry restoreRegistry;
    ScopedTimer timer(samples);
    SaveManager::Get().restoreFromSnapshot(restoreRegistry, snapshot);
    size_t localPlayers = 0;
    auto players = restoreRegistry.view<PlayerTag>();
    for (entt::entity e : players) {
      (void)e;
      ++localPlayers;
    }
    playerCountSink += localPlayers;
  }

  CHECK(playerCountSink > 0);
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("SaveManager restoreFromSnapshot 1000", stats, "< 15.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "SaveManager restoreFromSnapshot 1000", stats, 15.0, 25.0);
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
