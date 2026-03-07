#pragma once

#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"

namespace NoMoreDay::systems {

class BladeMasteryService {
public:
  static bool IsDebugUnlockOverrideEnabled();
  static void SetDebugUnlockOverrideEnabled(bool enabled);

  static bool HasBladeAscendantProfession(const entt::registry &registry,
                                          entt::entity entity);
  static int GetCurrentLevel(const entt::registry &registry, entt::entity entity);
  static void RefreshPlayerState(entt::registry &registry, entt::entity entity);
  static bool IsMasteryUnlocked(const entt::registry &registry, entt::entity entity,
                                BladeMasteryId mastery_id);
  static bool SelectMastery(entt::registry &registry, entt::entity entity,
                            BladeMasteryId mastery_id);
  static BladeMasteryId GetSelectedMastery(const entt::registry &registry,
                                           entt::entity entity);
  static bool IsSignatureSkillUnlocked(const entt::registry &registry,
                                       entt::entity entity, uint32_t skill_id);
};

} // namespace NoMoreDay::systems
