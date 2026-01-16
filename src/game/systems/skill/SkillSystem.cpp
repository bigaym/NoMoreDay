#include "game/systems/skill/SkillSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FrameRateUtils.hpp" // Frame-rate independent utilities
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "game/components/AIComponent.hpp" // For EnemyTag
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp" // For Position
#include "game/components/EffectComponent.hpp"
#include "game/components/PlayerState.hpp" // For DashComponent
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/systems/skill/behaviors/MindBlade.hpp"
#include "game/systems/skill/behaviors/PhantomFlash.hpp" // Added
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SwordArray.hpp"
#include "raymath.h"
#include <algorithm>
#include <map>
#include <unordered_set>

namespace NoMoreDay {

static std::map<uint32_t, SkillSystem::CastCallback> s_skill_callbacks;
static std::vector<SkillSystem::SkillHook> s_pre_cast_hooks;
static std::vector<SkillSystem::SkillHook> s_post_cast_hooks;

void SkillSystem::InitHooks() {
  LOG_INFO("Initializing Skill Hooks...");
  SkillBehaviorRegistry::Initialize();
  ClearHooks();
  s_skill_callbacks.clear();

  BehaviorInjectionRegistry::Init();

  // 0. Generic Behavior Injection
  AddPreCastHook([](entt::registry &registry, entt::entity execution_ent,
                    SkillExecution &exec) {
    if (exec.active_nodes.none())
      return;
    if (!registry.valid(exec.owner))
      return;

    const auto *tree = SkillRegistry::Get().GetSkillTree(exec.skill_id);
    if (!tree) {
      // Log warning only if strictly needed, otherwise silent return helps
      // robustness for dummy skills
      return;
    }

    for (size_t i = 0; i < exec.active_nodes.size(); ++i) {
      if (exec.active_nodes.test(i)) {
        uint32_t node_id = (exec.skill_id * 100) + (uint32_t)i;
        auto it = tree->nodes.find(node_id);
        if (it != tree->nodes.end()) {
          const auto &node = it->second;
          if (!node.behavior_id.empty()) {
            BehaviorInjectionRegistry::Apply(node.behavior_id, registry,
                                             exec.owner);
          }
        }
      }
    }
  });

  // 1. Sword Intent & Empowered Logic
  AddPreCastHook([](entt::registry &registry, entt::entity execution_ent,
                    SkillExecution &exec) {
    entt::entity caster = exec.owner;
    if (!registry.valid(caster))
      return;

    if (registry.any_of<ShadowCastTag>(execution_ent))
      return;

    if (auto *intent = registry.try_get<SwordIntentComponent>(caster)) {
      if (intent->stacks >= intent->max_stacks) {
        exec.is_empowered = true;
        intent->stacks = 0;
        LOG_INFO("Skill {} empowered by Sword Intent for entity {}",
                 exec.skill_id, (uint32_t)caster);
      }
    }
  });

  // 2. Sword Intent Gain on Hit
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [](entt::registry &registry, const CombatEvent &evt) {
        // evt.source is the actual caster (fixed in DamagePipeline)
        entt::entity caster = evt.source;

        if (!registry.valid(caster)) {
          return;
        }

        auto *intent = registry.try_get<SwordIntentComponent>(caster);
        if (intent) {
          // Only trigger sword intent gain for skills with Hit tag
          if (HasTag(evt.tags, Tag::Hit)) {
            bool gainStack = false;
            float currentTime = (float)GetTime();

            // Check if skill is Continuous (Channeled or Aura)
            bool isContinuous =
                HasTag(evt.tags, Tag::Channeled) || HasTag(evt.tags, Tag::Aura);

            // Use cast_id if available, otherwise fallback to skill_id (less
            // reliable for rapid casts)
            uint64_t trackingKey =
                (evt.cast_id != 0) ? evt.cast_id : (uint64_t)evt.skill_id;

            auto &tracking = intent->hit_tracking[trackingKey];

            if (isContinuous) {
              // Continuous Skills: Max 1 stack per second per cast
              float timeSinceLastGain = currentTime - tracking.last_gain_time;

              if (timeSinceLastGain >= 1.0f) {
                gainStack = true;
                tracking.last_gain_time = currentTime;
                tracking.stacks_gained++;
              }
            } else {
              // Instant/Hit Skills: One stack per CAST
              if (tracking.stacks_gained == 0) {
                gainStack = true;
                tracking.last_gain_time = currentTime;
                tracking.stacks_gained++;
              }
            }

            if (gainStack && intent->stacks < intent->max_stacks) {
              intent->stacks++;
              intent->time_since_last_gain = 0.0f;
              intent->decay_tick_timer = 0.0f;
              LOG_INFO("Sword Intent: Entity {} gained stack via skill {} hit. "
                       "Stacks: {}/{}",
                       (uint32_t)caster, evt.skill_id, intent->stacks,
                       intent->max_stacks);
            }
          }
        }

        // Dispatch to specific Skill Behavior
        if (evt.skill_id != 0) {
          if (auto hitFunc = SkillBehaviorRegistry::GetHit(evt.skill_id)) {
            hitFunc(registry, evt.source, evt.target, evt.tags, evt.is_crit);
          }
        }
      },
      50);

