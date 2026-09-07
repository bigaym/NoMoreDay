#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "raymath.h"
#include <algorithm>
#include <vector>

namespace NoMoreDay {

void BeamChannelDeliverySystem::Update(entt::registry &registry,
                                       systems::SpatialHashGrid &grid,
                                       float dt) {
  // 1. 纯参数驱动的现代 BeamChannelComponent (Task 2.4b, 2.4d)
  static thread_local std::vector<entt::entity> s_beam_to_remove;
  s_beam_to_remove.clear();

  auto beam_view = registry.view<BeamChannelComponent, Position>();
  for (auto entity : beam_view) {
    auto &beam = beam_view.get<BeamChannelComponent>(entity);
    const auto &pos = beam_view.get<Position>(entity);

    beam.current_channel_time += dt;
    if (beam.current_channel_time >= beam.max_channel_time) {
      if (beam.finisher_trigger_skill_id != 0) {
        auto finisher_ent = registry.create();
        registry.emplace<LocalLevelTag>(finisher_ent);
        registry.emplace<ShadowCastTag>(finisher_ent);
        registry.emplace<Position>(finisher_ent, pos.x, pos.y);

        auto &exec = registry.emplace<SkillExecution>(finisher_ent);
        exec.skill_id = beam.finisher_trigger_skill_id;
        exec.owner = entity;
        exec.state = SkillState::Preparing;
        exec.timer = 0.0f;
        exec.target_pos = beam.target_pos;
        exec.is_empowered = true;
      }
      s_beam_to_remove.push_back(entity);
      continue;
    }

    beam.tick_timer -= dt;
    if (beam.tick_timer <= 0.0f) {
      beam.tick_timer = std::max(0.05f, beam.tick_interval);

      Vector2 targetPos = beam.target_pos;
      if (beam.aim_assist) {
        float bestDistSq = 900.0f * 900.0f;
        entt::entity bestTarget = entt::null;
        grid.query({targetPos.x, targetPos.y}, 900.0f,
                   [&](entt::entity e, const Position &ep) {
                     if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
                       float distSq = Vector2DistanceSqr({targetPos.x, targetPos.y}, {ep.x, ep.y});
                       if (distSq < bestDistSq) {
                         bestDistSq = distSq;
                         bestTarget = e;
                       }
                     }
                   });
        if (registry.valid(bestTarget) && registry.all_of<Position>(bestTarget)) {
          const auto &tp = registry.get<Position>(bestTarget);
          targetPos = {tp.x, tp.y};
        }
      }

      if (beam.mode == BeamChannelMode::BarrageEmitter) {
        Vector2 dirToTarget = Vector2Normalize(Vector2Subtract(targetPos, {pos.x, pos.y}));
        int count = beam.is_empowered ? 4 : 2;
        auto *stats = registry.try_get<CombatStats>(entity);
        for (int i = 0; i < count; ++i) {
          float spreadAmt = static_cast<float>(GetRandomValue(-20, 20)) * DEG2RAD;
          Vector2 fireDir = Vector2Rotate(dirToTarget, spreadAmt);

          auto proj_ent = registry.create();
          registry.emplace<LocalLevelTag>(proj_ent);
          registry.emplace<Position>(proj_ent, pos.x + fireDir.x * 20.0f, pos.y + fireDir.y * 20.0f);
          registry.emplace<Velocity>(proj_ent, fireDir.x * 1000.0f, fireDir.y * 1000.0f);

          auto &proj = registry.emplace<Projectile>(proj_ent);
          proj.owner = entity;
          proj.cast_id = beam.cast_id;
          proj.lifeTime = 1.0f;
          proj.radius = 35.0f;
          proj.speed = 1000.0f;
          proj.pierce = true;
          proj.max_pierce = 1;
          proj.visualType = 2;

          if (stats) {
            DamagePayloadContext ctx{};
            ctx.base_damage_min = stats->min_weapon_damage;
            ctx.base_damage_max = stats->max_weapon_damage;
            ctx.crit_chance = stats->crit_chance / 100.0f; // payload crit_chance 须归一化 [0,1]
            ctx.crit_multiplier = stats->crit_damage;
            ctx.increased_damage = 0.0f;
            ctx.more_damage = 0.35f * beam.bonus_damage_mult;
            ctx.effective_tags = Tag::Physical;
            ctx.source_skill_id = beam.skill_id ? beam.skill_id : 5;
            proj.payload_context = ctx;

            proj.snapshot = *stats;
            for (auto &mult : proj.snapshot.damage_multipliers) {
              mult *= (0.35f * beam.bonus_damage_mult);
            }
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
          }
          registry.emplace<SkillComponent>(proj_ent, beam.skill_id ? beam.skill_id : 5, entity);

          auto &particleSys = systems::GPUParticleSystem::Get();
          components::GPUParticle p;
          p.position = {pos.x + fireDir.x * 30.0f, pos.y + fireDir.y * 30.0f};
          p.velocity = Vector2Scale(fireDir, 200.0f);
          p.color = beam.is_empowered ? GOLD : ColorAlpha(WHITE, 0.6f);
          p.lifetime = 0.2f;
          p.maxLifetime = 0.2f;
          p.scale = 2.0f;
          p.flags = 2;
          particleSys.Emit(p);
        }
      } else if (beam.mode == BeamChannelMode::ContinuousLaser) {
        auto *stats = registry.try_get<CombatStats>(entity);
        grid.query({targetPos.x, targetPos.y}, 60.0f, [&](entt::entity e, const Position &ep) {
          if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
            DamageRequest req{};
            req.attacker = entity;
            req.defender = e;
            req.skill_id = beam.skill_id ? beam.skill_id : 7;
            req.source_entity = entity;
            req.added_effectiveness = beam.bonus_damage_mult;
            if (stats) {
              req.base_pool.Add(Tag::Physical, (stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f);
            } else {
              req.base_pool.Add(Tag::Physical, 30.0f);
            }
            (void)ResolveDamage(registry, req, e);
          }
        });
      }
    }
  }

