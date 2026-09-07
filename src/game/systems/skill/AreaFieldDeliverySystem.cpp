#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "raymath.h"
#include <vector>

namespace NoMoreDay {

namespace {
constexpr float kSkyfallImpactEffectiveness = 1.5f;
constexpr float kSkyfallResidualFieldDuration = 5.0f;
constexpr float kSkyfallResidualFieldPulseInterval = 0.3f;
constexpr float kSkyfallResidualFieldDamageMult = 0.5f;
constexpr float kDefaultAilmentDuration = 3.0f;
constexpr float kDefaultSkillVfxIntensity = 1.0f;
} // namespace

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
        vfx.intensity = kDefaultSkillVfxIntensity;
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
              applyReq.duration = (payload.duration > 0.0f) ? payload.duration : kDefaultAilmentDuration;
              (void)systems::AilmentApplier::Apply(registry, target, applyReq);
            }
          }
        }
      });
    }
  }

  // 处理天降打击与流星轰击 (SkyfallImpactComponent - Task 2.6)
  auto skyfall_view = registry.view<SkyfallImpactComponent, Position>();
  for (auto entity : skyfall_view) {
    auto &skyfall = skyfall_view.get<SkyfallImpactComponent>(entity);
    auto &pos = skyfall_view.get<Position>(entity);
    skyfall.timer += dt;

    if (skyfall.waves_spawned < skyfall.wave_count) {
      const float nextWaveTime = skyfall.delay_before_impact + static_cast<float>(skyfall.waves_spawned) * skyfall.wave_interval;
      if (skyfall.timer >= nextWaveTime) {
        skyfall.waves_spawned++;

        grid.query({pos.x, pos.y}, skyfall.impact_radius, [&](entt::entity target, const Position &tPos) {
          if (!registry.valid(target) || target == skyfall.owner || registry.any_of<KilledTag>(target)) {
            return;
          }
          const bool ownerIsEnemy = registry.valid(skyfall.owner) && registry.any_of<EnemyTag>(skyfall.owner);
          const bool targetIsEnemy = registry.any_of<EnemyTag>(target);
          if (ownerIsEnemy != targetIsEnemy) {
            const auto *p = SkillSystem::GetBakedSkillProfile(registry, skyfall.owner, skyfall.skill_id);
            const float eff = (p && p->more_damage_mult > 0.0f)
                                  ? (kSkyfallImpactEffectiveness * p->more_damage_mult)
                                  : kSkyfallImpactEffectiveness;
            DamageRequest req;
            req.attacker = skyfall.owner;
            req.defender = target;
            req.skill_id = skyfall.skill_id;
            req.source_entity = entity;
            req.added_effectiveness = eff;
            if (p) {
              req.additional_tags = p->effective_tags;
            }
            (void)ResolveDamage(registry, req, target);
          }
        });
      }
    }

    if (skyfall.waves_spawned >= skyfall.wave_count) {
      if (skyfall.leave_field_skill_id != 0) {
        const auto *p = SkillSystem::GetBakedSkillProfile(registry, skyfall.owner, skyfall.leave_field_skill_id);
        const float resDuration = (p && p->delivery.duration > 0.0f)
                                      ? p->delivery.duration
                                      : kSkyfallResidualFieldDuration;
        const float resInterval = (p && p->delivery.sub_interval > 0.0f)
                                      ? p->delivery.sub_interval
                                      : kSkyfallResidualFieldPulseInterval;
        const float resDmgMult = (p && p->more_damage_mult > 0.0f)
                                     ? (kSkyfallResidualFieldDamageMult * p->more_damage_mult)
                                     : kSkyfallResidualFieldDamageMult;
        const float resRadius = (p && p->area_radius > 1.0f) ? p->area_radius : skyfall.impact_radius;
        AreaFieldComponent field{};
        field.owner = skyfall.owner;
        field.source_skill_id = skyfall.leave_field_skill_id;
        field.cast_id = skyfall.cast_id;
        field.remaining_duration = resDuration;
        field.pulse_interval = resInterval;
        field.radius = resRadius;
        field.payload_count = 1;
        field.payloads[0].type = PayloadType::Damage;
        field.payloads[0].value_mult = resDmgMult;
        if (p) {
          field.payloads[0].damage_tags = p->effective_tags;
        }
        registry.emplace_or_replace<AreaFieldComponent>(entity, field);
        registry.remove<SkyfallImpactComponent>(entity);
        continue;
      }
      s_to_destroy.push_back(entity);
    }
  }

  for (auto entity : s_to_destroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

} // namespace NoMoreDay