  // 3. Phantom Flash Counter (Defensive)
  CombatEventDispatcher::Register(
      CombatEventType::OnTakeDamage,
      [](entt::registry &registry, const CombatEvent &evt) {
        entt::entity victim =
            evt.source; // In OnTakeDamage, source is the victim (defender)
        entt::entity attacker =
            evt.target; // In OnTakeDamage, target is the attacker

        if (!registry.valid(victim) || !registry.valid(attacker))
          return;

        if (auto *pf = registry.try_get<PhantomFlashComponent>(victim)) {
          if (!pf->triggered && pf->counter_window > 0.0f) {
            pf->triggered = true;
            LOG_INFO("Phantom Flash Counter Triggered for entity {}!",
                     (uint32_t)victim);

            // Logic: Deal counter damage to attacker
            if (registry.all_of<CombatStats>(attacker)) {
              // Create a "Counter Strike" execution
              auto exec_ent = registry.create();
              registry.emplace<LocalLevelTag>(exec_ent);
              auto &exec = registry.emplace<SkillExecution>(exec_ent);
              exec.skill_id = 9; // Phantom Flash
              exec.owner = victim;
              exec.state = SkillState::Preparing;
              exec.timer = 0.0f; // Instant

              if (registry.all_of<Position>(attacker)) {
                const auto &attr_pos = registry.get<Position>(attacker);
                exec.target_pos = {attr_pos.x, attr_pos.y};
              }

              // Add counter-attack tag or logic?
              // For now, let the skill behavior handle it if we cast it,
              // OR just apply damage directly here.
              // Applying damage directly is simpler for a universal counter.

              // Simple Counter Damage: 200% of incoming damage? Or Stat based?
              // Let's settle for flat damage for now.
              if (auto *victim_stats = registry.try_get<CombatStats>(victim)) {
                // TODO: Calculate damage
              }
            }
          }
        }
      },
      50);

  LOG_INFO("Skill Hooks initialized. Skills are now loaded from "
           "SkillBehaviorRegistry.");
}

