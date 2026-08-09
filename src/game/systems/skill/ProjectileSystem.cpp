#include "game/systems/skill/ProjectileSystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/physics/PhysicsUtils.hpp"
#include "engine/render/SIMDSpatialGrid.hpp" // Phase 4 Integration
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "game/components/AIComponent.hpp"
#include "game/systems/skill/ProjectileConstants.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/HazardComponents.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/components/vfx/VisualGhostComponent.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raylib.h"

namespace NoMoreDay {

// Helper struct for deferred actions
struct DeferredAction {
  enum Type { Destroy, Damage, Pull, CounterShot, CounterSpin };
  Type type;
  entt::entity entity;     // Subject (Projectile or Target)
  entt::entity target;     // Target for damage/pull
  float value;             // Damage amount or Pull strength
  bool flag;               // Critical hit or specialized flag
  entt::entity instigator; // Who caused it
  Vector2 pos;
};

void ProjectileSystem::Update(entt::registry &registry,
                              systems::SpatialHashGrid & /*grid*/,
                              float dt, // Ignore old grid
                              tf::Executor *executor) {
  auto view = registry.view<Position, Velocity, Projectile>();

  // Phase 4: Rebuild SIMD Grid with Enemies Only
  // This drastically reduces grid pollution from projectiles/items
  s_enemyGrid.rebuild<Position>(registry.view<EnemyTag, Position>(), registry);

  // Cache Player for scalar check (Player is not in Enemy Grid)
  entt::entity playerEnt = entt::null;
  auto playerView = registry.view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end())
    playerEnt = playerView.front();

  // Helper: Combined Query (SIMD Enemy Grid + Scalar Player Check)
  auto QueryWorld = [&](Vector2 center, float radius, auto &&callback) {
    // 1. SIMD Grid (Enemies)
    s_enemyGrid.query({center.x, center.y}, radius, callback);

    // 2. Scalar Player Check
    if (registry.valid(playerEnt)) {
      const auto &pPos = registry.get<Position>(playerEnt);
      float dx = pPos.x - center.x;
      float dy = pPos.y - center.y;
      if (dx * dx + dy * dy <= radius * radius) {
        callback(playerEnt, pPos);
      }
    }
  };

  if (view.begin() == view.end())
    return;

