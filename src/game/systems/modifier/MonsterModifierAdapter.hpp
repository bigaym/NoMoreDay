#pragma once

#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

#include <unordered_set>

namespace NoMoreDay {

class MonsterModifierAdapter {
public:
  struct MonsterAffixEventSet {
    std::unordered_set<uint32_t> onUpdateAffixIds;
    std::unordered_set<uint32_t> onHitAffixIds;
    std::unordered_set<uint32_t> onDeathAffixIds;

    [[nodiscard]] bool HasOnUpdate() const { return !onUpdateAffixIds.empty(); }
    [[nodiscard]] bool HasOnHit() const { return !onHitAffixIds.empty(); }
    [[nodiscard]] bool HasOnDeath() const { return !onDeathAffixIds.empty(); }
  };

  struct MonsterAffixBehaviorOpSet {
    std::unordered_set<uint16_t> onUpdateOpcodes;
    std::unordered_set<uint16_t> onHitOpcodes;
    std::unordered_set<uint16_t> onDeathOpcodes;

    [[nodiscard]] bool HasOnUpdate() const { return !onUpdateOpcodes.empty(); }
    [[nodiscard]] bool HasOnHit() const { return !onHitOpcodes.empty(); }
    [[nodiscard]] bool HasOnDeath() const { return !onDeathOpcodes.empty(); }

    [[nodiscard]] bool HasOnUpdateOpcode(const ModifierOpCode opcode) const {
      return onUpdateOpcodes.contains(static_cast<uint16_t>(opcode));
    }

    [[nodiscard]] bool HasOnHitOpcode(const ModifierOpCode opcode) const {
      return onHitOpcodes.contains(static_cast<uint16_t>(opcode));
    }

    [[nodiscard]] bool HasOnDeathOpcode(const ModifierOpCode opcode) const {
      return onDeathOpcodes.contains(static_cast<uint16_t>(opcode));
    }
  };

  [[nodiscard]] static ModifierDelta
  EvaluateAffixDelta(const MonsterAffixComponent &affixComponent);

  [[nodiscard]] static MonsterAffixEventSet
  EvaluateAffixEvents(const MonsterAffixComponent &affixComponent);

  [[nodiscard]] static MonsterAffixBehaviorOpSet
  EvaluateBehaviorOps(const MonsterAffixComponent &affixComponent);

  [[nodiscard]] static float
  GetBerserkWeaponDamageMultiplier(const MonsterAffixComponent &affixComponent);
};

} // namespace NoMoreDay
