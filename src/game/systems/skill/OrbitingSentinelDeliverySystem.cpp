#include "game/systems/skill/OrbitingSentinelDeliverySystem.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "raymath.h"
#include <cmath>

namespace NoMoreDay {

void OrbitingSentinelDeliverySystem::Update(entt::registry &registry,
                                           systems::SpatialHashGrid &grid,
                                           float dt)
{
  auto view = registry.view<OrbitingSentinelComponent>();
  for (auto entity : view) {
    auto &sentinel = view.get<OrbitingSentinelComponent>(entity);

    // 1. 更新公转角速度
    sentinel.current_angle += sentinel.angular_velocity * dt;
    if (sentinel.current_angle >= 360.0f) {
      sentinel.current_angle -= 360.0f;
    } else if (sentinel.current_angle < 0.0f) {
      sentinel.current_angle += 360.0f;
    }

    // 2. 更新位置 (若锚定实体有效且具有 Position)
    if (registry.valid(sentinel.anchor_entity) && registry.all_of<Position>(sentinel.anchor_entity)) {
      const auto &anchorPos = registry.get<Position>(sentinel.anchor_entity);
      const float rad = sentinel.current_angle * DEG2RAD;
      const float targetX = anchorPos.x + cosf(rad) * sentinel.orbit_radius;
      const float targetY = anchorPos.y + sinf(rad) * sentinel.orbit_radius;

      // 仅当实体不是锚定实体自身时，更新其实体位置（防止污染施法者自身的 Position）
      if (entity != sentinel.anchor_entity) {
        if (auto *pos = registry.try_get<Position>(entity)) {
          pos->x = targetX;
          pos->y = targetY;
        }
      }

      const float checkX = (entity != sentinel.anchor_entity) ? targetX : anchorPos.x;
      const float checkY = (entity != sentinel.anchor_entity) ? targetY : anchorPos.y;

      // 3. 离体索敌射击 / 周期攻击 (消费 attack_scan_radius / attack_interval / damage_mult)
      if (sentinel.attack_scan_radius > 0.0f && sentinel.attack_interval > 0.0f) {
        sentinel.attack_timer -= dt;
        if (sentinel.attack_timer <= 0.0f) {
          sentinel.attack_timer = sentinel.attack_interval;
          const float scanRadiusSq = sentinel.attack_scan_radius * sentinel.attack_scan_radius;

          auto enemy_view = registry.view<EnemyTag, Position>();
          for (auto enemy_ent : enemy_view) {
            if (registry.any_of<KilledTag>(enemy_ent)) continue;
            const auto &epos = enemy_view.get<Position>(enemy_ent);
            if (Vector2DistanceSqr({checkX, checkY}, {epos.x, epos.y}) <= scanRadiusSq) {
              if (registry.valid(sentinel.anchor_entity) &&
                  registry.all_of<CombatStats>(sentinel.anchor_entity) &&
                  registry.all_of<CombatStats>(enemy_ent)) {
                const auto &attStats = registry.get<CombatStats>(sentinel.anchor_entity);
                float baseDmg = std::max(20.0f, (attStats.min_weapon_damage + attStats.max_weapon_damage) * 0.5f) * sentinel.damage_mult;
                DamagePool pool;
                pool.Add(Tag::Physical, baseDmg);
                DamageRequest req;
                req.origin = DamageOrigin::SecondaryProc;
                req.attacker = sentinel.anchor_entity;
                req.defender = enemy_ent;
                req.skill_id = sentinel.skill_id;
                req.base_pool = pool;
                req.additional_tags = Tag::Hit;
                req.source_entity = entity;
                (void)ResolveDamage(registry, req, sentinel.anchor_entity);
              }
              break;
            }
          }
        }
      }
    }
  }
}

} // namespace NoMoreDay