  // Logic implementation lambda (Thread-Safe Simulation Phase)
  auto SimulateProjectile = [&](entt::entity entity, Position &pos,
                                Velocity &vel, Projectile &proj,
                                std::vector<DeferredAction> &actions) -> bool {
    bool hasAction = false;

    // 1. Boomerang Logic
    if (auto *bc = registry.try_get<BoomerangComponent>(entity)) {
      if (bc->phase == BoomerangComponent::Outward) {
        bc->returnTimer -= dt;
        if (bc->returnTimer <= 0.0f) {
          bc->phase = BoomerangComponent::Paused;
          bc->pauseTimer = 0.2f;

          // Emit Shockwave on apex
          components::GPUParticle p;
          p.position = {pos.x, pos.y};
          p.velocity = {0, 0};
          p.color = {180, 240, 255, 200};
          p.lifetime = 0.4f;
          p.maxLifetime = 0.4f;
          p.scale = proj.radius;
          p.flags = 2;           // Glow
          p.growthRate = 120.0f; // Rapid expansion
          systems::GPUParticleSystem::Get().Emit(p);
        }
      } else if (bc->phase == BoomerangComponent::Paused) {
        bc->pauseTimer -= dt;
        vel.vx = 0;
        vel.vy = 0;
        if (bc->pauseTimer <= 0.0f) {
          bc->phase = BoomerangComponent::Returning;
        }
      } else {
        entt::entity targetEnt =
            registry.valid(bc->returnTarget) ? bc->returnTarget : bc->owner;
        if (registry.valid(targetEnt) && registry.all_of<Position>(targetEnt)) {
          const auto &targetPos = registry.get<Position>(targetEnt);
          Vector2 toTarget =
              Vector2Subtract({targetPos.x, targetPos.y}, {pos.x, pos.y});
          float dist = Vector2Length(toTarget);

          using namespace NoMoreDay::Constants::Skill;
          if (dist < PROJECTILE_RETURN_THRESHOLD) {
            const auto *skillComp = registry.try_get<SkillComponent>(entity);
            if (skillComp && skillComp->skill_id == 8 &&
                registry.valid(bc->owner)) {
              auto *active = registry.try_get<ActiveSkillsComponent>(bc->owner);
              auto *stats = registry.try_get<CombatStats>(bc->owner);
              if (active) {
                for (const auto &spec : active->specialized_slots) {
                  if (spec.skill_id != 8) {
                    continue;
                  }
                  auto it = spec.allocated_points.find(831u);
                  if (it != spec.allocated_points.end() && it->second > 0) {
                    const float manaGain = 2.0f * static_cast<float>(it->second);
                    if (stats) {
                      stats->mana += manaGain;
                      if (stats->mana > stats->max_mana) {
                        stats->mana = stats->max_mana;
                      }
                    }
                    for (auto &slot : active->slots) {
                      if (slot.id != 8 || slot.cooldown <= 0.0f) {
                        continue;
                      }
                      slot.cooldown -= 0.35f * static_cast<float>(it->second);
                      if (slot.cooldown < 0.0f) {
                        slot.cooldown = 0.0f;
                      }
                    }
                    break;
                  }
                }
              }
            }
            actions.push_back({DeferredAction::Destroy, entity});
            return true;
          }

          float speed =
              (bc->returnSpeed > 0.1f)
                  ? bc->returnSpeed
                  : (proj.speed > 0.1f ? proj.speed
                                       : PROJECTILE_DEFAULT_RETURN_SPEED);
          Vector2 dir = Vector2Scale(Vector2Normalize(toTarget), speed);
          vel.vx = dir.x;
          vel.vy = dir.y;
        } else {
          bc->phase = BoomerangComponent::Outward;
        }
      }
    }

    // 1.5 Homing Auto-Targeting
    bool shouldHome = registry.any_of<HomingTag>(entity);
    if (!shouldHome && registry.valid(proj.owner)) {
      if (auto *affix = registry.try_get<MonsterAffixComponent>(proj.owner)) {
        if (affix->HasAffix(MonsterAffixType::Accurate)) {
          shouldHome = true;
        }
      }
    }

    if (shouldHome) {
      if (auto *seeker = registry.try_get<SeekerComponent>(entity)) {
        if (!registry.valid(seeker->target)) {
          float minDist = seeker->range;
          entt::entity bestTarget = entt::null;

          // Phase 4: Use QueryWorld
          QueryWorld({pos.x, pos.y}, seeker->range,
                     [&](entt::entity t, const Position &tp) -> bool {
                       if (t == entity || t == proj.owner)
                         return true;
                       if (!registry.valid(t))
                         return true;

                       bool ownerIsPlayer =
                           registry.any_of<PlayerTag>(proj.owner);
                       bool tIsPlayer = registry.any_of<PlayerTag>(t);
                       bool tIsEnemy = registry.any_of<EnemyTag>(t);

                       if (ownerIsPlayer && !tIsEnemy)
                         return true;
                       if (!ownerIsPlayer && !tIsPlayer)
                         return true;
                       if (registry.any_of<KilledTag>(t))
                         return true;

                       float dx = tp.x - pos.x;
                       float dy = tp.y - pos.y;
                       float dist = std::sqrt(dx * dx + dy * dy);
                       if (dist < minDist) {
                         minDist = dist;
                         bestTarget = t;
                       }
                       return true;
                     });

          if (bestTarget != entt::null) {
            seeker->target = bestTarget;
          }
        }
      }
    }

    // 2. Seeker Logic
    if (auto *seeker = registry.try_get<SeekerComponent>(entity)) {
      if (registry.valid(seeker->target) &&
          registry.all_of<Position>(seeker->target)) {
        const auto &tPos = registry.get<Position>(seeker->target);
        Vector2 desired = Vector2Subtract({tPos.x, tPos.y}, {pos.x, pos.y});
        float dist = Vector2Length(desired);

        if (dist > 0.001f && dist <= seeker->range) {
          desired = Vector2Scale(Vector2Normalize(desired), proj.speed);
          Vector2 current = {vel.vx, vel.vy};
          float currentAngle = atan2f(current.y, current.x);
          float desiredAngle = atan2f(desired.y, desired.x);
          float diff = desiredAngle - currentAngle;
          while (diff <= -PI)
            diff += 2 * PI;
          while (diff > PI)
            diff -= 2 * PI;
          float turn = seeker->turn_rate * dt;
          if (abs(diff) < turn) {
            vel.vx = desired.x;
            vel.vy = desired.y;
          } else {
            float newAngle = currentAngle + copysignf(turn, diff);
            vel.vx = cosf(newAngle) * proj.speed;
            vel.vy = sinf(newAngle) * proj.speed;
          }
          if (seeker->stop_on_arrival && dist < seeker->arrival_threshold) {
            vel.vx = 0;
            vel.vy = 0;
          }
        }
      }
    }

    // 3. Pull Logic (Deferred)
    if (proj.hasPull) {
      using namespace NoMoreDay::Constants::Skill;
      float pullRadius = proj.radius * PROJECTILE_PULL_RADIUS_MULTIPLIER;
      // Phase 4: Use QueryWorld
      QueryWorld({pos.x, pos.y}, pullRadius,
                 [&](entt::entity target, const Position &tPos) -> bool {
                   if (target == proj.owner || target == entity)
                     return true;
                   if (!registry.valid(target) ||
                       !registry.all_of<Velocity, Position>(target))
                     return true;
                   if (!registry.any_of<EnemyTag>(target))
                     return true;

                   DeferredAction act;
                   act.type = DeferredAction::Pull;
                   act.entity = target;
                   act.value = proj.pullStrength;
                   act.instigator = entity;
                   act.pos = {pos.x, pos.y};
                   actions.push_back(act);
                   return true;
                 });
      if (!actions.empty() && actions.back().type == DeferredAction::Pull)
        hasAction = true;
    }

    // 4. Position Sync
    if (auto *skillComp = registry.try_get<SkillComponent>(entity)) {
      if (skillComp->skill_id == 1 && registry.valid(proj.owner)) {
        if (auto *ownerPos = registry.try_get<Position>(proj.owner)) {
          if (auto *ownerVel = registry.try_get<Velocity>(proj.owner)) {
            float speedSq =
                ownerVel->vx * ownerVel->vx + ownerVel->vy * ownerVel->vy;
            if (speedSq > 0.1f) {
              float invSpeed = 1.0f / sqrtf(speedSq);
              float dirX = ownerVel->vx * invSpeed;
              float dirY = ownerVel->vy * invSpeed;

              float forwardOffset = proj.radius * 1.2f;
              pos.x = ownerPos->x + dirX * forwardOffset;
              pos.y = ownerPos->y + dirY * forwardOffset;

              vel.vx = ownerVel->vx;
              vel.vy = ownerVel->vy;
            }
          }
        }
      }
    }

    // 5. Visual Effects (Thread Local Buffer)
    static thread_local std::vector<components::GPUParticle> s_particles;
    s_particles.clear();

    uint32_t skill_id = 0;
    if (auto *sc = registry.try_get<SkillComponent>(entity))
      skill_id = sc->skill_id;

    if (skill_id == 1 || skill_id == 2 || skill_id == 7 || skill_id == 8 ||
        skill_id == 9) {
      using namespace NoMoreDay::Constants::Skill;
      Vector2 trailVel =
          Vector2Scale({vel.vx, vel.vy}, PROJECTILE_TRAIL_VEL_SCALE);

      if (skill_id == 1) {
        static thread_local float snapshotTimer = 0.0f;
        snapshotTimer += dt;
        if (snapshotTimer >= 0.05f) {
          snapshotTimer = 0.0f;
          if (registry.valid(proj.owner)) {
            if (auto *ownerSprite =
                    registry.try_get<SpriteComponent>(proj.owner)) {
              actions.push_back(
                  {DeferredAction::CounterSpin, entity, proj.owner});
            }
          }
        }
      }

      if (skill_id == 8) {
        float time = (float)GetTime() * 10.0f;
        Vector2 off1 = {cosf(time) * PROJECTILE_ROTATING_TRAIL_RADIUS,
                        sinf(time) * PROJECTILE_ROTATING_TRAIL_RADIUS};
        s_particles.push_back(systems::InkEffectHelper::CreateInkTrail(
            {pos.x + off1.x, pos.y + off1.y}, trailVel, 1.0f, 0.3f));
        s_particles.push_back(systems::InkEffectHelper::CreateInkTrail(
            {pos.x - off1.x, pos.y - off1.y}, trailVel, 1.0f, 0.3f));
      } else if (skill_id == 7) {
        auto p = systems::InkEffectHelper::CreateInkTrail({pos.x, pos.y},
                                                          trailVel, 2.0f, 0.25f); // 0.5 -> 0.25
        p.color = GOLD;
        s_particles.push_back(p);
      } else {
        // Skill 2 (Rift Slash): Extremely short lifetime (0.07s) for peak snappiness
        if (skill_id == 2) {
          Color trailColor = NoMoreDay::components::Colors::BLADE_CYAN;
          if (auto *col = registry.try_get<ColorComponent>(entity)) {
            trailColor = col->color;
          }

          const float speed = sqrtf(vel.vx * vel.vx + vel.vy * vel.vy);
          Vector2 dir = {1.0f, 0.0f};
          if (speed > 1e-4f) {
            dir = {vel.vx / speed, vel.vy / speed};
          }

          const int sampleCount = 3;
          for (int i = 0; i < sampleCount; ++i) {
            const float t = static_cast<float>(i) /
                            static_cast<float>(sampleCount - 1);
            const float backOffset = 2.0f + 6.0f * t;
            Vector2 samplePos = {pos.x - dir.x * backOffset,
                                 pos.y - dir.y * backOffset};

            auto trail = systems::InkEffectHelper::CreateInkTrail(
                samplePos, Vector2Scale(trailVel, 0.55f + 0.1f * (1.0f - t)),
                0.42f + 0.08f * (1.0f - t), 0.085f + 0.02f * t);
            trail.color = trailColor;
            trail.color.a = static_cast<unsigned char>(150.0f - 40.0f * t);
            trail.flags = 13;          // Soft edge ink, avoids hard dot look.
            trail.growthRate = -1.1f;  // Slight shrink keeps line compact.
            trail.scale = 0.52f - 0.10f * t;
            s_particles.push_back(trail);
          }
        } else {
          s_particles.push_back(systems::InkEffectHelper::CreateInkTrail(
              {pos.x, pos.y}, trailVel, 1.2f, 0.2f)); // 0.4 -> 0.2
        }
      }
    }

    if (!s_particles.empty())
      systems::GPUParticleSystem::Get().EmitBatch(s_particles);

    // 6. Lifetime
    proj.lifeTime -= dt;
    if (proj.lifeTime <= 0.0f) {
      auto onDeath = proj.on_death;
      if (onDeath != Projectile::OnDeathBehavior::None) {
        OnProjectileDeath(registry, entity, proj, DeathReason::Expired);
        if (onDeath == Projectile::OnDeathBehavior::Hover)
          return true;
      }
      actions.push_back({DeferredAction::Destroy, entity});
      return true;
    }

    // 7. Collision (Phase 4: Use QueryWorld)
    bool hit = false;
    using namespace NoMoreDay::Constants::Skill;
    float check_radius = proj.radius + PROJECTILE_COLLISION_RADIUS_OFFSET;

    static thread_local std::vector<entt::entity> s_uniqueHits;
    s_uniqueHits.clear();

    QueryWorld(
        {pos.x, pos.y}, check_radius,
        [&](entt::entity target, const Position &tPos) -> bool {
          if (hit && !proj.pierce)
            return false;
          if (proj.pierce && proj.pierceCount < 0)
            return false;
          if (target == proj.owner || target == entity)
            return true;

          if (!registry.valid(target))
            return true;

          bool ownerIsPlayer = registry.any_of<PlayerTag>(proj.owner);
          bool targetIsEnemy = registry.any_of<EnemyTag>(target);
          bool ownerIsEnemy = registry.any_of<EnemyTag>(proj.owner);
          bool targetIsPlayer = registry.any_of<PlayerTag>(target);

          if (ownerIsPlayer && !targetIsEnemy)
            return true;
          if (ownerIsEnemy && !targetIsPlayer)
            return true;

          for (auto e : s_uniqueHits)
            if (e == target)
              return true;
          s_uniqueHits.push_back(target);

          for (auto e : proj.hitEntities)
            if (e == target)
              return true;

          // Distance check already done by QueryWorld/query mostly, but double
          // check doesn't hurt if grid returns candidates
          float dx = tPos.x - pos.x;
          float dy = tPos.y - pos.y;
          if (dx * dx + dy * dy <= check_radius * check_radius) {
            // Interception Logic
            if (auto *ward = registry.try_get<BladeWardComponent>(target)) {
              float chance = ward->sword_count * ward->interception_chance;
              if ((float)GetRandomValue(0, 1000) / 1000.0f < chance) {
                DeferredAction hitAct;
                hitAct.type = DeferredAction::Damage;
                hitAct.entity = entity;
                hitAct.target = target;
                hitAct.instigator = proj.owner;
                hitAct.pos = {pos.x, pos.y};
                actions.push_back(hitAct);
                return true;
              }
            }

            // Valid Hit
            DeferredAction hitAct;
            hitAct.type = DeferredAction::Damage;
            hitAct.entity = entity;
            hitAct.target = target;
            hitAct.instigator = proj.owner;
            hitAct.pos = {pos.x, pos.y};
            actions.push_back(hitAct);

            proj.hitEntities.push_back(target);
            if (!proj.pierce) {
              hit = true;
              proj.hitLimitReached = true;
              return false; // Stop query after first hit if no pierce
            } else {
              proj.pierceCount--;
              if (proj.pierceCount < 0) {
                hit = true;
                proj.hitLimitReached = true;
                return false;
              }
            }
          }
          return true;
        });

    if (proj.hitLimitReached && (proj.hasRendered || proj.lifeTime <= 0.0f)) {
      auto onDeath = proj.on_death;
      if (onDeath != Projectile::OnDeathBehavior::None) {
        OnProjectileDeath(registry, entity, proj, DeathReason::Collision);
        if (onDeath == Projectile::OnDeathBehavior::Hover)
          return true;
      }
      actions.push_back({DeferredAction::Destroy, entity});
      hasAction = true;
    }
    return hasAction;
  };

