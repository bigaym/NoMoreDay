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
#include <cfloat>
#include "engine/physics/SpatialGrid.hpp"      // Added
#include "engine/render/GPUParticleSystem.hpp" // Added
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp" // Added for Position, Vector2
#include "game/components/Projectile.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

namespace MindBladeNodes {
// 基础分支 / Base
constexpr uint32_t Focus = 700;      // 专注 / Focus
constexpr uint32_t Reserve = 701;    // 积蓄 / Reserve

// 心流分支 / Mindflow branch
constexpr uint32_t Mindflow = 710;   // 心流积蓄 / Mindflow
constexpr uint32_t Selfless = 711;   // 无我境界 / Selfless
constexpr uint32_t SwordWave = 712;  // 剑气激荡 / Sword Wave
constexpr uint32_t HeavenMan = 713;  // 天人合一 / Heaven and Man

// 锁定分支 / Lock branch
constexpr uint32_t MindLock = 730;   // 神识锁定 / Mind Lock
constexpr uint32_t Trackless = 731;  // 无影追踪 / Trackless
constexpr uint32_t WeakInsight = 732; // 破绽洞察 / Weak Insight
constexpr uint32_t MindSuppress = 733; // 神识压制 / Mind Suppress

// 智力分支 / Intelligence branch
constexpr uint32_t MindUnity = 750;   // 意念合一 / Mind Unity
constexpr uint32_t MultiMind = 751;   // 多重思维 / Multi Mind
constexpr uint32_t OneLaw = 752;      // 万法归一 / One Law

// 元素分支 / Element branch
constexpr uint32_t RayFocus = 770;    // 射线聚焦 / Ray Focus
constexpr uint32_t VoidRift = 771;    // 虚空裂痕 / Void Rift
} // namespace MindBladeNodes

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

  LOG_INFO("Mind Blade (ID {}) spawned for entity {}", kSkillId,
           (uint32_t)owner);
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

  // Calculate effective attack interval based on stack count
  // Each stack increases attack rate by 20% (diminishing return handled by
  // division)
  float effective_interval =
      ai.base_interval / (1.0f + (comp.stack_count - 1) * 0.2f);
  if (effective_interval < 0.1f)
    effective_interval = 0.1f; // Cap max speed

  // 2. Targeting
  ai.retarget_timer -= dt;
  if (!registry.valid(ai.target) || ai.retarget_timer <= 0.0f) {
    ai.retarget_timer = 0.3f; // Check more frequently (0.3s)
    ai.target = entt::null;

    const auto &pos = registry.get<Position>(entity);

    // Intelligent Targeting:
    // If INT is high (>50), prioritize Lowest HP (execution).
    // Otherwise, prioritize Closest (safety).
    bool prioritize_low_hp = false;
    if (registry.all_of<CombatStats>(comp.owner)) {
      if (registry.get<CombatStats>(comp.owner).effective_intelligence >
          50.0f) {
        prioritize_low_hp = true;
      }
    }

    float best_score = -1.0f; // For HP (higher score = lower hp ratio?)
    // Actually simpler: min_dist for Closest, min_hp for LowHP.

    float min_metric = FLT_MAX;

    grid.query({pos.x, pos.y}, ai.range,
               [&](entt::entity target_ent, const Position &t_pos) {
                 if (target_ent == comp.owner || target_ent == entity)
                   return;
                 if (!registry.all_of<EnemyTag, CombatStats>(target_ent))
                   return;

                 const auto &stats = registry.get<CombatStats>(target_ent);
                 if (stats.health <= 0)
                   return;

                 float dist_sq =
                     Vector2DistanceSqr({pos.x, pos.y}, {t_pos.x, t_pos.y});

                 if (prioritize_low_hp) {
                   // Metric: HP (absolute or ratio). Let's use HP.
                   // Penalize distance slightly so it doesn't target map-wide
                   // low hp
                   float metric = stats.health + (dist_sq * 0.01f);
                   if (metric < min_metric) {
                     min_metric = metric;
                     ai.target = target_ent;
                   }
                 } else {
                   // Metric: Distance
                   if (dist_sq < min_metric) {
                     min_metric = dist_sq;
                     ai.target = target_ent;
                   }
                 }
               });
  }

  // 3. Attack
  ai.attack_timer += dt;
  if (registry.valid(ai.target) && ai.attack_timer >= effective_interval) {
    // Fire!
    ai.attack_timer = 0.0f;

    const auto &pos = registry.get<Position>(entity);

    if (registry.all_of<Position>(ai.target)) {
      const auto &t_pos = registry.get<Position>(ai.target);

      // Create Projectile
      auto proj = registry.create();
      registry.emplace<Position>(proj, pos.x, pos.y);
      registry.emplace<LocalLevelTag>(proj);

      Vector2 dir =
          Vector2Normalize(Vector2Subtract({t_pos.x, t_pos.y}, {pos.x, pos.y}));
      registry.emplace<Velocity>(proj, dir.x * 900.0f,
                                 dir.y * 900.0f); // Fast projectile

      auto &p = registry.emplace<Projectile>(proj);
      p.owner = comp.owner; // Owner gets credit for damage

      // Copy owner stats for snapshot
      if (registry.all_of<CombatStats>(comp.owner)) {
        p.snapshot = registry.get<CombatStats>(comp.owner);
      }

      // Set Base Damage based on INT
      float base_dmg = 20.0f;
      if (registry.all_of<CombatStats>(comp.owner)) {
        base_dmg +=
            registry.get<CombatStats>(comp.owner).effective_intelligence *
            comp.intelligence_scaling;
      }

      // Override weapon damage in snapshot for this skill
      p.snapshot.min_weapon_damage = base_dmg;
      p.snapshot.max_weapon_damage = base_dmg;

      // Critical Chance Inheritance (Mind Blade has high base crit)
      p.snapshot.crit_chance += 0.1f;

      p.speed = 900.0f;
      p.lifeTime = 2.0f;
      p.radius = 12.0f;

      // Visuals
      auto &particleSys = systems::GPUParticleSystem::Get();
      components::GPUParticle gp = systems::InkEffectHelper::CreateSpark(
          {pos.x, pos.y}, {dir.x * 100, dir.y * 100},
          systems::InkEffectHelper::COLOR_SHADOW_CORE, 0.5f);
      gp.scale = 2.0f;
      particleSys.Emit(gp);
    }
  }

  return true;
}

REGISTER_SKILL_BEHAVIOR(MindBlade)

void RegisterMindBlade() {}

} // namespace NoMoreDay::skills
