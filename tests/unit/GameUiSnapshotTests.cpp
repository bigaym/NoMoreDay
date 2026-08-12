#pragma once
#include "doctest.h"

#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include "game/foundation/components/AIComponent.hpp"      // EnemyTag
#include "game/foundation/components/Common.hpp"           // PlayerTag, HealthComponent, Position
#include "game/foundation/components/EnemyComponent.hpp"   // EnemyRarityComponent
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"      // PlayerStats
#include "game/foundation/components/Progression.hpp"      // AstrolabeComponent
#include "game/foundation/components/SkillDefs.hpp"        // ActiveSkillsComponent, BladeResourceComponent, SummonComponent
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/Stats.hpp"            // CombatStats, PrimaryStats

#include <entt/entt.hpp>
#include <cstdint>

namespace NoMoreDay {
namespace {

// Player with HUD/panel-relevant components filled with distinct values.
entt::entity CreatePlayer(entt::registry& registry) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 10.0f, 20.0f);

  auto& health = registry.emplace<HealthComponent>(player);
  health.current = 80.0f;
  health.max = 100.0f;

  auto& stats = registry.emplace<PlayerStats>(player);
  stats.level = 3;
  stats.current_xp = 40.0f;
  stats.required_xp = 120.0f;
  stats.available_attribute_points = 2;
  stats.available_skill_points = 1;

  auto& inventory = registry.emplace<InventoryComponent>(player);
  inventory.gold = 123;

  auto& combat = registry.emplace<CombatStats>(player);
  combat.health = 80.0f;
  combat.max_health = 100.0f;
  combat.mana = 30.0f;
  combat.max_mana = 50.0f;
  combat.effective_strength = 15.0f;
  combat.resistances[0] = 12.0f;

  auto& primary = registry.emplace<PrimaryStats>(player);
  primary.strength = 12.0f;
  primary.dexterity = 14.0f;
  primary.intelligence = 8.0f;
  primary.vitality = 20.0f;
  return player;
}

entt::entity AddItemToInventory(entt::registry& registry, entt::entity player,
                                std::uint32_t itemId, int slotIndex,
                                std::uint32_t quantity = 1) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.quantity = static_cast<int>(quantity);
  itemComp.maxStack = 10;
  itemComp.rarity = Rarity::Rare;
  itemComp.itemLevel = 7;
  itemComp.textureId = 555;
  Affix affix;
  affix.type = AffixType::Strength;
  affix.value = 5.0f;
  affix.tier = 2;
  affix.isPrefix = true;
  itemComp.affixes.push_back(affix);
  auto& inventory = registry.get<InventoryComponent>(player);
  inventory.items[slotIndex] = item;
  return item;
}

// A ground item at the given offset from the player.
entt::entity AddGroundItem(entt::registry& registry, std::uint32_t itemId,
                           float x, float y) {
  const entt::entity item = registry.create();
  auto& itemComp = registry.emplace<ItemComponent>(item);
  itemComp.id = itemId;
  itemComp.quantity = 1;
  itemComp.maxStack = 1;
  registry.emplace<Position>(item, x, y);
  return item;
}

TEST_CASE("[Unit] GameUiSnapshot - revision increments on every build") {
  entt::registry registry;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot first = builder.Build(registry);
  const ui::GameUiSnapshot second = builder.Build(registry);
  const ui::GameUiSnapshot third = builder.Build(registry);

  CHECK(first.revision == 1);
  CHECK(second.revision == 2);
  CHECK(third.revision == 3);
}

TEST_CASE("[Unit] GameUiSnapshot - empty world yields a neutral snapshot") {
  entt::registry registry;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK_FALSE(snapshot.player.hasPlayer);
  CHECK(snapshot.player.domainId == ui::kInvalidDomainId);
  CHECK(snapshot.inventory.items.empty());
  CHECK(snapshot.pickups.empty());
  CHECK(snapshot.monsters.empty());
  CHECK(snapshot.displayedItems.empty());
}

TEST_CASE("[Unit] GameUiSnapshot - player and character stats are value-copied") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasPlayer);
  CHECK(snapshot.player.domainId == entt::to_integral(player));
  CHECK(snapshot.player.health == 80.0f);
  CHECK(snapshot.player.maxHealth == 100.0f);
  CHECK(snapshot.player.mana == 30.0f);
  CHECK(snapshot.player.maxMana == 50.0f);
  CHECK(snapshot.player.level == 3);
  CHECK(snapshot.player.currentXp == 40.0f);
  CHECK(snapshot.player.requiredXp == 120.0f);
  CHECK(snapshot.player.availableAttributePoints == 2);
  CHECK(snapshot.player.availableSkillPoints == 1);
  CHECK(snapshot.player.inventoryCapacity ==
        InventoryComponent::BASE_CAPACITY);
  CHECK(snapshot.player.gold == 123);
  CHECK_FALSE(snapshot.player.hasBladeResource);
  CHECK_FALSE(snapshot.player.hasSummon);

  CHECK(snapshot.characterStats.strength == 12.0f);
  CHECK(snapshot.characterStats.dexterity == 14.0f);
  CHECK(snapshot.characterStats.intelligence == 8.0f);
  CHECK(snapshot.characterStats.vitality == 20.0f);
  CHECK(snapshot.characterStats.effectiveStrength == 15.0f);
  CHECK(snapshot.characterStats.maxHealth == 100.0f);
  CHECK(snapshot.characterStats.resistances[0] == 12.0f);
}

