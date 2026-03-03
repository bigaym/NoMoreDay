#include "game/systems/skill/SkillCastConstraintService.hpp"

#include "core/logging/Logger.hpp"
#include "game/data/SkillRegistry.hpp"

#include <algorithm>

namespace NoMoreDay {
namespace skill {

bool ValidateContractCastConstraints(const SkillRegistry &registryData,
                                     const SkillContract *contract,
                                     const SpecializedSkill *specialized,
                                     uint32_t skillId,
                                     std::vector<uint32_t> *allocatedTransmuters,
                                     std::vector<uint32_t> *allocatedTriggers) {
  if (!contract || !specialized) {
    return true;
  }

  uint32_t transmuterCount = 0;
  uint32_t triggerCount = 0;
  allocatedTransmuters->clear();
  allocatedTriggers->clear();

  for (const auto &[nodeId, points] : specialized->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *nodeContract = registryData.GetNodeContract(skillId, nodeId);
    if (!nodeContract) {
      continue;
    }
    if (nodeContract->role == SpecNodeRole::Transmuter) {
      ++transmuterCount;
      allocatedTransmuters->push_back(nodeId);
    } else if (nodeContract->role == SpecNodeRole::Trigger) {
      ++triggerCount;
      allocatedTriggers->push_back(nodeId);
    }
  }

  if (transmuterCount > contract->max_transmuters) {
    LOG_WARN("TryCast blocked: skill {} has {} transmuters > max {}", skillId,
             transmuterCount, static_cast<uint32_t>(contract->max_transmuters));
    return false;
  }
  if (triggerCount > contract->max_triggers) {
    LOG_WARN("TryCast blocked: skill {} has {} triggers > max {}", skillId,
             triggerCount, static_cast<uint32_t>(contract->max_triggers));
    return false;
  }

  if (allocatedTransmuters->size() > 1) {
    uint32_t selected = 0;
    for (const uint32_t preferred : contract->transmuter_node_ids) {
      if (preferred == 0) {
        continue;
      }
      if (std::find(allocatedTransmuters->begin(), allocatedTransmuters->end(),
                    preferred) != allocatedTransmuters->end()) {
        selected = preferred;
        break;
      }
    }
    if (selected == 0) {
      selected = allocatedTransmuters->front();
    }
    allocatedTransmuters->erase(
        std::remove_if(allocatedTransmuters->begin(), allocatedTransmuters->end(),
                       [selected](uint32_t nodeId) { return nodeId != selected; }),
        allocatedTransmuters->end());
    LOG_WARN("[SKILL_GUARD_TRANSMUTER_MUTEX] skill={} selected_transmuter={} "
             "conflicting_transmuters={}",
             skillId, selected, transmuterCount);
  }

  return true;
}

} // namespace skill
} // namespace NoMoreDay