  std::vector<entt::entity> entities;
  for (auto e : view)
    entities.push_back(e);

  if (entities.empty()) return;

  const int chunkSize = 64;
  const int numChunks = (int)((entities.size() + chunkSize - 1) / chunkSize);
  std::vector<std::vector<DeferredAction>> perTaskActions(numChunks);

  auto run_chunk = [&](int chunkIdx) {
    int start = chunkIdx * chunkSize;
    int end = std::min(start + chunkSize, (int)entities.size());
    auto& localActions = perTaskActions[chunkIdx];
    localActions.reserve(chunkSize / 2); // Heuristic

    for (int i = start; i < end; ++i) {
      entt::entity e = entities[i];
      if (!registry.valid(e))
        continue;

      auto &pos = registry.get<Position>(e);
      auto &vel = registry.get<Velocity>(e);
      auto &proj = registry.get<Projectile>(e);

      SimulateProjectile(e, pos, vel, proj, localActions);
    }
  };

  if (executor && numChunks > 1) {
    tf::Taskflow tf;
    tf.for_each_index(0, numChunks, 1, run_chunk);
    executor->run(tf).wait();
  } else {
    for (int i = 0; i < numChunks; ++i) run_chunk(i);
  }

  // Aggregate results (Serial Phase)
  std::vector<DeferredAction> globalActions;
  size_t totalActions = 0;
  for (const auto& vec : perTaskActions) totalActions += vec.size();
  globalActions.reserve(totalActions);
  
