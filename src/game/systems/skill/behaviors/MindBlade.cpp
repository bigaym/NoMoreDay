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
#include "game/systems/physics/SpatialGrid.hpp"      // Added
#include "engine/render/GPUParticleSystem.hpp" // Added
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp" // Added for Position, Vector2
#include "game/components/Projectile.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"
#include <algorithm>

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
  auto ownerPos = Vector2{0.0f, 0.0f};
  if (registry.valid(owner) && registry.all_of<Position>(owner)) {
    const auto &pos = registry.get<Position>(owner);
    ownerPos = {pos.x, pos.y};
  }

  // CastStart: energy gathers in hand (cyan -> dark purple, short inward collapse).
  auto &particleSys = systems::GPUParticleSystem::Get();
  for (int i = 0; i < 12; ++i) {
    const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
    const float radius = static_cast<float>(GetRandomValue(10, 26));
    Vector2 spawn = {ownerPos.x + cosf(angle) * radius,
                     ownerPos.y - 28.0f + sinf(angle) * radius};
    Vector2 toCore = Vector2Normalize(Vector2Subtract(
        Vector2{ownerPos.x, ownerPos.y - 28.0f}, spawn));

    components::GPUParticle p = {};
    p.position = spawn;
    p.velocity = Vector2Scale(toCore, static_cast<float>(GetRandomValue(90, 160)));
    p.acceleration = {0.0f, 0.0f};
    p.color = (i % 2 == 0) ? Color{195, 248, 245, 220} : Color{110, 80, 170, 210};
    p.scale = 3.0f;
    p.lifetime = 0.20f;
    p.maxLifetime = 0.20f;
    p.flags = 2;
    p.growthRate = -6.0f;
    particleSys.Emit(p);
  }

  // CastStart: visible hand core so startup is readable even in particle-light tiers.
  {
    components::GPUSkillEffect handCore = {};
    handCore.position = {ownerPos.x, ownerPos.y - 28.0f};
    handCore.velocity = {0.0f, 0.0f};
    handCore.coreColor = {0.70f, 0.92f, 1.00f, 0.86f};
    handCore.glowColor = {0.48f, 0.32f, 0.78f, 0.78f};
    handCore.radius = 14.0f;
    handCore.sectorAngle = 360.0f;
    handCore.type = 1.0f;
    handCore.flags = NoMoreDay::render::skillfx::PackSkillEffectFlags(0u, 7u);
    systems::GPUSkillEffectSystem::Get().Submit(handCore);
  }

  // CastStart: crack preview at cursor (thin flicker lines, ~100ms).
  for (int i = 0; i < 8; ++i) {
    const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
    const float len = static_cast<float>(GetRandomValue(8, 18));
    Vector2 dir = {cosf(angle), sinf(angle)};

    components::GPUParticle p = {};
    p.position = {exec.target_pos.x + dir.x * len * 0.5f,
                  exec.target_pos.y + dir.y * len * 0.5f};
    p.velocity = Vector2Scale(dir, static_cast<float>(GetRandomValue(30, 70)));
    p.acceleration = {0.0f, 0.0f};
    p.color = Color{210, 245, 255, 180};
    p.scale = 2.0f;
    p.lifetime = 0.10f;
    p.maxLifetime = 0.10f;
    p.flags = 2;
    p.growthRate = -8.0f;
    particleSys.Emit(p);
  }

  // CastStart: cursor crack preview body (short, thin slash) to match prototype.
  {
    components::GPUSkillEffect crackPreview = {};
    crackPreview.position = exec.target_pos;
    crackPreview.velocity = {1.0f, 0.0f};
    crackPreview.coreColor = {0.82f, 0.94f, 1.00f, 0.74f};
    crackPreview.glowColor = {0.54f, 0.74f, 1.00f, 0.62f};
    crackPreview.radius = 18.0f;
    crackPreview.sectorAngle = 0.0f;
    crackPreview.type = 2.0f;
    crackPreview.flags = NoMoreDay::render::skillfx::PackSkillEffectFlags(0u, 7u);
    systems::GPUSkillEffectSystem::Get().Submit(crackPreview);
  }

  // Mind Blade is a channeled skill in current design.
  auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
  chan.skill_id = kSkillId;
  chan.channel_timer = 0.25f;   // Must be refreshed by input hold
  chan.tick_interval = 0.3f;    // Matches design baseline: every 0.3s
  chan.tick_timer = -0.01f;     // Fire first tick immediately
  chan.target_pos = exec.target_pos;
  chan.is_empowered = exec.is_empowered;
  chan.total_duration = 0.0f;
  chan.cast_id = exec.cast_id;
  chan.conversion_tag = Tag::None;
  chan.bonus_damage_mult = 1.0f;
  chan.bonus_crit_chance = 0.0f;
  chan.synergy_lock = false;

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, owner, kSkillId);
  if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id != kSkillId) {
        continue;
      }
      if (spec.allocated_points.contains(MindBladeNodes::MindLock) &&
          spec.allocated_points.at(MindBladeNodes::MindLock) > 0) {
        chan.synergy_lock = true;
      }
      if (activeTransmuter == MindBladeNodes::RayFocus &&
          spec.allocated_points.contains(MindBladeNodes::RayFocus) &&
          spec.allocated_points.at(MindBladeNodes::RayFocus) > 0) {
        chan.conversion_tag = Tag::Lightning;
      } else if (activeTransmuter == MindBladeNodes::MindUnity &&
                 spec.allocated_points.contains(MindBladeNodes::MindUnity) &&
                 spec.allocated_points.at(MindBladeNodes::MindUnity) > 0) {
        chan.conversion_tag = Tag::Void;
        chan.bonus_damage_mult += 0.15f;
      }
      if (spec.allocated_points.contains(MindBladeNodes::OneLaw) &&
          spec.allocated_points.at(MindBladeNodes::OneLaw) > 0) {
        int stacks = 0;
        if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
          stacks = intent->stacks;
        }
        chan.bonus_damage_mult += std::clamp(stacks * 0.02f, 0.0f, 0.3f);
        chan.bonus_crit_chance += 5.0f;
      }
      if (spec.allocated_points.contains(MindBladeNodes::HeavenMan) &&
          spec.allocated_points.at(MindBladeNodes::HeavenMan) > 0) {
        chan.bonus_crit_chance += 8.0f;
      }
      break;
    }
  }

  if (chan.conversion_tag == Tag::None) {
    chan.conversion_tag =
        systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry,
                                                                       owner);
  }

  LOG_INFO("Mind Blade channeling started for entity {} (cast_id={})",
           (uint32_t)owner, static_cast<unsigned long long>(exec.cast_id));
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
      
      // FIX: Add SkillComponent to projectile for attribution and modifiers
      registry.emplace<SkillComponent>(proj, kSkillId, comp.owner);

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
      
      // FIX: Pass SkillModifierComponent to the projectile 
      if (const auto *ownerMods = registry.try_get<SkillModifierComponent>(comp.owner)) {
        auto &projMods = registry.emplace<SkillModifierComponent>(proj);
        // Add existing damage modifiers
        projMods.damage_modifiers = ownerMods->damage_modifiers;
      }
      
      // Support elemental conversion via active skill slots
      if (auto *active = registry.try_get<ActiveSkillsComponent>(comp.owner)) {
        const uint32_t activeTransmuter =
            SkillSystem::GetActiveTransmuterNode(registry, comp.owner, kSkillId);
        for (const auto &spec : active->specialized_slots) {
          if (spec.skill_id == kSkillId) {
            auto *projMods = registry.try_get<SkillModifierComponent>(proj);
            if (!projMods) {
              projMods = &registry.emplace<SkillModifierComponent>(proj);
            }
            if (activeTransmuter == MindBladeNodes::RayFocus &&
                spec.allocated_points.contains(MindBladeNodes::RayFocus) &&
                spec.allocated_points.at(MindBladeNodes::RayFocus) > 0) {
              projMods->damage_modifiers.push_back(DamageModifier{
                  Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});
            } else if (activeTransmuter == MindBladeNodes::MindUnity &&
                       spec.allocated_points.contains(MindBladeNodes::MindUnity) &&
                       spec.allocated_points.at(MindBladeNodes::MindUnity) > 0) {
              projMods->damage_modifiers.push_back(
                  DamageModifier{Tag::Physical, Tag::Void, 1.0f,
                                 ModifierType::Convert});
            } else if (spec.allocated_points.contains(MindBladeNodes::VoidRift) &&
                       spec.allocated_points.at(MindBladeNodes::VoidRift) > 0) {
              // Historical node-id alias for save compatibility.
              projMods->damage_modifiers.push_back(
                  DamageModifier{Tag::Physical, Tag::Void, 1.0f,
                                 ModifierType::Convert});
            }
            break;
          }
        }
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
