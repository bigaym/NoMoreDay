#include "doctest.h"

#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NoMoreDay::data::SkillMechanicsRegistry;

// 临时配置一律写入系统临时目录子目录，避免污染仓库根目录。
std::filesystem::path WriteTempText(const std::string &fileName,
                                    std::string_view text) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "nmd_skill_mechanics_tests";
  std::filesystem::create_directories(dir);
  const std::filesystem::path path = dir / fileName;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  return path;
}

// 构造 12 个必需技能、节点 100 只含单个键的合法机制表；misspelledSkill 非 0 时
// 该技能使用错误键名 "probe_typo"，用于模拟代码与 JSON 不同步的漂移。
std::string BuildUniformMechanics(uint32_t misspelledSkill) {
  std::string json = "{";
  for (uint32_t id = 1; id <= 12; ++id) {
    if (id > 1) {
      json += ",";
    }
    const std::string key = (id == misspelledSkill) ? "probe_typo" : "probe";
    json += "\"" + std::to_string(id) + "\":{\"100\":{\"" + key + "\":1.0}}";
  }
  json += "}";
  return json;
}

// 与 BuildUniformMechanics(0) 完全匹配的 schema：登记 12 个正确三元组。
std::string BuildUniformSchema() {
  std::string json = "{\"version\":1,\"entries\":[";
  for (uint32_t id = 1; id <= 12; ++id) {
    if (id > 1) {
      json += ",";
    }
    json += "[" + std::to_string(id) + ",100,\"probe\"]";
  }
  json += "],\"dynamic_keys\":[],\"unreferenced\":[]}";
  return json;
}

bool Contains(const std::vector<std::string> &warnings,
              std::string_view needle) {
  for (const std::string &warning : warnings) {
    if (warning.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// 从测试工作目录逐级上溯定位仓库根（schema 生成脚本所在处）。
std::filesystem::path ResolveRepoRoot() {
  std::filesystem::path dir = std::filesystem::current_path();
  for (int depth = 0; depth < 6; ++depth) {
    if (std::filesystem::exists(dir / "scripts" /
                                "gen_skill_mechanics_schema.py")) {
      return dir;
    }
    if (!dir.has_parent_path()) {
      break;
    }
    dir = dir.parent_path();
  }
  throw std::runtime_error(
      "Unable to locate repo root (scripts/gen_skill_mechanics_schema.py)");
}

int RunCommand(const std::string &command) {
  return std::system(command.c_str());
}

// 返回可用的 Python 解释器名；不可用返回空串（跳过硬校验，见测试内说明）。
std::string ResolvePythonInterpreter() {
  for (const char *candidate : {"python", "python3"}) {
    const std::string probe = std::string(candidate) + " --version";
    if (RunCommand(probe) == 0) {
      return candidate;
    }
  }
  return {};
}

} // namespace

TEST_CASE("[Unit] SkillMechanicsKeySchema - Misspelled Key Is Diagnosed") {
  SkillMechanicsRegistry::Get().ResetForTests();
  const auto mechanics = WriteTempText("key_schema_typo_mechanics.json",
                                       BuildUniformMechanics(1));
  const auto schema =
      WriteTempText("key_schema_typo_schema.json", BuildUniformSchema());

  REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(mechanics.string(),
                                                      schema.string()));
  const auto &warnings = SkillMechanicsRegistry::Get().GetLastLoadWarnings();
  // 期望同时命中：JSON 出现未登记键名（probe_typo），以及代码登记的 (1,100,probe)
  // 在 JSON 中缺失。二者共同指向同一处键名漂移。
  CHECK(warnings.size() >= 2);
  CHECK(Contains(warnings, "probe_typo"));
  CHECK(Contains(warnings, "1:100:probe"));
}

TEST_CASE("[Unit] SkillMechanicsKeySchema - Valid Load Produces No Warnings") {
  SkillMechanicsRegistry::Get().ResetForTests();
  const auto mechanics =
      WriteTempText("key_schema_valid_mechanics.json", BuildUniformMechanics(0));
  const auto schema =
      WriteTempText("key_schema_valid_schema.json", BuildUniformSchema());

  REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(mechanics.string(),
                                                      schema.string()));
  // schema 与机制表逐项一致时不得产生任何诊断。
  CHECK(SkillMechanicsRegistry::Get().GetLastLoadWarnings().empty());
}

TEST_CASE("[Unit] SkillMechanicsKeySchema - On-Disk Manifest Matches Generator") {
  // 本用例仅校验磁盘 schema 与生成器一致（--check）；真实机制表的零告警语义
  // 已由上方合成数据用例覆盖，此处不重复断言真实表（其告警受代码读取覆盖率影响）。
  const std::string interpreter = ResolvePythonInterpreter();
  if (interpreter.empty()) {
    MESSAGE("python unavailable; skipping schema generator --check");
    return;
  }
  const std::filesystem::path root = ResolveRepoRoot();
  const std::filesystem::path script =
      root / "scripts" / "gen_skill_mechanics_schema.py";
  const std::string command =
      interpreter + " \"" + script.string() + "\" --check";
  CHECK(RunCommand(command) == 0);
}