  for (auto& vec : perTaskActions) {
    globalActions.insert(globalActions.end(), 
                         std::make_move_iterator(vec.begin()), 
                         std::make_move_iterator(vec.end()));
  }

  // SERIAL PHASE: Process Deferred Actions
  auto &particleSys = systems::GPUParticleSystem::Get();

  for (const auto &act : globalActions) {
    if (!registry.valid(act.entity) && act.type != DeferredAction::Damage)
      continue;

    if (act.type == DeferredAction::Destroy) {
      if (registry.valid(act.entity))
        registry.destroy(act.entity);
    } else if (act.type == DeferredAction::Pull) {
      if (registry.valid(act.entity) && registry.all_of<Velocity>(act.entity)) {
        auto &tVel = registry.get<Velocity>(act.entity);
        if (registry.all_of<Position>(act.entity)) {
          auto &tPos = registry.get<Position>(act.entity);
          Vector2 dir =
              Vector2Normalize(Vector2Subtract(act.pos, {tPos.x, tPos.y}));
          tVel.vx += dir.x * act.value * dt;
          tVel.vy += dir.y * act.value * dt;
        }
      }
    } else if (act.type == DeferredAction::Damage) {
      entt::entity projEnt = act.entity;
      entt::entity target = act.target;

      if (!registry.valid(target))
        continue;

      bool intercepted = false;
      if (auto *ward = registry.try_get<BladeWardComponent>(target)) {
        float chance = ward->sword_count * ward->interception_chance;
        if (!ward->is_solidified && ward->sword_count > 0 &&
            (float)GetRandomValue(0, 1000) / 1000.0f < chance) {
          intercepted = true;
          ward->sword_count--;
          particleSys.Emit(systems::InkEffectHelper::CreateGoldParticle(
              act.pos, {0, -50.0f}, 1.5f));
        }
      }

      if (intercepted) {
        if (auto *ward = registry.try_get<BladeWardComponent>(target)) {
          if (ward->trigger_counter && registry.valid(act.instigator) &&
              registry.all_of<CombatStats>(act.instigator)) {
            DamagePool counterPool;
            counterPool.Add(ward->has_rainbow_qi ? Tag::Lightning : Tag::Physical,
                            ward->has_blink_counter ? 55.0f : 35.0f);
            DamageRequest counterRequest;
            counterRequest.attacker = target;
            counterRequest.defender = act.instigator;
            counterRequest.skill_id = 4;
            counterRequest.base_pool = counterPool;
            counterRequest.additional_tags = Tag::Hit | Tag::Melee;
            counterRequest.source_entity = target;
            (void)ResolveDamage(registry, counterRequest, target);
            if (ward->has_agile_counter) {
              SkillSystem::GainSwordIntent(registry, target, 1, 4);
            }
            if (ward->counter_spin) {
              particleSys.Emit(systems::InkEffectHelper::CreateGoldParticle(
                  act.pos, {0.0f, -80.0f}, 1.2f));
            }
          }
        }
        if (registry.valid(projEnt))
          registry.destroy(projEnt);
        continue;
      }

      uint32_t skill_id = 0;
      float knockback = 0;
      if (registry.valid(projEnt)) {
        if (auto *sc = registry.try_get<SkillComponent>(projEnt))
          skill_id = sc->skill_id;
        if (auto *p = registry.try_get<Projectile>(projEnt))
          knockback = p->snapshot.knockback;
      }

      DamagePool base;
      Tag hit_tags = Tag::Projectile | Tag::Hit;
      entt::entity attacker =
          registry.valid(projEnt) && registry.all_of<CombatStats>(projEnt)
              ? projEnt
              : act.instigator;

      DamageRequest request;
      request.attacker = attacker;
      request.defender = target;
      request.skill_id = skill_id;
      request.base_pool = base;
      request.additional_tags = hit_tags;
      request.source_entity = projEnt;
      (void)ResolveDamage(registry, request, act.instigator);
      {
        components::GPULight flash = {};
        flash.posX = act.pos.x;
        flash.posY = act.pos.y;
        flash.radius = Constants::Lighting::EXPLOSION_RADIUS;
        flash.intensity = Constants::Lighting::EXPLOSION_INTENSITY;
        flash.colorR = 1.0f;
        flash.colorG = 1.0f;
        flash.colorB = 1.0f;
        switch (skill_id) {
        case 1: // Flowing Thrust
          flash.radius = 180.0f;
          flash.intensity = 2.0f;
          flash.colorR = 1.0f;
          flash.colorG = 0.9f;
          flash.colorB = 0.75f;
          break;
        case 2: // Rending Wave
          flash.radius = 42.0f;
          flash.intensity = 0.42f;
          flash.colorR = 0.52f;
          flash.colorG = 0.75f;
          flash.colorB = 0.92f;
          break;
        case 7:
          flash.radius = 210.0f;
          flash.intensity = 2.5f;
          flash.colorR = 1.0f;
          flash.colorG = 0.75f;
          flash.colorB = 0.35f;
          break;
        case 8:
        case 9:
          flash.radius = 220.0f;
          flash.intensity = 2.8f;
          flash.colorR = 0.78f;
          flash.colorG = 0.92f;
          flash.colorB = 1.0f;
          break;
        default:
          break;
        }
        flash.colorA = 1.0f;
        render::lighting::LightManager::Get().AddTransientLight(flash);
      }

      if (skill_id == 2) {
        for (int i = 0; i < 3; ++i) {
          components::GPUParticle p;
          p.position = act.pos;
          float angle = (float)GetRandomValue(0, 360) * (PI / 180.0f);
          float speed = (float)GetRandomValue(45, 110);
          p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
          p.color = {165, 215, 255, 130};
          p.lifetime = 0.08f + (float)GetRandomValue(0, 5) / 100.0f;
          p.maxLifetime = p.lifetime;
          p.scale = 0.14f + (float)GetRandomValue(0, 10) / 100.0f;
          p.flags = 13;
          p.growthRate = -1.2f;
          particleSys.Emit(p);
        }
      }

      if (knockback > 0)
        Utils::ApplyKnockback(registry, target, act.pos, knockback);

      if (skill_id == 7) {
        // Impact: converging white sparks into rift center.
        for (int i = 0; i < 4; ++i) {
          const float a = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
          const float r = static_cast<float>(GetRandomValue(6, 14));
          const Vector2 start = {act.pos.x + cosf(a) * r, act.pos.y + sinf(a) * r};
          const Vector2 toCenter = Vector2Normalize(Vector2Subtract(act.pos, start));

          components::GPUParticle spark = {};
          spark.position = start;
          spark.velocity = Vector2Scale(toCenter, static_cast<float>(GetRandomValue(70, 120)));
          spark.acceleration = {0.0f, 0.0f};
          spark.color = Color{245, 250, 255, 210};
          spark.scale = 2.0f;
          spark.lifetime = 0.10f;
          spark.maxLifetime = 0.10f;
          spark.flags = 2;
          spark.growthRate = -8.0f;
          particleSys.Emit(spark);
        }

        // Impact: tiny pull visual toward center (~1px equivalent).
        if (registry.all_of<Velocity, Position>(target)) {
          auto &tp = registry.get<Position>(target);
          auto &tv = registry.get<Velocity>(target);
          const Vector2 pullDir =
              Vector2Normalize(Vector2Subtract(act.pos, Vector2{tp.x, tp.y}));
          tv.vx += pullDir.x * 12.0f;
          tv.vy += pullDir.y * 12.0f;
        }
      }
    } else if (act.type == DeferredAction::CounterSpin) {
      entt::entity projEnt = act.entity;
      entt::entity owner = act.target;

      if (registry.valid(owner) && registry.valid(projEnt)) {
        auto *sprite = registry.try_get<SpriteComponent>(owner);
        auto *pos = registry.try_get<Position>(projEnt);
        if (sprite && pos) {
          auto ghostEnt = registry.create();
          registry.emplace<Position>(ghostEnt, pos->x, pos->y);
          auto &ghost = registry.emplace<components::VisualGhost>(ghostEnt);
          ghost.texture = sprite->texture;
          ghost.source = {0.0f, 0.0f, (float)sprite->texture.width,
                          (float)sprite->texture.height};
          ghost.alpha = 0.6f;
          ghost.fadeSpeed = 5.0f;
          ghost.scale = sprite->scale;
          ghost.color = {180, 220, 255, 255};
        }
      }
    }
  }
}

