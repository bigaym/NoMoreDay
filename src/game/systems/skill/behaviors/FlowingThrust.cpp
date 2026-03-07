/**
 * @file FlowingThrust.cpp
 * @brief 娴佷簯鍒?(ID 1) - 绐佸埡鎶€鑳借涓哄疄鐜?
 *
 * 鍐插埡鍚戠洰鏍囨柟鍚戯紝娌块€旈€犳垚鐗╃悊浼ゅ銆?
 *
 * 澶╄祴鍒嗘敮:
 * - 110 璐棩: 鏃犻檺绌块€?
 * - 112 椋庤: 閲婃斁鍚庤幏寰楃Щ閫熷姞鎴?
 * - 113 杩呮嵎涔嬪垉: 绉婚€熻浆鍖栦负浼ゅ
 * - 114 鍔垮鐮寸: 杩滆窛绂诲浼?
 * - 120 鐣欏奖: 鐢熸垚娈嬪奖閲嶅鏂芥硶
 * - 121 鍓戞剰鐩堢泩: 鍛戒腑鏃舵鐜囪幏寰楀墤鎰?
 * - 124 褰辨潃闃? 寮哄寲鏃惰Е鍙?
 * - 130 娲炴倝寮辩偣: 瀵规弧琛€鏁屼汉鏆村嚮鐜囨彁鍗?
 * - 140 瀵掗湝鍒? 鐗╃悊杞啺闇?
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp" // For ActiveSkillsComponent
#include "game/components/vfx/MotionTrailComponent.hpp"
#include "game/components/vfx/SwordIntentVisualComponent.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSystem.hpp" // Added for SkillExecution definition
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "raymath.h"
#include <string>

namespace NoMoreDay::skills {
namespace {

bool TryPlayFlowingThrustSequence(entt::registry &registry, entt::entity owner,
                          const std::string &sequenceName) {
  if (!registry.valid(owner) || sequenceName.empty()) {
    return false;
  }

  auto &manager = vfx::VFXSequenceManager::Get();
  if (manager.GetSequence(sequenceName) == nullptr) {
    return false;
  }

  manager.Play(registry, owner, sequenceName);
  return true;
}

void SpawnFlowingThrustEcho(entt::registry &registry, entt::entity owner,
                            const Vector2 &startPos, const SkillExecution &exec,
                            const CombatStats *stats, float damageScale,
                            float delay, float lifetime, Color tint) {
  auto shadow_ent = registry.create();

  SkillSnapshot snapshot;
  snapshot.skill_id = 1;
  snapshot.position = startPos;
  snapshot.target_pos = exec.target_pos;
  snapshot.active_nodes = exec.active_nodes;

  if (stats) {
    snapshot.stats = *stats;
    snapshot.stats.min_weapon_damage *= damageScale;
    snapshot.stats.max_weapon_damage *= damageScale;
    for (auto &val : snapshot.stats.flat_damage) {
      val *= damageScale;
    }
  }

  ShadowComponent shadow_comp;
  shadow_comp.snapshot = snapshot;
  shadow_comp.delay = delay;
  shadow_comp.lifetime = lifetime;
  shadow_comp.damage_scale = damageScale;

  registry.emplace<ShadowComponent>(shadow_ent, shadow_comp);
  registry.emplace<Position>(shadow_ent, startPos.x, startPos.y);
  registry.emplace<ShadowVisualComponent>(
      shadow_ent,
      ShadowVisualComponent{.color_tint = tint, .use_shader = true});
  registry.emplace<DelayedDestroyComponent>(shadow_ent,
                                            DelayedDestroyComponent{2.0f});
}

void RefundRendingWaveCooldown(entt::registry &registry, entt::entity owner,
                               uint32_t skillId, float amount) {
  if (amount <= 0.0f) {
    return;
  }
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return;
  }
  for (auto &slot : active->slots) {
    if (slot.id != skillId || slot.cooldown <= 0.0f) {
      continue;
    }
    slot.cooldown = std::max(0.0f, slot.cooldown - amount);
  }
}

} // namespace

namespace FlowingThrustNodes {
// 鍩虹鍒嗘敮
constexpr uint32_t Speed = 100;      // 杩呮嵎涔嬪垉
constexpr uint32_t CritChance = 101; // 鍓戝績

// 璐┛鍒嗘敮 (宸︿笂)
constexpr uint32_t Pierce = 110;     // 璐棩
constexpr uint32_t Charges = 111;    // 杩炵幆
constexpr uint32_t Momentum = 112;   // 鍔垮鐮寸
constexpr uint32_t Windrunner = 113; // 椋庤鑰?
constexpr uint32_t SwordEcho = 114;  // 鍓戞皵鍥炲搷

// 娈嬪奖鍒嗘敮 (鍙充笂)
constexpr uint32_t Shadow = 130;       // 鐣欏奖
constexpr uint32_t Teleport = 131;     // 绉诲舰鎹綅
constexpr uint32_t ShadowDomain = 132; // 褰卞煙
constexpr uint32_t PrisonSlash = 133;  // 鐬嫳褰辨潃
constexpr uint32_t Phantom = 134;      // 铏氬疄鐩哥敓

// 鏆村嚮鍒嗘敮 (宸︿笅)
constexpr uint32_t WeakPoint = 150;  // 瑕佸鎰熺煡
constexpr uint32_t Bleed = 151;      // 閲嶅垱
constexpr uint32_t FatalBlow = 152;  // 缁濆懡涓€鍑?
constexpr uint32_t AllIn = 153;      // 瀛ゆ敞涓€鎺?
constexpr uint32_t ArmorBreak = 154; // 鐮寸敳涔嬪織

// 鍏冪礌鍒嗘敮 (鍙充笅)
constexpr uint32_t ElementShift = 170; // 鍏冪礌骞诲寲
constexpr uint32_t ElementBody = 171;  // 鍏冪礌韬硶
constexpr uint32_t QiShield = 172;     // 姘斿姴鎶や綋
constexpr uint32_t Agility = 173;      // 鐏靛姩
} // namespace FlowingThrustNodes

struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
  static constexpr uint32_t kSkillId = 1;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    auto *dash = registry.try_get<DashComponent>(owner);
    if (!pos)
      return;

    const auto sevenStarLink = seven_star_shared::ConsumeLinkBuffs(
        registry, owner, kSkillId, true, exec.cast_id);
    if (sevenStarLink.consumed_any) {
      exec.is_empowered = true;
    }

    // 1. Dash towards target
    Vector2 startPos = {pos->x, pos->y};
    Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, startPos));
    float speed = 400.0f;
    if (const auto *resource = registry.try_get<BladeResourceComponent>(owner);
        resource != nullptr && resource->kind == BladeResourceKind::SwordFlow) {
      const float flowStacks =
          static_cast<float>(std::clamp(resource->current, 0, 10));
      speed *= (1.0f + flowStacks * 0.04f);
    }
    TryPlayFlowingThrustSequence(registry, owner, "SwordSlash");

    ElementalConversion elementalConv;
    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id != kSkillId)
          continue;
        auto it = spec.allocated_points.find(FlowingThrustNodes::ElementShift);
        if (it != spec.allocated_points.end() && it->second > 0) {
          elementalConv = ResolveElementalConversion(
              FlowingThrustNodes::ElementShift, it->second);
        }
        break;
      }
    }

    // Apply burst velocity to owner
    if (auto *vel = registry.try_get<Velocity>(owner)) {
      vel->vx = dir.x * speed;
      vel->vy = dir.y * speed;
    }

    // Talent 170: ElementShift (Phys -> Elemental)
    // We apply a temporary tag or handle it in DoHit.
    // For visual effects, we might want to change the particle color here.
    bool is_elemental = elementalConv.IsActive();

    bool spawnedEcho = false;

    // Talent 130: Shadow (Spawn Shadow Echo)
    // Prevent recursion: Don't spawn shadow if owner is already a shadow
    if (exec.active_nodes.test(FlowingThrustNodes::Shadow % 100) &&
        !registry.any_of<ShadowComponent>(owner)) {
      SpawnFlowingThrustEcho(registry, owner, startPos, exec, stats, 0.3f,
                             0.15f, 1.5f, {50, 0, 80, 180});
      spawnedEcho = true;
    }

    if (!spawnedEcho) {
      if (const auto *resource = registry.try_get<BladeResourceComponent>(owner);
          resource != nullptr && resource->kind == BladeResourceKind::SwordFlow &&
          resource->current >= 5 && !registry.any_of<ShadowComponent>(owner)) {
        SpawnFlowingThrustEcho(registry, owner, startPos, exec, stats, 0.22f,
                               0.08f, 1.0f, {120, 240, 255, 180});
        if (resource->current >= 8) {
          SpawnFlowingThrustEcho(registry, owner, startPos, exec, stats, 0.18f,
                                 0.16f, 0.9f, {180, 255, 255, 170});
        }
        if (resource->current >= 10) {
          SpawnFlowingThrustEcho(registry, owner, startPos, exec, stats, 0.14f,
                                 0.24f, 0.85f, {255, 240, 180, 180});
        }
      }
    }

    // Integrate with DashComponent
    if (dash) {
      dash->isDashing = true;
      dash->dashTimer = 0.375f;
      dash->dirX = dir.x;
      dash->dirY = dir.y;
      dash->dashSpeed = speed;

      // Add Trail Visual - Particle Flow Mode
      auto &trail = registry.get_or_emplace<components::MotionTrail>(owner);

      // Configuration values
      float targetWidth = 20.0f; // Adjusted to 20.0f per user request
      float targetLifetime =
          0.1f; // User: "1/4 of skill duration" (0.375s / 4 ~= 0.09s)

      trail.isActive = true;
      trail.maxWidth = targetWidth; // Apply width to component
      trail.useParticles = true;
      trail.emitInterval =
          0.0007f; // Increased density (3x more particles, 0.002 -> 0.0007)
      trail.lifetime = targetLifetime;
      trail.particleSize = 3.5f; // Large particles

      // Color Config: Center Bright, Edge Dim
      trail.coreColor = components::Colors::SPEED_ACCENT; // Bright Cyan Center

      Color glowColor =
          is_elemental
              ? elementalConv.glow_color
              : (exec.is_empowered ? GOLD : components::Colors::INK_TRAIL_PALE);
      glowColor.a = 20;        // Low alpha for glow/edge
      trail.color = glowColor; // Edge color
    }

    // --- VISUAL EFFECTS: Ink Trail & Speed Lines ---
    // Replaced by MotionTrail Particle System (Time-based emission)

    // 2. End Point Burst - Removed per feedback

    if (exec.is_empowered) {
      if (!TryPlayFlowingThrustSequence(registry, owner, "CriticalHit")) {
        auto &particleSys = systems::GPUParticleSystem::Get();
        auto goldParticles = systems::InkEffectHelper::CreateInkSplash(
            startPos, 12, 10.0f, 150.0f);
        for (auto &p : goldParticles) {
          p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
          p.flags |= 2;
          particleSys.Emit(p);
        }
        RenderSystem::AddScreenShake(0.15f);
      }

      // Talent: Prison Slash (鐬嫳褰辨潃) - ID 133 (Was 124)
      if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
        for (const auto &spec : active->specialized_slots) {
          if (spec.skill_id == 1 &&
              spec.allocated_points.contains(FlowingThrustNodes::PrisonSlash) &&
              spec.allocated_points.at(FlowingThrustNodes::PrisonSlash) > 0) {
            registry.emplace_or_replace<ShadowKillArrayReady>(owner);
            LOG_INFO("Shadow Kill Array (Prison Slash) Ready for entity {}",
                     (uint32_t)owner);
            break;
          }
        }
      }
    }

    // --- TALENT BRANCH LOGIC ---
    float moreDamageMult = 1.0f;
    moreDamageMult *= sevenStarLink.damage_multiplier;
    bool forcePierce = false;
    bool spawnShadow = false;
    bool hasElementBody = false;

    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == 1) {
          // Talent: Guan Ri (璐棩) - ID 110
          if (spec.allocated_points.contains(FlowingThrustNodes::Pierce) &&
              spec.allocated_points.at(FlowingThrustNodes::Pierce) > 0) {
            forcePierce = true;
          }

          // Talent: Liu Ying (鐣欏奖) - ID 130
          if (spec.allocated_points.contains(FlowingThrustNodes::Shadow) &&
              spec.allocated_points.at(FlowingThrustNodes::Shadow) > 0) {
            spawnShadow = true;
          }

          // Talent: Element Shift (鍏冪礌骞诲寲) - ID 170
          if (spec.allocated_points.contains(
                  FlowingThrustNodes::ElementShift) &&
              spec.allocated_points.at(FlowingThrustNodes::ElementShift) > 0) {
            elementalConv = ResolveElementalConversion(
                FlowingThrustNodes::ElementShift,
                spec.allocated_points.at(FlowingThrustNodes::ElementShift));
          }
          if (spec.allocated_points.contains(FlowingThrustNodes::ElementBody) &&
              spec.allocated_points.at(FlowingThrustNodes::ElementBody) > 0) {
            hasElementBody = true;
          }

          // Talent: Momentum (鍔垮鐮寸) - ID 112
          if (spec.allocated_points.contains(FlowingThrustNodes::Momentum) &&
              spec.allocated_points.at(FlowingThrustNodes::Momentum) > 0) {
            float dist = Vector2Distance(startPos, exec.target_pos);
            if (dist > 150.0f) {
              moreDamageMult *= 1.3f;
              LOG_INFO("Momentum: +30% More damage due to distance ({:.1f})",
                       dist);
            }
          }

          // Talent: Feng Xing (椋庤鑰? - ID 113
          if (spec.allocated_points.contains(FlowingThrustNodes::Windrunner) &&
              spec.allocated_points.at(FlowingThrustNodes::Windrunner) > 0) {
            auto &effects =
                registry.get_or_emplace<ActiveEffectsComponent>(owner);
            BuffEffect swift;
            swift.id = "flowing_thrust_swift";
            swift.name = "Feng Xing";
            swift.type = BuffType::SpeedUp;
            swift.duration = 2.0f;
            swift.remaining = 2.0f;
            swift.modifiers.push_back({.value = 30.0f,
                                       .type = StatType::MoveSpeed,
                                       .mode = ModifierMode::PercentAdd});
            effects.AddOrRefresh(swift);
            registry.get_or_emplace<StatsDirty>(owner);
            // Windrunner: phase through collision during this dash.
            registry.emplace_or_replace<PhaseTag>(owner);
            LOG_INFO("Feng Xing swiftness applied to entity {}",
                     (uint32_t)owner);
          }

          // Talent: Xun Jie Zhi Ren (杩呮嵎涔嬪垉) - ID 100
          if (spec.allocated_points.contains(FlowingThrustNodes::Speed) &&
              spec.allocated_points.at(FlowingThrustNodes::Speed) > 0) {
            if (auto *combat = registry.try_get<CombatStats>(owner)) {
              float ms = combat->move_speed;
              float ms_bonus =
                  (ms / 10.0f) * 0.01f *
                  spec.allocated_points.at(FlowingThrustNodes::Speed);
              moreDamageMult *= (1.0f + ms_bonus);
              LOG_INFO("Xun Jie Zhi Ren: +{:.1f}% More damage from MoveSpeed "
                       "({:.1f})",
                       ms_bonus * 100.0f, ms);
            }
          }
          break;
        }
      }
    }

    // --- Branch B: Liu Ying (Shadow Echo) ---
    if (spawnShadow && !registry.any_of<ShadowCastTag>(owner)) {
      auto shadow_ent = registry.create();
      registry.emplace<LocalLevelTag>(shadow_ent);
      registry.emplace<Position>(shadow_ent, startPos.x, startPos.y);
      registry.emplace<AnimationStateComponent>(shadow_ent);
      registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(SKYBLUE, 0.5f));

      auto &sc = registry.emplace<ShadowComponent>(shadow_ent);
      sc.delay = 0.5f;
      sc.lifetime = 1.5f;
      sc.snapshot.skill_id = 1;
      sc.snapshot.position = startPos;
      sc.snapshot.target_pos = exec.target_pos;

      auto &summon = registry.emplace<SummonComponent>(shadow_ent);
      summon.owner = owner;
      summon.skill_id = 1;
      summon.archetype_id = SummonArchetype::ShadowEcho;
      summon.lifetime = 1.5f;
      summon.max_lifetime = 1.5f;

      if (stats) {
        sc.snapshot.stats = *stats;
        for (auto &mult : sc.snapshot.stats.damage_multipliers)
          mult *= 0.3f;
      }
      LOG_INFO("Liu Ying: Shadow Echo created for Flowing Thrust");
    }

    // Contract transmuter support node 171: add short elemental body buff.
    if (hasElementBody && elementalConv.IsActive()) {
      auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
      BuffEffect body;
      body.id = "flowing_thrust_element_body";
      body.name = "Element Body";
      body.type = BuffType::Shield;
      body.duration = 1.5f;
      body.remaining = 1.5f;
      body.modifiers.push_back({.value = 8.0f,
                                .type = StatType::ResistAll,
                                .mode = ModifierMode::Flat});
      effects.AddOrRefresh(body);
    }

    // 2. Spawn a "Thrust" projectile
    // Offset spawn position so the visual is not centered on the player, but
    // extending forward User wants 20% behind, 80% forward. The shape is
    // roughly 2*radius long. Shifting forward by approx 0.6 * radius aligns the
    // "center" of the shape forward.
    float renderRadius =
        exec.is_empowered ? 35.0f : 20.0f; // Reduced by ~1/3 (was 50/30)
    float forwardOffset =
        renderRadius * 1.2f; // Offset refined to 1.2f per user request

    auto proj_ent = registry.create();
    registry.emplace<LocalLevelTag>(proj_ent);

    // Calculate offset position
    Vector2 spawnPos = {pos->x + dir.x * forwardOffset,
                        pos->y + dir.y * forwardOffset};
    registry.emplace<Position>(proj_ent, spawnPos.x, spawnPos.y);

    registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
    registry.emplace<ColorComponent>(
        proj_ent,
        elementalConv.IsActive()
            ? elementalConv.projectile_color
            : NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT);

    auto &proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = owner;
    proj.cast_id = exec.cast_id;
    proj.speed = speed; // Bind to dash displacement speed (User requested)
    proj.lifeTime = 0.375f;
    proj.radius = renderRadius;
    proj.pierce = true;
    proj.pierceCount = forcePierce ? 999 : 99;
    proj.visualType = 2; // Beam/Box for Thrust

    if (stats) {
      proj.snapshot = *stats;
      for (auto &mult : proj.snapshot.damage_multipliers)
        mult *= moreDamageMult;

      if (exec.is_empowered) {
        for (auto &mult : proj.snapshot.damage_multipliers)
          mult *= 1.5f;
        LOG_INFO("Empowered Flowing Thrust spawned with 1.5x damage and larger "
                 "radius");
      }
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    }

    if (elementalConv.IsActive()) {
      auto &skillMods = registry.emplace<SkillModifierComponent>(proj_ent);
      skillMods.damage_modifiers.push_back(
          {Tag::Physical, elementalConv.target_element, 1.0f,
           ModifierType::Convert});
      LOG_INFO("Flowing Thrust converted from Physical to elemental damage");
    }

    registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);
    if (sevenStarLink.consume_returning_step) {
      seven_star_shared::ApplyReturningStepOverride(registry, owner, kSkillId);
    }
    LOG_INFO("Flowing Thrust executed by entity {}", (uint32_t)owner);
  }

  static void DoHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, Tag hit_tags, bool is_crit) {
    if (const auto *resource = registry.try_get<BladeResourceComponent>(attacker);
        resource != nullptr && resource->kind == BladeResourceKind::SwordFlow) {
      (void)SkillSystem::GainSwordIntent(registry, attacker, 1, kSkillId);
      (void)systems::BladeResourceService::TryConsumeSwordFlowRestartWindow(
          registry, attacker, kSkillId);
      RefundRendingWaveCooldown(registry, attacker, 2, 0.75f);
    }

    if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          // ID 121: Jian Yi Ying Ying (Chance to gain Intent) - Legacy, not in
          // JSON
          /*
          if (spec.allocated_points.contains(121)) {
            int pts = spec.allocated_points.at(121);
            if (pts > 0 && GetRandomValue(0, 100) < 25 * pts) {
              if (auto *intent =
                      registry.try_get<SwordIntentComponent>(attacker)) {
                if (intent->stacks < intent->max_stacks) {
                  intent->stacks++;
                  intent->time_since_last_gain = 0.0f;
                  intent->decay_tick_timer = 0.0f;
                  LOG_DEBUG("Flowing Thrust (121 Legacy): Gained Intent via
          Hit");
                }
              }
            }
          }
          */

          // ID 150: Weak Point (Was 130)
          // Vital Sense crit logic is handled in DamagePipeline pre-crit stage.
          if (spec.allocated_points.contains(FlowingThrustNodes::WeakPoint)) {
            (void)target;
          }

          // ID 170: ElementShift (Was 140)
          if (spec.allocated_points.contains(
                  FlowingThrustNodes::ElementShift)) {
            // Logic handled in DoCast or separate system
          }

          // ID 133: PrisonSlash (Was 124)
          // Shadow Kill Array logic
          if (is_crit &&
              spec.allocated_points.contains(FlowingThrustNodes::PrisonSlash)) {
            int pts = spec.allocated_points.at(FlowingThrustNodes::PrisonSlash);
            if (pts > 0 && GetRandomValue(0, 100) < 20 * pts) {
              registry.get_or_emplace<ShadowKillArrayReady>(attacker);
              LOG_INFO("Flowing Thrust (133): Shadow Kill Array READY");
            }
          }
          break;
        }
      }
    }
  }
};

// Auto-register on static initialization
REGISTER_SKILL_BEHAVIOR(FlowingThrust)

void RegisterFlowingThrust() {
  // This function is called to force the linker to include this translation
  // unit
}

} // namespace NoMoreDay::skills