void SkillSystem::Update(entt::registry &registry,
                         systems::SpatialHashGrid &grid, float dt,
                         tf::Executor *executor) {
  UpdateCooldowns(registry, dt);
  UpdateStates(registry, dt);
  UpdateSwordIntent(registry, dt);

  // Update Blade Formation (ID 3)
  auto formation_view = registry.view<BladeFormationComponent, Position>();
  for (auto entity : formation_view) {
    auto &formation = formation_view.get<BladeFormationComponent>(entity);
    const auto &pos = formation_view.get<Position>(entity);

    // Update current_swords count from actual entities
    int count = 0;
    auto swordView = registry.view<SpiritSwordTag, SummonComponent>();
    for (auto swordEnt : swordView) {
      if (swordView.get<SummonComponent>(swordEnt).owner == entity) {
        count++;
      }
    }
    formation.current_swords = count;

    // Talent: Ling Jian Hu Ti (灵剑护体) - ID 320
    if (auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == 3 && spec.allocated_points.contains(320) &&
            spec.allocated_points.at(320) > 0) {
          auto &effects =
              registry.get_or_emplace<ActiveEffectsComponent>(entity);
          BuffEffect bladeDR;
          bladeDR.id = "ling_jian_hu_ti";
          bladeDR.name = "Ling Jian Hu Ti";
          bladeDR.type = BuffType::Shield;
          bladeDR.duration = 0.2f; // Short duration, refreshed every update
          bladeDR.remaining = 0.2f;

          float dr_per_sword = 2.0f * spec.allocated_points.at(320);
          float total_dr = formation.current_swords * dr_per_sword;

          bladeDR.modifiers.push_back(
              {StatType::ResistAll, ModifierMode::Flat, total_dr});
          effects.AddOrRefresh(bladeDR);
          break;
        }
      }
    }
  }

  // Update Sword Array (ID 6)
  auto array_view = registry.view<SwordArrayComponent, Position>();
  for (auto entity : array_view) {
    auto &array = array_view.get<SwordArrayComponent>(entity);
    skills::SwordArray::Update(registry, entity, array, dt, grid);
  }

  // Update Mind Blade (ID 7)
  auto mind_blade_view =
      registry.view<MindBladeComponent, MindBladeAI, Position>();
  std::vector<entt::entity> mb_to_destroy;
  for (auto entity : mind_blade_view) {
    auto &mc = mind_blade_view.get<MindBladeComponent>(entity);
    auto &ai = mind_blade_view.get<MindBladeAI>(entity);
    if (!skills::MindBlade::Update(registry, entity, ai, mc, dt, grid)) {
      mb_to_destroy.push_back(entity);
    }
  }
  for (auto e : mb_to_destroy) {
    registry.destroy(e);
  }

  // Update Channeling (ID 5 & 7)
  auto chan_view = registry.view<ChannelingComponent, Position>();
  for (auto entity : chan_view) {
    auto &chan = chan_view.get<ChannelingComponent>(entity);
    const auto &pos = chan_view.get<Position>(entity);

    // For skill 7, target_pos is updated in InputSystem.cpp to follow mouse
    // accurately.

    // 1. Duration Limit (5s hard cap)
    chan.total_duration += dt;
    if (chan.total_duration >= 5.0f) {
      registry.remove<ChannelingComponent>(entity);
      continue;
    }

    chan.channel_timer -= dt;
    if (chan.channel_timer <= 0.0f) {
      // Burst Finisher (Talent 513)
      if (chan.skill_id == 5 && chan.burst_finisher) {
        // Giant Sword Slash logic
        auto finisher_ent = registry.create();
        registry.emplace<LocalLevelTag>(finisher_ent);
        registry.emplace<ShadowCastTag>(finisher_ent);
        registry.emplace<Position>(finisher_ent, pos.x, pos.y);

        // Use Rending Wave (ID 2) but massively scaled
        auto &exec = registry.emplace<SkillExecution>(finisher_ent);
        exec.skill_id = 2;
        exec.owner = entity;
        exec.state = SkillState::Preparing;
        exec.timer = 0.0f;                 // Instant
        exec.target_pos = chan.target_pos; // Final aim
        exec.is_empowered = true;

        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          exec.has_snapshot = true;
          exec.snapshot.stats = *stats;
          exec.snapshot.skill_id = 5; // Attribute to Infinite Blades
          // 500% Damage
          for (auto &mult : exec.snapshot.stats.damage_multipliers) {
            mult *= 5.0f;
          }
          // Size scale handled in RendingWave logic or Projectile logic?
          // Ideally we'd set a specific flag or use a different skill ID (e.g.
          // 2 with "Giant" tag) For now, relies on base damage scaling.
        }
        LOG_INFO("Infinite Blades: Triggered Burst Finisher!");
      }

      registry.remove<ChannelingComponent>(entity);
      continue;
    }

    chan.tick_timer -= dt;

    // 2. VFX: Channeling Aura
    if (chan.skill_id == 5 || chan.skill_id == 7) {
      auto &particleSys = systems::GPUParticleSystem::Get();
      if ((float)GetRandomValue(0, 1000) < 500.0f * dt) {
        components::GPUParticle p;
        p.position = {pos.x + (float)GetRandomValue(-20, 20),
                      pos.y + (float)GetRandomValue(-10, 10)};
        p.velocity = {(float)GetRandomValue(-20, 20),
                      -50.0f - (float)GetRandomValue(0, 50)};
        p.color = chan.is_empowered ? GOLD : SKYBLUE;
        p.lifetime = 0.6f;
        p.maxLifetime = 0.6f;
        p.scale = 2.0f;
        p.flags = 2; // Spark
        particleSys.Emit(p);
      }
    }

    if (chan.tick_timer <= 0.0f) {
      if (chan.skill_id == 5) {
        // VFX: Sword emission toward target
        auto &particleSys = systems::GPUParticleSystem::Get();
        Vector2 dir =
            Vector2Normalize(Vector2Subtract(chan.target_pos, {pos.x, pos.y}));

        components::GPUParticle p;
        p.position = {pos.x + dir.x * 20.0f, pos.y + dir.y * 20.0f};
        p.velocity = Vector2Scale(dir, 400.0f);
        p.color = chan.is_empowered ? GOLD : WHITE;
        p.lifetime = 0.4f;
        p.maxLifetime = 0.4f;
        p.scale = 4.0f;
        p.flags = 13; // Sword shape (hypothetically if index 13 is sword)
        particleSys.Emit(p);

        // Logic: Cast Rending Wave (ID 2)
        Vector2 targetPos = chan.target_pos;

        // Talent: Full Screen Lock (530)
        if (chan.full_screen_lock) {
          // Simple: Find closest enemy to mouse cursor (chan.target_pos is
          // mouse pos usually) Or if "Full Screen", maybe random enemy on
          // screen? "Lock" implies targeting valid enemies. Let's iterate all
          // enemies? No, too slow. Use SpatialGrid? "prioritize mouse but
          // search wider". We'll search radius 800 around target_pos.
          float bestDistSq = 800.0f * 800.0f;
          entt::entity bestTarget = entt::null;

          // Access grid from System context?
          // SkillSystem::Update takes `systems::SpatialHashGrid &grid`.
          // Use the grid provided to the function.
          grid.query({targetPos.x, targetPos.y}, 800.0f,
                     [&](entt::entity e, const Position &ep) {
                       if (registry.any_of<EnemyTag>(e) &&
                           !registry.any_of<KilledTag>(e)) {
                         float distSq = Vector2DistanceSqr(
                             {targetPos.x, targetPos.y}, {ep.x, ep.y});
                         if (distSq < bestDistSq) {
                           bestDistSq = distSq;
                           bestTarget = e;
                         }
                       }
                     });

          if (registry.valid(bestTarget) &&
              registry.all_of<Position>(bestTarget)) {
            const auto &tp = registry.get<Position>(bestTarget);
            targetPos = {tp.x, tp.y};
          }
        }

        Vector2 target_dir =
            Vector2Normalize(Vector2Subtract(targetPos, {pos.x, pos.y}));
        // Add some random spread
        float spread = (float)GetRandomValue(-30, 30) * DEG2RAD;
        Vector2 fireDir = Vector2Rotate(target_dir, spread);

        Vector2 strike_target = {pos.x + fireDir.x * 250.0f,
                                 pos.y + fireDir.y * 250.0f};

        auto exec_ent = registry.create();
        registry.emplace<LocalLevelTag>(exec_ent);
        registry.emplace<ShadowCastTag>(exec_ent);
        auto &exec = registry.emplace<SkillExecution>(exec_ent);
        exec.skill_id = 2; // Rending Wave
        exec.owner = entity;
        exec.state =
            SkillState::Preparing; // Must be Preparing to trigger callback
        exec.timer = 0.05f;
        exec.target_pos = strike_target;
        exec.is_empowered = chan.is_empowered;

        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          exec.has_snapshot = true;
          exec.snapshot.stats = *stats;
          exec.snapshot.skill_id = 2;
        }

        // Talent: Yi Qi Bao Fa (意气爆发) - ID 520: Extra Projectile
        if (chan.extra_projectiles) {
          auto extra_exec_ent = registry.create();
          registry.emplace<LocalLevelTag>(extra_exec_ent);
          registry.emplace<ShadowCastTag>(extra_exec_ent);
          auto &extra_exec = registry.emplace<SkillExecution>(extra_exec_ent);
          extra_exec.skill_id = 2;
          extra_exec.owner = entity;
          extra_exec.state = SkillState::Preparing;
          extra_exec.timer = 0.05f;

          // Slightly varied target for extra projectle
          extra_exec.target_pos =
              Vector2Add(strike_target, {(float)GetRandomValue(-20, 20),
                                         (float)GetRandomValue(-20, 20)});
          extra_exec.is_empowered = chan.is_empowered;

          if (auto *stats = registry.try_get<CombatStats>(entity)) {
            extra_exec.has_snapshot = true;
            extra_exec.snapshot.stats = *stats;
            extra_exec.snapshot.skill_id = 2;
            for (auto &mult : extra_exec.snapshot.stats.damage_multipliers)
              mult *= 0.8f; // 80% damage for extra
          }
          LOG_DEBUG("Yi Qi Bao Fa: Spawned EXTRA Rending Wave execution.");
        }

      } else if (chan.skill_id == 7) {
        // Heart Sword: Shadowless (Spatial Cut)
        LOG_INFO("[DEBUG-SKILL7] TICK TRIGGERED! Emitting Spatial Cut VFX at "
                 "tick_timer={:.3f}",
                 chan.tick_timer);

        // 1. Calculate Cut Position (Clamped to Range)
        Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
        float dist = Vector2Length(diff);
        float max_range = 350.0f;
        Vector2 cutPos = chan.target_pos;
        Vector2 dir = {1.0f, 0.0f}; // Default if dist is 0

        if (dist > 0.001f) {
          dir = Vector2Scale(diff, 1.0f / dist); // Normalize
          if (dist > max_range) {
            cutPos = {pos.x + dir.x * max_range, pos.y + dir.y * max_range};
          }
        } else {
          cutPos = {pos.x + 50.0f, pos.y};
        }

        LOG_INFO("[DEBUG-SKILL7] Cut position: ({:.1f},{:.1f}), dir: "
                 "({:.2f},{:.2f})",
                 cutPos.x, cutPos.y, dir.x, dir.y);

        // 2. VFX: Spatial Cut (Perpendicular Line)
        auto &particleSys = systems::GPUParticleSystem::Get();
        Vector2 perp = {-dir.y, dir.x};
        float cutWidth = 100.0f;

        int emittedCount = 0;
        for (int i = 0; i < 15; ++i) {
          float t = (float)i / 14.0f;
          float offset = (t - 0.5f) * cutWidth;
          Vector2 pPos = {cutPos.x + perp.x * offset,
                          cutPos.y + perp.y * offset};

          // Core Spark - Golden flash along the cut line
          components::GPUParticle spark;
          spark.position = pPos;
          spark.velocity = Vector2Scale(dir, 60.0f);
          spark.acceleration = {0.0f, 0.0f};
          spark.color = GOLD;
          spark.scale = 4.0f; // Smaller spark
          spark.lifetime = 0.3f;
          spark.maxLifetime = 0.3f;
          spark.flags = 2;          // Spark/diamond shape
          spark.growthRate = -6.0f; // Shrink to nothing
          particleSys.Emit(spark);

          // Outer Glow - Softer surrounding effect
          components::GPUParticle glow;
          glow.position = pPos;
          glow.velocity = Vector2Scale(dir, 30.0f);
          glow.acceleration = {0.0f, 0.0f};
          glow.color = ColorAlpha(ORANGE, 0.5f);
          glow.scale = 6.0f; // Smaller glow
          glow.lifetime = 0.4f;
          glow.maxLifetime = 0.4f;
          glow.flags = 1;         // Soft glow
          glow.growthRate = 3.0f; // Expand slightly
          particleSys.Emit(glow);

          emittedCount += 2;
        }

        LOG_DEBUG("[MindBlade] Emitted {} VFX particles at ({:.1f},{:.1f})",
                  emittedCount, cutPos.x, cutPos.y);

        // 3. Logic: Spawn "Cut" Hitbox (Stationary Projectile)
        auto exec_ent = registry.create();
        registry.emplace<LocalLevelTag>(exec_ent);
        registry.emplace<Position>(exec_ent, cutPos.x, cutPos.y);
        registry.emplace<Velocity>(exec_ent, 0.0f, 0.0f);

        auto &proj = registry.emplace<Projectile>(exec_ent);
        proj.owner = entity;
        proj.cast_id = chan.cast_id;
        proj.radius = 60.0f; // AoE size
        proj.speed = 0.0f;
        proj.lifeTime = 0.1f; // Instant hit (one frame)
        proj.pierce = true;
        proj.pierceCount = 999;

        // Link stats
        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          // We can use SkillExecution to snapshot or just pass stats via new
          // Projectile system features? ProjectileSystem reads owner's stats
          // via DamagePipeline usually, OR it reads 'proj.snapshot' if we add
          // it to Projectile component? Looking at ProjectileSystem.cpp:204, it
          // reads registry.get<CombatStats>(entity) aka Projectile Entity? No,
          // "damage_attacker = proj.owner". So as long as owner has stats, we
          // are good.
        }

        auto &sc = registry.emplace<SkillComponent>(exec_ent);
        sc.skill_id = 7;

        chan.tick_timer = chan.tick_interval;
      }
    }
  }

  // Update Blade Ward
  auto ward_view = registry.view<BladeWardComponent>();
  for (auto entity : ward_view) {
    auto &ward = ward_view.get<BladeWardComponent>(entity);
    ward.remaining -= dt;
    if (ward.remaining <= 0.0f) {
      registry.remove<BladeWardComponent>(entity);
      continue;
    }

    // Keep the buff refreshed if we want it to stay for the duration
    // Actually, the buff has its own duration in ActiveEffectsComponent.
    // We just need to sync them or let them be independent.
  }

  // Update Phantom Flash
  auto pf_view = registry.view<PhantomFlashComponent>();
  // Use a temporary list to avoid iterator invalidation if Update removes
  // component? PhantomFlash::Update removes component immediately if expired.
  // View iteration is generally safe for component removal in EnTT?
  // EnTT views are safe if we don't destroy the entity. Removing component
  // might invalidate view iterator? Safest to collect list or use safe
  // iteration pattern if available. Standard pattern:
  std::vector<entt::entity> pf_entities;
  for (auto entity : pf_view) {
    pf_entities.push_back(entity);
  }
  for (auto entity : pf_entities) {
    if (registry.valid(entity) &&
        registry.all_of<PhantomFlashComponent>(entity)) {
      auto &pf = registry.get<PhantomFlashComponent>(entity);
      skills::PhantomFlash::Update(registry, entity, pf, dt);
    }
  }
}

