#include "TestCommon.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/SalvageSystem.hpp"

TEST_CASE("[Unit] ItemFeedbackP3 - createItem applies deterministic fallback visuals") {
  entt::registry registry;

  ItemComponent item;
  item.id = 101;
  item.type = ItemType::Consumable;
  item.textureId = 0;

  const entt::entity created =
      InventorySystem::createItem(registry, item, 10.0f, 20.0f);

  REQUIRE(registry.valid(created));
  CHECK(registry.all_of<ItemComponent>(created));
  CHECK(registry.all_of<Position>(created));
  CHECK(registry.all_of<ColorComponent>(created));

  const Color color = registry.get<ColorComponent>(created).color;
  CHECK(color.r == RED.r);
  CHECK(color.g == RED.g);
  CHECK(color.b == RED.b);
}

TEST_CASE("[Unit] ItemFeedbackP3 - bag and potions provide icon-ready metadata") {
  TestSetupScope scope;
  entt::registry registry;

  const entt::entity bag = ItemFactory::createBag(registry, 1, Rarity::Common);
  const auto &bagItem = registry.get<ItemComponent>(bag);

  CHECK(bagItem.textureId != 0);
  const bool bagHasVisual = registry.any_of<SpriteComponent>(bag) ||
                            registry.any_of<ColorComponent>(bag);
  CHECK(bagHasVisual);

  const entt::entity hpPotion = ItemFactory::createPotion(registry, 0, 2);
  const entt::entity mpPotion = ItemFactory::createPotion(registry, 1, 2);

  const auto &hp = registry.get<ItemComponent>(hpPotion);
  const auto &mp = registry.get<ItemComponent>(mpPotion);

  CHECK(hp.textureId != 0);
  CHECK(mp.textureId != 0);
  CHECK(hp.textureId != mp.textureId);
  const bool hpHasVisual = registry.any_of<SpriteComponent>(hpPotion) ||
                           registry.any_of<ColorComponent>(hpPotion);
  const bool mpHasVisual = registry.any_of<SpriteComponent>(mpPotion) ||
                           registry.any_of<ColorComponent>(mpPotion);
  CHECK(hpHasVisual);
  CHECK(mpHasVisual);
}

TEST_CASE("[Unit] ItemFeedbackP3 - salvage emits feedback effect and clears inventory reference") {
  entt::registry registry;
  const entt::entity player = registry.create();
  auto &inventory = registry.emplace<InventoryComponent>(player);
  registry.emplace<MaterialBankComponent>(player);
  registry.emplace<Position>(player, 42.0f, 84.0f);

  const entt::entity item = registry.create();
  auto &itemComp = registry.emplace<ItemComponent>(item);
  itemComp.name = "Test Magic Sword";
  itemComp.type = ItemType::Weapon;
  itemComp.rarity = Rarity::Magic;
  registry.emplace<Position>(item, 10.0f, 20.0f);
  inventory.items[0] = item;

  SalvageSystem::Execute(registry, item, player);

  CHECK_FALSE(registry.valid(item));
  CHECK(inventory.items[0] == static_cast<entt::entity>(entt::null));

  auto vfxView = registry.view<VisualEffect, Position>();
  REQUIRE(vfxView.begin() != vfxView.end());

  const entt::entity fxEntity = *vfxView.begin();
  const auto &effect = vfxView.get<VisualEffect>(fxEntity);
  CHECK(effect.type == VisualEffectType::GoldSparkle);
}