// --- Lifecycle & Behaviors (Phase 2 & 3 & 4) ---

void ProjectileSystem::OnProjectileDeath(entt::registry &registry,
                                         entt::entity entity,
                                         const Projectile &proj,
                                         DeathReason reason) {
  if (proj.on_death == Projectile::OnDeathBehavior::None)
    return;

  if (proj.on_death == Projectile::OnDeathBehavior::Split) {
    SpawnSplitProjectiles(registry, entity, proj);
  } else if (proj.on_death == Projectile::OnDeathBehavior::Explode) {
    // Explode triggers on Collision usually, or Death?
    // Plan assumes Explode on hit/death.
    SpawnExplosionProjectiles(registry, entity, proj);
  } else if (proj.on_death == Projectile::OnDeathBehavior::Hover) {
    // Only convert on Expiry ? Or collision too?
    // Usually Hover happens at end of flight (Expiry).
    if (reason == DeathReason::Expired) {
      ConvertToHoveringHazard(registry, entity, proj);
    }
  }
}

void ProjectileSystem::SpawnSplitProjectiles(entt::registry &registry,
                                             entt::entity parent_ent,
                                             const Projectile &parent) {
  if (!registry.valid(parent_ent) || !registry.all_of<Position>(parent_ent))
    return;

  auto &pos = registry.get<Position>(parent_ent);
  auto *vel = registry.try_get<Velocity>(parent_ent);
  Vector2 baseDir = vel ? Vector2Normalize({vel->vx, vel->vy}) : Vector2{1, 0};

  float angleStep = parent.split_spread / (parent.split_count + 1);
  float startAngle = -parent.split_spread / 2.0f + angleStep;

  for (int i = 0; i < parent.split_count; ++i) {
    float angle = startAngle + i * angleStep;
    Vector2 dir = Vector2Rotate(baseDir, angle);
    float childSpeed = parent.speed * parent.split_speed_mult;

    auto child = registry.create();
    registry.emplace<Position>(child, pos);
    registry.emplace<Velocity>(child, dir.x * childSpeed,
                               dir.y * childSpeed);

    // Clone Projectile
    auto &p = registry.emplace<Projectile>(child, parent);
    p.on_death =
        Projectile::OnDeathBehavior::None; // Prevent infinite recursion
    p.pierce = false;                      // Reset pierce
    p.speed = childSpeed;
    p.radius = parent.radius * parent.split_radius_mult;
    for (auto &m : p.snapshot.damage_multipliers)
      m *= parent.split_damage_mult;
    // Reset lifetime - Reduced from 3.0f to 0.6f to prevent visual clutter
    p.lifeTime = 0.6f;
    p.hitLimitReached = false;
    p.hasRendered = false;
    p.hitEntities.clear();

    // Visuals
    if (auto *col = registry.try_get<ColorComponent>(parent_ent)) {
      registry.emplace<ColorComponent>(child, *col);
    }

    // Tags
    if (registry.any_of<SkillComponent>(parent_ent)) {
      registry.emplace<SkillComponent>(
          child, registry.get<SkillComponent>(parent_ent));
    }
    registry.emplace<LocalLevelTag>(child);
  }
}

