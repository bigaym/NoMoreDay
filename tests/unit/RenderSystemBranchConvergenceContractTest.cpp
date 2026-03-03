#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace {

std::string ReadTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  std::stringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

} // namespace

TEST_CASE("[Unit] RenderSystem Contracts - Offscreen post-process legacy fallback route removed") {
  const std::string source =
      ReadTextFile(std::filesystem::path("src") / "engine" / "render" /
                   "RenderSystem.cpp");
  REQUIRE(!source.empty());

  CHECK(source.find("offscreenPostProcessOnly") == std::string::npos);
  CHECK(source.find("!offscreenV3SafeMode ||") == std::string::npos);

  const std::regex strictPostProcessGate(
      R"(useHdrSceneBuffer\s*&&\s*!offscreenV3SafeMode\s*&&\s*g_postProcessPass\s*!=\s*nullptr)");
  CHECK(std::regex_search(source, strictPostProcessGate));
}
