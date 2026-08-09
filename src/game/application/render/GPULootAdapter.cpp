#include "game/application/render/GPULootAdapter.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kLootFlagGold = 1u << 0;
constexpr uint32_t kLootFlagItem = 1u << 1;
constexpr float kDefaultItemLabelOffsetY = -24.0f;
constexpr float kDefaultGoldLabelOffsetY = -20.0f;

uint32_t PackRarityColor(const NoMoreDay::Rarity rarity) {
  using namespace NoMoreDay::components::Colors;
  switch (rarity) {
  case NoMoreDay::Rarity::Common:
    return static_cast<uint32_t>(ColorToInt(RARITY_COMMON));
  case NoMoreDay::Rarity::Uncommon:
    return static_cast<uint32_t>(ColorToInt(RARITY_UNCOMMON));
  case NoMoreDay::Rarity::Magic:
    return static_cast<uint32_t>(ColorToInt(RARITY_MAGIC));
  case NoMoreDay::Rarity::Rare:
    return static_cast<uint32_t>(ColorToInt(RARITY_RARE));
  case NoMoreDay::Rarity::Set:
    return static_cast<uint32_t>(ColorToInt(RARITY_SET));
  case NoMoreDay::Rarity::Epic:
    return static_cast<uint32_t>(ColorToInt(RARITY_EPIC));
  case NoMoreDay::Rarity::Legendary:
    return static_cast<uint32_t>(ColorToInt(RARITY_LEGENDARY));
  case NoMoreDay::Rarity::Mythic:
    return static_cast<uint32_t>(ColorToInt(RARITY_MYTHIC));
  case NoMoreDay::Rarity::Ancient:
    return static_cast<uint32_t>(ColorToInt(RARITY_ANCIENT));
  }
  return static_cast<uint32_t>(ColorToInt(WHITE));
}

float ComputeGlowIntensity(const NoMoreDay::Rarity rarity) {
  switch (rarity) {
  case NoMoreDay::Rarity::Common:
  case NoMoreDay::Rarity::Uncommon:
    return 0.0f;
  case NoMoreDay::Rarity::Magic:
    return 0.20f;
  case NoMoreDay::Rarity::Rare:
    return 0.35f;
  case NoMoreDay::Rarity::Set:
  case NoMoreDay::Rarity::Epic:
    return 0.50f;
  case NoMoreDay::Rarity::Legendary:
  case NoMoreDay::Rarity::Mythic:
  case NoMoreDay::Rarity::Ancient:
    return 0.65f;
  }
  return 0.0f;
}

} // namespace

LootProjection GPULootAdapter::BuildLoot(entt::registry &registry) {
  LootProjection projection;

  auto view = registry.view<const LootTag, const Position>();
  uint32_t requiredCount = 0;
  for (const auto entity : view) {
    if (registry.any_of<NoMoreDay::ItemComponent, GoldComponent>(entity)) {
      ++requiredCount;
    }
  }
  projection.instances.reserve(requiredCount);

  for (const auto entity : view) {
    const auto &position = view.get<const Position>(entity);
    components::GPULootInstance instance = {};
    instance.worldPosX = position.x;
    instance.worldPosY = position.y;
    instance.labelOffsetX = 0.0f;
    instance.labelOffsetY = kDefaultItemLabelOffsetY;

    if (const auto *item =
            registry.try_get<const NoMoreDay::ItemComponent>(entity)) {
      instance.itemId = item->id;
      instance.rarityColor = PackRarityColor(item->rarity);
      instance.glowIntensity = ComputeGlowIntensity(item->rarity);
      instance.flags = kLootFlagItem;
    } else if (registry.try_get<const GoldComponent>(entity) != nullptr) {
      instance.itemId = 0u;
      instance.rarityColor = static_cast<uint32_t>(ColorToInt(GOLD));
      instance.glowIntensity = 0.35f;
      instance.flags = kLootFlagGold;
      instance.labelOffsetY = kDefaultGoldLabelOffsetY;
    } else {
      continue;
    }

    projection.instances.push_back(instance);
  }

  return projection;
}

} // namespace NoMoreDay