TEST_CASE("[Unit] GameUiSnapshot - blade resource and summons are surfaced") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  auto& blade = registry.emplace<BladeResourceComponent>(player);
  blade.kind = BladeResourceKind::SwordIntent;
  blade.current = 4;
  blade.max = 10;

  const entt::entity summon = registry.create();
  auto& summonComp = registry.emplace<SummonComponent>(summon);
  // R5 adaptation: the builder groups summons by owner (only the player's
  // summons are surfaced to the HUD); the test must attribute the summon.
  summonComp.owner = player;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.player.hasBladeResource);
  CHECK(snapshot.player.bladeResourceKind ==
        static_cast<std::uint8_t>(BladeResourceKind::SwordIntent));
  CHECK(snapshot.player.bladeResourceCurrent == 4);
  CHECK(snapshot.player.bladeResourceMax == 10);
  CHECK(snapshot.player.hasSummon);
  CHECK(snapshot.player.summonCount == 1);
}

TEST_CASE("[Unit] GameUiSnapshot - inventory view carries indices and bags") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  const entt::entity itemA = AddItemToInventory(registry, player, 1, 2);
  const entt::entity itemB = AddItemToInventory(registry, player, 2, 5);
  const entt::entity bag = registry.create();
  registry.emplace<ItemComponent>(bag);
  registry.get<InventoryComponent>(player).bag_slots[0] = bag;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.inventory.items.size() == 2);
  CHECK(snapshot.inventory.used == 2);
  CHECK(snapshot.inventory.capacity == InventoryComponent::BASE_CAPACITY);
  CHECK(snapshot.inventory.gold == 123);

  // First item in slot order.
  CHECK(snapshot.inventory.items[0].domainId == entt::to_integral(itemA));
  CHECK(snapshot.inventory.items[0].inventoryIndex == 2);
  CHECK(snapshot.inventory.items[0].bagSlotIndex == -1);
  CHECK(snapshot.inventory.items[0].itemId == 1);
  CHECK(snapshot.inventory.items[0].quantity == 1);
  CHECK(snapshot.inventory.items[0].maxStack == 10);
  CHECK(snapshot.inventory.items[0].rarity ==
        static_cast<std::uint8_t>(Rarity::Rare));
  CHECK(snapshot.inventory.items[0].itemLevel == 7);
  CHECK(snapshot.inventory.items[0].textureId == 555);
  REQUIRE(snapshot.inventory.items[0].affixes.size() == 1);
  CHECK(snapshot.inventory.items[0].affixes[0].tier == 2);
  CHECK(snapshot.inventory.items[0].affixes[0].isPrefix);

  CHECK(snapshot.inventory.items[1].domainId == entt::to_integral(itemB));
  CHECK(snapshot.inventory.items[1].inventoryIndex == 5);

  CHECK(snapshot.inventory.bagSlots[0].domainId == entt::to_integral(bag));
  CHECK(snapshot.inventory.bagSlots[1].domainId == ui::kInvalidDomainId);
}

TEST_CASE("[Unit] GameUiSnapshot - equipment view lists occupied slots") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  const entt::entity weapon = registry.create();
  registry.emplace<ItemComponent>(weapon);
  auto& equipment = registry.emplace<EquipmentComponent>(player);
  equipment.Set(EquipmentSlot::MainHand, weapon);
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.equipment.size() == 1);
  CHECK(snapshot.equipment[0].slotIndex ==
        static_cast<std::uint8_t>(EquipmentSlot::MainHand));
  CHECK(snapshot.equipment[0].domainId == entt::to_integral(weapon));
}

