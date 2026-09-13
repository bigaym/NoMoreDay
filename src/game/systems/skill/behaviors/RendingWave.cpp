/**
 * @file RendingWave.cpp
 * @brief 裂空斩 (ID 2) - 模块化投射物波刃技能实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "SevenStarSlashShared.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"
#include <algorithm>
#include <vector>

namespace NoMoreDay::skills {

namespace RendingWaveNodes {
// Base Tier (基础核心)
constexpr uint32_t WaveExpansion = 200;      // 剑气纵横
constexpr uint32_t Focus = 201;              // 凝神
constexpr uint32_t Edge = 202;               // 锋芒
constexpr uint32_t QiBurst = 203;            // 气劲爆发

// Branch A: 剑雨流 (Multi, Split & Orbit)
constexpr uint32_t MultiWave = 210;          // 多重剑气
constexpr uint32_t Fracture = 211;           // 碎裂之刃
constexpr uint32_t ChainReaction = 212;      // 连锁反应
constexpr uint32_t Scatter = 213;            // 万剑归宗-残篇 (Keystone)
constexpr uint32_t Orbit = 214;              // 星环护体 (Keystone)
constexpr uint32_t SpiritPursuit = 215;      // 灵剑追击 (Synergy)

// Branch B: 逆冲流 (Return & Gravity)
constexpr uint32_t Boomerang = 230;          // 回旋劲
constexpr uint32_t DoubleHit = 231;          // 重叠打击
constexpr uint32_t GravityWell = 232;        // 引力陷阱
constexpr uint32_t AbyssEdge = 233;          // 深渊边缘
constexpr uint32_t TimeLock = 234;           // 时空停滞 (Keystone)
constexpr uint32_t SwordStepGravity = 235;   // 御剑引力

// Branch C: 剑意乾坤 (Intent & Amplification)
constexpr uint32_t QiBrand = 250;            // 剑气烙印
constexpr uint32_t IntentBurst = 251;        // 剑意爆发
constexpr uint32_t Bottomless = 252;         // 无底深渊
constexpr uint32_t ObliterationWave = 253;   // 湮灭波 (Keystone)
constexpr uint32_t EchoedSlash = 254;        // 回响斩 (Trigger)
constexpr uint32_t IntentRecovery = 255;     // 意念回流

// Branch D: 灵根变转 (Elemental Conversion)
constexpr uint32_t FrostForm = 270;          // 霜寒之刃 (Transmuter)
constexpr uint32_t ShatterCascade = 271;     // 冰晶碎裂
constexpr uint32_t LightningForm = 272;      // 雷光 (Transmuter)
constexpr uint32_t StaticConduction = 273;   // 感电传导
constexpr uint32_t ElementalAffinity = 274;  // 灵根亲和
constexpr uint32_t Proliferation = 275;      // 异常扩散
} // namespace RendingWaveNodes

struct RendingWaveStateComponent {
  float last_echo_time = -10.0f;
};

struct RendingWave : SkillBehaviorBase<RendingWave> {
  static constexpr uint32_t kSkillId = 2;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    if (!pos || !stats) return;

    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, false, exec.cast_id);
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

    auto getPts = [&](uint32_t nid) -> int {
      if (registry.all_of<ActiveSkillsComponent>(owner)) {
        for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
          if (spec.skill_id == kSkillId) {
            return ReadPoints(spec, nid);
          }
        }
      }
      return 0;
    };

    const auto &mech = data::SkillMechanicsRegistry::Get();

    // 剑意结算 (251 剑意爆发: ≥5层消耗全部剑意; 253 湮灭波: 10层巨波)
    const bool hasIntentBurst = profile ? ((profile->delivery.feature_flags & 4096) != 0) : (getPts(RendingWaveNodes::IntentBurst) > 0);
    int currentIntent = 0;
    if (const auto *res = registry.try_get<BladeResourceComponent>(owner)) {
      currentIntent = res->current;
    } else if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
      currentIntent = intent->stacks;
    }
    int consumedIntent = 0;
    const float threshold = mech.GetFloat(kSkillId, RendingWaveNodes::IntentBurst, "intent_threshold", 5.0f);
    if (hasIntentBurst && currentIntent >= static_cast<int>(threshold)) {
      if (SkillSystem::ConsumeSwordIntent(registry, owner, currentIntent, kSkillId)) {
        consumedIntent = currentIntent;
        exec.is_empowered = true;
        if (consumedIntent >= SkillConstants::DEFAULT_MAX_SWORD_INTENT) {
          // 满10层额外重置流云刺冷却：GDD《职业设计草案_剑修.md》L210 (251 剑意爆发)
          // 明文效果——消耗满层(10层)剑意时，额外重置 [流云刺] 的冷却时间。
          seven_star_shared::ResetSkillCooldown(
              registry, owner, seven_star_shared::kFlowingThrustSkillId);
        }
      }
    }

    // 252 无底深渊: 消耗剑意施放时 10%...30% 几率返还法力
    const int pts_252 = getPts(RendingWaveNodes::Bottomless);
    if (consumedIntent > 0 && pts_252 > 0) {
      const float refundChance = mech.GetFloat(kSkillId, RendingWaveNodes::Bottomless, "mana_refund_chance_pct_per_point", 10.0f) * static_cast<float>(pts_252);
      if (GetRandomValue(1, 100) <= static_cast<int>(refundChance)) {
        float refundAmount = profile ? profile->effective_mana_cost : 10.0f;
        stats->mana = std::min(stats->max_mana, stats->mana + refundAmount);
      }
    }

    // 253 湮灭波: 满层(10)剑意巨波
    const bool hasObliteration = profile ? ((profile->delivery.feature_flags & 32768) != 0) : (getPts(RendingWaveNodes::ObliterationWave) > 0);
    const bool isObliteration = (hasObliteration && consumedIntent >= 10);

    Vector2 baseDir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));
    // 272 雷光的弹速加成由 Baker 写入 delivery.speed (相对倍率: 1.0 + speed_bonus_pct/100)，
    // 行为层以基准弹速 300 相乘接线 mech 数值，不在代码中硬编码节点倍率
    const float deliverySpeed = profile ? profile->delivery.speed : 1.0f;
    float baseSpeed = 300.0f * deliverySpeed;
    float baseRadius = profile ? profile->area_radius : 35.0f;
    float moreDamageMult = profile ? profile->more_damage_mult : 1.0f;
    float extraCritChance = 0.0f;

    if (consumedIntent > 0) {
      const float areaPct = mech.GetFloat(kSkillId, RendingWaveNodes::IntentBurst, "area_pct_per_intent", 8.0f) / 100.0f;
      const float critPct = mech.GetFloat(kSkillId, RendingWaveNodes::IntentBurst, "crit_pct_per_intent", 3.0f) / 100.0f;
      baseRadius *= (1.0f + areaPct * static_cast<float>(consumedIntent));
      extraCritChance += critPct * static_cast<float>(consumedIntent);
    }

    if (isObliteration) {
      const float oblitMore = mech.GetFloat(kSkillId, RendingWaveNodes::ObliterationWave, "obliteration_more_damage", 0.80f);
      const float oblitRad = mech.GetFloat(kSkillId, RendingWaveNodes::ObliterationWave, "obliteration_radius_mult", 2.5f);
      moreDamageMult *= (1.0f + oblitMore);
      baseRadius *= oblitRad;
    }

    // 元素转换 (270 霜寒之刃 / 272 雷光)
    Tag effTag = profile ? profile->effective_tags : Tag::Physical;
    const int pts_270 = getPts(RendingWaveNodes::FrostForm);
    const int pts_272 = getPts(RendingWaveNodes::LightningForm);
    const bool isCold = (effTag == Tag::Cold) || (pts_270 > 0);
    const bool isLightning = (effTag == Tag::Lightning) || (pts_272 > 0);
    if (isCold) effTag = Tag::Cold;
    else if (isLightning) effTag = Tag::Lightning;

    // 272 雷光: 物理转闪电的元素转换修饰符由下方 SkillModifierComponent 注入；
    // 弹速加成已收敛到 delivery.speed (Baker 按 speed_bonus_pct 写入)，此处不再 ×2

    const Tag attunement = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);

    // 形态判定
    const bool isOrbit = profile ? ((profile->delivery.feature_flags & 64) != 0) : (getPts(RendingWaveNodes::Orbit) > 0);
    const bool isTimeLock = profile ? ((profile->delivery.feature_flags & 32) != 0) : (getPts(RendingWaveNodes::TimeLock) > 0);
    const bool isBoomerang = profile ? ((profile->delivery.feature_flags & 1) != 0) : (getPts(RendingWaveNodes::Boomerang) > 0 || getPts(RendingWaveNodes::GravityWell) > 0 || getPts(RendingWaveNodes::AbyssEdge) > 0);
    const bool isSplit = profile ? ((profile->delivery.feature_flags & 4) != 0) : (getPts(RendingWaveNodes::Fracture) > 0);
    const bool isScatter = profile ? ((profile->delivery.feature_flags & 8) != 0) : (getPts(RendingWaveNodes::Scatter) > 0);
    const bool isPursuit = profile ? ((profile->delivery.feature_flags & 128) != 0) : (getPts(RendingWaveNodes::SpiritPursuit) > 0);

    const int pts_212 = getPts(RendingWaveNodes::ChainReaction);
    const int pts_231 = getPts(RendingWaveNodes::DoubleHit);
    const int pts_233 = getPts(RendingWaveNodes::AbyssEdge);
    const int pts_235 = getPts(RendingWaveNodes::SwordStepGravity);
    const bool inSwordStep = registry.any_of<PhaseTag>(owner);
    const float maxRange = (profile && profile->delivery.range > 0.0f) ? profile->delivery.range : 400.0f;

    // 214 星环护体 (Keystone): 环绕周身旋转 3s
    if (isOrbit) {
      const float orbitDuration = mech.GetFloat(kSkillId, RendingWaveNodes::Orbit, "orbit_duration", 3.0f);
      const float orbitRad = mech.GetFloat(kSkillId, RendingWaveNodes::Orbit, "orbit_radius", 60.0f);
      const float orbitTick = mech.GetFloat(kSkillId, RendingWaveNodes::Orbit, "orbit_tick_rate", 0.2f);
      const float orbitDmgMult = mech.GetFloat(kSkillId, RendingWaveNodes::Orbit, "orbit_damage_mult", 0.35f);

      auto proj_ent = registry.create();
      registry.emplace<LocalLevelTag>(proj_ent);
      registry.emplace<Position>(proj_ent, pos->x + baseDir.x * orbitRad, pos->y + baseDir.y * orbitRad);
      registry.emplace<Velocity>(proj_ent, 0.0f, 0.0f);

      auto &proj = registry.emplace<Projectile>(proj_ent);
      proj.owner = owner;
      proj.cast_id = exec.cast_id;
      proj.speed = 0.0f;
      proj.lifeTime = orbitDuration;
      proj.radius = baseRadius;
      proj.pierce = true;
      proj.pierceCount = 99;
      proj.max_pierce = Projectile::kUnlimitedPiercing;
      proj.snapshot = *stats;
      proj.payload_context = {
        .base_damage_min = stats->min_weapon_damage,
        .base_damage_max = stats->max_weapon_damage,
        .crit_chance = stats->crit_chance + extraCritChance,
        .crit_multiplier = stats->crit_damage,
        // more_damage 须含玩家全局乘区 damage_multipliers[0] (与 SkillSystem 标准路径一致)，
        // 否则轨道伤害丢失全局增伤
        .more_damage = stats->damage_multipliers[0] * moreDamageMult,
        .effective_tags = effTag,
        .source_skill_id = kSkillId
      };
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
      registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);

      auto &sentinel = registry.emplace<OrbitingSentinelComponent>(proj_ent);
      sentinel.anchor_entity = owner;
      sentinel.skill_id = kSkillId;
      sentinel.count = 1;
      sentinel.orbit_radius = orbitRad;
      sentinel.angular_velocity = 240.0f;
      sentinel.attack_scan_radius = baseRadius * 1.2f;
      sentinel.attack_interval = orbitTick;
      // 哨兵伤害乘子同样叠加玩家全局乘区，保持与 payload_context 一致的增伤语义
      sentinel.damage_mult = orbitDmgMult * stats->damage_multipliers[0] * moreDamageMult;

      // 注意：哨兵周期攻击的 DamageRequest.attacker 锚定为玩家本体
      // (OrbitingSentinelDeliverySystem)，DoHit 的 255 意念回流标记校验挂在
      // 投射物实体上，对哨兵命中不适用——星环护体的命中不参与 255 roll

      if (isCold) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Cold, 1.0f, ModifierType::Convert});
      } else if (isLightning) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});
      }
      return;
    }

    // 投射物波发射函数
    int totalCount = (profile ? (profile->projectile_count > 0 ? profile->projectile_count : 1) : (1 + getPts(RendingWaveNodes::MultiWave)));
    float spread = 0.4f + (totalCount * 0.05f);
    float startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f;
    float angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;

    // fromIntentCast: 是否源自「消耗剑意的裂空斩」施放 (GDD §3.2 行214)。
    // 255 意念回流的 roll 前提即该标记；主波按 consumedIntent 传入，
    // 254 回响斩衍生波与普通施放传 false，命中不触发回剑意
    auto spawnOneWave = [&](Vector2 dir, float scaleEffectiveness = 1.0f, bool isBackward = false,
                            bool fromIntentCast = false) {
      (void)isBackward;
      auto proj_ent = registry.create();
      registry.emplace<LocalLevelTag>(proj_ent);
      registry.emplace<Position>(proj_ent, pos->x + dir.x * (baseRadius * 0.6f), pos->y + dir.y * (baseRadius * 0.6f));
      registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed, dir.y * baseSpeed);
      auto &proj = registry.emplace<Projectile>(proj_ent);
      proj.owner = owner;
      proj.cast_id = exec.cast_id;
      proj.speed = baseSpeed;
      proj.radius = baseRadius;
      proj.snapshot = *stats;
      if (link.consumed_any && !proj.snapshot.damage_multipliers.empty()) {
        proj.snapshot.damage_multipliers.front() *= link.damage_multiplier;
      }
      if (isObliteration) {
        // 253 湮灭波：以 Projectile::ignore_resist 显式标志替代旧
        // "snapshot.armor_pen += 500" 哨兵值；消费侧 (DamageMitigationService)
        // 读取该标志将有效护甲减半 (50% 无视，比例数据驱动 mech 253.physical_ignore_res_pct)
        proj.ignore_resist = true;
      }

      // 击中/死亡机制分支
      if (isScatter) {
        // 213 万剑归宗-残篇: 命中引爆散落 8 道微型穿刺剑气
        proj.pierce = false;
        proj.on_death = Projectile::OnDeathBehavior::Explode;
        proj.explode_count = 8;
        proj.lifeTime = maxRange / baseSpeed;
      } else if (isSplit) {
        // 211 碎裂之刃: 首击或最大距离分裂成 3 道较小追踪剑气
        proj.pierce = false;
        proj.on_death = Projectile::OnDeathBehavior::Split;
        proj.split_count = 3;
        // 分裂参数自 skill_mechanics.json (节点211) 接线：施法期读一次写入父
        // 投射物，SpawnSplitProjectiles 克隆父组件并按 parent.split_* 计算子弹
        // 伤害/速度，分裂事件路径无字符串查找，也不再依赖 Projectile.hpp 默认值巧合
        const float splitDamageBase = mech.GetFloat(kSkillId, RendingWaveNodes::Fracture, "split_damage_mult", 0.5f);
        const float splitSpeedBase = mech.GetFloat(kSkillId, RendingWaveNodes::Fracture, "split_speed_mult", 0.8f);
        proj.split_damage_mult = splitDamageBase;
        proj.split_speed_mult = splitSpeedBase;
        proj.lifeTime = maxRange / baseSpeed;
        registry.emplace<HomingTag>(proj_ent);
        auto &seeker = registry.emplace<SeekerComponent>(proj_ent);
        if (pts_212 > 0) {
          const float homingAnglePct = mech.GetFloat(kSkillId, RendingWaveNodes::ChainReaction, "homing_angle_pct_per_point", 15.0f);
          const float homingSpeedPct = mech.GetFloat(kSkillId, RendingWaveNodes::ChainReaction, "homing_speed_pct_per_point", 10.0f);
          seeker.turn_rate = 5.0f * (1.0f + (homingAnglePct / 100.0f) * pts_212);
          proj.split_speed_mult = splitSpeedBase * (1.0f + (homingSpeedPct / 100.0f) * pts_212);
        }
      } else if (isTimeLock) {
        // 234 时空停滞: 最远端原地停滞旋转 2s 剑气风暴，不折返
        proj.pierce = true;
        proj.pierceCount = 99;
        proj.max_pierce = Projectile::kUnlimitedPiercing;
        proj.on_death = Projectile::OnDeathBehavior::Hover;
        proj.hover_duration = mech.GetFloat(kSkillId, RendingWaveNodes::TimeLock, "timelock_duration", 2.0f);
        proj.hover_tick_rate = mech.GetFloat(kSkillId, RendingWaveNodes::TimeLock, "timelock_tick_rate", 0.2f);
        const float hlDmgMult = mech.GetFloat(kSkillId, RendingWaveNodes::TimeLock, "timelock_damage_mult", 0.5f);
        // 悬停危区伤害 = 20 * hover_damage_mult，不读 payload more_damage，
        // 故全局乘区与节点 more 须并入该倍率，避免 234 风暴丢失全局增伤
        proj.hover_damage_mult = stats->damage_multipliers[0] * moreDamageMult * hlDmgMult * scaleEffectiveness;
        proj.lifeTime = maxRange / baseSpeed;
        if (inSwordStep && pts_235 > 0) {
          proj.radius *= (1.0f + 0.15f * static_cast<float>(pts_235));
        }
      } else {
        // 普通穿透波刃
        proj.pierce = true;
        proj.pierceCount = 99;
        proj.max_pierce = Projectile::kUnlimitedPiercing;
        proj.lifeTime = isBoomerang ? 3.5f : (maxRange / baseSpeed);
      }

      proj.payload_context = {
        .base_damage_min = stats->min_weapon_damage,
        .base_damage_max = stats->max_weapon_damage,
        .crit_chance = stats->crit_chance + extraCritChance,
        .crit_multiplier = stats->crit_damage,
        // more_damage 须含玩家全局乘区 (与 SkillSystem 标准路径一致)，
        // 叠加节点 more (253 湮灭波等) 与局部效力系数，否则波刃伤害丢失全局增伤
        .more_damage = stats->damage_multipliers[0] * moreDamageMult * scaleEffectiveness,
        .effective_tags = effTag,
        .source_skill_id = kSkillId
      };
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
      registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);

      // 255 意念回流前提：消耗剑意施放的主波打施法标记，DoHit 校验后才 roll；
      // 分裂/引爆子投射物不克隆该标记 (ProjectileSystem 白名单边界)，其命中不触发 255
      if (fromIntentCast) {
        registry.emplace<IntentConsumedCastTag>(proj_ent);
      }

      // 元素转换修饰器注入实体
      if (isCold) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Cold, 1.0f, ModifierType::Convert});
      } else if (isLightning) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});
      } else if (attunement != Tag::None) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent).damage_modifiers.push_back(
          {Tag::Physical, attunement, 0.5f, ModifierType::Convert});
      } else if (auto *om = registry.try_get<SkillModifierComponent>(owner)) {
        registry.emplace_or_replace<SkillModifierComponent>(proj_ent, *om);
      }

      // 折返组件 (230 回旋劲 / 231 重叠打击 / 232 引力陷阱 / 233 深渊边缘)
      if (isBoomerang && !isTimeLock) {
        auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.owner = owner;
        bc.skill_id = kSkillId;
        bc.catch_by_owner = false;
        bc.returnTimer = maxRange / baseSpeed;
        bc.phase = BoomerangPhase::Outward;
        bc.returnSpeed = baseSpeed * 1.2f;

        const float returnPenalty = mech.GetFloat(kSkillId, RendingWaveNodes::Boomerang, "return_damage_penalty", 0.30f);
        const float doubleHitMore = mech.GetFloat(kSkillId, RendingWaveNodes::DoubleHit, "return_more_pct_per_point", 15.0f) / 100.0f;
        bc.returning_damage_mult = (1.0f - returnPenalty) * (1.0f + doubleHitMore * static_cast<float>(pts_231));

        if (profile && (profile->delivery.feature_flags & 16)) {
          float pullR = (profile->delivery.pull_radius > 0.0f) ? profile->delivery.pull_radius : (120.0f * (1.0f + 0.20f * static_cast<float>(pts_233)));
          if (pts_233 > 0 || (profile->delivery.feature_flags & 512)) {
            bc.stun_on_apex_end = true;
          }
          if (inSwordStep && pts_235 > 0) {
            pullR *= (1.0f + 0.15f * static_cast<float>(pts_235));
          }
          bc.pull_radius = pullR;
          bc.pull_strength = 250.0f * (inSwordStep && pts_235 > 0 ? (1.0f + 0.15f * static_cast<float>(pts_235)) : 1.0f);
        }
      }
    };

    // 多重剑气循环
    for (int i = 0; i < totalCount; ++i) {
      Vector2 dir = Vector2Rotate(baseDir, startAngle + i * angleStep);
      spawnOneWave(dir, 1.0f, false, consumedIntent > 0);
    }

    // 254 回响斩 (Trigger): 消耗剑意时向背后触发一道额外的裂空斩 (40% 效力，CD 2s)
    const bool hasEchoedSlash = profile ? ((profile->delivery.feature_flags & 65536) != 0) : (getPts(RendingWaveNodes::EchoedSlash) > 0);
    if (consumedIntent > 0 && hasEchoedSlash) {
      auto &state = registry.get_or_emplace<RendingWaveStateComponent>(owner);
      const float now = static_cast<float>(GetTime());
      const float icd = mech.GetFloat(kSkillId, RendingWaveNodes::EchoedSlash, "echo_icd", 2.0f);
      if (now - state.last_echo_time >= icd) {
        state.last_echo_time = now;
        const float echoEff = mech.GetFloat(kSkillId, RendingWaveNodes::EchoedSlash, "echo_effectiveness", 0.40f);
        Vector2 backDir = {-baseDir.x, -baseDir.y};
        // 回响斩为触发衍生攻击，自身不消耗剑意：fromIntentCast=false，
        // 其命中不触发 255 意念回流 (GDD §3.2 行214 仅限消耗剑意的裂空斩)
        spawnOneWave(backDir, echoEff, true, false);
      }
    }

    // 215 灵剑追击 (Synergy): 活跃灵剑决时 1 柄灵剑伴飞同行 (20% 效力)
    if (isPursuit && registry.all_of<BladeFormationComponent>(owner)) {
      const float pursuitEff = mech.GetFloat(kSkillId, RendingWaveNodes::SpiritPursuit, "pursuit_effectiveness", 0.20f);
      auto spirit_ent = registry.create();
      registry.emplace<LocalLevelTag>(spirit_ent);
      registry.emplace<SpiritSwordTag>(spirit_ent);
      registry.emplace<Position>(spirit_ent, pos->x + baseDir.x * (baseRadius * 0.6f), pos->y + baseDir.y * (baseRadius * 0.6f));
      registry.emplace<Velocity>(spirit_ent, baseDir.x * baseSpeed, baseDir.y * baseSpeed);
      auto &spProj = registry.emplace<Projectile>(spirit_ent);
      spProj.owner = owner;
      spProj.cast_id = exec.cast_id;
      spProj.speed = baseSpeed;
      spProj.lifeTime = maxRange / baseSpeed;
      spProj.radius = baseRadius * 0.6f;
      spProj.pierce = false;
      spProj.snapshot = *stats;
      spProj.payload_context = {
        .base_damage_min = stats->min_weapon_damage,
        .base_damage_max = stats->max_weapon_damage,
        .crit_chance = stats->crit_chance,
        .crit_multiplier = stats->crit_damage,
        // 灵剑伤害同样并入玩家全局乘区，避免 215 伴飞攻击丢失全局增伤
        .more_damage = stats->damage_multipliers[0] * moreDamageMult * pursuitEff,
        .effective_tags = effTag,
        .source_skill_id = kSkillId
      };
      registry.emplace<CombatStats>(spirit_ent, spProj.snapshot);
      registry.emplace<SkillComponent>(spirit_ent, kSkillId, owner);
      if (isCold) {
        registry.emplace_or_replace<SkillModifierComponent>(spirit_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Cold, 1.0f, ModifierType::Convert});
      } else if (isLightning) {
        registry.emplace_or_replace<SkillModifierComponent>(spirit_ent).damage_modifiers.push_back(
          {Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});
      }
    }
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity victim, Tag element_tag, bool is_crit) {
    entt::entity actualAttacker = attacker;
    if (const auto *summon = reg.try_get<SummonComponent>(attacker); summon && reg.valid(summon->owner)) {
      actualAttacker = summon->owner;
    }

    auto getPts = [&](uint32_t nid) -> int {
      if (reg.all_of<ActiveSkillsComponent>(actualAttacker)) {
        for (const auto &spec : reg.get<ActiveSkillsComponent>(actualAttacker).specialized_slots) {
          if (spec.skill_id == kSkillId) {
            return ReadPoints(spec, nid);
          }
        }
      }
      return 0;
    };

    const auto &mech = data::SkillMechanicsRegistry::Get();
    const int pts_203 = getPts(RendingWaveNodes::QiBurst);
    const float ailmentScale = 1.0f + 0.15f * static_cast<float>(pts_203);

    // 250 剑气烙印: 25%...100% 几率施加剑气烙印 (受暴伤+4%, max 5)
    const int pts_250 = getPts(RendingWaveNodes::QiBrand);
    if (pts_250 > 0) {
      const float chance = mech.GetFloat(kSkillId, RendingWaveNodes::QiBrand, "brand_chance_pct_per_point", 25.0f) * pts_250;
      if (GetRandomValue(1, 100) <= static_cast<int>(chance)) {
        auto &vicEffects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        BuffEffect brand{
          .id = "QiBrand",
          .name = "Qi Brand",
          .type = BuffType::DefenseDown,
          // 数值类别：伤害管线热路径按 BuffKind 查找烙印层数 (GetByKind)，
          // id 字符串仅保留给序列化与日志边界
          .kind = BuffKind::QiBrand,
          .duration = mech.GetFloat(kSkillId, RendingWaveNodes::QiBrand, "brand_duration", 4.0f),
          .remaining = mech.GetFloat(kSkillId, RendingWaveNodes::QiBrand, "brand_duration", 4.0f),
          .stacks = 1,
          .max_stacks = static_cast<int>(mech.GetFloat(kSkillId, RendingWaveNodes::QiBrand, "brand_max_stacks", 5.0f)),
          .is_debuff = true
        };
        brand.modifiers.push_back({
          .value = mech.GetFloat(kSkillId, RendingWaveNodes::QiBrand, "brand_crit_dmg_pct", 4.0f),
          .type = StatType::CritDamage,
          .mode = ModifierMode::PercentAdd
        });
        vicEffects.AddOrRefresh(brand);
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 255 意念回流 (GDD §3.2 行214)：仅「消耗剑意的裂空斩」每命中一名敌人才
    // 有几率回 1 层剑意。DoCast 消耗剑意时对主波打 IntentConsumedCastTag 标记；
    // 普通施放与 254 回响斩衍生波不带标记，不触发回剑意。214 哨兵攻击的
    // attacker 为玩家锚点，天然不满足标记，同样不触发。
    const int pts_255 = getPts(RendingWaveNodes::IntentRecovery);
    if (pts_255 > 0 && reg.all_of<IntentConsumedCastTag>(attacker)) {
      const float chance = mech.GetFloat(kSkillId, RendingWaveNodes::IntentRecovery, "intent_recovery_chance_pct_per_point", 10.0f) * pts_255;
      if (GetRandomValue(1, 100) <= static_cast<int>(chance)) {
        SkillSystem::GainSwordIntent(reg, actualAttacker, 1, kSkillId);
      }
    }

    // 235 御剑引力: 御剑步状态下折返几率施加护甲击碎
    const int pts_235 = getPts(RendingWaveNodes::SwordStepGravity);
    if (pts_235 > 0 && reg.any_of<PhaseTag>(actualAttacker)) {
      int rollChance = 50 * pts_235;
      int stacks = rollChance / 100;
      if (GetRandomValue(1, 100) <= (rollChance % 100)) {
        stacks += 1;
      }
      if (stacks > 0) {
        // 护甲击碎为跨技能共享的减益（BuffId::ArmorShred）。数值口径：
        // AttributePipeline 直接累加 modifiers 且不乘 .stacks，故 modifier
        // 承载完整降甲量 (-10×层数)，.stacks 仅作展示并同步为层数。
        BuffEffect shred{
          .id = std::string(BuffIdToString(BuffId::ArmorShred)),
          .name = "Armor Shred",
          .type = BuffType::DefenseDown,
          .duration = 4.0f,
          .remaining = 4.0f,
          .stacks = stacks,
          .max_stacks = stacks,
          .is_debuff = true
        };
        shred.modifiers.push_back({
          .value = -10.0f * static_cast<float>(stacks),
          .type = StatType::Armor,
          .mode = ModifierMode::Flat
        });
        reg.get_or_emplace<ActiveEffectsComponent>(victim).AddOrRefresh(shred);
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 270 霜寒之刃: 命中 100% 寒冷，满血敌人强制冻结 1s
    const int pts_270 = getPts(RendingWaveNodes::FrostForm);
    const bool isCold = HasTag(element_tag, Tag::Cold) || (pts_270 > 0);
    if (isCold) {
      systems::AilmentApplyRequest chillReq{
        .ailment = AilmentType::Chill,
        .source = actualAttacker,
        .magnitude = 20.0f * ailmentScale,
        .duration = 3.0f,
        .stacks = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, chillReq);

      const auto *vicStats = reg.try_get<CombatStats>(victim);
      if (vicStats && vicStats->health >= vicStats->max_health - 0.001f) {
        systems::AilmentApplyRequest freezeReq{
          .ailment = AilmentType::Freeze,
          .source = actualAttacker,
          .duration = mech.GetFloat(kSkillId, RendingWaveNodes::FrostForm, "freeze_duration", 1.0f),
          .stacks = 1
        };
        (void)systems::AilmentApplier::Apply(reg, victim, freezeReq);
      }
    }

    // 271 冰晶碎裂: 命中冻结目标触发冰爆
    const int pts_271 = getPts(RendingWaveNodes::ShatterCascade);
    if (pts_271 > 0 && isCold) {
      bool victimFrozen = false;
      if (auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
        for (const auto &b : fx->effects) {
          // 冻结判定按 BuffType 枚举比较：AilmentEngine 托管冻结与手工冻结
          // (如流云刺 172) 的 legacy 类型均为 BuffType::Freeze，字符串 id
          // 比较 ("Frozen") 在热路径违规且冗余
          if (b.type == BuffType::Freeze) {
            victimFrozen = true;
            break;
          }
        }
      }
      if (victimFrozen && reg.all_of<Position>(victim)) {
        const auto &vpos = reg.get<Position>(victim);
        const float shatterRadius = mech.GetFloat(kSkillId, RendingWaveNodes::ShatterCascade, "shatter_radius", 200.0f);
        const float shatterPct = mech.GetFloat(kSkillId, RendingWaveNodes::ShatterCascade, "shatter_dmg_pct_per_point", 20.0f) * pts_271 / 100.0f;
        auto enemyView = reg.view<EnemyTag, Position, CombatStats>();
        for (auto enemyEnt : enemyView) {
          if (enemyEnt == victim || reg.any_of<KilledTag>(enemyEnt)) continue;
          const auto &epos = enemyView.get<Position>(enemyEnt);
          float dist = Vector2Distance({vpos.x, vpos.y}, {epos.x, epos.y});
          if (dist <= shatterRadius) {
            auto *attStats = reg.try_get<CombatStats>(actualAttacker);
            float baseDmg = attStats ? (attStats->min_weapon_damage + attStats->max_weapon_damage) * 0.5f : 20.0f;
            DamagePool pool;
            pool.Add(Tag::Cold, baseDmg * shatterPct);
            DamageRequest req;
            req.origin = DamageOrigin::SecondaryProc;
            req.attacker = actualAttacker;
            req.defender = enemyEnt;
            req.skill_id = kSkillId;
            req.base_pool = pool;
            // 二次命中标记：冰爆为衍生命中 (直连 ResolveDamage，trigger_depth 恒 0)，
            // hitFunc 消费侧以该标记阻断二次命中再次驱动行为层 DoHit (主代理统一接线)
            req.additional_tags = Tag::Area | Tag::Hit | Tag::SecondaryHit;
            (void)ResolveDamage(reg, req, actualAttacker);

            systems::AilmentApplyRequest coldReq{
              .ailment = AilmentType::Chill,
              .source = actualAttacker,
              .magnitude = 20.0f * ailmentScale,
              .duration = 3.0f,
              .stacks = 1
            };
            (void)systems::AilmentApplier::Apply(reg, enemyEnt, coldReq);
          }
        }
      }
    }

    // 272 雷光: 暴击时感电
    const int pts_272 = getPts(RendingWaveNodes::LightningForm);
    const bool isLightning = HasTag(element_tag, Tag::Lightning) || (pts_272 > 0);
    if (isLightning && is_crit) {
      systems::AilmentApplyRequest shockReq{
        .ailment = AilmentType::Shock,
        .source = actualAttacker,
        .magnitude = 15.0f * ailmentScale,
        .duration = 4.0f,
        .stacks = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, shockReq);
    }

    // 273 感电传导: 命中感电目标连锁闪电
    const int pts_273 = getPts(RendingWaveNodes::StaticConduction);
    if (pts_273 > 0 && isLightning) {
      bool victimShocked = false;
      if (auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
        for (const auto &b : fx->effects) {
          // 感电判定按 BuffType 枚举比较：托管感电的 legacy 类型即
          // BuffType::Shock，删除热路径字符串 find
          if (b.type == BuffType::Shock) {
            victimShocked = true;
            break;
          }
        }
      }
      if (victimShocked && reg.all_of<Position>(victim)) {
        const auto &vpos = reg.get<Position>(victim);
        const float chainRadius = mech.GetFloat(kSkillId, RendingWaveNodes::StaticConduction, "chain_radius", 300.0f);
        // 连锁伤害比例自 skill_mechanics.json (节点273 chain_damage_pct) 接线，
        // 替代硬编码 0.30f，后续数值调参直接生效
        const float chainDmgPct = mech.GetFloat(kSkillId, RendingWaveNodes::StaticConduction, "chain_damage_pct", 30.0f) / 100.0f;
        const int maxTargets = std::min(3, pts_273);
        int chained = 0;
        auto enemyView = reg.view<EnemyTag, Position, CombatStats>();
        for (auto enemyEnt : enemyView) {
          if (enemyEnt == victim || reg.any_of<KilledTag>(enemyEnt)) continue;
          const auto &epos = enemyView.get<Position>(enemyEnt);
          float dist = Vector2Distance({vpos.x, vpos.y}, {epos.x, epos.y});
          if (dist <= chainRadius) {
            auto *attStats = reg.try_get<CombatStats>(actualAttacker);
            float baseDmg = attStats ? (attStats->min_weapon_damage + attStats->max_weapon_damage) * 0.5f : 20.0f;
            DamagePool pool;
            pool.Add(Tag::Lightning, baseDmg * chainDmgPct);
            DamageRequest req;
            req.origin = DamageOrigin::SecondaryProc;
            req.attacker = actualAttacker;
            req.defender = enemyEnt;
            req.skill_id = kSkillId;
            req.base_pool = pool;
            // 二次命中标记：连锁为衍生命中 (直连 ResolveDamage，trigger_depth 恒 0)，
            // hitFunc 消费侧以该标记阻断二次命中再次驱动行为层 DoHit (主代理统一接线)
            req.additional_tags = Tag::Hit | Tag::SecondaryHit;
            (void)ResolveDamage(reg, req, actualAttacker);
            chained++;
            if (chained >= maxTargets) break;
          }
        }
      }
    }

    // 275 异常扩散: 击杀带异常敌人时传染周围并回复法力
    const int pts_275 = getPts(RendingWaveNodes::Proliferation);
    if (pts_275 > 0) {
      const auto *vicStats = reg.try_get<CombatStats>(victim);
      const bool isKilled = reg.any_of<KilledTag>(victim) || (vicStats && vicStats->health <= 0.0f);
      if (isKilled) {
        AilmentType spreadAilment = AilmentType::None;
        if (auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
          for (const auto &b : fx->effects) {
            // 异常家族判定按 BuffType 枚举比较：AilmentAdapter 将托管寒冷映射
            // 到 BuffType::SpeedDown、冻结到 BuffType::Freeze、感电到
            // BuffType::Shock，删除热路径字符串 find
            if (b.type == BuffType::SpeedDown || b.type == BuffType::Freeze) {
              spreadAilment = AilmentType::Chill;
              break;
            }
            if (b.type == BuffType::Shock) {
              spreadAilment = AilmentType::Shock;
              break;
            }
          }
        }
        if (spreadAilment != AilmentType::None && reg.all_of<Position>(victim)) {
          const auto &vpos = reg.get<Position>(victim);
          const float spreadRadius = mech.GetFloat(kSkillId, RendingWaveNodes::Proliferation, "proliferate_radius_per_point", 300.0f) * pts_275;
          const int maxSpread = static_cast<int>(mech.GetFloat(kSkillId, RendingWaveNodes::Proliferation, "proliferate_max_targets", 5.0f));
          int spreadCount = 0;
          auto enemyView = reg.view<EnemyTag, Position>();
          for (auto enemyEnt : enemyView) {
            if (enemyEnt == victim || reg.any_of<KilledTag>(enemyEnt)) continue;
            const auto &epos = enemyView.get<Position>(enemyEnt);
            float dist = Vector2Distance({vpos.x, vpos.y}, {epos.x, epos.y});
            if (dist <= spreadRadius) {
              systems::AilmentApplyRequest spreadReq{
                .ailment = spreadAilment,
                .source = actualAttacker,
                .magnitude = (spreadAilment == AilmentType::Chill ? 20.0f : 15.0f) * 1.5f * ailmentScale,
                .duration = 3.0f,
                .stacks = 1
              };
              (void)systems::AilmentApplier::Apply(reg, enemyEnt, spreadReq);
              spreadCount++;
              if (spreadCount >= maxSpread) break;
            }
          }
          if (spreadCount > 0) {
            const float manaGain = mech.GetFloat(kSkillId, RendingWaveNodes::Proliferation, "proliferate_mana_gain", 2.0f);
            if (auto *attStats = reg.try_get<CombatStats>(actualAttacker)) {
              attStats->mana = std::min(attStats->max_mana, attStats->mana + manaGain * spreadCount);
            }
          }
        }
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(RendingWave)
void RegisterRendingWave() {}

} // namespace NoMoreDay::skills
