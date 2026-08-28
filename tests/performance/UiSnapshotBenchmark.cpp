#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "doctest.h"

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/GameUiSnapshotBuilder.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/item/ItemFactory.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace NoMoreDay::tests {
namespace ui_snapshot_benchmark_detail {

/**
 * @brief Helper to create an item entity populated with rich attributes (affixes, sockets, implicits).
 */
entt::entity CreateRichItem(entt::registry& registry, uint32_t id, int level,
                            ItemType type, EquipmentSlot slot, Rarity rarity) {
  entt::entity itemEntity = entt::null;
  if (type == ItemType::Weapon) {
    itemEntity = ItemFactory::createWeapon(registry, level, rarity);
  } else if (type == ItemType::Armor || type == ItemType::Shield ||
             type == ItemType::Jewelry) {
    itemEntity = ItemFactory::createArmor(registry, level, rarity, slot);
  } else {
    itemEntity = ItemFactory::createRandomLoot(registry, level);
  }

  if (itemEntity != entt::null && registry.valid(itemEntity)) {
    auto& item = registry.get<ItemComponent>(itemEntity);
    item.id = id;

    // Ensure item has rich affix set for realistic snapshot overhead
    if (item.affixes.empty()) {
      item.affixes.push_back({AffixType::Strength, 25.0f, 3, true});
      item.affixes.push_back({AffixType::CritChance, 5.5f, 4, false});
      item.affixes.push_back({AffixType::FlatPhysicalDamage, 18.0f, 3, true});
      item.affixes.push_back({AffixType::PercentHealth, 12.0f, 4, false});
    }
    if (item.implicits.empty()) {
      item.implicits.push_back({AffixType::FlatArmor, 45.0f, 2, true});
    }
    item.forgingPotential = 28;
    item.itemLevel = level;

    // Populate socketed items on chest, helm, or weapons
    if (item.socketCount == 0 &&
        (slot == EquipmentSlot::Chest || slot == EquipmentSlot::MainHand ||
         slot == EquipmentSlot::Head)) {
      item.socketCount = 2;
      item.sockets.resize(2, entt::null);

      // Create a socketed rune entity
      const entt::entity runeEntity = registry.create();
      auto& runeComp = registry.emplace<ItemComponent>(runeEntity);
      runeComp.id = 9001;
      runeComp.name = "El Rune";
      runeComp.type = ItemType::Material;
      runeComp.rarity = Rarity::Magic;
      item.sockets[0] = runeEntity;
    }
  }

  return itemEntity;
}

/**
 * @brief Setup realistic snapshot fixture:
 * - 1 Player with stats & tags
 * - 10 Equipped items (Head, Chest, Hands, Legs, Feet, MainHand, OffHand, Neck, Ring1, Ring2)
 * - 40 Inventory items (rich affixes, sockets, potentials)
 * - 3 Stash tabs * 144 items = 432 Stash items
 * - 20 Ground items (within pickup range of player)
 */
void SetupUiBenchmarkFixture(entt::registry& registry, entt::entity& outPlayer,
                             std::vector<entt::entity>& outSampleItems) {
  outPlayer = registry.create();
  registry.emplace<PlayerTag>(outPlayer);
  registry.emplace<Position>(outPlayer, 500.0f, 500.0f);
  registry.emplace<HealthComponent>(outPlayer, 450.0f, 500.0f);

  auto& playerStats = registry.emplace<PlayerStats>(outPlayer);
  playerStats.level = 65;
  playerStats.current_xp = 12000;
  playerStats.required_xp = 25000;
  playerStats.available_attribute_points = 5;
  playerStats.available_skill_points = 2;

  auto& combatStats = registry.emplace<CombatStats>(outPlayer);
  combatStats.health = 450.0f;
  combatStats.max_health = 500.0f;
  combatStats.mana = 200.0f;
  combatStats.max_mana = 250.0f;
  combatStats.barrier = 50.0f;
  combatStats.max_barrier = 100.0f;
  combatStats.armor = 350.0f;

  auto& primaryStats = registry.emplace<PrimaryStats>(outPlayer);
  primaryStats.strength = 80.0f;
  primaryStats.dexterity = 60.0f;
  primaryStats.intelligence = 45.0f;
  primaryStats.vitality = 90.0f;

  registry.emplace<ActiveSkillsComponent>(outPlayer);

  uint32_t itemIdCounter = 1000;

  // 1. Equipment (Head, Chest, Hands, Legs, Feet, MainHand, OffHand, Neck, Ring1, Ring2)
  auto& equipment = registry.emplace<EquipmentComponent>(outPlayer);
  const std::array<EquipmentSlot, 10> equipSlots = {
      EquipmentSlot::Head,     EquipmentSlot::Chest,   EquipmentSlot::Hands,
      EquipmentSlot::Legs,     EquipmentSlot::Feet,    EquipmentSlot::MainHand,
      EquipmentSlot::OffHand,  EquipmentSlot::Neck,    EquipmentSlot::Ring1,
      EquipmentSlot::Ring2};

  for (EquipmentSlot slot : equipSlots) {
    ItemType type = ItemType::Armor;
    if (slot == EquipmentSlot::MainHand) {
      type = ItemType::Weapon;
    } else if (slot == EquipmentSlot::OffHand) {
      type = ItemType::Shield;
    } else if (slot == EquipmentSlot::Neck || slot == EquipmentSlot::Ring1 ||
               slot == EquipmentSlot::Ring2) {
      type = ItemType::Jewelry;
    }

    entt::entity item = CreateRichItem(registry, itemIdCounter++, 65, type,
                                       slot, Rarity::Rare);
    equipment.set(slot, item);
    outSampleItems.push_back(item);
  }

  // 2. Inventory (40 items)
  auto& inventory = registry.emplace<InventoryComponent>(outPlayer);
  inventory.capacity = 40;
  inventory.items.resize(40, entt::null);
  inventory.gold = 50000;

  for (int i = 0; i < 40; ++i) {
    ItemType type = (i % 4 == 0)   ? ItemType::Weapon
                    : (i % 4 == 1) ? ItemType::Armor
                    : (i % 4 == 2) ? ItemType::Jewelry
                                   : ItemType::Material;
    EquipmentSlot slot = (type == ItemType::Weapon) ? EquipmentSlot::MainHand
                         : (type == ItemType::Armor) ? EquipmentSlot::Chest
                                                     : EquipmentSlot::Ring;
    Rarity rarity = (i % 5 == 0)   ? Rarity::Legendary
                    : (i % 2 == 0) ? Rarity::Rare
                                   : Rarity::Magic;

    entt::entity item = CreateRichItem(registry, itemIdCounter++,
                                       60 + (i % 15), type, slot, rarity);
    inventory.items[i] = item;
    outSampleItems.push_back(item);
  }

  // 3. Stash (3 tabs * 144 items = 432 items total)
  auto& stash = registry.emplace<PersonalStashComponent>(outPlayer);
  stash.unlockedTabs = 3;
  stash.tabs.resize(3);
  for (int t = 0; t < 3; ++t) {
    stash.tabs[t].name = "Stash Tab " + std::to_string(t + 1);
    stash.tabs[t].type = StashTabType::Normal;
    for (int s = 0; s < 144; ++s) {
      ItemType type = (s % 3 == 0) ? ItemType::Weapon : ItemType::Armor;
      EquipmentSlot slot = (type == ItemType::Weapon) ? EquipmentSlot::MainHand
                                                      : EquipmentSlot::Legs;
      Rarity rarity = (s % 7 == 0)   ? Rarity::Set
                      : (s % 3 == 0) ? Rarity::Rare
                                     : Rarity::Magic;

      entt::entity item = CreateRichItem(registry, itemIdCounter++,
                                         50 + (s % 25), type, slot, rarity);
      stash.tabs[t].items[s] = item;
    }
  }

  // 4. Ground items (20 items placed in pickup range around player)
  for (int g = 0; g < 20; ++g) {
    const float angle = static_cast<float>(g) * 0.314159f;
    const float dist = 40.0f + static_cast<float>(g % 8) * 15.0f; // within pickup range (180.0f)
    const float gx = 500.0f + dist * std::cos(angle);
    const float gy = 500.0f + dist * std::sin(angle);

    entt::entity groundItem =
        CreateRichItem(registry, itemIdCounter++, 60, ItemType::Weapon,
                       EquipmentSlot::MainHand, Rarity::Rare);
    registry.emplace<Position>(groundItem, gx, gy);
    outSampleItems.push_back(groundItem);
  }
}

} // namespace ui_snapshot_benchmark_detail

/**
 * @brief Baseline performance benchmark for GameUiSnapshotBuilder::Build.
 *
 * Measures current full snapshot construction cost across:
 * - Player stats, skills, combat components
 * - 10 Equipped items
 * - 40 Inventory items
 * - 432 Stash items (3 tabs x 144)
 * - 20 Ground items
 * - Displayed items / tooltip generation
 */
TEST_CASE("[Performance] UiSnapshot - Baseline Build") {
  TestSetupScope scope;
  entt::registry registry;
  entt::entity player = entt::null;
  std::vector<entt::entity> sampleItems;
  ui_snapshot_benchmark_detail::SetupUiBenchmarkFixture(registry, player,
                                                        sampleItems);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;
  options.isStashOpen = true;
  options.isCraftingOpen = true;
  if (!sampleItems.empty()) {
    options.hoveredItem = entt::to_integral(sampleItems.front());
  }

  // Warmup runs (强制 InvalidateCache 以度量全量构建性能)
  for (int i = 0; i < 5; ++i) {
    builder.InvalidateCache();
    const NoMoreDay::ui::GameUiSnapshot warmSnap =
        builder.Build(registry, options);
    (void)warmSnap;
  }

  // Sampled benchmark runs (100 iterations, 每次冷构建以度量基准开销)
  std::vector<double> samples;
  samples.reserve(100);

  std::size_t checksum = 0;
  for (int iter = 0; iter < 100; ++iter) {
    builder.InvalidateCache();
    ScopedTimer timer(samples);
    const NoMoreDay::ui::GameUiSnapshot snapshot =
        builder.Build(registry, options);
    checksum += snapshot.inventory.items.size();
    checksum += snapshot.equipment.size();
    checksum += snapshot.stash.tabs.size();
    checksum += snapshot.pickups.size();
    checksum += snapshot.displayedItems.size();
  }

  CHECK(checksum > 0);
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK(
      "UiSnapshot - Baseline Build (40 Inv + 10 Equip + 432 Stash + 20 Ground)",
      stats, "< 2.0ms");
  LOG_WARN("UiSnapshot Baseline Stats: Min={:.3f}ms, Max={:.3f}ms, "
           "Mean={:.3f}ms, Median={:.3f}ms, P99={:.3f}ms (Samples: {})",
           stats.min_ms, stats.max_ms, stats.mean_ms, stats.median_ms,
           stats.p99_ms, samples.size());

  CHECK(samples.size() == 100);
  CHECK(stats.mean_ms > 0.0);
  CHECK(stats.mean_ms < 2.0); // 硬断言：全量基线构建平均耗时必须低于 2.0ms 预算
}

/**
 * @brief Benchmark for GameUiSnapshotBuilder::Build with version/state unchanged (T-P5-2 cache hit).
 *
 * Measures cached container views reuse performance:
 * - When ItemStore version and options are unchanged, builder reuses cached container views.
 * - Target overhead: < 0.05ms (near zero per-frame cost).
 */
TEST_CASE("[Performance] UiSnapshot - Version Unchanged Cached Build") {
  TestSetupScope scope;
  entt::registry registry;
  entt::entity player = entt::null;
  std::vector<entt::entity> sampleItems;
  ui_snapshot_benchmark_detail::SetupUiBenchmarkFixture(registry, player,
                                                        sampleItems);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;
  options.isStashOpen = true;
  options.isCraftingOpen = true;
  if (!sampleItems.empty()) {
    options.hoveredItem = entt::to_integral(sampleItems.front());
  }

  // Initial build to populate cache
  const NoMoreDay::ui::GameUiSnapshot initialSnap =
      builder.Build(registry, options);
  const std::uint64_t initialRevision = initialSnap.revision;

  // Sampled benchmark runs (100 iterations with state/version unchanged)
  std::vector<double> samples;
  samples.reserve(100);

  std::size_t checksum = 0;
  std::uint64_t lastRev = initialRevision;
  for (int iter = 0; iter < 100; ++iter) {
    ScopedTimer timer(samples);
    const NoMoreDay::ui::GameUiSnapshot snapshot =
        builder.Build(registry, options);
    checksum += snapshot.inventory.items.size();
    checksum += snapshot.equipment.size();
    checksum += snapshot.stash.tabs.size();
    checksum += snapshot.pickups.size();
    checksum += snapshot.displayedItems.size();
    CHECK(snapshot.revision > lastRev);
    lastRev = snapshot.revision;
  }

  CHECK(checksum > 0);
  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("UiSnapshot - Version Unchanged Cached Build (Cache Hit)",
                stats, "< 0.05ms");
  LOG_WARN("UiSnapshot Cached Stats: Min={:.4f}ms, Max={:.4f}ms, "
           "Mean={:.4f}ms, Median={:.4f}ms, P99={:.4f}ms (Samples: {})",
           stats.min_ms, stats.max_ms, stats.mean_ms, stats.median_ms,
           stats.p99_ms, samples.size());

  CHECK(samples.size() == 100);
  CHECK(stats.mean_ms < 0.05); // 硬断言：缓存命中构建平均耗时必须低于 0.05ms 预算
}

} // namespace NoMoreDay::tests
