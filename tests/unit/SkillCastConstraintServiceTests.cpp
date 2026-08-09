#include "TestCommon.hpp"

#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillCastConstraintService.hpp"

#include <algorithm>
#include <vector>

namespace NoMoreDay {
namespace {

std::vector<uint32_t> CollectNodeIdsByRole(
    const SkillContractDefinition *definition, SpecNodeRole role) {
  std::vector<uint32_t> nodeIds;
  if (!definition) {
    return nodeIds;
  }

  for (const auto &[nodeId, nodeContract] : definition->nodes) {
    if (nodeContract.role == role) {
      nodeIds.push_back(nodeId);
    }
  }
  std::sort(nodeIds.begin(), nodeIds.end());
  return nodeIds;
}

} // namespace

TEST_CASE("[Unit] SkillCastConstraintService - Contract guard evaluation") {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  SUBCASE("Mutual transmuters collapse to contract preferred node") {
    constexpr uint32_t kSkillId = 8;
    const auto *contract = SkillRegistry::Get().GetSkillContract(kSkillId);
    REQUIRE(contract != nullptr);
    REQUIRE(contract->transmuter_node_ids[0] != 0);
    REQUIRE(contract->transmuter_node_ids[1] != 0);

    SpecializedSkill specialized;
    specialized.skill_id = kSkillId;
    specialized.allocated_points[contract->transmuter_node_ids[0]] = 1;
    specialized.allocated_points[contract->transmuter_node_ids[1]] = 1;

    std::vector<uint32_t> allocatedTransmuters;
    std::vector<uint32_t> allocatedTriggers;
    CHECK(skill::ValidateContractCastConstraints(
        SkillRegistry::Get(), contract, &specialized, kSkillId,
        &allocatedTransmuters, &allocatedTriggers));

    REQUIRE(allocatedTransmuters.size() == 1);
    CHECK(allocatedTransmuters.front() == contract->transmuter_node_ids[0]);
    CHECK(allocatedTriggers.empty());
  }

  SUBCASE("Trigger limit blocks casts when trigger count exceeds max") {
    constexpr uint32_t kSkillId = 1;
    const auto *definition =
        SkillRegistry::Get().GetSkillContractDefinition(kSkillId);
    REQUIRE(definition != nullptr);

    const std::vector<uint32_t> triggerNodes =
        CollectNodeIdsByRole(definition, SpecNodeRole::Trigger);
    REQUIRE_FALSE(triggerNodes.empty());

    SkillContract strictContract = definition->contract;
    strictContract.max_triggers = 0;

    SpecializedSkill specialized;
    specialized.skill_id = kSkillId;
    specialized.allocated_points[triggerNodes.front()] = 1;

    std::vector<uint32_t> allocatedTransmuters;
    std::vector<uint32_t> allocatedTriggers;
    CHECK_FALSE(skill::ValidateContractCastConstraints(
        SkillRegistry::Get(), &strictContract, &specialized, kSkillId,
        &allocatedTransmuters, &allocatedTriggers));

    CHECK(allocatedTransmuters.empty());
    REQUIRE(allocatedTriggers.size() == 1);
    CHECK(allocatedTriggers.front() == triggerNodes.front());
  }

  SUBCASE("Null contract or specialization short-circuits as valid") {
    std::vector<uint32_t> allocatedTransmuters = {111u};
    std::vector<uint32_t> allocatedTriggers = {222u};

    CHECK(skill::ValidateContractCastConstraints(
        SkillRegistry::Get(), nullptr, nullptr, 999u, &allocatedTransmuters,
        &allocatedTriggers));

    CHECK(allocatedTransmuters.size() == 1);
    CHECK(allocatedTransmuters.front() == 111u);
    CHECK(allocatedTriggers.size() == 1);
    CHECK(allocatedTriggers.front() == 222u);
  }
}

} // namespace NoMoreDay
