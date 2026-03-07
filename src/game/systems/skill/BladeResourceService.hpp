#pragma once

#include "game/components/SkillDefs.hpp"

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
  static bool TryGrantSwordFlowCritBonus(entt::registry &registry,
                                         entt::entity entity,
                                         uint32_t source_skill_id,
                                         float current_time,
                                         float proc_roll);
  static void Update(entt::registry &registry, float dt);
  static bool ShouldAutoEmpowerOnCast(const entt::registry &registry,
                                      entt::entity entity);
  static void SyncLegacySwordIntent(entt::registry &registry, entt::entity entity);
};

} // namespace NoMoreDay::systems