void SkillSystem::RegisterEffect(uint32_t skill_id, CastCallback callback) {
  s_skill_callbacks[skill_id] = callback;
}

void SkillSystem::AddPreCastHook(SkillHook hook) {
  s_pre_cast_hooks.push_back(hook);
}

void SkillSystem::AddPostCastHook(SkillHook hook) {
  s_post_cast_hooks.push_back(hook);
}

void SkillSystem::ClearHooks() {
  s_pre_cast_hooks.clear();
  s_post_cast_hooks.clear();
}

bool SkillSystem::ShadowCast(entt::registry &registry, entt::entity owner,
                             uint32_t skill_id, Vector2 position,
                             Vector2 target_pos) {
  const auto *data = SkillRegistry::Get().GetSkill(skill_id);
  if (!data)
    return false;

  entt::entity shadow = owner;

  if (!registry.any_of<ShadowComponent>(owner) &&
      !registry.any_of<ShadowLifetime>(owner)) {
    shadow = registry.create();
    registry.emplace<LocalLevelTag>(shadow);
    registry.emplace<Position>(shadow, position.x, position.y);
    registry.emplace<Velocity>(shadow, 0.0f,
                               0.0f); // Ensure it has velocity for grid
    registry.emplace<AnimationStateComponent>(shadow);
    registry.emplace<ShadowLifetime>(shadow, 1.0f);

    if (registry.any_of<SpiritSwordTag>(owner)) {
      registry.emplace<SpiritSwordTag>(shadow);
    }
  }

  auto exec_ent = registry.create();
  registry.emplace<LocalLevelTag>(exec_ent);
  auto &exec = registry.emplace<SkillExecution>(exec_ent);
  exec.skill_id = skill_id;
  exec.owner = shadow;
  exec.state = SkillState::Preparing;
  exec.timer = 0.05f;
  exec.target_pos = target_pos;

  // Generate unique cast ID for shadow
  static uint64_t s_shadowCastId = 1000000;
  exec.cast_id = s_shadowCastId++;

  // Check if the caller provided a snapshot (either via ShadowComponent or
  // manual call)
  if (auto *sc = registry.try_get<ShadowComponent>(owner)) {
    exec.has_snapshot = true;
    exec.snapshot = sc->snapshot;
    exec.is_empowered = sc->snapshot.is_empowered;
    exec.active_nodes = sc->snapshot.active_nodes;

    // Inherit damage scale from the shadow that cast this (if chaining shadows)
    // Or strictly use the component's value if we want to mutate it.
    // Actually, shadows don't usually cast shadows?
    // If owner is a shadow, 'shadow' variable is 'owner'.
  } else if (auto *stats = registry.try_get<CombatStats>(owner)) {
    // Fallback: Use current owner stats
    exec.has_snapshot = true;
    exec.snapshot.stats = *stats;
    exec.snapshot.skill_id = skill_id;
    // No empowerment by default for non-snapshot casts unless we want it?
  }

  // Ensure shadow components are initialized
  if (!registry.all_of<ShadowComponent>(shadow)) {
    auto &sc = registry.get_or_emplace<ShadowComponent>(shadow);
    sc.damage_scale = 0.3f; // Default 30%

    if (!registry.all_of<ShadowVisualComponent>(shadow)) {
      auto &visual = registry.emplace<ShadowVisualComponent>(shadow);
      visual.color_tint = {40, 0, 60, 180}; // Deep ink purple
    }
  }

  registry.emplace_or_replace<CombatStats>(
      shadow, exec.snapshot.stats); // Ensure stats are on the entity

  registry.emplace<ShadowCastTag>(exec_ent);
  LOG_INFO("Shadow casting skill: {}", data->name_key);
  return true;
}

