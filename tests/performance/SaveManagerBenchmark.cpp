#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerProfile.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/data/PlayerCombatHistory.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <array>
#include <sstream>
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

void SetupService1000Items(ItemStorageService &service) {
  service.setGold(999999);
  service.setUnlockedPages(ContainerKind::PersonalStash, 10);
  service.setUnlockedPages(ContainerKind::SharedStash, 5);

  uint32_t id = 10000;
  // Fill Inventory (40 slots)
  for (uint16_t i = 0; i < 40; ++i) {
    ItemInstance inst;
    inst.instanceId = id++;
    inst.baseId = (i % 2 == 0) ? 1001 : 2011;
    inst.itemLevel = 70;
    inst.rarity = 3;
    inst.attack = 50.0f;
    ItemHandle h = service.getStoreMutable().create(inst);
    service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, i}, h);
  }

  // Fill Equipment (11 slots)
  for (uint8_t s = 0; s < 11; ++s) {
    ItemInstance inst;
    inst.instanceId = id++;
    inst.baseId = 1001;
    inst.itemLevel = 75;
    inst.rarity = 4;
    ItemHandle h = service.getStoreMutable().create(inst);
    service.setSlotHandle(SlotRef{ContainerKind::Equipment, s, 0, 0}, h);
  }

  // Fill Personal Stash (7 pages x 140 = 980 items)
  for (uint16_t p = 0; p < 7; ++p) {
    for (uint16_t s = 0; s < 140; ++s) {
      ItemInstance inst;
      inst.instanceId = id++;
      inst.baseId = (s % 3 == 0) ? 1001 : 2011;
      inst.itemLevel = 60;
      inst.rarity = 2;
      ItemHandle h = service.getStoreMutable().create(inst);
      service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, p, s}, h);
    }
  }
}

} // namespace save_manager_benchmark_detail

TEST_CASE("[Performance] SaveManager - createSnapshot (1000 items)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;
  save_manager_benchmark_detail::SetupService1000Items(service);
  SaveManager sm;
  sm.SetItemStorageService(&service);

  entt::registry registry;
  save_manager_benchmark_detail::SetupPlayerSnapshotFixture(registry, 1000);

  // Warmup
  for (int i = 0; i < 5; ++i) {
    auto warmData = sm.createSnapshot(registry);
    (void)warmData;
  }

  std::vector<double> samples;
  samples.reserve(50);
  size_t sink = 0;
  for (int i = 0; i < 50; ++i) {
    ScopedTimer timer(samples);
    CharacterSaveData data = sm.createSnapshot(registry);
    sink += data.inventory.size();
    sink += data.equipment.size();
    if (data.personalStash.has_value()) {
      sink += data.personalStash->tabs.size();
    }
  }

  CHECK(sink > 0);
  const BenchmarkStats stats = CalculateStats(samples);
  // 新预算目标 < 1.0ms（P4 冻结快照）
  LOG_BENCHMARK("SaveManager createSnapshot 1000", stats, "< 1.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "SaveManager createSnapshot 1000", stats, 1.0, 3.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] SaveManager - restoreFromSnapshot (1000 items)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  entt::registry sourceRegistry;
  save_manager_benchmark_detail::SetupPlayerSnapshotFixture(sourceRegistry, 1000);

  SaveManager sm;
  const CharacterSaveData snapshot = sm.createSnapshot(sourceRegistry);

  // Warmup
  for (int i = 0; i < 3; ++i) {
    entt::registry warmRegistry;
    sm.restoreFromSnapshot(warmRegistry, snapshot);
  }

  std::vector<double> samples;
  samples.reserve(40);
  size_t playerCountSink = 0;
  for (int i = 0; i < 40; ++i) {
    entt::registry restoreRegistry;
    ScopedTimer timer(samples);
    sm.restoreFromSnapshot(restoreRegistry, snapshot);
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
  // 新预算目标 < 2.0ms
  LOG_BENCHMARK("SaveManager restoreFromSnapshot 1000", stats, "< 2.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "SaveManager restoreFromSnapshot 1000", stats, 2.0, 5.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] ItemPersistenceCodec - Binary Encode/Decode (1000 items)") {
  TestSetupScope scope;
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemStorageService service;
  save_manager_benchmark_detail::SetupService1000Items(service);

  const std::string mockProgression = "{\"level\":70,\"gold\":999999,\"astrolabe\":[1,2,3,4,5]}";

  // 1. Encode 性能采样 (1000 items binary encode)
  std::vector<double> encodeSamples;
  encodeSamples.reserve(50);
  std::string encodedBinary;
  for (int i = 0; i < 50; ++i) {
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    ScopedTimer timer(encodeSamples);
    bool ok = ItemPersistenceCodec::encode(service, ss, nullptr, ContainerDirtyFlags::All, mockProgression);
    if (i == 0) {
      CHECK(ok);
      encodedBinary = ss.str();
    }
  }

  const BenchmarkStats encStats = CalculateStats(encodeSamples);
  LOG_BENCHMARK("ItemPersistenceCodec encode 1000", encStats, "< 1.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "ItemPersistenceCodec encode 1000", encStats, 1.0, 2.5);

  // 2. Decode 性能采样 (1000 items binary decode)
  std::vector<double> decodeSamples;
  decodeSamples.reserve(50);
  for (int i = 0; i < 50; ++i) {
    std::stringstream ss(encodedBinary, std::ios::in | std::ios::binary);
    ItemStorageService dstService;
    std::string outProg;
    ScopedTimer timer(decodeSamples);
    bool ok = ItemPersistenceCodec::decode(ss, dstService, &outProg);
    (void)ok;
  }

  const BenchmarkStats decStats = CalculateStats(decodeSamples);
  LOG_BENCHMARK("ItemPersistenceCodec decode 1000", decStats, "< 2.0ms");
  save_manager_benchmark_detail::LogThresholdWarn(
      "ItemPersistenceCodec decode 1000", decStats, 2.0, 5.0);
}

} // namespace NoMoreDay::tests
