#include "core/logging/Logger.hpp"
#include "doctest.h"
#include "engine/persistence/SaveManager.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/data/SerializedItem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("Item Restoration - Deterministic Attributes") {
  entt::registry registry;
  ItemFactory::initialize(); // This might need more setup depending on what it
                             // loads
  // For unit testing, we might need to mock some registries if ItemFactory
  // depends on them

  // 1. Create a random item through the factory
  auto originalEntity = ItemFactory::createWeapon(registry, 10, Rarity::Rare);
  REQUIRE(registry.valid(originalEntity));

  const auto &originalItem = registry.get<ItemComponent>(originalEntity);
  float origAttack = originalItem.attack;
  std::string origName = originalItem.name;
  auto origRarity = originalItem.rarity;
  auto origAffixCount = originalItem.affixes.size();

  // 2. Mock a Snapshot by manually calling SaveManager logic or a helper
  // Since SaveManager::createSnapshot is private in some designs or requires
  // complex setup, we manually use the logic we implemented.

  // We can use a trick: save to DTO and then restore.
  // Let's use a helper in the test to convert Item to DTO.

  auto serializeItemHelper = [](entt::registry &reg, entt::entity e) {
    SerializedItem dto;
    const auto &item = reg.get<ItemComponent>(e);
    dto.itemId = item.id;
    dto.name = item.name;
    dto.type = item.type;
    dto.textureId = item.textureId;
    dto.quantity = item.quantity;
    dto.stats.rarity = item.rarity;
    dto.stats.slot = item.slot;
    dto.stats.attack = item.attack;
    dto.stats.defense = item.defense;
    dto.stats.forgingPotential = item.forgingPotential;
    dto.stats.legendaryPotential = item.legendaryPotential;
    dto.stats.value = item.value;

    for (const auto &aff : item.affixes) {
      SerializedItem::SavedAffix sAff;
      sAff.type = aff.type;
      sAff.tier = aff.tier;
      sAff.value = aff.value;
      sAff.isPrefix = aff.isPrefix;
      sAff.isLegendary = aff.isLegendary;
      // sAff.name = aff.name;
      sAff.required_tags = aff.required_tags;
      dto.affixes.push_back(sAff);
    }
    return dto;
  };

  auto dto = serializeItemHelper(registry, originalEntity);

  // 3. Destroy original and Restore
  registry.destroy(originalEntity);

  auto restoredEntity = ItemFactory::restoreItem(registry, dto);

  // 4. Verification
  REQUIRE(registry.valid(restoredEntity));
  const auto &restoredItem = registry.get<ItemComponent>(restoredEntity);

  CHECK(restoredItem.name == origName);
  CHECK(restoredItem.attack == origAttack);
  CHECK(restoredItem.rarity == origRarity);
  CHECK(restoredItem.affixes.size() == origAffixCount);

  for (size_t i = 0; i < origAffixCount; ++i) {
    CHECK(restoredItem.affixes[i].type == dto.affixes[i].type);
    CHECK(restoredItem.affixes[i].value == dto.affixes[i].value);
  }
}

TEST_CASE("SaveManager - Snapshot & Full Recovery") {
  entt::registry registry;
  tf::Executor executor;
  SaveManager::Get().Initialize(&executor);

  // Setup a player with some dummy data
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 100.0f, 200.0f);
  registry.emplace<PrimaryStats>(player);
  auto &inv = registry.emplace<InventoryComponent>(player);
  inv.gold = 1337;

  // Add an item to inventory
  // (Assuming ItemFactory::initialize() and assets are ready,
  // but for unit test we might just manually add ItemComponent)
  auto item = registry.create();
  ItemComponent ic;
  ic.name = "Test Sword";
  ic.attack = 42.0f;
  registry.emplace<ItemComponent>(item, ic);
  inv.items[0] = item;

  // 1. Snapshot
  CharacterSaveData data = SaveManager::Get().createSnapshot(registry);

  CHECK(data.position.x == 100.0f);
  CHECK(data.gold == 1337);
  REQUIRE(data.inventory.size() == 1);
  CHECK(data.inventory[0].name == "Test Sword");

  // 2. Restore
  entt::registry newRegistry;
  SaveManager::Get().restoreFromSnapshot(newRegistry, data);

  auto newView = newRegistry.view<PlayerTag, Position, InventoryComponent>();
  REQUIRE(newView.begin() != newView.end());
  auto newPlayer = newView.front();

  CHECK(newRegistry.get<Position>(newPlayer).x == 100.0f);
  CHECK(newRegistry.get<InventoryComponent>(newPlayer).gold == 1337);

  auto restoredItemEntity =
      newRegistry.get<InventoryComponent>(newPlayer).items[0];
  REQUIRE(newRegistry.valid(restoredItemEntity));
  CHECK(newRegistry.get<ItemComponent>(restoredItemEntity).name ==
        "Test Sword");
  CHECK(newRegistry.get<ItemComponent>(restoredItemEntity).attack == 42.0f);

  // Reset singleton to prevent dangling pointer for subsequent tests
  SaveManager::Get().Initialize(nullptr);
}
