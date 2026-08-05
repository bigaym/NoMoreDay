// Phase F (RG-4) contract: ShaderReloadGovernance is the single owner of
// reload state / failure-retry records. These tests pin:
//   - ComputeIncludeHash covers recursive #include chains (a change in an
//     included file changes the fingerprint),
//   - RecordReloadAttempt is applied to VS/FS and compute alike,
//   - a failed reload keeps the last successful fingerprint and bumps the
//     retry counter, and a later success resets the retry state.
#include "doctest.h"

#include "engine/render/debug/ShaderReloadGovernance.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Unique temp workspace per test-case so the singleton record map never
// collides across tests.
struct TempShaderWorkspace {
  fs::path root;

  explicit TempShaderWorkspace(const std::string &tag) {
    root = fs::temp_directory_path() /
           ("nmd_shader_reload_" + tag + "_" +
            std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::error_code ec;
    fs::create_directories(root, ec);
  }

  ~TempShaderWorkspace() {
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  std::string Write(const std::string &relPath, const std::string &content) {
    const fs::path full = root / relPath;
    std::error_code ec;
    fs::create_directories(full.parent_path(), ec);
    std::ofstream out(full, std::ios::binary);
    out << content;
    return full.string();
  }
};

} // namespace

TEST_CASE("[Unit] ShaderReloadGovernance - include hash is recursive") {
  using namespace NoMoreDay::render::debug;
  ShaderReloadGovernance::Get().Reset();

  TempShaderWorkspace ws("include_recursive");
  ws.Write("shaders/common/defs.glslinc", "const int kFoo = 1;\n");
  const std::string shaderPath = ws.Write(
      "shaders/main.frag",
      "#version 430\n#include \"common/defs.glslinc\"\nvoid main() {}\n");

  std::vector<std::string> chain;
  const uint64_t first =
      ShaderReloadGovernance::Get().ComputeIncludeHash(shaderPath, chain);

  // The include chain must contain the top-level shader plus the recursive
  // include file it pulls in.
  bool sawShader = false;
  bool sawInclude = false;
  for (const auto &entry : chain) {
    if (entry.find("main.frag") != std::string::npos) {
      sawShader = true;
    }
    if (entry.find("defs.glslinc") != std::string::npos) {
      sawInclude = true;
    }
  }
  CHECK(sawShader);
  CHECK(sawInclude);

  // Editing the *included* file must change the fingerprint.
  ws.Write("shaders/common/defs.glslinc", "const int kFoo = 2;\n");
  std::vector<std::string> chain2;
  const uint64_t second =
      ShaderReloadGovernance::Get().ComputeIncludeHash(shaderPath, chain2);
  CHECK(first != second);

  // The fingerprint is deterministic for unchanged content.
  std::vector<std::string> chain3;
  const uint64_t third =
      ShaderReloadGovernance::Get().ComputeIncludeHash(shaderPath, chain3);
  CHECK(second == third);
}

TEST_CASE("[Unit] ShaderReloadGovernance - failed reload keeps last successful fingerprint") {
  using namespace NoMoreDay::render::debug;
  ShaderReloadGovernance::Get().Reset();

  const std::string path = "fake/shaders/vfx/comp.frag";
  auto &gov = ShaderReloadGovernance::Get();

  gov.RecordReloadAttempt(path, true, 1000, {"a.glslinc"}, "PassA", "resA", "");
  ShaderReloadRecord rec = gov.GetRecord(path);
  CHECK(rec.lastSuccessfulHash == 1000);
  CHECK(rec.currentHash == 1000);
  CHECK(rec.isLastReloadSuccess);
  CHECK(rec.retryCount == 0);

  // A failed reload keeps the last successful fingerprint, records the new
  // (uncompiled) fingerprint and bumps the retry counter.
  gov.RecordReloadAttempt(path, false, 2000, {"a.glslinc"}, "PassA", "resA",
                          "driver boom");
  rec = gov.GetRecord(path);
  CHECK(rec.lastSuccessfulHash == 1000);
  CHECK(rec.currentHash == 2000);
  CHECK_FALSE(rec.isLastReloadSuccess);
  CHECK(rec.retryCount == 1);
  CHECK(rec.driverErrorMessage.find("driver boom") != std::string::npos);

  // The record surfaces through the failed-reload query.
  bool found = false;
  for (const auto &failed : gov.GetFailedReloadRecords()) {
    if (failed.filePath == path) {
      found = true;
    }
  }
  CHECK(found);

  // A later success adopts the new fingerprint and resets retry state.
  gov.RecordReloadAttempt(path, true, 3000, {"a.glslinc"}, "PassA", "resA", "");
  rec = gov.GetRecord(path);
  CHECK(rec.lastSuccessfulHash == 3000);
  CHECK(rec.retryCount == 0);
  CHECK(gov.GetFailedReloadRecords().empty());
}

TEST_CASE("[Unit] ShaderReloadGovernance - missing file hash is stable") {
  using namespace NoMoreDay::render::debug;
  ShaderReloadGovernance::Get().Reset();

  std::vector<std::string> chain;
  const uint64_t first = ShaderReloadGovernance::Get().ComputeIncludeHash(
      "definitely/missing/file.frag", chain);
  CHECK(first != 0);
  CHECK(chain.size() == 1);

  std::vector<std::string> chain2;
  const uint64_t second = ShaderReloadGovernance::Get().ComputeIncludeHash(
      "definitely/missing/file.frag", chain2);
  CHECK(first == second);
}