void SkillSystem::UpdateSwordIntent(entt::registry &registry, float dt) {
  auto view = registry.view<SwordIntentComponent>();
  for (auto entity : view) {
    auto &intent = view.get<SwordIntentComponent>(entity);

    // 1. Passive Gain - REMOVED per design change
    // Previously gained 1 stack/sec. Now only skill hits gain stacks.
    intent.passive_timer = 0.0f;

    // 2. Decay Logic
    if (intent.stacks > 0) {
      intent.time_since_last_gain += dt;

      if (intent.time_since_last_gain >= intent.grace_period) {
        // New Design: Clear ALL stacks after grace period (default 5s)
        intent.stacks = 0;
        intent.time_since_last_gain = 0.0f;
        LOG_INFO("Entity {} Sword Intent cleared (Inactive for {:.1f}s)",
                 (uint32_t)entity, intent.grace_period);
      }

      // Visuals
      if (IsWindowReady() && registry.all_of<Position>(entity)) {
        const auto &pos = registry.get<Position>(entity);
        if (utils::FrameRateUtils::ShouldTrigger(
                static_cast<float>(intent.stacks * 3), dt)) {
          components::GPUParticle p;
          p.position = {pos.x + GetRandomValue(-15, 15),
                        pos.y + GetRandomValue(-30, 0)};
          p.velocity = {0, -30.0f};
          p.acceleration = {0, 0};
          p.color = ColorAlpha(WHITE, 0.4f);
          p.lifetime = 0.5f;
          p.maxLifetime = 0.5f;
          p.scale = 1.0f + (intent.stacks * 0.1f);
          p.flags = 2; // Spark
          systems::GPUParticleSystem::Get().Emit(p);
        }
      }
    } else {
      intent.time_since_last_gain = 0.0f;
    }

    // Clean up old hit tracking entries to prevent memory leak
    for (auto it = intent.hit_tracking.begin();
         it != intent.hit_tracking.end();) {
      if (it->second.last_gain_time < (float)GetTime() - 10.0f) {
        it = intent.hit_tracking.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void SkillSystem::UpdateCooldowns(entt::registry &registry, float dt) {
  auto view = registry.view<ActiveSkillsComponent>();
  for (auto entity : view) {
    auto &active = view.get<ActiveSkillsComponent>(entity);
    for (auto &slot : active.slots) {
      if (slot.id == 0)
        continue;

      const auto *data = SkillRegistry::Get().GetSkill(slot.id);
      if (!data)
        continue;

      if (slot.current_charges < data->max_charges) {
        slot.cooldown -= dt;
        if (slot.cooldown <= 0.0f) {
          slot.current_charges++;
          if (slot.current_charges < data->max_charges) {
            auto *stats = registry.try_get<CombatStats>(entity);
            float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
            float cdr = StatsSystem::GetStatWithTags(
                            registry, entity, StatType::CooldownReduction,
                            data->tags, slot.id) /
                        100.0f;
            slot.cooldown =
                (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
          } else {
            slot.cooldown = 0.0f;
          }
        }
      }
    }
  }
}

void SkillSystem::UpdateStates(entt::registry &registry, float dt) {
  auto view = registry.view<SkillExecution>();
  for (auto entity : view) {
    auto &exec = view.get<SkillExecution>(entity);
    exec.timer -= dt;

    if (exec.timer <= 0.0f) {
      switch (exec.state) {
      case SkillState::Preparing:
        for (auto &hook : s_pre_cast_hooks) {
          hook(registry, entity, exec);
        }
        exec.state = SkillState::Casting;
        exec.timer = 0.05f;

        LOG_INFO("UpdateStates: Executing skill ID {} for entity {}",
                 exec.skill_id, (uint32_t)exec.owner);

        // NEW: Try SkillBehaviorRegistry first (new modular system)
        if (auto castFunc = SkillBehaviorRegistry::GetCast(exec.skill_id)) {
          castFunc(registry, exec.owner, exec);
        }
        // FALLBACK: Try legacy s_skill_callbacks (for gradual migration)
        else if (s_skill_callbacks.contains(exec.skill_id)) {
          s_skill_callbacks[exec.skill_id](registry, exec.owner, exec);
        } else {
          LOG_WARN(
              "UpdateStates: No callback found for skill ID {} on entity {}",
              exec.skill_id, (uint32_t)entity);
        }
        break;
      case SkillState::Casting:
        exec.state = SkillState::Settle;
        exec.timer = 0.1f;
        for (auto &hook : s_post_cast_hooks) {
          hook(registry, entity, exec);
        }
        break;
      case SkillState::Settle:
        if (auto *anim = registry.try_get<AnimationStateComponent>(entity)) {
          anim->state = EntityAnimState::Idle;
        }
        registry.remove<SkillExecution>(entity);
        continue;
      default:
        registry.remove<SkillExecution>(entity);
        continue;
      }
    }

    if (auto *anim = registry.try_get<AnimationStateComponent>(entity)) {
      switch (exec.state) {
      case SkillState::Preparing:
        anim->state = EntityAnimState::SkillWindup;
        break;
      case SkillState::Casting:
        anim->state = EntityAnimState::SkillCasting;
        break;
      case SkillState::Settle:
        anim->state = EntityAnimState::SkillRecovery;
        break;
      default:
        break;
      }
      anim->state_timer = exec.timer;
    }
  }
}

bool SkillSystem::TryCast(entt::registry &registry, entt::entity entity,
                          int slot_index, Vector2 target_pos) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active || slot_index < 0 || slot_index >= (int)active->slots.size())
    return false;

  auto &slot = active->slots[slot_index];
  if (slot.id == 0) {
    LOG_WARN("TryCast FAILED: No skill in slot {}", slot_index);
    return false;
  }

  if (registry.any_of<SkillExecution>(entity)) {
    LOG_TRACE("TryCast: Entity {} is already executing a skill",
              (uint32_t)entity);
    return false;
  }

  const auto *data = SkillRegistry::Get().GetSkill(slot.id);
  if (!data) {
    LOG_ERROR("TryCast FAILED: Skill ID {} data not found", slot.id);
    return false;
  }

  if (slot.current_charges <= 0) {
    LOG_TRACE("TryCast: Skill {} has no charges ({} / {})", data->name_key,
              slot.current_charges, data->max_charges);
    return false;
  }

  auto *stats = registry.try_get<CombatStats>(entity);
  float rcr = stats ? StatsSystem::GetStatWithTags(
                          registry, entity, StatType::ResourceCostReduction,
                          data->tags, slot.id) /
                          100.0f
                    : 0.0f;
  float base_cost = data->mana_cost * (1.0f - std::min(0.9f, rcr));

  // --- Shadow Kill Array (ID 124) Duplication Logic ---
  bool shadow_duplicate = false;
  if (registry.any_of<ShadowKillArrayReady>(entity)) {
    bool excluded =
        HasTag(data->tags, Tag::Movement) || HasTag(data->tags, Tag::Buff) ||
        HasTag(data->tags, Tag::Aura) || HasTag(data->tags, Tag::Channeled);

    if (!excluded) {
      auto *pStats = registry.try_get<PlayerStats>(entity);
      float currentTime = (float)GetTime();
      if (pStats && (currentTime - pStats->last_shadow_trigger_time >= 3.0f)) {
        float extra_cost = base_cost * 0.5f;
        if (stats && stats->mana >= (base_cost + extra_cost)) {
          shadow_duplicate = true;
        }
      }
    }
  }

  if (stats) {
    float total_cost = base_cost;
    if (shadow_duplicate)
      total_cost += base_cost * 0.5f;

    if (stats->mana < total_cost)
      return false;
    stats->mana -= total_cost;
  }

  if (shadow_duplicate) {
    auto *pStats = registry.try_get<PlayerStats>(entity);
    if (pStats)
      pStats->last_shadow_trigger_time = (float)GetTime();
    registry.remove<ShadowKillArrayReady>(entity);

    auto *pos = registry.try_get<Position>(entity);
    Vector2 spawnPos = pos ? Vector2{pos->x, pos->y} : Vector2{0, 0};

    auto shadow_ent = registry.create();
    registry.emplace<LocalLevelTag>(shadow_ent);
    registry.emplace<Position>(shadow_ent, spawnPos.x, spawnPos.y);
    registry.emplace<AnimationStateComponent>(shadow_ent);
    registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(PURPLE, 0.4f));
    registry.emplace<ShadowCloneComponent>(shadow_ent);

    auto &sc = registry.emplace<ShadowComponent>(shadow_ent);
    sc.damage_scale = 0.5f; // Explicit 50% for Shadow Kill Array
    sc.delay = 0.1f;
    sc.lifetime = 1.0f;
    sc.snapshot.skill_id = slot.id;
    sc.snapshot.position = spawnPos;
    sc.snapshot.target_pos = target_pos;
    if (stats) {
      sc.snapshot.stats = *stats;
    }

    registry.emplace<ShadowVisualComponent>(shadow_ent).color_tint = {
        60, 0, 80, 200}; // Distinct visual for clone
    LOG_INFO("Shadow Kill Array: Duplicating skill {} for entity {}", slot.id,
             (uint32_t)entity);
  }

  if (slot.current_charges == data->max_charges) {
    float cdr = StatsSystem::GetStatWithTags(registry, entity,
                                             StatType::CooldownReduction,
                                             data->tags, slot.id) /
                100.0f;
    float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
    // Optimization: For Channeled skills with very long cooldowns (like 60s),
    // we might NOT want to start cooldown here but when channeling ends?
    // But preventing abuse is safer.
    slot.cooldown = (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
  }
  slot.current_charges--;

  static uint64_t s_nextCastId = 1;
  uint64_t cast_id = s_nextCastId++;

  auto &exec = registry.emplace<SkillExecution>(entity);
  exec.skill_id = slot.id;
  exec.owner = entity;
  exec.cast_id = cast_id;
  exec.slot_index = slot_index;
  exec.state = SkillState::Preparing;
  exec.timer = 0.1f;
  exec.target_pos = target_pos;

  // Populate active_nodes from Specialization
  if (slot_index >= 0 && slot_index < (int)active->specialized_slots.size()) {
    const auto &spec = active->specialized_slots[slot_index];
    if (spec.skill_id == slot.id) {
      for (auto const &[node_id, points] : spec.allocated_points) {
        if (points > 0) {
          // ID Convention: SkillID * 100 + Index (0-99)
          uint32_t bit_idx = node_id % 100;
          if (bit_idx < 128) {
            exec.active_nodes.set(bit_idx);
          }
        }
      }
    }
  }

  LOG_INFO("TryCast SUCCESS: Entity {} casting skill ID {} ({})",
           (uint32_t)entity, slot.id, data->name_key);
  return true;
}

void SkillSystem::HandleSkillInput(entt::registry &registry,
                                   entt::entity entity, int slot_index,
                                   Vector2 target_pos) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active || slot_index < 0 || slot_index >= (int)active->slots.size())
    return;

  auto &slot = active->slots[slot_index];
  if (slot.id == 0)
    return;

  // 1. Maintain Channeling
  if (auto *chan = registry.try_get<ChannelingComponent>(entity)) {
    if (chan->skill_id == slot.id) {
      chan->channel_timer = 0.25f; // Keep alive
      chan->target_pos = target_pos;
      // Maybe handle ticking here if we want instant feedback?
      // No, update loop handles it.
      return;
    }
    // If channeling something else, we ignore input (or we could interrupt)
    return;
  }

  // 2. Start New Cast
  TryCast(registry, entity, slot_index, target_pos);
}

bool SkillSystem::AddTalentPoint(entt::registry &registry, entt::entity entity,
                                 uint32_t skill_id, uint32_t node_id) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  SpecializedSkill *specialized = nullptr;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == skill_id) {
      specialized = &slot;
      break;
    }
  }
  if (!specialized) {
    LOG_WARN(
        "Cannot add talent point: Skill {} is not specialized for entity {}",
        skill_id, (uint32_t)entity);
    return false;
  }

  if (active->available_talent_points <= 0) {
    LOG_WARN("Cannot add talent point: No points available for entity {}",
             (uint32_t)entity);
    return false;
  }

  if (specialized->GetPointsSpent() >= specialized->GetMaxPoints()) {
    LOG_WARN("Cannot add talent point: Skill {} has reached max points ({}/{})",
             skill_id, specialized->GetPointsSpent(),
             specialized->GetMaxPoints());
    return false;
  }

  const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
  if (!tree)
    return false;

  auto node_it = tree->nodes.find(node_id);
  if (node_it == tree->nodes.end())
    return false;
  const auto &node = node_it->second;

  int current_pts = specialized->allocated_points.contains(node_id)
                        ? specialized->allocated_points.at(node_id)
                        : 0;
  if (current_pts >= node.max_points) {
    LOG_WARN("Cannot add talent point: Node {} already at max ({}/{})", node_id,
             current_pts, node.max_points);
    return false;
  }

  for (uint32_t pre_id : node.prerequisites) {
    int pre_pts = specialized->allocated_points.contains(pre_id)
                      ? specialized->allocated_points.at(pre_id)
                      : 0;
    if (pre_pts <= 0) {
      LOG_WARN("Cannot add talent point: Prerequisite {} not met for node {}",
               pre_id, node_id);
      return false;
    }
  }

  active->available_talent_points--;
  specialized->allocated_points[node_id] = current_pts + 1;
  registry.get_or_emplace<StatsDirty>(entity);

  LOG_INFO("Entity {} spent talent point on Skill {} -> Node {} ({}/{})",
           (uint32_t)entity, skill_id, node_id,
           specialized->allocated_points[node_id], node.max_points);

  return true;
}

