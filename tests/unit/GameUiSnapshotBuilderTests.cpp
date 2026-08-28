#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"

#include <entt/entt.hpp>
#include <cstdint>

using namespace NoMoreDay;

namespace {

entt::entity CreateWorldItem(entt::registry& registry, float x, float y,
                             std::uint32_t itemId) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.name = "Test Item";
  registry.emplace<Position>(item, x, y);
  return item;
}

} // namespace

TEST_CASE("[Unit] GameUiSnapshot - player fields are extracted from components") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<HealthComponent>(player, 75.0f, 100.0f);
  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 7;
  auto& inventory = registry.emplace<InventoryComponent>(player);
  inventory.gold = 42;

  const entt::entity bagItem = registry.create();
  inventory.items[0] = bagItem;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(75.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(100.0f));
  CHECK(snapshot.player.level == 7);
  CHECK(snapshot.player.inventoryUsed == 1);
  CHECK(snapshot.player.inventoryCapacity == InventoryComponent::BASE_CAPACITY);
  CHECK(snapshot.player.gold == 42);
  CHECK(snapshot.pickups.empty());
  CHECK(snapshot.notifications.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - pickups include in-range items and skip far ones") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  // 150 units away: in range.
  const entt::entity nearItem = CreateWorldItem(registry, 100.0f, 350.0f, 1);
  // Exactly 180 units away: boundary, still in range (<=).
  const entt::entity boundaryItem = CreateWorldItem(registry, 100.0f, 380.0f, 2);
  // 300 units away: out of range.
  CreateWorldItem(registry, 100.0f, 500.0f, 3);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.pickups.size() == 2);
  CHECK(snapshot.pickups[0].domainId == entt::to_integral(nearItem));
  CHECK(snapshot.pickups[0].distance == doctest::Approx(150.0f));
  CHECK(snapshot.pickups[0].source ==
        NoMoreDay::ui::GameUiPickupSource::World);
  CHECK(snapshot.pickups[1].domainId == entt::to_integral(boundaryItem));
  CHECK(snapshot.pickups[1].distance == doctest::Approx(180.0f));
}

TEST_CASE("[Unit] GameUiSnapshot - pickups are sorted by distance ascending") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Far first on purpose: the builder must sort them.
  CreateWorldItem(registry, 0.0f, 160.0f, 1);
  const entt::entity closest = CreateWorldItem(registry, 0.0f, 40.0f, 2);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.pickups.size() == 2);
  CHECK(snapshot.pickups[0].domainId == entt::to_integral(closest));
  CHECK(snapshot.pickups[0].distance == doctest::Approx(40.0f));
  CHECK(snapshot.pickups[1].distance == doctest::Approx(160.0f));
}

TEST_CASE("[Unit] GameUiSnapshot - non-pickable entities are skipped") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Entity with Position but no ItemComponent: not pickable.
  const entt::entity noItem = registry.create();
  registry.emplace<Position>(noItem, 0.0f, 50.0f);

  // Entity with ItemComponent but no Position: not a ground item.
  const entt::entity noPosition = registry.create();
  registry.emplace<ItemComponent>(noPosition);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.pickups.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - destroyed entities do not crash the builder") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  const entt::entity item = CreateWorldItem(registry, 0.0f, 50.0f, 1);
  registry.destroy(item);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.pickups.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - empty registry yields a safe default snapshot") {
  entt::registry registry;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK_FALSE(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(0.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(0.0f));
  CHECK(snapshot.player.level == 1);
  CHECK(snapshot.pickups.empty());
  CHECK(snapshot.notifications.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - player without optional components keeps defaults") {
  entt::registry registry;

  // PlayerTag and Position only; no Health/PlayerStats/Inventory components.
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.player.health == doctest::Approx(0.0f));
  CHECK(snapshot.player.maxHealth == doctest::Approx(0.0f));
  CHECK(snapshot.player.level == 1);
  CHECK(snapshot.player.inventoryUsed == 0);
  CHECK(snapshot.player.inventoryCapacity == 0);
  CHECK(snapshot.pickups.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - all-empty skill slots yield empty skill bar views") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  // Default-constructed ActiveSkillsComponent: every slot has id == 0 and every
  // specialized slot has skill_id == INVALID_SKILL_ID (the empty sentinels).
  registry.emplace<ActiveSkillsComponent>(player);

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.player.hasPlayer);
  // No non-empty skill slot: the bar view must not expose any slot.
  CHECK(snapshot.skillBar.slots.empty());
  CHECK(snapshot.skillBar.availableTalentPoints == 0);
  // No learned skills are reported for all-empty slots.
  CHECK(snapshot.skillTree.skills.empty());
  CHECK(snapshot.skillTree.availableTalentPoints == 0);
  // Every specialized slot view stays at its default value.
  for (const auto& specialized : snapshot.skillTree.specializedSlots) {
    CHECK(specialized.skillId == NoMoreDay::ui::kInvalidSkillId);
    CHECK(specialized.level == 0);
    CHECK(specialized.iconAssetId == 0);
    CHECK(specialized.allocatedPoints.empty());
  }
}

