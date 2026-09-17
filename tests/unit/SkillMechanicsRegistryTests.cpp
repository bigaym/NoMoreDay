#include "doctest.h"

#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NoMoreDay::data::SkillMechanicsRegistry;

// 测试工作目录随构建方式而变，逐级向上探测仓库内的真实配置。
std::filesystem::path ResolveMechanicsPath() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/skill_mechanics.json",
      "../assets/data/skill_mechanics.json",
      "../../assets/data/skill_mechanics.json",
      "../../../assets/data/skill_mechanics.json",
  };
  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }
  throw std::runtime_error(
      "Unable to locate assets/data/skill_mechanics.json from test cwd");
}

// 临时配置一律写入系统临时目录子目录，避免污染仓库根目录。
std::filesystem::path WriteTempJson(const std::string &fileName,
                                    std::string_view jsonText) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "nmd_skill_mechanics_tests";
  std::filesystem::create_directories(dir);
  const std::filesystem::path path = dir / fileName;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out.write(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
  return path;
}

// 构造仅缺少 missingId 的合法配置骨架，用于验证必需技能检测。
std::string BuildConfigMissingSkill(uint32_t missingId) {
  std::string json = "{";
  bool first = true;
  for (uint32_t id = 1; id <= 12; ++id) {
    if (id == missingId) {
      continue;
    }
    if (!first) {
      json += ",";
    }
    first = false;
    json += "\"" + std::to_string(id) + "\":{\"1\":{\"probe\":1.0}}";
  }
  json += "}";
  return json;
}

} // namespace

TEST_CASE("[Unit] SkillMechanicsRegistry - Loads Real Config And Reads Values") {
  SkillMechanicsRegistry::Get().ResetForTests();
  const auto path = ResolveMechanicsPath();

  REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(path.string()));
  CHECK(SkillMechanicsRegistry::Get().HasNode(5, 512));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(
            5, 512, "fate_mark_chance_pct_per_point", -1.0f) > 0.0f);
  CHECK(SkillMechanicsRegistry::Get().GetFloat(
            5, 512, "fate_mark_chance_pct_per_point", -1.0f) ==
        doctest::Approx(0.20f));
  // 技能 7/8/9 的 0 号基础节点必须保留，运行时确有读取。
  CHECK(SkillMechanicsRegistry::Get().HasNode(7, 0));
  // 技能 10（七星斩）节点 1001 的暴击数值已迁移至 UMR 单源交付（记录 2010010）；
  // 机制表节点与键退役，加载器按缺失回退，防止旧键回流。
  CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(10, 1001));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(10, 1001,
                                               "crit_chance_per_point", -1.0f) ==
        doctest::Approx(-1.0f));
  // 技能 11（天剑降临）已纳入必需技能；节点 0 为技能级默认参数。
  CHECK(SkillMechanicsRegistry::Get().HasNode(11, 0));
  CHECK(SkillMechanicsRegistry::Get().HasNode(11, 1102));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(
            11, 1102, "impact_damage_per_point_per_tier", -1.0f) ==
        doctest::Approx(0.04f));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(11, 0, "field_damage_per_tier",
                                               -1.0f) ==
        doctest::Approx(0.08f));
  // 技能 12（血海）已纳入必需技能；节点 0 为技能级默认参数。
  CHECK(SkillMechanicsRegistry::Get().HasNode(12, 0));
  CHECK(SkillMechanicsRegistry::Get().HasNode(12, 1211));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(12, 1224,
                                               "resist_shred_per_point", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(12, 0, "low_life_threshold",
                                               -1.0f) ==
        doctest::Approx(0.35f));

  SkillMechanicsRegistry::Get().ResetForTests();
}

