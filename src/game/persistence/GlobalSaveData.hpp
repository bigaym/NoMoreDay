#pragma once
#include "game/data/StashData.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

struct GlobalSaveData {
    SerializedStash sharedStash;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GlobalSaveData, sharedStash)

} // namespace NoMoreDay
