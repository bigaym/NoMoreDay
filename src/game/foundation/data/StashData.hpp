#pragma once

#include "game/foundation/data/SerializedItem.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// Enum serialization
NLOHMANN_JSON_SERIALIZE_ENUM(StashTabType, {
    {StashTabType::Normal, 0},
    {StashTabType::Equipment, 1},
    {StashTabType::Material, 2},
    {StashTabType::Runeword, 3},
    {StashTabType::Custom, 4},
})

struct SerializedStashSlot {
    int slotIndex;
    SerializedItem item;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedStashSlot, slotIndex, item)

struct SerializedStashTab {
    std::string name;
    StashTabType type = StashTabType::Normal;
    uint32_t iconId = 0;
    uint32_t color = 0xFFFFFFFF;
    std::vector<SerializedStashSlot> items;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedStashTab, name, type, iconId, color, items)

struct SerializedStash {
    int unlockedTabs = 1;
    std::vector<SerializedStashTab> tabs;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedStash, unlockedTabs, tabs)

} // namespace NoMoreDay
