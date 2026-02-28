#include "doctest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> ReadAllBytes(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

std::vector<uint8_t> CompileModifierFixtureToBytes(const std::string &fixturePath) {
  namespace fs = std::filesystem;

  const fs::path outBin = fs::path("bin") / "tmp_modifier_fixture.bin";
  const fs::path outDebug = fs::path("bin") / "tmp_modifier_fixture.debug.json";

  const std::string command =
      "python scripts/gen_modifier_runtime_v2.py --input-dir \"" +
      fs::path(fixturePath).generic_string() + "\" --output-bin \"" +
      outBin.generic_string() + "\" --output-debug \"" +
      outDebug.generic_string() + "\" --check > NUL 2>&1";

  REQUIRE(std::system(command.c_str()) == 0);

  auto bytes = ReadAllBytes(outBin);
  std::error_code ec;
  (void)fs::remove(outBin, ec);
  (void)fs::remove(outDebug, ec);
  return bytes;
}

} // namespace

TEST_CASE("[Unit] ModifierCompiler - Deterministic bytes for same input") {
  auto a = CompileModifierFixtureToBytes("tests/fixtures/modifier_v2/basic");
  auto b = CompileModifierFixtureToBytes("tests/fixtures/modifier_v2/basic");
  CHECK(a == b);
}
