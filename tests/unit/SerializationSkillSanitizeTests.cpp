#include "doctest.h"

#include "game/foundation/components/SkillDefs.hpp"

#include <nlohmann/json.hpp>

namespace NoMoreDay {
namespace {

// 测试专用技能 id：与既有测试（990001/990002 等）错开，避免单例污染。
constexpr uint32_t kSanitizeValidSkillId = 990301;

// M4 回归：加载期剔除 0/负点数条目，保证「0 点即不存」不变量，使
// skills::ReadPoints > 0 与实际存在性判定等价（见 SkillPointAccess.hpp）。
TEST_CASE("[Unit] SpecializedSkill - from_json prunes non-positive allocated points") {
  const nlohmann::json j = nlohmann::json{
      {"skill_id", kSanitizeValidSkillId},
      {"bonus_levels", 0},
      {"allocated_points", {{900301, 0}, {900302, -2}, {900303, 3}}},
  };

  const SpecializedSkill spec = j.get<SpecializedSkill>();

  CHECK(spec.allocated_points.size() == 1);
  REQUIRE(spec.allocated_points.count(900303) == 1);
  CHECK(spec.allocated_points.at(900303) == 3);
}

}  // namespace
}  // namespace NoMoreDay
