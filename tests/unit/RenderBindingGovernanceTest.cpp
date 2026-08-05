#include "doctest.h"

#include "engine/render/RenderConstants.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string ReadFileText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

} // namespace

TEST_CASE("[Unit] Render Binding Governance - No raw BindBase literal in src") {
  namespace fs = std::filesystem;
  const std::regex bindBaseLiteral(R"(BindBase\s*\(\s*\d+\s*\))");
  std::vector<std::string> offenders;

  for (const auto &entry : fs::recursive_directory_iterator("src")) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const fs::path path = entry.path();
    const std::string ext = path.extension().string();
    if (ext != ".cpp" && ext != ".hpp") {
      continue;
    }
    const std::string content = ReadFileText(path);
    if (std::regex_search(content, bindBaseLiteral)) {
      offenders.push_back(path.generic_string());
    }
  }

  for (const std::string &offender : offenders) {
    INFO("Offender: " << offender);
  }
  CHECK(offenders.empty());
}

TEST_CASE("[Unit] Render Binding Governance - Shader binding registry alignment") {
  using NoMoreDay::RenderConstants::Binding;

  const std::vector<std::pair<std::string, uint32_t>> checks = {
      {"assets/shaders/cull.compute",
       static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA)},
      {"assets/shaders/entity_mdi.vert",
       static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA)},
      {"assets/shaders/sh_skill_effect.vs",
       static_cast<uint32_t>(Binding::SSBO_SKILL_EFFECTS)},
      {"assets/shaders/vfx/popup.vert",
       static_cast<uint32_t>(Binding::SSBO_POPUP_DATA)},
      {"assets/shaders/ui/glyph.vert",
       static_cast<uint32_t>(Binding::SSBO_GLYPH_INSTANCE)},
      {"assets/shaders/trail/trail.vert",
       static_cast<uint32_t>(Binding::SSBO_TRAIL_HEADERS)},
      {"assets/shaders/postprocess/distortion_write.frag",
       static_cast<uint32_t>(Binding::SSBO_DISTORTION_DATA)},
      {"assets/shaders/vfx/holo_blade_instanced.vs",
       static_cast<uint32_t>(Binding::SSBO_HOLOBLADE_INSTANCE)},
  };

  for (const auto &[filePath, binding] : checks) {
    const std::string content = ReadFileText(filePath);
    REQUIRE(!content.empty());
    const std::string needle = "binding = " + std::to_string(binding);
    INFO("File: " << filePath << ", expected token: " << needle);
    CHECK(content.find(needle) != std::string::npos);
  }
}