TEST_CASE("[Unit] SkillMechanicsRegistry - Missing Entry Returns Default") {
  SkillMechanicsRegistry::Get().ResetForTests();
  const auto path = ResolveMechanicsPath();
  REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(path.string()));

  // 已加载但键不存在：运行期回退到调用方默认值。
  CHECK(SkillMechanicsRegistry::Get().GetFloat(5, 512, "no_such_key", 42.0f) ==
        doctest::Approx(42.0f));
  CHECK(SkillMechanicsRegistry::Get().GetFloat(999, 999, "no_such_key", 7.0f) ==
        doctest::Approx(7.0f));
  CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(5, 999));

  SkillMechanicsRegistry::Get().ResetForTests();
}

TEST_CASE("[Unit] SkillMechanicsRegistry - Rejects Malformed Config") {
  const auto realPath = ResolveMechanicsPath();

  SUBCASE("Non-numeric top-level key") {
    const auto badPath = WriteTempJson(
        "bad_top_level.json",
        R"JSON({"version":1,"comment":"x","1":{"1":{"probe":1.0}},"not_a_skill":{"1":{"probe":1.0}}})JSON");
    SkillMechanicsRegistry::Get().ResetForTests();
    REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(realPath.string()));
    REQUIRE(SkillMechanicsRegistry::Get().HasNode(5, 512));

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    // 失败后必须清空状态，不能保留任何部分加载结果。
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(5, 512));
    CHECK(SkillMechanicsRegistry::Get().GetFloat(
              5, 512, "fate_mark_chance_pct_per_point", -1.0f) ==
          doctest::Approx(-1.0f));
  }

  SUBCASE("Missing required skill id 3") {
    const auto badPath =
        WriteTempJson("missing_skill.json", BuildConfigMissingSkill(3));
    SkillMechanicsRegistry::Get().ResetForTests();
    REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(realPath.string()));

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(5, 512));
  }

  SUBCASE("Missing required skill id 10") {
    const auto badPath =
        WriteTempJson("missing_skill_10.json", BuildConfigMissingSkill(10));
    SkillMechanicsRegistry::Get().ResetForTests();
    REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(realPath.string()));

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(10, 1001));
  }

  SUBCASE("Missing required skill id 11") {
    const auto badPath =
        WriteTempJson("missing_skill_11.json", BuildConfigMissingSkill(11));
    SkillMechanicsRegistry::Get().ResetForTests();
    REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(realPath.string()));

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(11, 1102));
  }

  SUBCASE("Missing required skill id 12") {
    const auto badPath =
        WriteTempJson("missing_skill_12.json", BuildConfigMissingSkill(12));
    SkillMechanicsRegistry::Get().ResetForTests();
    REQUIRE(SkillMechanicsRegistry::Get().LoadFromFile(realPath.string()));

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(12, 1224));
  }

  SUBCASE("Skill section is not an object") {
    const auto badPath =
        WriteTempJson("bad_skill_section.json", R"JSON({"1":5.0})JSON");
    SkillMechanicsRegistry::Get().ResetForTests();

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(1, 1));
  }

  SUBCASE("Node key is not an integer") {
    const auto badPath = WriteTempJson(
        "bad_node_key.json", R"JSON({"1":{"abc":{"probe":1.0}}})JSON");
    SkillMechanicsRegistry::Get().ResetForTests();

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(1, 1));
  }

  SUBCASE("Node value is not an object") {
    const auto badPath = WriteTempJson("bad_node_value.json",
                                       R"JSON({"1":{"112":42.0}})JSON");
    SkillMechanicsRegistry::Get().ResetForTests();

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(1, 112));
  }

  SUBCASE("Leaf value is not a number") {
    const auto badPath =
        WriteTempJson("bad_leaf.json",
                      R"JSON({"1":{"112":{"bad":"not_a_number"}}})JSON");
    SkillMechanicsRegistry::Get().ResetForTests();

    CHECK_FALSE(SkillMechanicsRegistry::Get().LoadFromFile(badPath.string()));
    CHECK_FALSE(SkillMechanicsRegistry::Get().HasNode(1, 112));
  }
}
