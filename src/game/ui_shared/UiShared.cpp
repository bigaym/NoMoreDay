#include "game/ui_shared/UiShared.hpp"

#include "engine/render/GPUData.hpp" // components::Colors

namespace NoMoreDay {

// --- 战利品空间网格（item 写 / render 读）---
std::unique_ptr<systems::SIMDSpatialGrid> UiShared::s_itemGrid;
bool UiShared::s_itemGridDirty = true;

// --- 可见战利品标签缓存（render 写 / ui 读）---
std::vector<UiShared::VisibleItemCache::ItemData>
    UiShared::VisibleItemCache::visibleItems;

// --- UI 主题/状态（ui 写 / render 读）---
Font UiShared::s_globalFont = {};
entt::entity UiShared::s_hoveredItem = entt::null;

entt::entity &UiShared::HoveredItem() { return s_hoveredItem; }

const Font &UiShared::GlobalFont() { return s_globalFont; }

void UiShared::SetGlobalFont(Font font) { s_globalFont = font; }

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

void UiShared::Init() {
  s_itemGrid = std::make_unique<systems::SIMDSpatialGrid>(256, 256, 128.0f);
  s_itemGridDirty = true;
}

void UiShared::Shutdown() { s_itemGrid = nullptr; }

} // namespace NoMoreDay
