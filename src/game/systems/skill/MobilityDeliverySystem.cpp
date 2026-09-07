#include "game/systems/skill/MobilityDeliverySystem.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <vector>

namespace NoMoreDay {

void MobilityDeliverySystem::Update(entt::registry &registry, float dt) {
  // 1. 处理位移冲刺 (MobilityComponent)
  static thread_local std::vector<entt::entity> s_mobility_finished;
  s_mobility_finished.clear();

  auto mob_view = registry.view<MobilityComponent>();
  for (auto entity : mob_view) {
    auto &mob = mob_view.get<MobilityComponent>(entity);
    mob.timer += dt;

    entt::entity target = (mob.target_entity != entt::null && registry.valid(mob.target_entity))
                              ? mob.target_entity
                              : entity;

    if (registry.valid(target)) {
      bool handledByDash = false;
      if (const auto *dash = registry.try_get<DashComponent>(target)) {
        if (dash->isDashing) {
          handledByDash = true;
        }
      }
      if (!handledByDash) {
        if (auto *pos = registry.try_get<Position>(target)) {
          pos->x += mob.direction.x * mob.speed * dt;
          pos->y += mob.direction.y * mob.speed * dt;
        }
      }
      if (auto *vel = registry.try_get<Velocity>(target)) {
        vel->vx = mob.direction.x * mob.speed;
        vel->vy = mob.direction.y * mob.speed;
      }
    }

    if (mob.timer >= mob.duration) {
      s_mobility_finished.push_back(entity);

      if (registry.valid(target)) {
        if (auto *vel = registry.try_get<Velocity>(target)) {
          vel->vx = 0.0f;
          vel->vy = 0.0f;
        }

        // 冲刺结束时，若配置了残影节点，则自动生成残影实体 (Task 2.6)
        if (mob.spawn_clone_node > 0) {
          auto clone_ent = registry.create();
          if (const auto *target_pos = registry.try_get<Position>(target)) {
            registry.emplace<Position>(clone_ent, target_pos->x, target_pos->y);
          }
          PhantasmCloneComponent clone{};
          clone.creator = target;
          clone.skill_id = mob.skill_id;
          clone.delay_before_cast = 0.2f;
          clone.lifetime = 1.5f;
          clone.damage_scale = 0.5f;
          registry.emplace<PhantasmCloneComponent>(clone_ent, clone);
        }
      }
    }
  }

  for (auto e : s_mobility_finished) {
    if (registry.valid(e)) {
      registry.remove<MobilityComponent>(e);
    }
  }

  // 2. 处理伴随残影 (PhantasmCloneComponent - Task 2.6)
  static thread_local std::vector<entt::entity> s_clones_to_destroy;
  s_clones_to_destroy.clear();

  auto clone_view = registry.view<PhantasmCloneComponent>();
  for (auto entity : clone_view) {
    auto &clone = clone_view.get<PhantasmCloneComponent>(entity);
    clone.timer += dt;

    if (!clone.has_cast && clone.timer >= clone.delay_before_cast) {
      clone.has_cast = true;
      if (clone.skill_id != 0) {
        if (auto castFunc = SkillBehaviorRegistry::GetCast(clone.skill_id)) {
          SkillExecution exec{};
          exec.skill_id = clone.skill_id;
          exec.owner = entity;
          exec.has_snapshot = true;
          exec.snapshot = clone.snapshot;
          castFunc(registry, entity, exec);
        }
      }
    }

    if (clone.timer >= clone.lifetime) {
      s_clones_to_destroy.push_back(entity);
    }
  }

  for (auto e : s_clones_to_destroy) {
    if (registry.valid(e)) {
      registry.destroy(e);
    }
  }
}

} // namespace NoMoreDay