TEST_CASE("[Unit] GameUiSnapshot - skill bar keeps only non-empty slots in source order") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 2001;
  active.slots[0].cooldown = 1.5f;
  active.slots[0].current_charges = 2;
  // slots[1] intentionally left as the default empty sentinel (id == 0).
  active.slots[3].id = 2003;
  active.slots[3].cooldown = 0.75f;
  active.slots[3].current_charges = 1;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  const NoMoreDay::ui::GameUiSnapshot snapshot = builder.Build(registry);

  // Only the two non-empty slots are exposed, in ascending source order.
  REQUIRE(snapshot.skillBar.slots.size() == 2);
  CHECK(snapshot.skillBar.slots[0].slotIndex == 0);
  CHECK(snapshot.skillBar.slots[0].skillId == 2001);
  CHECK(snapshot.skillBar.slots[0].cooldown == doctest::Approx(1.5f));
  CHECK(snapshot.skillBar.slots[0].currentCharges == 2);
  CHECK(snapshot.skillBar.slots[1].slotIndex == 3);
  CHECK(snapshot.skillBar.slots[1].skillId == 2003);
  CHECK(snapshot.skillBar.slots[1].cooldown == doctest::Approx(0.75f));
  CHECK(snapshot.skillBar.slots[1].currentCharges == 1);
  // Views must be ordered by ascending source slot index.
  CHECK(snapshot.skillBar.slots[0].slotIndex < snapshot.skillBar.slots[1].slotIndex);
  // Learned-skill views mirror the same non-empty slots only.
  REQUIRE(snapshot.skillTree.skills.size() == 2);
  CHECK(snapshot.skillTree.skills[0].skillId == 2001);
  CHECK(snapshot.skillTree.skills[1].skillId == 2003);
}

TEST_CASE("[Unit] GameUiSnapshot - gating skips stash and crafting when closed") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // 添加 PersonalStashComponent
  auto& stash = registry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 2;
  stash.tabs.resize(2);
  stash.tabs[0].name = "Tab1";
  stash.tabs[0].type = StashTabType::Normal;
  const entt::entity stashItem = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(stashItem);
  itemComp.id = 5001;
  itemComp.name = "Stash Item";
  stash.tabs[0].items[0] = stashItem;

  // 添加 MaterialBankComponent
  auto& materialBank = registry.emplace<MaterialBankComponent>(player);
  materialBank.materials.push_back({4001, 10});

  NoMoreDay::ui::GameUiSnapshotBuilder builder;

  // 1. 默认门控关闭状态：isStashOpen = false, isCraftingOpen = false
  NoMoreDay::ui::GameUiSnapshotOptions closedOptions;
  closedOptions.isStashOpen = false;
  closedOptions.isCraftingOpen = false;

  const NoMoreDay::ui::GameUiSnapshot closedSnapshot =
      builder.Build(registry, closedOptions);
  CHECK(closedSnapshot.stash.tabs.empty());
  CHECK(closedSnapshot.crafting.materials.empty());

  // 2. 门控开启状态：isStashOpen = true, isCraftingOpen = true
  NoMoreDay::ui::GameUiSnapshotOptions openOptions;
  openOptions.isStashOpen = true;
  openOptions.isCraftingOpen = true;

  const NoMoreDay::ui::GameUiSnapshot openSnapshot =
      builder.Build(registry, openOptions);
  CHECK_FALSE(openSnapshot.stash.tabs.empty());
  CHECK(openSnapshot.stash.tabs.size() == 2);
  CHECK(openSnapshot.stash.tabs[0].slots.size() == 1);
  CHECK(openSnapshot.stash.tabs[0].slots[0].domainId == entt::to_integral(stashItem));
  CHECK(openSnapshot.crafting.materials.size() == 1);
  CHECK(openSnapshot.crafting.materials[0].materialId == 4001);
  CHECK(openSnapshot.crafting.materials[0].count == 10);
}

