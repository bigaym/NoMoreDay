#pragma once

#include "game/foundation/components/StashComponent.hpp"
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

} // namespace NoMoreDay