  for (auto e : s_beam_to_remove) {
    if (registry.valid(e)) {
      registry.remove<BeamChannelComponent>(e);
    }
  }

  // 2. 兼容并存旧版 ChannelingComponent (平滑回滚窗口)
  auto chan_view = registry.view<ChannelingComponent, Position>();
  for (auto entity : chan_view) {
    if (registry.any_of<BeamChannelComponent>(entity)) {
      continue;
    }
    auto &chan = chan_view.get<ChannelingComponent>(entity);
    const auto &pos = chan_view.get<Position>(entity);

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
        auto finisher_ent = registry.create();
        registry.emplace<LocalLevelTag>(finisher_ent);
        registry.emplace<ShadowCastTag>(finisher_ent);
        registry.emplace<Position>(finisher_ent, pos.x, pos.y);

        auto &exec = registry.emplace<SkillExecution>(finisher_ent);
        exec.skill_id = 2;
        exec.owner = entity;
        exec.state = SkillState::Preparing;
        exec.timer = 0.0f;
        exec.target_pos = chan.target_pos;
        exec.is_empowered = true;

        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          exec.has_snapshot = true;
          exec.snapshot.stats = *stats;
          exec.snapshot.skill_id = 5;
          for (auto &mult : exec.snapshot.stats.damage_multipliers) {
            mult *= 5.0f;
          }
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
        p.flags = 2;
        particleSys.Emit(p);
      }
    }

    // Skill 7 continuous channel visuals
    if (chan.skill_id == 7) {
      Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
      float dist = Vector2Length(diff);
      float max_range = 350.0f;
      Vector2 cutPos = chan.target_pos;
      Vector2 dir = {1.0f, 0.0f};
      if (dist > 0.001f) {
        dir = Vector2Scale(diff, 1.0f / dist);
        if (dist > max_range) {
          cutPos = {pos.x + dir.x * max_range, pos.y + dir.y * max_range};
        }
      } else {
        cutPos = {pos.x + 50.0f, pos.y};
      }

      if (chan.synergy_lock) {
        float bestDistSq = 450.0f * 450.0f;
        entt::entity bestTarget = entt::null;
        grid.query({pos.x, pos.y}, 450.0f,
                   [&](entt::entity e, const Position &ep) {
                     if (!registry.any_of<EnemyTag>(e) ||
                         registry.any_of<KilledTag>(e)) {
                       return;
                     }
                     float distSq = Vector2DistanceSqr({pos.x, pos.y}, {ep.x, ep.y});
                     if (distSq < bestDistSq) {
                       bestDistSq = distSq;
                       bestTarget = e;
                     }
                   });
        if (registry.valid(bestTarget) && registry.all_of<Position>(bestTarget)) {
          const auto &tp = registry.get<Position>(bestTarget);
          cutPos = {tp.x, tp.y};
        }
      }

      Tag effectiveTags = SkillSystem::GetEffectiveSkillTags(registry, entity, 7u);
      if (chan.conversion_tag != Tag::None) {
        effectiveTags = (effectiveTags & ~Tag::Physical) | chan.conversion_tag;
      }
      const uint8_t elementType =
          SkillSystem::EncodeSkillVfxElementType(effectiveTags);
      const bool hasVoidRift = (chan.conversion_tag == Tag::Void);
      const bool isCold = HasTag(effectiveTags, Tag::Cold);
      const bool isLightning = HasTag(effectiveTags, Tag::Lightning);
      const bool isEmpowered = chan.is_empowered;

      {
        components::GPUSkillEffect riftRing = {};
        riftRing.position = cutPos;
        riftRing.velocity = Vector2Scale(dir, 40.0f);
        riftRing.coreColor =
            isEmpowered ? Vector4{0.22f, 0.30f, 0.48f, 0.98f}
                        : (hasVoidRift ? Vector4{0.08f, 0.07f, 0.14f, 0.95f}
                                       : Vector4{0.18f, 0.24f, 0.38f, 0.90f});
        riftRing.glowColor =
            isLightning ? Vector4{0.72f, 0.56f, 1.00f, 0.90f}
                        : (isCold ? Vector4{0.76f, 0.92f, 1.00f, 0.88f}
                                  : Vector4{0.46f, 0.74f, 1.00f, 0.84f});
        riftRing.radius = 24.0f;
        riftRing.sectorAngle = 360.0f;
        riftRing.type = isEmpowered   ? 7.0f
                        : hasVoidRift ? 3.0f
                        : isLightning ? 6.0f
                        : isCold      ? 5.0f
                                      : 4.0f;
        riftRing.flags =
            NoMoreDay::render::skillfx::PackSkillEffectFlags(elementType, 7u);
        systems::GPUSkillEffectSystem::Get().Submit(riftRing);
      }
      const float distortionRadius = isEmpowered ? 34.0f : (hasVoidRift ? 32.0f : 28.0f);
      const float distortionStrength =
          isEmpowered ? 0.30f : (hasVoidRift ? 0.26f : 0.22f);
      RenderSystem::AddDistortionSource(cutPos.x, cutPos.y, distortionRadius,
                                        distortionStrength);

      auto &particleSys = systems::GPUParticleSystem::Get();
      if ((float)GetRandomValue(0, 1000) < 700.0f * dt) {
        constexpr int kSamples = 6;
        for (int i = 1; i <= kSamples; ++i) {
          const float t = static_cast<float>(i) / static_cast<float>(kSamples + 1);
          const Vector2 samplePos = Vector2Lerp({pos.x, pos.y}, cutPos, t);
          components::GPUParticle link = {};
          link.position = samplePos;
          link.velocity = {0.0f, 0.0f};
          link.acceleration = {0.0f, 0.0f};
          link.color = isEmpowered ? Color{236, 246, 255, 96}
                                   : Color{185, 225, 240, 48};
          link.scale = isEmpowered ? 3.2f : 2.6f;
          link.lifetime = 0.13f;
          link.maxLifetime = 0.13f;
          link.flags = 1;
          link.growthRate = -4.0f;
          particleSys.Emit(link);
        }
      }

      if (isLightning && (float)GetRandomValue(0, 1000) < 800.0f * dt) {
        for (int i = 0; i < 2; ++i) {
          components::GPUParticle arc = {};
          arc.position = {cutPos.x + (float)GetRandomValue(-18, 18),
                          cutPos.y + (float)GetRandomValue(-18, 18)};
          arc.velocity = {(float)GetRandomValue(-40, 40),
                          (float)GetRandomValue(-40, 40)};
          arc.acceleration = {0.0f, 0.0f};
          arc.color = Color{220, 188, 255, 215};
          arc.scale = 4.4f;
          arc.lifetime = 0.18f;
          arc.maxLifetime = 0.18f;
          arc.flags = 2;
          arc.growthRate = -9.0f;
          particleSys.Emit(arc);
        }
      }
    }

    if (chan.tick_timer <= 0.0f) {
      if (chan.skill_id == 5) {
        // Infinite Blades (Wan Jian Gui Zong)
        int projectileCount = 2;
        if (chan.extra_projectiles) {
          projectileCount += 2;
        }

        Vector2 targetPos = chan.target_pos;
        if (chan.full_screen_lock) {
          float bestDistSq = 900.0f * 900.0f;
          entt::entity bestTarget = entt::null;
          grid.query({targetPos.x, targetPos.y}, 900.0f,
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

        Vector2 dirToTarget =
            Vector2Normalize(Vector2Subtract(targetPos, {pos.x, pos.y}));

        for (int i = 0; i < projectileCount; ++i) {
          float spreadAmt = (float)GetRandomValue(-20, 20) * DEG2RAD;
          Vector2 fireDir = Vector2Rotate(dirToTarget, spreadAmt);

          auto proj_ent = registry.create();
          registry.emplace<LocalLevelTag>(proj_ent);
          registry.emplace<ShadowCastTag>(proj_ent);
          registry.emplace<Position>(proj_ent, pos.x + fireDir.x * 20.0f,
                                     pos.y + fireDir.y * 20.0f);

          float speed = 1000.0f;
          registry.emplace<Velocity>(proj_ent, fireDir.x * speed,
                                     fireDir.y * speed);

          Color swordColor = chan.is_empowered
                                 ? GOLD
                                 : ColorAlpha(Color{0, 170, 255, 255}, 0.5f);
          if (chan.conversion_tag == Tag::Fire) {
            swordColor = ORANGE;
          } else if (chan.conversion_tag == Tag::Cold) {
            swordColor = SKYBLUE;
          } else if (chan.conversion_tag == Tag::Lightning) {
            swordColor = PURPLE;
          } else if (chan.conversion_tag == Tag::Void) {
            swordColor = Color{120, 90, 180, 255};
          }
          registry.emplace<ColorComponent>(proj_ent, swordColor);

          auto &proj = registry.emplace<Projectile>(proj_ent);
          proj.owner = entity;
          proj.cast_id = chan.cast_id;
          proj.radius = 35.0f;
          proj.speed = speed;
          proj.lifeTime = 1.2f;
          proj.visualType = 2;

          if (auto *stats = registry.try_get<CombatStats>(entity)) {
            DamagePayloadContext ctx{};
            ctx.base_damage_min = stats->min_weapon_damage;
            ctx.base_damage_max = stats->max_weapon_damage;
            ctx.crit_chance = (stats->crit_chance + chan.bonus_crit_chance) / 100.0f; // payload crit_chance 归一化（统一单位）
            ctx.crit_multiplier = stats->crit_damage;
            ctx.increased_damage = 0.0f;
            ctx.more_damage = 0.35f * chan.bonus_damage_mult;
            ctx.effective_tags = Tag::Physical | (chan.conversion_tag != Tag::None ? chan.conversion_tag : Tag::None);
            ctx.source_skill_id = 5;
            proj.payload_context = ctx;

            proj.snapshot = *stats;
            for (auto &mult : proj.snapshot.damage_multipliers) {
              mult *= (0.35f * chan.bonus_damage_mult);
            }
            proj.snapshot.crit_chance += chan.bonus_crit_chance;
            proj.snapshot.armor_pen += chan.bonus_armor_pen;
          }

          auto &sc = registry.emplace<SkillComponent>(proj_ent);
          sc.skill_id = 5;
          if (chan.conversion_tag != Tag::None) {
            auto &mods = registry.emplace<SkillModifierComponent>(proj_ent);
            mods.damage_modifiers.push_back(
                DamageModifier{Tag::Physical, chan.conversion_tag, 1.0f,
                               ModifierType::Convert});
          }

          auto &particleSys = systems::GPUParticleSystem::Get();
          components::GPUParticle p;
          p.position = {pos.x + fireDir.x * 30.0f, pos.y + fireDir.y * 30.0f};
          p.velocity = Vector2Scale(fireDir, 200.0f);
          p.color = chan.is_empowered ? GOLD : ColorAlpha(WHITE, 0.6f);
          p.lifetime = 0.2f;
          p.maxLifetime = 0.2f;
          p.scale = 1.8f;
          p.flags = 2;
          particleSys.Emit(p);
        }

        chan.tick_timer = std::max(0.08f, chan.tick_interval);

      } else if (chan.skill_id == 7) {
        Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
        float dist = Vector2Length(diff);
        float max_range = 350.0f;
        Vector2 cutPos = chan.target_pos;
        Vector2 dir = {1.0f, 0.0f};

        if (dist > 0.001f) {
          dir = Vector2Scale(diff, 1.0f / dist);
          if (dist > max_range) {
            cutPos = {pos.x + dir.x * max_range, pos.y + dir.y * max_range};
          }
        } else {
          cutPos = {pos.x + 50.0f, pos.y};
        }

        const Tag effectiveTags = SkillSystem::GetEffectiveSkillTags(registry, entity, 7u);
        const uint8_t elementType =
            SkillSystem::EncodeSkillVfxElementType(effectiveTags);
        const bool isCold = HasTag(effectiveTags, Tag::Cold);
        const bool isLightning = HasTag(effectiveTags, Tag::Lightning);
        const bool isEmpowered = chan.is_empowered;

        auto &particleSys = systems::GPUParticleSystem::Get();
        const int slashCount =
            (isEmpowered ? GetRandomValue(4, 8) : GetRandomValue(2, 4)) +
            ((chan.bonus_damage_mult > 1.2f) ? 1 : 0);
        for (int s = 0; s < slashCount; ++s) {
          const float slashAngle =
              static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
          const Vector2 slashDir = {cosf(slashAngle), sinf(slashAngle)};
          const float halfLen =
              isEmpowered ? static_cast<float>(GetRandomValue(20, 34))
                          : static_cast<float>(GetRandomValue(16, 26));
          constexpr int kSegments = 12;
          for (int i = 0; i < kSegments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSegments - 1);
            const float offset = (t - 0.5f) * (halfLen * 2.0f);
            const Vector2 pPos = {cutPos.x + slashDir.x * offset,
                                  cutPos.y + slashDir.y * offset};

            components::GPUParticle cut = {};
            cut.position = pPos;
            cut.velocity = Vector2Scale(slashDir, static_cast<float>(GetRandomValue(20, 60)));
            cut.acceleration = {0.0f, 0.0f};
            cut.color = isLightning ? Color{214, 188, 255, 220}
                        : (isCold ? Color{210, 245, 255, 218}
                                  : Color{200, 242, 255, 210});
            cut.scale = isEmpowered ? 5.0f : 4.0f;
            cut.lifetime = 0.20f;
            cut.maxLifetime = 0.20f;
            cut.flags = 2;
            cut.growthRate = -10.5f;
            particleSys.Emit(cut);
          }

          components::GPUSkillEffect slashBody = {};
          slashBody.position = cutPos;
          slashBody.velocity = Vector2Scale(slashDir, 900.0f);
          slashBody.coreColor = isLightning ? Vector4{0.66f, 0.58f, 1.00f, 0.96f}
                               : (isCold ? Vector4{0.72f, 0.90f, 1.00f, 0.96f}
                                         : Vector4{0.42f, 0.82f, 1.00f, 0.95f});
          slashBody.glowColor = isLightning ? Vector4{0.90f, 0.84f, 1.00f, 0.90f}
                               : (isCold ? Vector4{0.88f, 0.96f, 1.00f, 0.90f}
                                         : Vector4{0.24f, 0.56f, 0.96f, 0.88f});
          slashBody.radius = halfLen * 1.35f;
          slashBody.sectorAngle = 0.0f;
          slashBody.type = 2.0f;
          slashBody.flags =
              NoMoreDay::render::skillfx::PackSkillEffectFlags(elementType, 7u);
          systems::GPUSkillEffectSystem::Get().Submit(slashBody);
        }

        const int shardCount = GetRandomValue(2, 4);
        for (int i = 0; i < shardCount; ++i) {
          const float a = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
          const float r = static_cast<float>(GetRandomValue(12, 20));
          const Vector2 spawn = {cutPos.x + cosf(a) * r, cutPos.y + sinf(a) * r};

          components::GPUParticle shard = {};
          shard.position = spawn;
          shard.velocity = {cosf(a) * static_cast<float>(GetRandomValue(8, 20)),
                            sinf(a) * static_cast<float>(GetRandomValue(8, 20))};
          shard.acceleration = {0.0f, 0.0f};
          shard.color = isCold ? Color{62, 72, 90, 190}
                      : (isLightning ? Color{74, 54, 110, 190}
                                     : Color{36, 40, 52, 180});
          shard.scale = 4.8f;
          shard.lifetime = 0.62f;
          shard.maxLifetime = 0.62f;
          shard.flags = 2;
          shard.growthRate = -1.5f;
          particleSys.Emit(shard);
        }

        auto exec_ent = registry.create();
        registry.emplace<LocalLevelTag>(exec_ent);
        registry.emplace<Position>(exec_ent, cutPos.x, cutPos.y);
        registry.emplace<Velocity>(exec_ent, 0.0f, 0.0f);

        auto &proj = registry.emplace<Projectile>(exec_ent);
        proj.owner = entity;
        proj.cast_id = chan.cast_id;
        proj.radius = 60.0f * std::clamp(chan.bonus_damage_mult, 1.0f, 1.35f);
        proj.speed = 0.0f;
        proj.lifeTime = 0.1f;
        proj.pierce = true;
        proj.pierceCount = 999;
        proj.max_pierce = Projectile::kUnlimitedPiercing;

        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          DamagePayloadContext ctx{};
          ctx.base_damage_min = stats->min_weapon_damage;
          ctx.base_damage_max = stats->max_weapon_damage;
          ctx.crit_chance = (stats->crit_chance + chan.bonus_crit_chance) / 100.0f; // payload crit_chance 归一化（统一单位）
          ctx.crit_multiplier = stats->crit_damage;
          ctx.increased_damage = 0.0f;
          ctx.more_damage = chan.bonus_damage_mult;
          ctx.effective_tags = Tag::Physical | (chan.conversion_tag != Tag::None ? chan.conversion_tag : Tag::None);
          ctx.source_skill_id = 7;
          proj.payload_context = ctx;

          proj.snapshot = *stats;
          for (auto &mult : proj.snapshot.damage_multipliers) {
            mult *= chan.bonus_damage_mult;
          }
          proj.snapshot.crit_chance += chan.bonus_crit_chance;
          proj.snapshot.armor_pen += chan.bonus_armor_pen;
        }

        auto &sc = registry.emplace<SkillComponent>(exec_ent);
        sc.skill_id = 7;
        if (chan.conversion_tag != Tag::None) {
          auto &mods = registry.emplace<SkillModifierComponent>(exec_ent);
          mods.damage_modifiers.push_back(
              DamageModifier{Tag::Physical, chan.conversion_tag, 1.0f,
                             ModifierType::Convert});
        }

        chan.tick_timer = chan.tick_interval;
      }
    }
  }
}

} // namespace NoMoreDay
