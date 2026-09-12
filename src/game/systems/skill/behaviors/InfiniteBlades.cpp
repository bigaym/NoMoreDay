/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 模块化弹幕光束与天降打击交付实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay::skills {

namespace InfiniteBladesNodes {
// Base Tier
constexpr uint32_t Rainfall = 500;
constexpr uint32_t Resonance = 501;
constexpr uint32_t MeteoricIron = 502;
constexpr uint32_t Fluidity = 503;

// Branch A
constexpr uint32_t MindLock = 510;
constexpr uint32_t NoEscape = 511;
constexpr uint32_t FateMark = 512;
constexpr uint32_t Execution = 513;
constexpr uint32_t BladeStorm = 514;
constexpr uint32_t BladesToArray = 515;

// Branch B
constexpr uint32_t Composure = 530;
constexpr uint32_t SteeledBody = 531;
constexpr uint32_t AbundantQi = 532;
constexpr uint32_t ColossalBlades = 533;
constexpr uint32_t SwordGod = 534;
constexpr uint32_t Shockwave = 535;

// Branch C
constexpr uint32_t WalkThePath = 550;
constexpr uint32_t SwordStepChannel = 551;
constexpr uint32_t FollowingShadow = 552;
constexpr uint32_t IntentSiphon = 553;
constexpr uint32_t IntentBurst = 554;
constexpr uint32_t Multiplier = 555;

// Branch D
constexpr uint32_t MeteorShower = 570;
constexpr uint32_t DoomsdayAsh = 571;
constexpr uint32_t Blizzard = 572;
constexpr uint32_t AbsoluteZero = 573;
constexpr uint32_t ElementalAttunement = 574;
constexpr uint32_t Catastrophe = 575;
} // namespace InfiniteBladesNodes

struct InfiniteBlades : SkillBehaviorBase<InfiniteBlades> {
  static constexpr uint32_t kSkillId = 5;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    // 513 天诛 (Execution) 等触发调用处理：当 trigger_depth > 0 时，作为天诛穿透必爆主剑降落，
    // 不重置玩家正在进行的引导状态，彻底消解 C4 自打断死循环。
    if (exec.trigger_depth > 0) {
      auto proj_ent = registry.create();
      registry.emplace<LocalLevelTag>(proj_ent);
      registry.emplace<ShadowCastTag>(proj_ent);
      registry.emplace<Position>(proj_ent, exec.target_pos.x, exec.target_pos.y - 120.0f);
      registry.emplace<Velocity>(proj_ent, 0.0f, 1200.0f);

      auto &proj = registry.emplace<Projectile>(proj_ent);
      proj.owner = owner;
      proj.cast_id = exec.cast_id;
      proj.lifeTime = 0.4f;
      proj.radius = 70.0f;
      proj.speed = 1200.0f;
      proj.pierce = true;
      proj.max_pierce = 999;
      proj.visualType = 2;

      Tag effectiveTags = Tag::Physical;
      if (const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId)) {
        if (profile->effective_tags != Tag::None) effectiveTags = profile->effective_tags;
      }

      Color swordColor = GOLD;
      if (HasTag(effectiveTags, Tag::Fire)) swordColor = ORANGE;
      else if (HasTag(effectiveTags, Tag::Cold)) swordColor = SKYBLUE;
      registry.emplace<ColorComponent>(proj_ent, swordColor);

      if (auto *stats = registry.try_get<CombatStats>(owner)) {
        DamagePayloadContext ctx{};
        ctx.base_damage_min = stats->min_weapon_damage;
        ctx.base_damage_max = stats->max_weapon_damage;
        ctx.crit_chance = 1.0f; // 必爆
        ctx.crit_multiplier = stats->crit_damage;
        ctx.increased_damage = 0.0f;
        ctx.more_damage = 3.0f * (exec.trigger_effectiveness > 0.0f ? exec.trigger_effectiveness / 3.0f : 1.0f); // 300% 基础伤害
        ctx.effective_tags = effectiveTags;
        ctx.source_skill_id = kSkillId;
        proj.payload_context = ctx;

        proj.snapshot = *stats;
        for (auto &mult : proj.snapshot.damage_multipliers) {
          mult *= ctx.more_damage;
        }
        proj.snapshot.crit_chance = 1.0f; // 必爆（归一化）
        registry.emplace<CombatStats>(proj_ent, proj.snapshot);
      }
      registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);
      return;
    }

    // 正常主动施放：持续引导初始化 (基底 5.0s 引导上限，0.3s 发射间隔)
    auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
    chan.skill_id = kSkillId;
    chan.channel_timer = 5.0f;
    chan.tick_interval = 0.3f;
    chan.tick_timer = -0.01f;
    chan.target_pos = exec.target_pos;
    chan.is_empowered = exec.is_empowered;
    chan.cast_id = exec.cast_id;
    chan.conversion_tag = Tag::None;
    chan.bonus_damage_mult = 1.0f;
    chan.bonus_crit_chance = 0.0f;
    chan.bonus_armor_pen = 0.0f;
    chan.synergy_lock = false;

    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) {
          SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr);
          profile = &localProfile;
          break;
        }
      }
    }

    // 501 剑意共鸣: 发射频率加成 (写入 sub_interval)
    if (profile && profile->delivery.sub_interval > 0.0f) {
      chan.tick_interval = profile->delivery.sub_interval;
    }

    // 551 御剑风雷: 若在处于御剑步状态时开始引导，引导期间获得闪避加成 (+50..150)
    if (profile && (profile->delivery.feature_flags & 65536) != 0) {
      bool inSwordStep = false;
      if (const auto *effects = registry.try_get<ActiveEffectsComponent>(owner)) {
        inSwordStep = (effects->Get(BuffId::SwordStep) != nullptr);
      }
      if (!inSwordStep) {
        inSwordStep = registry.any_of<PhaseTag>(owner);
      }
      if (inSwordStep) {
        int pts_551 = 0;
        if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
          for (const auto &spec : active->specialized_slots) {
            if (spec.skill_id == kSkillId) {
              auto it = spec.allocated_points.find(InfiniteBladesNodes::SwordStepChannel);
              if (it != spec.allocated_points.end()) pts_551 = it->second;
              break;
            }
          }
        }
        if (pts_551 > 0) {
          auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
          // 闪避加成随 buff 修饰符携带，随 buff 生命周期生效/失效（N5 修复）：
          // 直接累加 dodge_rating 会被 AttributePipeline 属性重算整体覆盖。
          const float dodgeBonus =
              data::SkillMechanicsRegistry::Get().GetFloat(5, 551, "dodge_rating_per_point", 50.0f) *
              static_cast<float>(pts_551);
          BuffEffect dodgeBuff{
              .id = "SwordStepChannelDodge",
              .name = "Sword Step Channel Dodge",
              .type = BuffType::SpeedUp,
              .duration = 5.0f,
              .remaining = 5.0f,
              .stacks = 1,
              .max_stacks = 1,
              .is_debuff = false};
          dodgeBuff.modifiers.push_back({.value = dodgeBonus,
                                         .type = StatType::DodgeRating,
                                         .mode = ModifierMode::Flat,
                                         .source = ModifierSource::Buff});
          effects.AddOrRefresh(dodgeBuff);
          // 标记属性重算，使 buff 修饰符在属性管线消费路径生效
          registry.get_or_emplace<StatsDirty>(owner);
        }
      }
    }

    // 554 意气爆发: 若拥有满层 (10层) 剑意，消耗所有剑意使本次引导获得 100% 暴击率
    if (profile && (profile->delivery.feature_flags & 524288) != 0) {
      auto *intent = registry.try_get<SwordIntentComponent>(owner);
      // 满层阈值外置: intent_cost (默认 10 层)
      const int intentCost =
          static_cast<int>(data::SkillMechanicsRegistry::Get().GetFloat(5, 554, "intent_cost", 10.0f) + 0.5f);
      if (intent && intent->stacks >= intentCost) {
        SkillSystem::ConsumeSwordIntent(registry, owner, intentCost, kSkillId);
        chan.bonus_crit_chance += 1.0f; // 意气爆发必暴（归一化）
        chan.consume_intent = true;
        // 555 意念合一: 触发意气爆发时，暴伤额外提升
        if ((profile->delivery.feature_flags & 1048576) != 0) {
          chan.bonus_damage_mult *= (1.0f + profile->delivery.bonus_crit_damage);
        }
      }
    }

    // 574 灵根感应: 属性穿透写入 bonus_armor_pen
    if (profile && profile->delivery.armor_pen > 0.0f) {
      chan.bonus_armor_pen += profile->delivery.armor_pen;
    }

    // 元素转质与标签继承
    if (profile && (profile->effective_tags & (Tag::Fire | Tag::Cold | Tag::Lightning)) != Tag::None) {
      chan.conversion_tag = profile->effective_tags & (Tag::Fire | Tag::Cold | Tag::Lightning);
    }
    if (chan.conversion_tag == Tag::None) {
      chan.conversion_tag = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
    }

    // 综合增伤应用
    if (profile && profile->more_damage_mult > 0.0f) {
      chan.bonus_damage_mult *= profile->more_damage_mult;
    }

    // 同步配置并挂载 BeamChannelComponent，保证管线一致
    auto &beam = registry.emplace_or_replace<BeamChannelComponent>(owner);
    beam.owner = owner;
    beam.cast_id = exec.cast_id;
    beam.skill_id = kSkillId;
    beam.mode = BeamChannelMode::BarrageEmitter;
    beam.max_channel_time = chan.channel_timer;
    beam.tick_interval = chan.tick_interval;
    beam.target_pos = exec.target_pos;
    beam.is_empowered = exec.is_empowered;
    beam.bonus_damage_mult = chan.bonus_damage_mult;
    // 510 神识锁定: 开启 aim_assist
    beam.aim_assist = profile ? ((profile->delivery.feature_flags & 8) != 0) : false;
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity victim, Tag hit_tag, bool is_crit) {
    if (!reg.valid(attacker) || !reg.valid(victim)) return;

    int pts_512 = 0;
    int pts_514 = 0;
    int pts_553 = 0;
    int pts_571 = 0;
    int pts_573 = 0;
    int pts_575 = 0;
    int pts_502 = 0;
    int pts_572 = 0;
    if (const auto *active = reg.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          auto it512 = spec.allocated_points.find(InfiniteBladesNodes::FateMark);
          if (it512 != spec.allocated_points.end()) pts_512 = it512->second;
          auto it514 = spec.allocated_points.find(InfiniteBladesNodes::BladeStorm);
          if (it514 != spec.allocated_points.end()) pts_514 = it514->second;
          auto it553 = spec.allocated_points.find(InfiniteBladesNodes::IntentSiphon);
          if (it553 != spec.allocated_points.end()) pts_553 = it553->second;
          auto it571 = spec.allocated_points.find(InfiniteBladesNodes::DoomsdayAsh);
          if (it571 != spec.allocated_points.end()) pts_571 = it571->second;
          auto it573 = spec.allocated_points.find(InfiniteBladesNodes::AbsoluteZero);
          if (it573 != spec.allocated_points.end()) pts_573 = it573->second;
          auto it575 = spec.allocated_points.find(InfiniteBladesNodes::Catastrophe);
          if (it575 != spec.allocated_points.end()) pts_575 = it575->second;
          auto it502 = spec.allocated_points.find(InfiniteBladesNodes::MeteoricIron);
          if (it502 != spec.allocated_points.end()) pts_502 = it502->second;
          auto it572 = spec.allocated_points.find(InfiniteBladesNodes::Blizzard);
          if (it572 != spec.allocated_points.end()) pts_572 = it572->second;
          break;
        }
      }
    }

    // 512 天降命印: 20%..100% 几率附加命印 (BuffKind::FateMark, max 5)
    if (pts_512 > 0) {
      // 命印几率外置: fate_mark_chance_pct_per_point 为小数语义 (0.20 → 每点 20%)
      int chance = static_cast<int>(
          data::SkillMechanicsRegistry::Get().GetFloat(5, 512, "fate_mark_chance_pct_per_point", 0.20f) *
              static_cast<float>(pts_512) * 100.0f +
          0.5f);
      if (GetRandomValue(1, 100) <= chance) {
        auto &vicEffects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        BuffEffect mark{
          .id = "FateMark",
          .name = "Fate Mark",
          .type = BuffType::DefenseDown,
          .kind = BuffKind::FateMark,
          .duration = 5.0f,
          .remaining = 5.0f,
          .stacks = 1,
          .max_stacks = 5,
          .is_debuff = true
        };
        vicEffects.AddOrRefresh(mark);
        reg.get_or_emplace<StatsDirty>(victim);
      }

      // 叠满 5 层后，命中该目标时向周围溅射 20% 伤害
      if (const auto *vicEffects = reg.try_get<ActiveEffectsComponent>(victim)) {
        if (const auto *fateMark = vicEffects->GetByKind(BuffKind::FateMark); fateMark && fateMark->stacks >= 5) {
          if (reg.all_of<Position>(victim)) {
            const auto &vPos = reg.get<Position>(victim);
            // 溅射半径外置: splash_radius (默认 60)
            const float splashRadius =
                data::SkillMechanicsRegistry::Get().GetFloat(5, 512, "splash_radius", 60.0f);
            float splashRadiusSqr = splashRadius * splashRadius;
            float baseHit = 20.0f;
            if (const auto *st = reg.try_get<CombatStats>(attacker)) {
              baseHit = (st->min_weapon_damage + st->max_weapon_damage) * 0.5f * 0.40f;
            }
            auto enemyView = reg.view<EnemyTag, Position>();
            for (auto eEnt : enemyView) {
              if (eEnt == victim || reg.any_of<KilledTag>(eEnt)) continue;
              const auto &ePos = enemyView.get<Position>(eEnt);
              if (Vector2DistanceSqr({vPos.x, vPos.y}, {ePos.x, ePos.y}) <= splashRadiusSqr) {
                DamagePool pool;
                // 满层溅射比例外置: full_stack_splash_pct (默认 0.20)
                pool.Add(hit_tag != Tag::None ? hit_tag : Tag::Physical,
                         baseHit * data::SkillMechanicsRegistry::Get().GetFloat(5, 512, "full_stack_splash_pct", 0.20f));
                DamageRequest req;
                req.origin = DamageOrigin::SecondaryProc;
                req.attacker = attacker;
                req.defender = eEnt;
                req.skill_id = kSkillId;
                req.base_pool = pool;
                req.additional_tags = (hit_tag != Tag::None ? hit_tag : Tag::Physical) | Tag::SecondaryHit;
                (void)ResolveDamage(reg, req, attacker);
              }
            }
          }
        }
      }
    }

    // 502 陨铁: 命中时向周围敌人造成主命中伤害 15% 的微小溅射 (N7 补全)
    // 伤害基数与 512 满层溅射保持一致: 单剑主命中 = 40% 武器均值, 溅射 = 15% 主命中
    if (pts_502 > 0 && reg.all_of<Position>(victim)) {
      const auto &vPos = reg.get<Position>(victim);
      float splashRadius = data::SkillMechanicsRegistry::Get().GetFloat(5, 502, "splash_radius", 30.0f);
      float splashRadiusSqr = splashRadius * splashRadius;
      float baseHit = 20.0f;
      if (const auto *st = reg.try_get<CombatStats>(attacker)) {
        baseHit = (st->min_weapon_damage + st->max_weapon_damage) * 0.5f * 0.40f;
      }
      auto enemyView = reg.view<EnemyTag, Position>();
      for (auto eEnt : enemyView) {
        if (eEnt == victim || reg.any_of<KilledTag>(eEnt)) continue;
        const auto &ePos = enemyView.get<Position>(eEnt);
        if (Vector2DistanceSqr({vPos.x, vPos.y}, {ePos.x, ePos.y}) <= splashRadiusSqr) {
          DamagePool pool;
          pool.Add(hit_tag != Tag::None ? hit_tag : Tag::Physical, baseHit * 0.15f);
          DamageRequest req;
          req.origin = DamageOrigin::SecondaryProc;
          req.attacker = attacker;
          req.defender = eEnt;
          req.skill_id = kSkillId;
          req.base_pool = pool;
          req.additional_tags = (hit_tag != Tag::None ? hit_tag : Tag::Physical) | Tag::SecondaryHit;
          (void)ResolveDamage(reg, req, attacker);
        }
      }
    }

    // 514 剑刃风暴: 被击杀的敌人向周围碎裂出小剑气产生 30%..90% 的分裂伤害
    bool isKilled = reg.any_of<KilledTag>(victim);
    if (!isKilled) {
      if (const auto *hp = reg.try_get<HealthComponent>(victim)) {
        if (hp->current <= 0.0f) isKilled = true;
      }
    }
    if (isKilled && pts_514 > 0 && reg.all_of<Position>(victim)) {
      const auto &vPos = reg.get<Position>(victim);
      // 分裂伤害与数量外置: kill_split_damage_pct_per_point / split_count
      float splitMult =
          data::SkillMechanicsRegistry::Get().GetFloat(5, 514, "kill_split_damage_pct_per_point", 0.30f) *
          static_cast<float>(pts_514);
      const int splitCount = static_cast<int>(
          data::SkillMechanicsRegistry::Get().GetFloat(5, 514, "split_count", 3.0f) + 0.5f);
      auto *stats = reg.try_get<CombatStats>(attacker);
      for (int i = 0; i < splitCount; ++i) {
        float angle = static_cast<float>(i) * (2.0f * PI / 3.0f);
        Vector2 dir = {std::cos(angle), std::sin(angle)};
        auto shard = reg.create();
        reg.emplace<LocalLevelTag>(shard);
        reg.emplace<Position>(shard, vPos.x + dir.x * 10.0f, vPos.y + dir.y * 10.0f);
        reg.emplace<Velocity>(shard, dir.x * 600.0f, dir.y * 600.0f);
        auto &proj = reg.emplace<Projectile>(shard);
        proj.owner = attacker;
        proj.lifeTime = 0.4f;
        proj.radius = 20.0f;
        proj.speed = 600.0f;
        proj.pierce = true;
        proj.max_pierce = 1;
        proj.visualType = 2;
        if (stats) {
          DamagePayloadContext ctx{};
          ctx.base_damage_min = stats->min_weapon_damage * splitMult;
          ctx.base_damage_max = stats->max_weapon_damage * splitMult;
          ctx.more_damage = 1.0f;
          ctx.effective_tags = hit_tag != Tag::None ? hit_tag : Tag::Physical;
          ctx.source_skill_id = kSkillId;
          proj.payload_context = ctx;
          proj.snapshot = *stats;
          reg.emplace<CombatStats>(shard, proj.snapshot);
        }
        reg.emplace<SkillComponent>(shard, kSkillId, attacker);
      }
    }

    // 553 剑意回流: 连击命中 10 次或击杀回复 1..4 层剑意
    if (pts_553 > 0) {
      struct InfiniteBladesHitTracker { int hit_count = 0; };
      auto &tracker = reg.get_or_emplace<InfiniteBladesHitTracker>(attacker);
      tracker.hit_count++;
      // 连击阈值外置: hits_required (默认 10)
      const int hitsRequired = static_cast<int>(
          data::SkillMechanicsRegistry::Get().GetFloat(5, 553, "hits_required", 10.0f) + 0.5f);
      if (tracker.hit_count >= hitsRequired || isKilled) {
        tracker.hit_count = 0;
        if (!systems::BladeResourceService::Gain(reg, attacker, pts_553, kSkillId)) {
          if (auto *intent = reg.try_get<SwordIntentComponent>(attacker)) {
            intent->stacks = std::min(SkillConstants::DEFAULT_MAX_SWORD_INTENT, intent->stacks + pts_553);
          }
        }
      }
    }

    // 571 末日余烬: 陨石落地后留下燃烧区域，对经过的敌人每秒造成本次命中伤害 10%..30% 的持续火焰 DoT，持续 3 秒
    if (pts_571 > 0 && HasTag(hit_tag, Tag::Fire) && reg.all_of<Position>(victim)) {
      const auto &vPos = reg.get<Position>(victim);
      auto fire_field = reg.create();
      reg.emplace<LocalLevelTag>(fire_field);
      reg.emplace<Position>(fire_field, vPos.x, vPos.y);
      auto &area = reg.emplace<AreaFieldComponent>(fire_field);
      area.owner = attacker;
      area.source_skill_id = kSkillId;
      area.remaining_duration = 3.0f;
      area.radius = 50.0f;
      area.pulse_interval = 0.5f;
      area.payload_count = 1;
      area.payloads[0].type = PayloadType::Damage;
      // 设计为每秒 10%..30%×点数的 DoT，此处按 pulse 间隔折算到每次脉冲
      // (每秒伤害占比 × 脉冲间隔秒数 = 单脉冲伤害占比)
      area.payloads[0].value_mult =
          data::SkillMechanicsRegistry::Get().GetFloat(5, 571, "burn_damage_pct_per_point", 0.10f) *
          static_cast<float>(pts_571) *
          data::SkillMechanicsRegistry::Get().GetFloat(5, 571, "burn_tick_interval", 0.5f);
      area.payloads[0].damage_tags = Tag::Fire | Tag::Area;
    }

    // 572 凛冬暴雪: 冰锥命中施加寒冷减速，并按几率冻结 (N7 补全)
    // 沿用 575 天灾的 Chill (SpeedDown) 写法 + FlowingThrust 的 Frozen (Freeze) 写法
    if (pts_572 > 0 && HasTag(hit_tag, Tag::Cold)) {
      auto &vicEffects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
      // 寒冷减速: 速度降低幅度体现 chill_magnitude, 通过 MoveSpeed PercentAdd 修饰符表达
      const float chillMagnitude =
          data::SkillMechanicsRegistry::Get().GetFloat(5, 572, "chill_magnitude", 0.20f);
      BuffEffect chill{
          .id = "Chill",
          .name = "Chill",
          .type = BuffType::SpeedDown,
          .duration = 3.0f,
          .remaining = 3.0f,
          .stacks = 1,
          .max_stacks = 3,
          .is_debuff = true};
      chill.modifiers.push_back({.value = -chillMagnitude * 100.0f,
                                 .type = StatType::MoveSpeed,
                                 .mode = ModifierMode::PercentAdd,
                                 .source = ModifierSource::Buff});
      vicEffects.AddOrRefresh(chill);
      // 冻结: 按 freeze_chance 几率施加 (参照 FlowingThrust 的 Frozen buff 写法)
      const float freezeChance =
          data::SkillMechanicsRegistry::Get().GetFloat(5, 572, "freeze_chance", 0.25f);
      if (GetRandomValue(1, 100) <= static_cast<int>(freezeChance * 100.0f + 0.5f)) {
        BuffEffect frozen{
            .id = "Frozen",
            .name = "Frozen",
            .type = BuffType::Freeze,
            .duration = 1.5f,
            .remaining = 1.5f,
            .is_debuff = true};
        vicEffects.AddOrRefresh(frozen);
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 573 绝对零度: 冻结延长 0.1s..0.3s
    if (pts_573 > 0 && HasTag(hit_tag, Tag::Cold)) {
      if (auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
        // 冻结延长外置: freeze_extension_per_point (默认 0.10s/点)
        const float freezeExt =
            data::SkillMechanicsRegistry::Get().GetFloat(5, 573, "freeze_extension_per_point", 0.10f) *
            static_cast<float>(pts_573);
        for (auto &b : fx->effects) {
          if (b.type == BuffType::Freeze && b.remaining > 0.0f) {
            b.remaining += freezeExt;
            b.duration += freezeExt;
            break;
          }
        }
      }
    }

    // 575 天灾: 元素异常几率 +30%..90%
    if (pts_575 > 0) {
      // 异常几率外置: ailment_chance_pct_per_point (默认 30%/点)
      int procChance = static_cast<int>(
          data::SkillMechanicsRegistry::Get().GetFloat(5, 575, "ailment_chance_pct_per_point", 30.0f) *
              static_cast<float>(pts_575) +
          0.5f);
      if (GetRandomValue(1, 100) <= procChance) {
        auto &vicEffects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        if (HasTag(hit_tag, Tag::Fire)) {
          BuffEffect ignite{
              .id = "Ignite",
              .name = "Ignite",
              .type = BuffType::Burn,
              .duration = 3.0f,
              .remaining = 3.0f,
              .stacks = 1,
              .max_stacks = 5,
              .is_debuff = true};
          vicEffects.AddOrRefresh(ignite);
        } else if (HasTag(hit_tag, Tag::Cold)) {
          BuffEffect chill{
              .id = "Chill",
              .name = "Chill",
              .type = BuffType::SpeedDown,
              .duration = 3.0f,
              .remaining = 3.0f,
              .stacks = 1,
              .max_stacks = 3,
              .is_debuff = true};
          vicEffects.AddOrRefresh(chill);
        }
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(InfiniteBlades)
void RegisterInfiniteBlades() {}
} // namespace NoMoreDay::skills

