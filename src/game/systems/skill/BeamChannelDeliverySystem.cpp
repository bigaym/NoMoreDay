#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
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

namespace {
// 查询实体在技能5专精树上某节点的已分配点数；未分配返回 0
int GetSkill5Point(entt::registry &registry, entt::entity entity, uint32_t node_id) {
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == 5u) {
        auto it = spec.allocated_points.find(node_id);
        if (it != spec.allocated_points.end()) {
          return it->second;
        }
        break;
      }
    }
  }
  return 0;
}
} // namespace

void BeamChannelDeliverySystem::Update(entt::registry &registry,
                                       systems::SpatialHashGrid &grid,
                                       float dt) {
  // 1. 纯参数驱动的现代 BeamChannelComponent (Task 2.4b, 2.4d)
  static thread_local std::vector<entt::entity> s_beam_to_remove;
  s_beam_to_remove.clear();

  auto beam_view = registry.view<BeamChannelComponent, Position>();
  auto triggerSwordGodFinisher = [&](entt::entity ent, const BeamChannelComponent &b) {
    // 534 天剑降世与 535 余波机制数值统一走 skill_mechanics.json（技能5/534、技能5/535）
    const float minChannelDuration =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "min_channel_duration", 2.0f);
    const float giantSwordRadius =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "giant_sword_radius", 120.0f);
    const float giantSwordDamageMult =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "giant_sword_damage_mult", 8.0f);
    const float knockupForce =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 534, "knockup_force", 300.0f);
    const float shockwaveRadiusPerPoint =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 535, "radius_pct_per_point", 0.20f);
    const float shockwaveStunDuration =
        data::SkillMechanicsRegistry::Get().GetFloat(5u, 535, "stun_duration", 1.5f);

    if (b.skill_id != 5 || b.current_channel_time < minChannelDuration) return;
    const auto *prof = SkillSystem::GetBakedSkillProfile(registry, ent, 5u);
    if (!prof || (prof->delivery.feature_flags & 8192) == 0) return; // 534 天剑降世

    int pts_535 = GetSkill5Point(registry, ent, 535);
    float explosion_radius = giantSwordRadius * (1.0f + shockwaveRadiusPerPoint * static_cast<float>(pts_535));
    auto *stats = registry.try_get<CombatStats>(ent);
    float baseDmg = stats ? ((stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f) : 50.0f;

    grid.query({b.target_pos.x, b.target_pos.y}, explosion_radius, [&](entt::entity e, const Position &ep) {
      if (registry.any_of<EnemyTag>(e) && !registry.any_of<KilledTag>(e)) {
        DamagePool pool;
        pool.Add(Tag::Physical, baseDmg * giantSwordDamageMult); // 800% 物理伤害
        DamageRequest req;
        req.attacker = ent;
        req.defender = e;
        req.skill_id = 5u;
        req.base_pool = pool;
        req.additional_tags = Tag::Physical | Tag::Area;
        (void)ResolveDamage(registry, req, ent);

        // 击飞 (击飞力度)
        if (auto *vel = registry.try_get<Velocity>(e)) {
          Vector2 kb = Vector2Normalize(Vector2Subtract({ep.x, ep.y}, {b.target_pos.x, b.target_pos.y}));
          vel->vx += kb.x * knockupForce;
          vel->vy += kb.y * knockupForce;
        }

        // 535 余波: 必定击晕普通怪
        if (pts_535 > 0) {
          auto &fx = registry.get_or_emplace<ActiveEffectsComponent>(e);
          BuffEffect stun{
            .id = "ShockwaveStun",
            .name = "Stun",
            .type = BuffType::Stun,
            .duration = shockwaveStunDuration,
            .remaining = shockwaveStunDuration,
            .stacks = 1,
            .max_stacks = 1,
            .is_debuff = true
          };
          fx.AddOrRefresh(stun);
        }
      }
    });
  };

  for (auto entity : beam_view) {
    auto &beam = beam_view.get<BeamChannelComponent>(entity);
    const auto &pos = beam_view.get<Position>(entity);

    beam.current_channel_time += dt;
    if (beam.current_channel_time >= beam.max_channel_time) {
      triggerSwordGodFinisher(entity, beam);
      s_beam_to_remove.push_back(entity);
      continue;
    }

    beam.tick_timer -= dt;
    if (beam.tick_timer <= 0.0f) {
      if (beam.skill_id == 5) {
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);

        // 501 剑意共鸣: 随着引导时间增加，发射频率逐步提升 (2秒达最大)
        if (profile && (profile->delivery.feature_flags & 1) != 0) {
          float progress = std::min(1.0f, beam.current_channel_time / 2.0f);
          int pts_501 = GetSkill5Point(registry, entity, 501);
          float freq_boost = progress * (0.15f * static_cast<float>(pts_501));
          // 幂等公式: 每次从 Baker 组合基准 (sub_interval，含 570/572 频率因子)
          // 重新计算，避免相对式除法随 tick 复利缩小 interval 而突破频率上限
          const float base_interval = (profile->delivery.sub_interval > 0.0f)
                                          ? profile->delivery.sub_interval
                                          : 0.3f;
          beam.tick_interval = base_interval / (1.0f + freq_boost);
        }

        // 531 不坏剑身: 引导期间获得 15..45 护甲/持续秒数 (最高叠加 10 层)，停止引导后 2s 清空
        if (profile && (profile->delivery.feature_flags & 1024) != 0) {
          int pts_531 = GetSkill5Point(registry, entity, 531);
          if (pts_531 > 0) {
            // 每层护甲 = armor_per_sec_per_point(15.0) × 投入点数；刷新时按当前层数重写 modifiers
            const float armorPerStack =
                data::SkillMechanicsRegistry::Get().GetFloat(5u, 531, "armor_per_sec_per_point", 15.0f) *
                static_cast<float>(pts_531);
            auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(entity);
            BuffEffect *existing = effects.Get("SteeledBodyArmor");
            if (existing) {
              existing->duration = 2.0f;
              existing->remaining = 2.0f;
              if (existing->stacks < 10) {
                existing->stacks++;
              }
              existing->max_stacks = 10;
              existing->modifiers.clear();
              existing->modifiers.push_back({
                  .value = armorPerStack * static_cast<float>(existing->stacks),
                  .type = StatType::Armor,
                  .mode = ModifierMode::Flat});
            } else {
              BuffEffect armorBuff{
                  .id = "SteeledBodyArmor",
                  .name = "Steeled Body",
                  .type = BuffType::DefenseUp,
                  .duration = 2.0f,
                  .remaining = 2.0f,
                  .stacks = 1,
                  .max_stacks = 10,
                  .is_debuff = false};
              armorBuff.modifiers.push_back({
                  .value = armorPerStack,
                  .type = StatType::Armor,
                  .mode = ModifierMode::Flat});
              effects.AddOrRefresh(armorBuff);
            }
            registry.get_or_emplace<StatsDirty>(entity);
          }
        }

        // 532 剑气充盈: 引导期间每秒回复 10..30 点护盾 (Ward)
        if (profile && (profile->delivery.feature_flags & 2048) != 0) {
          int pts_532 = GetSkill5Point(registry, entity, 532);
          if (pts_532 > 0) {
            if (auto *st = registry.try_get<CombatStats>(entity)) {
              const float wardPerSecPerPoint =
                  data::SkillMechanicsRegistry::Get().GetFloat(5u, 532, "ward_per_sec_per_point", 10.0f);
              st->barrier = std::min(st->max_barrier > 0.0f ? st->max_barrier : 1000.0f,
                                     st->barrier + wardPerSecPerPoint * static_cast<float>(pts_532) * beam.tick_interval);
              registry.get_or_emplace<StatsDirty>(entity);
            }
          }
        }
      }

      beam.tick_timer = std::max(0.05f, beam.tick_interval);

      if (beam.skill_id == 5) {
        // 持续引导每秒消耗 20 法力 (drain)；effective_mana_cost 为每秒法耗绝对值
        //（Baker 已对技能5 写入 20.0f，并将 500 减免/510 增耗乘入该值）
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
        float mana_rate = profile ? profile->effective_mana_cost : 20.0f;
        float mana_cost = mana_rate * beam.tick_interval;
        auto *stats = registry.try_get<CombatStats>(entity);
        if (stats) {
          if (stats->mana < mana_cost) {
            triggerSwordGodFinisher(entity, beam);
            s_beam_to_remove.push_back(entity);
            continue;
          }
          stats->mana -= mana_cost;
          registry.get_or_emplace<StatsDirty>(entity);
        }
      }

      Vector2 targetPos = beam.target_pos;

      // 515 万剑归阵: 位于剑阵内时，集中轰击该剑阵区域且剑阵持续时间判定暂停
      if (beam.skill_id == 5) {
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
        if (profile && (profile->delivery.feature_flags & 256) != 0) {
          auto arrayView = registry.view<SwordArrayComponent, Position>();
          for (auto arrayEnt : arrayView) {
            auto &array = arrayView.get<SwordArrayComponent>(arrayEnt);
            const auto &arrPos = arrayView.get<Position>(arrayEnt);
            if (array.owner == entity) {
              if (Vector2Distance(beam.target_pos, {arrPos.x, arrPos.y}) <= array.radius) {
                targetPos = {arrPos.x, arrPos.y};
                array.duration += beam.tick_interval; // 暂停剑阵持续时间判定
                break;
              }
            }
          }
        }

        // 552 随影: 剑气雨固定在自身周围半径 150 内的圆形区域持续降下
        if (profile && (profile->delivery.feature_flags & 131072) != 0) {
          float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
          float r = static_cast<float>(GetRandomValue(10, 150));
          targetPos = {pos.x + std::cos(angle) * r, pos.y + std::sin(angle) * r};
        }
      }

      if (beam.aim_assist) {
        // 锁敌半径基准挂 510 节点 (lock_range)，511 无处遁形按点数扩大
        float lock_radius = data::SkillMechanicsRegistry::Get().GetFloat(5u, 510, "lock_range", 450.0f);
        if (beam.skill_id == 5) {
          const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5u);
          if (profile && (profile->delivery.feature_flags & 16) != 0) {
            int pts_511 = GetSkill5Point(registry, entity, 511);
            lock_radius *= (1.0f + data::SkillMechanicsRegistry::Get()
                                       .GetFloat(5u, 511, "lock_radius_pct_per_point", 0.15f) *
                                       static_cast<float>(pts_511));
          }
        }
        float bestDistSq = lock_radius * lock_radius;
        entt::entity bestTarget = entt::null;
        grid.query({targetPos.x, targetPos.y}, lock_radius,
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
        const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, beam.skill_id ? beam.skill_id : 5);
        const auto *chan = registry.try_get<ChannelingComponent>(entity);

        // 533 巨剑术: 数量减半 (3 -> 1), 体积+100% (radius 35 -> 70)
        bool isColossal = profile && ((profile->delivery.feature_flags & 4096) != 0);
        int count = isColossal ? 1 : (beam.is_empowered ? 4 : 3);
        float proj_radius = isColossal ? 70.0f : 35.0f;

        Tag effectiveTags = SkillSystem::GetEffectiveSkillTags(registry, entity, beam.skill_id ? beam.skill_id : 5);
        if (profile && profile->effective_tags != Tag::None) {
          effectiveTags = profile->effective_tags;
        }
        if (chan && chan->conversion_tag != Tag::None) {
          effectiveTags = (effectiveTags & ~Tag::Physical) | chan->conversion_tag;
        }

        Color swordColor = beam.is_empowered ? GOLD : ColorAlpha(Color{0, 170, 255, 255}, 0.5f);
        if (HasTag(effectiveTags, Tag::Fire)) {
          swordColor = ORANGE;
        } else if (HasTag(effectiveTags, Tag::Cold)) {
          swordColor = SKYBLUE;
        } else if (HasTag(effectiveTags, Tag::Lightning)) {
          swordColor = PURPLE;
        }

        float bonus_crit = chan ? chan->bonus_crit_chance : 0.0f;
        float bonus_armor_pen = chan ? chan->bonus_armor_pen : 0.0f;
        float bonus_crit_dmg = profile ? profile->delivery.bonus_crit_damage : 0.0f;

        float speedMult = 1.0f;
        if (beam.skill_id == 5 && profile && (profile->delivery.feature_flags & 16) != 0) {
          int pts_511 = GetSkill5Point(registry, entity, 511);
          speedMult = 1.0f +
                      data::SkillMechanicsRegistry::Get()
                              .GetFloat(5u, 511, "fall_speed_mult_per_point", 0.25f) *
                          static_cast<float>(pts_511);
        }
        float finalSpeed = 1000.0f * speedMult;
        auto *stats = registry.try_get<CombatStats>(entity);

        for (int i = 0; i < count; ++i) {
          float spreadAmt = static_cast<float>(GetRandomValue(-20, 20)) * DEG2RAD;
          Vector2 fireDir = Vector2Rotate(dirToTarget, spreadAmt);

          auto proj_ent = registry.create();
          registry.emplace<LocalLevelTag>(proj_ent);
          registry.emplace<Position>(proj_ent, pos.x + fireDir.x * 20.0f, pos.y + fireDir.y * 20.0f);
          registry.emplace<Velocity>(proj_ent, fireDir.x * finalSpeed, fireDir.y * finalSpeed);
          registry.emplace<ColorComponent>(proj_ent, swordColor);

          auto &proj = registry.emplace<Projectile>(proj_ent);
          proj.owner = entity;
          proj.cast_id = beam.cast_id;
          proj.lifeTime = 1.0f;
          proj.radius = proj_radius;
          proj.speed = finalSpeed;
          proj.pierce = true;
          proj.max_pierce = isColossal ? 3 : 1;
          proj.visualType = 2;

          if (stats) {
            DamagePayloadContext ctx{};
            ctx.base_damage_min = stats->min_weapon_damage;
            ctx.base_damage_max = stats->max_weapon_damage;
            ctx.crit_chance = (stats->crit_chance + bonus_crit) / 100.0f;
            ctx.crit_multiplier = stats->crit_damage + bonus_crit_dmg;
            ctx.increased_damage = 0.0f;
            ctx.more_damage = 0.40f * beam.bonus_damage_mult; // 40% 基础物理伤害
            ctx.effective_tags = effectiveTags;
            ctx.source_skill_id = beam.skill_id ? beam.skill_id : 5;
            proj.payload_context = ctx;

            proj.snapshot = *stats;
            for (auto &mult : proj.snapshot.damage_multipliers) {
              mult *= (0.40f * beam.bonus_damage_mult);
            }
            proj.snapshot.crit_chance += bonus_crit;
            proj.snapshot.armor_pen += bonus_armor_pen;
            proj.snapshot.crit_damage += bonus_crit_dmg;
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
          }
          registry.emplace<SkillComponent>(proj_ent, beam.skill_id ? beam.skill_id : 5, entity);

          if (!HasTag(effectiveTags, Tag::Physical)) {
            Tag elem = Tag::None;
            if (HasTag(effectiveTags, Tag::Fire)) elem = Tag::Fire;
            else if (HasTag(effectiveTags, Tag::Cold)) elem = Tag::Cold;
            else if (HasTag(effectiveTags, Tag::Lightning)) elem = Tag::Lightning;
            if (elem != Tag::None) {
              auto &mods = registry.emplace<SkillModifierComponent>(proj_ent);
              mods.damage_modifiers.push_back(
                  DamageModifier{Tag::Physical, elem, 1.0f, ModifierType::Convert});
            }
          }

          auto &particleSys = systems::GPUParticleSystem::Get();
          components::GPUParticle p;
          p.position = {pos.x + fireDir.x * 30.0f, pos.y + fireDir.y * 30.0f};
          p.velocity = Vector2Scale(fireDir, 200.0f);
          p.color = beam.is_empowered ? GOLD : (HasTag(effectiveTags, Tag::Fire) ? ORANGE : (HasTag(effectiveTags, Tag::Cold) ? SKYBLUE : ColorAlpha(WHITE, 0.6f)));
          p.lifetime = 0.2f;
          p.maxLifetime = 0.2f;
          p.scale = isColossal ? 3.5f : 2.0f;
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
      if (registry.any_of<ChannelingComponent>(e)) {
        registry.remove<ChannelingComponent>(e);
      }
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
