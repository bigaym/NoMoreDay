#include "game/systems/skill/BoomerangDeliverySystem.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"
#include <vector>

namespace NoMoreDay {

void BoomerangDeliverySystem::Update(entt::registry &registry,
                                     systems::SpatialHashGrid &grid,
                                     float dt)
{
  static thread_local std::vector<entt::entity> s_to_destroy;
  s_to_destroy.clear();

  auto view = registry.view<BoomerangComponent, Position, Velocity>();
  for (auto entity : view) {
    auto &bc = view.get<BoomerangComponent>(entity);
    auto &pos = view.get<Position>(entity);
    auto &vel = view.get<Velocity>(entity);

    switch (bc.phase) {
    case BoomerangPhase::Outward: {
      bc.returnTimer -= dt;
      if (bc.returnTimer <= 0.0f) {
        bc.phase = BoomerangPhase::HoverApex;
        bc.hover_timer = bc.hover_duration;
        bc.apex_position = {pos.x, pos.y};
        vel.vx = 0.0f;
        vel.vy = 0.0f;
      }
      break;
    }

    case BoomerangPhase::HoverApex: {
      vel.vx = 0.0f;
      vel.vy = 0.0f;
      bc.hover_timer -= dt;

      // 黑洞/引力场牵引
      if (bc.pull_radius > 0.0f && bc.pull_strength > 0.0f) {
        grid.query({pos.x, pos.y}, bc.pull_radius, [&](entt::entity target, const Position &tpos) {
          if (registry.any_of<EnemyTag>(target) && !registry.any_of<KilledTag>(target)) {
            if (auto *tvel = registry.try_get<Velocity>(target)) {
              Vector2 toCenter = Vector2Subtract({pos.x, pos.y}, {tpos.x, tpos.y});
              float dist = Vector2Length(toCenter);
              if (dist > 1.0f) {
                Vector2 pullDir = Vector2Scale(toCenter, 1.0f / dist);
                tvel->vx += pullDir.x * bc.pull_strength * dt;
                tvel->vy += pullDir.y * bc.pull_strength * dt;
              }
            }
          }
        });
      }

      if (bc.hover_timer <= 0.0f) {
        bc.phase = BoomerangPhase::Returning;
      }
      break;
    }

    case BoomerangPhase::Returning: {
      entt::entity targetEnt = registry.valid(bc.returnTarget) ? bc.returnTarget : bc.owner;
      if (registry.valid(targetEnt) && registry.all_of<Position>(targetEnt)) {
        const auto &targetPos = registry.get<Position>(targetEnt);
        Vector2 toTarget = Vector2Subtract({targetPos.x, targetPos.y}, {pos.x, pos.y});
        float dist = Vector2Length(toTarget);

        constexpr float kReturnCatchThreshold = 32.0f;
        if (dist < kReturnCatchThreshold) {
          // 接剑回调 (Catch by owner, 需节点 831 门控 flag 8)
          if (bc.catch_by_owner && registry.valid(bc.owner)) {
            const auto *p = SkillSystem::GetBakedSkillProfile(registry, bc.owner, bc.skill_id);
            const bool hasCatchRefund = p ? ((p->delivery.feature_flags & 8) != 0) : true;
            if (hasCatchRefund) {
              if (auto *active = registry.try_get<ActiveSkillsComponent>(bc.owner)) {
                // 返还冷却与剑意
                for (auto &slot : active->slots) {
                  if (slot.id == bc.skill_id && slot.cooldown > 0.0f) {
                    slot.cooldown = std::max(0.0f, slot.cooldown - 1.0f);
                  }
                }
              }
              SkillSystem::GainSwordIntent(registry, bc.owner, 1, bc.skill_id);
            }
          }
          s_to_destroy.push_back(entity);
        } else {
          float speed = (bc.returnSpeed > 0.1f) ? bc.returnSpeed : 600.0f;
          Vector2 dir = Vector2Scale(toTarget, 1.0f / dist);
          vel.vx = dir.x * speed;
          vel.vy = dir.y * speed;
        }
      } else {
        s_to_destroy.push_back(entity);
      }
      break;
    }
    }
  }

  for (auto e : s_to_destroy) {
    if (registry.valid(e)) {
      registry.destroy(e);
    }
  }
}

} // namespace NoMoreDay