void ProjectileSystem::SpawnExplosionProjectiles(entt::registry &registry,
                                                 entt::entity parent_ent,
                                                 const Projectile &parent) {
  if (!registry.valid(parent_ent) || !registry.all_of<Position>(parent_ent))
    return;

  auto &pos = registry.get<Position>(parent_ent);

  float angleStep = 2.0f * PI / parent.explode_count;

  for (int i = 0; i < parent.explode_count; ++i) {
    float angle = i * angleStep;
    Vector2 dir = {cosf(angle), sinf(angle)};

    auto child = registry.create();
    registry.emplace<Position>(child, pos);
    registry.emplace<Velocity>(child, dir.x * parent.speed * 0.8f,
                               dir.y * parent.speed * 0.8f);

    auto &p = registry.emplace<Projectile>(child, parent);
    p.on_death = Projectile::OnDeathBehavior::None;
    for (auto &m : p.snapshot.damage_multipliers)
      m *= parent.explode_damage_mult;
    // Reduced from 2.0f to 0.4f
    p.lifeTime = 0.4f;
    p.hitLimitReached = false;
    p.hasRendered = false;
    p.hitEntities.clear();
    p.pierce = false;

    if (auto *col = registry.try_get<ColorComponent>(parent_ent)) {
      registry.emplace<ColorComponent>(child, *col);
    }
    if (registry.any_of<SkillComponent>(parent_ent)) {
      registry.emplace<SkillComponent>(
          child, registry.get<SkillComponent>(parent_ent));
    }
    registry.emplace<LocalLevelTag>(child);
  }
}

