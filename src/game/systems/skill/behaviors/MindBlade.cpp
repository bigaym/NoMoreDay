/**
 * @file MindBlade.cpp
 * @brief 心剑·无影 (ID 7) - 自动索敌高频技能
 *
 * 核心机制：独立于玩家动作的浮游剑，基于智力属性自动攻击。
 */

#include "MindBlade.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Projectile.hpp" // Added
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

// Implementations

void MindBlade::OnCast(entt::registry &registry, entt::entity owner,
                       SkillExecution &exec) {
  // Spawn a new Mind Blade entity relative to owner
  auto blade = registry.create();

  // Initial position slightly offset from owner
  Vector2 spawn_pos = exec.target_pos; // Usually mouse pos or owner pos
  if (registry.valid(owner) && registry.all_of<Position>(owner)) {
    const auto &owner_pos = registry.get<Position>(owner);
    spawn_pos = {owner_pos.x, owner_pos.y - 50.0f}; // Start above head
  }

  registry.emplace<Position>(blade, spawn_pos.x, spawn_pos.y);
  registry.emplace<Velocity>(blade, 0.0f, 0.0f);
  registry.emplace<LocalLevelTag>(blade);

  // Add components
  auto &mc = registry.emplace<MindBladeComponent>(blade);
  mc.owner = owner;
  mc.intelligence_scaling = 1.0f;
  mc.stack_count = 1;

  auto &ai = registry.emplace<MindBladeAI>(blade);
  ai.base_interval = 0.8f; // Fire every 0.8s
  ai.range = 600.0f;

  // Optional: Add visual component
  // registry.emplace<SpriteComponent>(blade, ...);

  LOG_INFO("Mind Blade (ID 9) spawned for entity {}", (uint32_t)owner);
}

bool MindBlade::Update(entt::registry &registry, entt::entity entity,
                       MindBladeAI &ai, MindBladeComponent &comp, float dt,
                       systems::SpatialHashGrid &grid) {
  if (!registry.valid(comp.owner)) {
    return false;
  }

  // 1. Movement: Soft follow owner
  if (registry.all_of<Position>(comp.owner)) {
    const auto &owner_pos = registry.get<Position>(comp.owner);
    auto &blade_pos = registry.get<Position>(entity);

    // Target hover position: Above head, oscillating
    float time = (float)GetTime();
    Vector2 hover_target = {owner_pos.x +
                                sinf(time * 2.0f + (float)entity) * 30.0f,
                            owner_pos.y - 60.0f + cosf(time * 1.5f) * 10.0f};

    // Lerp towards hover target
    blade_pos.x = Lerp(blade_pos.x, hover_target.x, dt * 5.0f);
    blade_pos.y = Lerp(blade_pos.y, hover_target.y, dt * 5.0f);
  }

  // 2. Targeting
  ai.retarget_timer -= dt;
  if (!registry.valid(ai.target) || ai.retarget_timer <= 0.0f) {
    ai.retarget_timer = 0.5f; // Check every 0.5s
    ai.target = entt::null;

    const auto &pos = registry.get<Position>(entity);

    // Query nearby enemies
    float min_dist_sq = ai.range * ai.range;

    grid.query({pos.x, pos.y}, ai.range,
               [&](entt::entity target_ent, const Position &t_pos) {
                 if (target_ent == comp.owner || target_ent == entity)
                   return;
                 if (!registry.all_of<EnemyTag, CombatStats>(target_ent))
                   return;
                 // Don't target dead things
                 if (registry.get<CombatStats>(target_ent).health <= 0)
                   return;

                 float dist_sq =
                     Vector2DistanceSqr({pos.x, pos.y}, {t_pos.x, t_pos.y});

                 if (dist_sq < min_dist_sq) {
                   min_dist_sq = dist_sq;
                   ai.target = target_ent;
                 }
               });
  }

  // 3. Attack
  ai.attack_timer += dt;
  if (registry.valid(ai.target) && ai.attack_timer >= ai.base_interval) {
    // Fire!
    ai.attack_timer = 0.0f;

    const auto &pos = registry.get<Position>(entity);
    const auto &t_pos = registry.get<Position>(ai.target);

    // Create Projectile
    auto proj = registry.create();
    registry.emplace<Position>(proj, pos.x, pos.y);
    registry.emplace<LocalLevelTag>(proj);

    Vector2 dir =
        Vector2Normalize(Vector2Subtract({t_pos.x, t_pos.y}, {pos.x, pos.y}));
    registry.emplace<Velocity>(proj, dir.x * 800.0f,
                               dir.y * 800.0f); // Fast projectile

    auto &p = registry.emplace<Projectile>(proj);
    p.owner = comp.owner; // Owner gets credit for damage

    // Copy owner stats for snapshot
    if (registry.all_of<CombatStats>(comp.owner)) {
      p.snapshot = registry.get<CombatStats>(comp.owner);
    }

    // Set Base Damage based on INT
    float base_dmg = 20.0f;
    if (registry.all_of<CombatStats>(comp.owner)) {
      base_dmg += registry.get<CombatStats>(comp.owner).effective_intelligence *
                  comp.intelligence_scaling;
    }

    // Override weapon damage in snapshot for this skill
    p.snapshot.min_weapon_damage = base_dmg;
    p.snapshot.max_weapon_damage = base_dmg;

    p.speed = 800.0f;
    p.lifeTime = 2.0f;
    p.radius = 10.0f;
  }

  return true;
}

REGISTER_SKILL_BEHAVIOR(MindBlade)

void RegisterMindBlade() {}

} // namespace NoMoreDay::skills