bool SkillSystem::ResetTalents(entt::registry &registry, entt::entity entity,
                               uint32_t skill_id) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  SpecializedSkill *specialized = nullptr;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == skill_id) {
      specialized = &slot;
      break;
    }
  }
  if (!specialized)
    return false;

  int points_to_refund = 0;
  for (auto [node_id, pts] : specialized->allocated_points) {
    points_to_refund += pts;
  }

  active->available_talent_points += points_to_refund;
  specialized->allocated_points.clear();

  registry.get_or_emplace<StatsDirty>(entity);
  LOG_INFO("Entity {} reset talents for Skill {}. Refunded {} points.",
           (uint32_t)entity, skill_id, points_to_refund);

  return true;
}

bool SkillSystem::ClearAllTalents(entt::registry &registry,
                                  entt::entity entity) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  int total_refunded = 0;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == 0)
      continue;

    for (auto [node_id, pts] : slot.allocated_points) {
      total_refunded += pts;
    }
    slot.allocated_points.clear();
  }

  active->available_talent_points += total_refunded;
  registry.get_or_emplace<StatsDirty>(entity);
  LOG_INFO("Entity {} cleared all talents. Refunded {} points.",
           (uint32_t)entity, total_refunded);

  return true;
}

Tag SkillSystem::GetEffectiveSkillTags(entt::registry &registry,
                                       entt::entity entity, uint32_t skill_id) {
  // Start with base tags from skill definition
  const auto *skill = SkillRegistry::Get().GetSkill(skill_id);
  if (!skill)
    return Tag::None;

  Tag tags = skill->tags;

  // Apply talent modifications
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return tags;

  // Find the specialized slot for this skill
  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id != skill_id)
      continue;

    // Get the skill tree definition
    const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
    if (!tree)
      break;

    // Apply tag modifications from allocated talent nodes
    for (const auto &[node_id, points] : spec.allocated_points) {
      if (points <= 0)
        continue;

      auto it = tree->nodes.find(node_id);
      if (it == tree->nodes.end())
        continue;

      const auto &node = it->second;

      // Add tags from this talent
      tags = tags | node.add_tags;

      // Remove tags from this talent (using bitwise AND with NOT)
      tags = tags & ~node.remove_tags;
    }
    break;
  }

  return tags;
}

} // namespace NoMoreDay