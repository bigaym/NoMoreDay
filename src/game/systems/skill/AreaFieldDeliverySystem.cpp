#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "raymath.h"
#include <vector>

namespace NoMoreDay {

void AreaFieldDeliverySystem::Update(entt::registry &registry,
                                     systems::SpatialHashGrid &grid,
                                     float dt) {
  auto view = registry.view<AreaFieldComponent, Position>();
  static thread_local std::vector<entt::entity> s_to_destroy;
  s_to_destroy.clear();

  for (auto entity : view) {
    // Specialized fields (HeavenlySword and BloodSea) manage their own custom pulse damage calculations
    // and cleanup callbacks. Skip here to avoid double damage ticks and destruction race conditions.
    if (registry.any_of<HeavenlySwordFieldComponent, BloodSeaFieldComponent>(entity)) {
      continue;
    }

    auto &field = view.get<AreaFieldComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    field.remaining_duration -= dt;
    if (field.remaining_duration <= 0.0f) {
      s_to_destroy.push_back(entity);
      continue;
    }

    field.timer += dt;
    if (field.timer >= field.pulse_interval) {
      field.timer = 0.0f;

      if (field.payload_count == 0) {
        continue;
      }

      // 提交领域脉冲视觉表现
      if (field.source_skill_id != 0) {
        SkillVfxEvent vfx{};
        vfx.skillId = field.source_skill_id;
        vfx.castId = field.cast_id;
        vfx.type = SkillVfxEventType::CastImpact;
        vfx.origin = {pos.x, pos.y};
        vfx.target = {pos.x, pos.y};
        vfx.intensity = 1.0f;
        systems::GPUSkillEffectSystem::Get().SubmitSkillEvent(vfx);
      }

      const bool ownerIsEnemy = registry.valid(field.owner) && registry.any_of<EnemyTag>(field.owner);

      grid.query({pos.x, pos.y}, field.radius, [&](entt::entity target, const Position &tPos) {
        if (!registry.valid(target) || target == field.owner || registry.any_of<KilledTag>(target)) {
          return;
        }

        const bool targetIsEnemy = registry.any_of<EnemyTag>(target);
        if (ownerIsEnemy == targetIsEnemy) {
          return;
        }

        const float dx = tPos.x - pos.x;
        const float dy = tPos.y - pos.y;
        if (dx * dx + dy * dy > field.radius * field.radius) {
          return;
        }

        for (uint8_t i = 0; i < field.payload_count; ++i) {
          const auto &payload = field.payloads[i];
          if (payload.type == PayloadType::Damage) {
            DamageRequest req;
            req.attacker = field.owner;
            req.defender = target;
            req.skill_id = field.source_skill_id;
            req.source_entity = entity;
            req.added_effectiveness = payload.value_mult;
            req.additional_tags = payload.damage_tags;
            (void)ResolveDamage(registry, req, target);
          } else if (payload.type == PayloadType::Ailment) {
            if (payload.ailment_id != 0) {
              systems::AilmentApplyRequest applyReq;
              applyReq.ailment = static_cast<AilmentType>(payload.ailment_id);
              applyReq.source = field.owner;
              applyReq.magnitude = payload.value_mult;
              applyReq.duration = (payload.duration > 0.0f) ? payload.duration : 3.0f;
              (void)systems::AilmentApplier::Apply(registry, target, applyReq);
            }
          }
        }
      });
    }
  }

  for (auto entity : s_to_destroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

} // namespace NoMoreDay
