#include "TestCommon.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/SkillRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] SkillContract - Registry loading and validation") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);
    const auto *contract = registry.GetSkillContract(skill_id);
    REQUIRE(contract != nullptr);
    std::string error;
    CHECK(registry.ValidateSkillContract(skill_id, &error));
    CHECK(error.empty());
  }
}

TEST_CASE("[Integration] SkillContract - Compact mapping materialized") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  SUBCASE("Skill 1 trigger contract") {
    const auto *node = registry.GetNodeContract(1, 114);
    REQUIRE(node != nullptr);
    CHECK(node->role == SpecNodeRole::Trigger);
    CHECK(node->trigger.trigger_skill_id == 2);
    CHECK(node->scope_policy == ScopePolicy::SkillOnly);
  }

  SUBCASE("Skill 8 transmuter role") {
    const auto *node = registry.GetNodeContract(8, 870);
    REQUIRE(node != nullptr);
    CHECK(node->role == SpecNodeRole::Transmuter);
  }

  SUBCASE("Skill 9 global scope policy") {
    const auto *node = registry.GetNodeContract(9, 971);
    REQUIRE(node != nullptr);
    CHECK(node->scope_policy == ScopePolicy::GlobalWhileBuffActive);
    CHECK(node->resist_model == ResistModel::TypeD_StatToPenetration);
  }
}

} // namespace NoMoreDay
