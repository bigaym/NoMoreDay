#include "game/foundation/ui_shared/UiShared.hpp"

#include "engine/render/GPUData.hpp" // components::Colors

namespace NoMoreDay {

Color UiShared::GetRarityColor(Rarity rarity) {
  switch (rarity) {
  case Rarity::Common:
    return components::Colors::RARITY_COMMON;
  case Rarity::Magic:
    return components::Colors::RARITY_MAGIC;
  case Rarity::Rare:
    return components::Colors::RARITY_RARE;
  case Rarity::Uncommon:
    return components::Colors::RARITY_UNCOMMON;
  case Rarity::Set:
    return components::Colors::RARITY_SET;
  case Rarity::Epic:
    return components::Colors::RARITY_EPIC;
  case Rarity::Legendary:
    return components::Colors::RARITY_LEGENDARY;
  case Rarity::Mythic:
    return components::Colors::RARITY_MYTHIC;
  case Rarity::Ancient:
    return components::Colors::RARITY_ANCIENT;
  default:
    return WHITE;
  }
}

} // namespace NoMoreDay