TEST_CASE("[Unit] GameUiSnapshot - stash view carries slots and unlock cost") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  auto& stash = registry.emplace<PersonalStashComponent>(player);
  stash.tabs[0].type = StashTabType::Material;
  stash.tabs[0].iconId = 77;
  const entt::entity stashItem = registry.create();
  auto& stashItemComp = registry.emplace<ItemComponent>(stashItem);
  stashItemComp.textureId = 66;
  stashItemComp.rarity = Rarity::Epic;
  stashItemComp.quantity = 3;
  stash.tabs[0].items[4] = stashItem;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.stash.tabs.size() == 1);
  const auto& tab = snapshot.stash.tabs[0];
  CHECK(tab.tabType == static_cast<std::uint8_t>(StashTabType::Material));
  CHECK(tab.iconId == 77);
  REQUIRE(tab.slots.size() == 1);
  CHECK(tab.slots[0].slotIndex == 4);
  CHECK(tab.slots[0].domainId == entt::to_integral(stashItem));
  CHECK(tab.slots[0].textureId == 66);
  CHECK(tab.slots[0].rarity == static_cast<std::uint8_t>(Rarity::Epic));
  CHECK(tab.slots[0].quantity == 3);
  CHECK(snapshot.stash.unlockedTabs == 1);
  CHECK(snapshot.stash.nextUnlockCost == 5000); // StashConfig table[1].
}

TEST_CASE("[Unit] GameUiSnapshot - crafting view merges bank and options") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  auto& bank = registry.emplace<MaterialBankComponent>(player);
  bank.Add(4001, 5);
  bank.Add(4005, 2);

  const entt::entity forgeTarget = AddItemToInventory(registry, player, 9, 0);
  ui::GameUiSnapshotOptions options;
  options.forgeTarget = entt::to_integral(forgeTarget);
  options.salvageItem = 0xDEAD; // Not resolvable: omitted from displayed items.
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry, options);

  REQUIRE(snapshot.crafting.materials.size() == 2);
  CHECK(snapshot.crafting.materials[0].materialId == 4001);
  CHECK(snapshot.crafting.materials[0].count == 5);
  CHECK(snapshot.crafting.materials[1].materialId == 4005);
  CHECK(snapshot.crafting.materials[1].count == 2);

  CHECK(snapshot.crafting.forgeTarget == entt::to_integral(forgeTarget));
  CHECK(snapshot.crafting.salvageItem == 0xDEAD);
  // The forge target resolves through the inventory cache...
  REQUIRE(snapshot.displayedItems.size() == 1);
  CHECK(snapshot.displayedItems[0].domainId == entt::to_integral(forgeTarget));
  CHECK(snapshot.displayedItems[0].inventoryIndex == 0);
  // ...while the unresolvable salvage id contributes nothing.
}

TEST_CASE("[Unit] GameUiSnapshot - skill bar and skill tree are populated") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 7;
  active.slots[0].cooldown = 2.5f;
  active.slots[0].current_charges = 3;
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].bonus_levels = 2;
  active.available_talent_points = 4;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.skillBar.slots.size() == 1);
  CHECK(snapshot.skillBar.slots[0].skillId == 7);
  CHECK(snapshot.skillBar.slots[0].slotIndex == 0);
  CHECK(snapshot.skillBar.slots[0].cooldown == 2.5f);
  CHECK(snapshot.skillBar.slots[0].currentCharges == 3);
  CHECK(snapshot.skillBar.availableTalentPoints == 4);

  // Specialized skill 9 at level 1 + bonus 2 = 3; active slot skill 7 at
  // level 1 is added afterwards (slot order, deduplicated).
  REQUIRE(snapshot.skillTree.skills.size() == 2);
  CHECK(snapshot.skillTree.skills[0].skillId == 9);
  CHECK(snapshot.skillTree.skills[0].level == 3);
  CHECK(snapshot.skillTree.skills[1].skillId == 7);
  CHECK(snapshot.skillTree.skills[1].level == 1);
  CHECK(snapshot.skillTree.availableSkillPoints == 1);
}

TEST_CASE("[Unit] GameUiSnapshot - astrolabe view is a value snapshot") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  auto& astro = registry.emplace<AstrolabeComponent>(player);
  astro.available_points = 6;
  astro.mainProfession = 2;
  astro.professionAffinity = {1, 2, 3, 4, 5, 6};
  astro.activated_nodes.insert(11);
  astro.activated_nodes.insert(22);
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  CHECK(snapshot.astrolabe.present);
  CHECK(snapshot.astrolabe.availablePoints == 6);
  CHECK(snapshot.astrolabe.mainProfession == 2);
  CHECK(snapshot.astrolabe.professionAffinity[0] == 1);
  CHECK(snapshot.astrolabe.professionAffinity[5] == 6);
  REQUIRE(snapshot.astrolabe.activatedNodes.size() == 2);
  CHECK(snapshot.astrolabe.activatedNodes[0] == 11);
  CHECK(snapshot.astrolabe.activatedNodes[1] == 22);
}