TEST_CASE("[Unit] GameUiSnapshot - list items have scalars only, displayed items have full tooltip data") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto& inventory = registry.emplace<InventoryComponent>(player);
  const entt::entity richItem = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(richItem);
  itemComp.id = 7001;
  itemComp.name = "Excalibur";
  itemComp.description = "Legendary sword";
  itemComp.attack = 120.0f;
  itemComp.defense = 0.0f;
  itemComp.rarity = Rarity::Legendary;
  itemComp.type = ItemType::Weapon;
  itemComp.slot = EquipmentSlot::MainHand;
  itemComp.socketCount = 1;
  Affix affix;
  affix.type = AffixType::Strength;
  affix.value = 30.0f;
  affix.tier = 5;
  affix.isPrefix = true;
  affix.isLegendary = true;
  itemComp.affixes.push_back(affix);

  Affix implicit;
  implicit.type = AffixType::CritChance;
  implicit.value = 10.0f;
  implicit.tier = 3;
  implicit.isPrefix = false;
  implicit.isLegendary = false;
  itemComp.implicits.push_back(implicit);

  inventory.items[0] = richItem;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;
  options.hoveredItem = entt::to_integral(richItem);

  const NoMoreDay::ui::GameUiSnapshot snapshot =
      builder.Build(registry, options);

  // 1. 常规背包列表：只携带标量，affixes/implicits 和 name 保持为空
  REQUIRE(snapshot.inventory.items.size() == 1);
  const auto& invItem = snapshot.inventory.items[0];
  CHECK(invItem.domainId == entt::to_integral(richItem));
  CHECK(invItem.itemId == 7001);
  CHECK(invItem.rarity == static_cast<std::uint8_t>(Rarity::Legendary));
  CHECK(invItem.affixes.empty());
  CHECK(invItem.implicits.empty());
  CHECK(invItem.name.empty());

  // 2. displayedItems：填充完整 tooltip、name、description、affixes 和 implicits
  REQUIRE(snapshot.displayedItems.size() == 1);
  const auto& dispItem = snapshot.displayedItems[0];
  CHECK(dispItem.domainId == entt::to_integral(richItem));
  CHECK(dispItem.name == "Excalibur");
  CHECK(dispItem.description == "Legendary sword");
  CHECK(dispItem.attack == doctest::Approx(120.0f));
  REQUIRE(dispItem.affixes.size() == 1);
  CHECK(dispItem.affixes[0].type == static_cast<std::uint16_t>(AffixType::Strength));
  CHECK(dispItem.affixes[0].value == doctest::Approx(30.0f));
  REQUIRE(dispItem.implicits.size() == 1);
  CHECK(dispItem.implicits[0].type == static_cast<std::uint16_t>(AffixType::CritChance));
  CHECK(dispItem.implicits[0].value == doctest::Approx(10.0f));
}

TEST_CASE("[Unit] GameUiSnapshot - dynamic HUD fields update per-frame while container views are cached") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<HealthComponent>(player, 80.0f, 100.0f);
  auto& inv = registry.emplace<InventoryComponent>(player);
  inv.gold = 500;
  const entt::entity item = registry.create();
  auto& comp = registry.emplace<ItemComponent>(item);
  comp.id = 1001;
  comp.name = "Steel Blade";
  inv.items[0] = item;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;

  // 第一次构建
  const auto snap1 = builder.Build(registry, options);
  CHECK(snap1.player.health == doctest::Approx(80.0f));
  CHECK(snap1.inventory.items.size() == 1);
  CHECK(snap1.inventory.items[0].itemId == 1001);

  // 动态数据变化：生命值恢复，物品未变
  registry.get<HealthComponent>(player).current = 95.0f;

  // 第二次构建：生命值立即刷新，物品容器视图命中复用
  const auto snap2 = builder.Build(registry, options);
  CHECK(snap2.revision == snap1.revision + 1);
  CHECK(snap2.player.health == doctest::Approx(95.0f)); // 动态字段即时更新，不 stale
  CHECK(snap2.inventory.items.size() == 1);
  CHECK(snap2.inventory.items[0].itemId == 1001);
}

