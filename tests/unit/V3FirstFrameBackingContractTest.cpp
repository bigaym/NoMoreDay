#include "doctest.h"

#include <array>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string ReadSourceRelative(const char *relativePath) {
  constexpr std::array<const char *, 4> kPrefixes = {
      "",
      "../",
      "../../",
      "../../../",
  };
  for (const char *prefix : kPrefixes) {
    std::ifstream in(std::string(prefix) + relativePath, std::ios::binary);
    if (!in) {
      continue;
    }
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
  }
  return {};
}

} // namespace

TEST_CASE("[Unit] V3 first frame - backing resources pre-allocated before snapshot") {
  const std::string renderSystem =
      ReadSourceRelative("src/engine/render/RenderSystem.cpp");
  const std::string shadowBuildCpp =
      ReadSourceRelative("src/engine/render/passes/ShadowBuildPass.cpp");
  const std::string shadowBuildHpp =
      ReadSourceRelative("src/engine/render/passes/ShadowBuildPass.hpp");
  const std::string clusterHpp =
      ReadSourceRelative("src/engine/render/lighting/ClusteredLightingState.hpp");

  REQUIRE_FALSE(renderSystem.empty());
  REQUIRE_FALSE(shadowBuildCpp.empty());
  REQUIRE_FALSE(shadowBuildHpp.empty());
  REQUIRE_FALSE(clusterHpp.empty());

  const size_t shadowPrealloc = renderSystem.find("EnsureBackingResources(");
  const size_t clusterPrealloc = renderSystem.find("EnsureBuffersAllocated(");
  const size_t snapshotCapture =
      renderSystem.find("importedBackings.push_back");

  CHECK(shadowPrealloc != std::string::npos);
  CHECK(clusterPrealloc != std::string::npos);
  CHECK(snapshotCapture != std::string::npos);
  CHECK(shadowPrealloc < snapshotCapture);
  CHECK(clusterPrealloc < snapshotCapture);

  CHECK(shadowBuildHpp.find("EnsureBackingResources") != std::string::npos);
  CHECK(shadowBuildCpp.find("EnsureBackingResources") != std::string::npos);
  CHECK(clusterHpp.find("EnsureBuffersAllocated") != std::string::npos);
}
