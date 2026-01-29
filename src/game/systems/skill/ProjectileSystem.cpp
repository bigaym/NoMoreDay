#include "game/systems/skill/ProjectileSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/PhysicsUtils.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp" // Phase 4 Integration
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/HazardComponents.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/components/vfx/VisualGhostComponent.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
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
  s_enemyGrid.rebuild(registry.view<EnemyTag, Position>(), registry);

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
                     [&](entt::entity t, const Position &tp) {
                       if (t == entity || t == proj.owner)
                         return;
                       if (!registry.valid(t))
                         return;

                       bool ownerIsPlayer =
                           registry.any_of<PlayerTag>(proj.owner);
                       bool tIsPlayer = registry.any_of<PlayerTag>(t);
                       bool tIsEnemy = registry.any_of<EnemyTag>(t);

                       if (ownerIsPlayer && !tIsEnemy)
                         return;
                       if (!ownerIsPlayer && !tIsPlayer)
                         return;
                       if (registry.any_of<KilledTag>(t))
                         return;

                       float dx = tp.x - pos.x;
                       float dy = tp.y - pos.y;
                       float dist = std::sqrt(dx * dx + dy * dy);
                       if (dist < minDist) {
                         minDist = dist;
                         bestTarget = t;
                       }
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
                 [&](entt::entity target, const Position &tPos) {
                   if (target == proj.owner || target == entity)
                     return;
                   if (!registry.valid(target) ||
                       !registry.all_of<Velocity, Position>(target))
                     return;
                   if (!registry.any_of<EnemyTag>(target))
                     return;

                   DeferredAction act;
                   act.type = DeferredAction::Pull;
                   act.entity = target;
                   act.value = proj.pullStrength;
                   act.instigator = entity;
                   act.pos = {pos.x, pos.y};
                   actions.push_back(act);
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
          if (auto* col = registry.try_get<ColorComponent>(entity)) {
             trailColor = col->color;
          }
          trailColor.a = 150;
          
          auto p = systems::InkEffectHelper::CreateInkTrail(
              {pos.x, pos.y}, trailVel, 0.6f, 0.075f); // 0.15 -> 0.075
          p.color = trailColor;
          p.scale = 1.2f;
          s_particles.push_back(p);
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
        [&](entt::entity target, const Position &tPos) {
          if (hit && !proj.pierce)
            return;
          if (proj.pierce && proj.pierceCount < 0)
            return;
          if (target == proj.owner || target == entity)
            return;

          if (!registry.valid(target))
            return;

          bool ownerIsPlayer = registry.any_of<PlayerTag>(proj.owner);
          bool targetIsEnemy = registry.any_of<EnemyTag>(target);
          bool ownerIsEnemy = registry.any_of<EnemyTag>(proj.owner);
          bool targetIsPlayer = registry.any_of<PlayerTag>(target);

          if (ownerIsPlayer && !targetIsEnemy)
            return;
          if (ownerIsEnemy && !targetIsPlayer)
            return;

          for (auto e : s_uniqueHits)
            if (e == target)
              return;
          s_uniqueHits.push_back(target);

          for (auto e : proj.hitEntities)
            if (e == target)
              return;

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
                return;
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
            } else {
              proj.pierceCount--;
              if (proj.pierceCount < 0) {
                hit = true;
                proj.hitLimitReached = true;
              }
            }
          }
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

      auto result = DamagePipeline::Calculate(
          registry, attacker, target, skill_id, base, hit_tags, projEnt);
      float finalDamage = result.total_damage > 0 ? result.total_damage : 1.0f;

      CombatSystem::ApplyDamage(registry, target, finalDamage, act.instigator,
                                result.is_crit);

      if (skill_id == 2) {
        for (int i = 0; i < 12; ++i) {
          components::GPUParticle p;
          p.position = act.pos;
          float angle = (float)GetRandomValue(0, 360) * (PI / 180.0f);
          float speed = (float)GetRandomValue(100, 300);
          p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
          p.color = {200, 250, 255, 200};
          p.lifetime = 0.3f + (float)GetRandomValue(0, 20) / 100.0f;
          p.maxLifetime = p.lifetime;
          p.scale = 2.0f + (float)GetRandomValue(0, 20) / 10.0f;
          p.flags = 2;
          p.growthRate = -5.0f;
          particleSys.Emit(p);
        }
      }

      if (knockback > 0)
        Utils::ApplyKnockback(registry, target, act.pos, knockback);
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

    auto child = registry.create();
    registry.emplace<Position>(child, pos);
    registry.emplace<Velocity>(child, dir.x * parent.speed,
                               dir.y * parent.speed);

    // Clone Projectile
    auto &p = registry.emplace<Projectile>(child, parent);
    p.on_death =
        Projectile::OnDeathBehavior::None; // Prevent infinite recursion
    p.pierce = false;                      // Reset pierce
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