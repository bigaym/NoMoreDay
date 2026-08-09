#pragma once

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillContract.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay {

class SkillRegistry;

namespace skill {

bool ValidateContractCastConstraints(const SkillRegistry &registryData,
                                     const SkillContract *contract,
                                     const SpecializedSkill *specialized,
                                     uint32_t skillId,
                                     std::vector<uint32_t> *allocatedTransmuters,
                                     std::vector<uint32_t> *allocatedTriggers);

} // namespace skill

} // namespace NoMoreDay