void ProjectileSystem::ConvertToHoveringHazard(entt::registry &registry,
                                               entt::entity proj_ent,
                                               const Projectile &proj) {
  // Cache values before removing component
  float dur = proj.hover_duration;
  float tick = proj.hover_tick_rate;
  float dmgMult = proj.hover_damage_mult;
  float rad = proj.radius * 1.5f;
  entt::entity owner = proj.owner;

  registry.remove<Projectile>(proj_ent); // proj reference is now invalid!

  auto &hazard = registry.emplace<HazardComponent>(proj_ent);
  hazard.duration = dur;
  hazard.tickInterval = tick;
  hazard.currentTickTimer = 0.0f;
  hazard.radius = rad;
  hazard.damagePerTick = 20.0f * dmgMult;
  hazard.owner = owner;
  hazard.hitsEnemies = true;
  hazard.hitsPlayers = false;
  hazard.damageType = DamageType::Physical;

  // Check if Visual Component already exists, if not add it
  if (!registry.any_of<HazardVisualComponent>(proj_ent)) {
    auto &vis = registry.emplace<HazardVisualComponent>(proj_ent);
    vis.tintColor = Color{100, 200, 255, 200};
    vis.particlesPerEmit = 2;
    vis.particleEmitInterval = 0.1f;
  }

  // Stop movement
  if (auto *vel = registry.try_get<Velocity>(proj_ent)) {
    vel->vx = 0;
    vel->vy = 0;
  }
}

} // namespace NoMoreDay
