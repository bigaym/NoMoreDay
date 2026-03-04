#pragma once

#include "doctest.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace NoMoreDay::tests {

inline nlohmann::json LoadJsonArtifact(const std::filesystem::path &artifactPath) {
  INFO("Loading JSON artifact: " << artifactPath.string());
  REQUIRE(std::filesystem::exists(artifactPath));

  std::ifstream in(artifactPath, std::ios::binary);
  REQUIRE(in.is_open());

  nlohmann::json root = nlohmann::json::object();
  REQUIRE_NOTHROW(in >> root);
  return root;
}

} // namespace NoMoreDay::tests
