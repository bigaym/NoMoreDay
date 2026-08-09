#pragma once

#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay::systems {

class BladeResourceService {
public:
  static bool HasBladeResource(const entt::registry &registry, entt::entity entity);
  static BladeResourceKind GetResourceKind(const entt::registry &registry,
                                           entt::entity entity);
  static void EnsureBladeResource(entt::registry &registry, entt::entity entity,
                                  BladeResourceKind kind, int max_resource,
                                  float grace_period, float decay_interval);
  static void RemoveBladeResource(entt::registry &registry, entt::entity entity);
  static void SetMaxResource(entt::registry &registry, entt::entity entity,
                             int max_resource);
  static bool Gain(entt::registry &registry, entt::entity entity, int amount,
                   uint32_t source_skill_id);
  static bool Consume(entt::registry &registry, entt::entity entity, int amount,
                      uint32_t source_skill_id);
  static int ConsumeUpTo(entt::registry &registry, entt::entity entity,
                         int amount, uint32_t source_skill_id);
  static bool TryGrantSwordFlowCritBonus(entt::registry &registry,
                                         entt::entity entity,
                                         uint32_t source_skill_id,
                                         float current_time,
                                         float proc_roll);
  static bool IsDemonBladeActive(const entt::registry &registry,
                                 entt::entity entity);
  static bool TrySpendLifeForDemonBladeCast(entt::registry &registry,
                                            entt::entity entity,
                                            float mana_cost,
                                            uint32_t source_skill_id);
  static bool TryGainBloodthirstOnLowLifeMeleeHit(entt::registry &registry,
                                                  entt::entity entity,
                                                  uint64_t tracking_key,
                                                  float current_time,
                                                  uint32_t source_skill_id);
  static bool TryGainBloodthirstFromOverflowHeal(entt::registry &registry,
                                                 entt::entity entity,
                                                 float attempted_heal,
                                                 float actual_heal,
                                                 uint32_t source_skill_id);
  static int ConsumeAll(entt::registry &registry, entt::entity entity,
                        uint32_t source_skill_id);
  static float GetBloodthirstDamageMultiplier(const entt::registry &registry,
                                              entt::entity entity);
  static float GetBloodthirstDamageTakenMultiplier(
      const entt::registry &registry, entt::entity entity);
  static bool TryConsumeSwordFlowRestartWindow(entt::registry &registry,
                                               entt::entity entity,
                                               uint32_t source_skill_id);
  static void Update(entt::registry &registry, float dt);
  static bool ShouldAutoEmpowerOnCast(const entt::registry &registry,
                                      entt::entity entity);
  static BladeAttunement GetHeavenlyAttunement(const entt::registry &registry,
                                               entt::entity entity);
  static Tag GetHeavenlyAttunementElementTag(const entt::registry &registry,
                                             entt::entity entity);
  static void SyncLegacySwordIntent(entt::registry &registry, entt::entity entity);
};

} // namespace NoMoreDay::systems
