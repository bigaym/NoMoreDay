#pragma once

#include <cstdint>
#include <vector>

namespace NoMoreDay {

struct EndgameModifierRuntimeComponent {
  std::vector<uint32_t> outgoing_modifier_ids;
  std::vector<uint32_t> incoming_modifier_ids;
};

} // namespace NoMoreDay