TEST_CASE("[Unit] GameUiSnapshot - stash search query changes invalidate cache and update matchesSearch") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& stash = registry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 1;
  stash.tabs.resize(1);

  const entt::entity sword = registry.create();
  auto& swordComp = registry.emplace<ItemComponent>(sword);
  swordComp.id = 2001;
  swordComp.name = "Fire Sword";
  stash.tabs[0].items[0] = sword;

  const entt::entity shield = registry.create();
  auto& shieldComp = registry.emplace<ItemComponent>(shield);
  shieldComp.id = 2002;
  shieldComp.name = "Iron Shield";
  stash.tabs[0].items[1] = shield;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;
  options.isStashOpen = true;
  options.SetStashSearchQuery("Fire");

  // 第一次构建：匹配 "Fire"
  const auto snap1 = builder.Build(registry, options);
  REQUIRE(snap1.stash.tabs.size() == 1);
  REQUIRE(snap1.stash.tabs[0].slots.size() >= 2);
  CHECK(snap1.stash.tabs[0].slots[0].matchesSearch == true);
  CHECK(snap1.stash.tabs[0].slots[1].matchesSearch == false);

  // 搜索词改变为 "Iron"
  options.SetStashSearchQuery("Iron");
  const auto snap2 = builder.Build(registry, options);
  REQUIRE(snap2.stash.tabs.size() == 1);
  REQUIRE(snap2.stash.tabs[0].slots.size() >= 2);
  CHECK(snap2.stash.tabs[0].slots[0].matchesSearch == false);
  CHECK(snap2.stash.tabs[0].slots[1].matchesSearch == true); // 搜索词改变立即生效
}

TEST_CASE("[Unit] GameUiSnapshot - item storage version mutation invalidates container cache") {
  entt::registry registry;
  SharedContext ctx;
  ItemStorageService storageService;
  ctx.itemStorage = &storageService;
  registry.ctx().emplace<SharedContext*>(&ctx);

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& inv = registry.emplace<InventoryComponent>(player);

  const entt::entity itemA = registry.create();
  auto& compA = registry.emplace<ItemComponent>(itemA);
  compA.id = 3001;
  inv.items[0] = itemA;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;

  const auto snap1 = builder.Build(registry, options);
  REQUIRE(snap1.inventory.items.size() == 1);
  CHECK(snap1.inventory.items[0].itemId == 3001);

  // 变更物品并递增 ItemStorageService 中的 ItemStore 版本
  const entt::entity itemB = registry.create();
  auto& compB = registry.emplace<ItemComponent>(itemB);
  compB.id = 3002;
  inv.items[0] = itemB;
  ItemInstance proto{};
  proto.baseId = 3002;
  storageService.getStoreMutable().create(proto); // 真实触发 store.version()++

  const auto snap2 = builder.Build(registry, options);
  REQUIRE(snap2.inventory.items.size() == 1);
  CHECK(snap2.inventory.items[0].itemId == 3002); // 成功使缓存失效并更新
}

TEST_CASE("[Unit] GameUiSnapshot - gold change invalidates container cache") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& inv = registry.emplace<InventoryComponent>(player);
  inv.gold = 100;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;

  const auto snap1 = builder.Build(registry, options);
  CHECK(snap1.inventory.gold == 100);

  // 金币变更
  inv.gold = 250;

  const auto snap2 = builder.Build(registry, options);
  CHECK(snap2.inventory.gold == 250); // 金币更新立即生效
}

TEST_CASE("[Unit] GameUiSnapshot - stash tab unlock invalidates container cache") {
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& stash = registry.emplace<PersonalStashComponent>(player);
  stash.unlockedTabs = 1;
  stash.tabs.resize(2);

  const entt::entity sword = registry.create();
  auto& swordComp = registry.emplace<ItemComponent>(sword);
  swordComp.id = 5001;
  stash.tabs[0].items[0] = sword;

  const entt::entity shield = registry.create();
  auto& shieldComp = registry.emplace<ItemComponent>(shield);
  shieldComp.id = 5002;
  stash.tabs[1].items[0] = shield;

  NoMoreDay::ui::GameUiSnapshotBuilder builder;
  NoMoreDay::ui::GameUiSnapshotOptions options;
  options.isStashOpen = true;

  const auto snap1 = builder.Build(registry, options);
  CHECK(snap1.stash.unlockedTabs == 1);
  CHECK(snap1.stash.tabs.size() == 1);

  // 解锁第 2 页
  stash.unlockedTabs = 2;

  const auto snap2 = builder.Build(registry, options);
  CHECK(snap2.stash.unlockedTabs == 2);
  CHECK(snap2.stash.tabs.size() == 2); // 新页即时生效
}
