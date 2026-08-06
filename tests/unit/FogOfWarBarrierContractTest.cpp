#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::string ReadFogSource() {
  constexpr std::array<const char *, 4> kCandidates = {
      "src/game/systems/world/FogOfWarSystem.cpp",
      "../src/game/systems/world/FogOfWarSystem.cpp",
      "../../src/game/systems/world/FogOfWarSystem.cpp",
      "../../../src/game/systems/world/FogOfWarSystem.cpp",
  };

  for (const char *candidate : kCandidates) {
    std::ifstream in(candidate, std::ios::binary);
    if (!in) {
      continue;
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  }
  return {};
}
} // namespace

TEST_CASE("[Unit] FogOfWar - image writes use compute-to-fragment barrier") {
  const std::string source = ReadFogSource();

  REQUIRE_FALSE(source.empty());
  CHECK(source.find("DispatchComputeNoBarrier(groupsX, groupsY, 1)") !=
        std::string::npos);
  CHECK(source.find("ApplyComputeToFragmentBarrierTemplate()") !=
        std::string::npos);
  CHECK(source.find("DispatchCompute(groupsX, groupsY, 1)") ==
        std::string::npos);
}