TEST_CASE("[Unit] GameUiSnapshot - monster health bars mark elites") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  const entt::entity normal = registry.create();
  registry.emplace<EnemyTag>(normal);
  auto& hpNormal = registry.emplace<HealthComponent>(normal);
  hpNormal.current = 10.0f;
  hpNormal.max = 50.0f;
  const entt::entity elite = registry.create();
  registry.emplace<EnemyTag>(elite);
  auto& hpElite = registry.emplace<HealthComponent>(elite);
  hpElite.current = 90.0f;
  hpElite.max = 100.0f;
  // R5 adaptation: isElite now means rarity > NORMAL (matching the legacy
  // MonsterHealthBarSystem), so the test must tag an actual elite rarity.
  registry.emplace<EnemyRarityComponent>(
      elite, EnemyRarityComponent::Rarity::ELITE);
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  REQUIRE(snapshot.monsters.size() == 2);
  bool foundNormal = false;
  bool foundElite = false;
  for (const auto& monster : snapshot.monsters) {
    if (monster.domainId == entt::to_integral(normal)) {
      foundNormal = true;
      CHECK(monster.current == 10.0f);
      CHECK(monster.max == 50.0f);
      CHECK_FALSE(monster.isElite);
    } else if (monster.domainId == entt::to_integral(elite)) {
      foundElite = true;
      CHECK(monster.current == 90.0f);
      CHECK(monster.max == 100.0f);
      CHECK(monster.isElite);
    }
  }
  CHECK(foundNormal);
  CHECK(foundElite);
}

TEST_CASE("[Unit] GameUiSnapshot - pickups are sorted nearest first") {
  entt::registry registry;
  CreatePlayer(registry); // At (10, 20).
  const entt::entity far = AddGroundItem(registry, 1, 400.0f, 400.0f);
  const entt::entity near = AddGroundItem(registry, 2, 15.0f, 20.0f);
  const entt::entity mid = AddGroundItem(registry, 3, 30.0f, 40.0f);
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);

  // Far item (beyond 180 units) is excluded.
  bool farIncluded = false;
  for (const auto& pickup : snapshot.pickups) {
    if (pickup.domainId == entt::to_integral(far)) {
      farIncluded = true;
    }
  }
  CHECK_FALSE(farIncluded);

  REQUIRE(snapshot.pickups.size() == 2);
  CHECK(snapshot.pickups[0].domainId == entt::to_integral(near));
  CHECK(snapshot.pickups[1].domainId == entt::to_integral(mid));
  CHECK(snapshot.pickups[0].distance < snapshot.pickups[1].distance);
  CHECK(snapshot.pickups[0].source == ui::GameUiPickupSource::World);
}

TEST_CASE("[Unit] GameUiSnapshot - displayed items deduplicate by domain id") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  const entt::entity item = AddItemToInventory(registry, player, 1, 0);
  const entt::entity ground = AddGroundItem(registry, 2, 15.0f, 20.0f);
  ui::GameUiSnapshotOptions options;
  options.hoveredItem = entt::to_integral(item);
  options.draggedItem = entt::to_integral(item); // Same id: deduped.
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry, options);

  REQUIRE(snapshot.displayedItems.size() == 1);
  CHECK(snapshot.displayedItems[0].domainId == entt::to_integral(item));
  CHECK(snapshot.tooltip.hoveredItem == entt::to_integral(item));

  ui::GameUiSnapshotOptions groundOptions;
  groundOptions.draggedItem = entt::to_integral(ground);
  const ui::GameUiSnapshot groundSnapshot = builder.Build(registry, groundOptions);
  REQUIRE(groundSnapshot.displayedItems.size() == 1);
  CHECK(groundSnapshot.displayedItems[0].domainId == entt::to_integral(ground));
}

TEST_CASE("[Unit] GameUiSnapshot - snapshot survives entity destruction") {
  entt::registry registry;
  const entt::entity player = CreatePlayer(registry);
  const entt::entity item = AddItemToInventory(registry, player, 42, 0);
  const entt::entity monster = registry.create();
  registry.emplace<EnemyTag>(monster);
  auto& hp = registry.emplace<HealthComponent>(monster);
  hp.current = 5.0f;
  hp.max = 9.0f;
  ui::GameUiSnapshotBuilder builder;

  const ui::GameUiSnapshot snapshot = builder.Build(registry);
  const std::uint64_t itemDomain = entt::to_integral(item);
  const std::uint64_t monsterDomain = entt::to_integral(monster);

  // Mutate/destroy the gameplay entities after the build.
  registry.destroy(item);
  registry.destroy(monster);
  registry.get<InventoryComponent>(player).items[0] = entt::null;

  // The snapshot holds plain values: nothing dangles.
  REQUIRE(snapshot.inventory.items.size() == 1);
  CHECK(snapshot.inventory.items[0].domainId == itemDomain);
  CHECK(snapshot.inventory.items[0].itemId == 42);
  REQUIRE(snapshot.monsters.size() == 1);
  CHECK(snapshot.monsters[0].domainId == monsterDomain);
  CHECK(snapshot.monsters[0].current == 5.0f);
}

} // namespace
} // namespace NoMoreDay
