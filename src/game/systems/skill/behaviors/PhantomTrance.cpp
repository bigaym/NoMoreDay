/**
 * @file PhantomTrance.cpp
 * @brief 绝影绝剑 (ID 9) - 绝影形态与附魔窗口行为实现
 *
 * 施放进入绝影形态，形态期间按已点专精节点提供形态增益、逆脉锁血、元素脉冲、
 * 死亡螺旋与孤注一掷等持续效果；形态结束后结算爆发、冷却返还、重生/嗜血回复，
 * 并开启附魔窗口（元素尾附与意念穿透）。
 */
#include "PhantomTrance.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace NoMoreDay::skills {

namespace {

// 技能9全部专精节点，用于无烘焙档案时按 active_nodes 合成专精后兜底烘焙。
constexpr std::array<uint32_t, 29> kPhantomTranceNodes = {
    902, 913, 914, 934, 935, 954, 955, 972, 973, 974, 975, 976, 977, 978, 979,
    980, 981, 982, 983, 984, 985, 986, 987, 988, 989, 990, 991, 992, 993};

// 六系伤害类型，用于逆脉增伤与穿行诅咒减伤。
constexpr std::array<StatType, 6> kDamageStatTypes = {
    StatType::PhysicalDamage, StatType::FireDamage, StatType::ColdDamage,
    StatType::LightningDamage, StatType::PoisonDamage, StatType::ShadowDamage};

constexpr float kSpiralSeekRange = 900.0f;    // 死亡螺旋飞剑索敌范围
constexpr float kSpiralSpeed = 750.0f;        // 死亡螺旋飞剑速度
constexpr float kSpiralLifeTime = 0.6f;       // 死亡螺旋飞剑存活时长
constexpr float kSpiralRadius = 14.0f;        // 死亡螺旋飞剑命中半径
constexpr float kPulseRadius = 160.0f;        // 元素脉冲/穿行半径
constexpr float kPassTickInterval = 0.5f;     // 穿行检测节拍
constexpr float kPassWeakenDuration = 3.0f;   // 穿行诅咒持续时间
constexpr float kFreezeDuration = 1.5f;       // 附魔冻结时长（策划近似值）
constexpr float kShockDuration = 4.0f;        // 附魔感电时长（策划近似值）
constexpr float kEnchantAilmentChance = 0.3f; // 附魔命中异常概率（策划近似值）
constexpr float kColdPulseMagnitude = 20.0f;  // 冰霜脉冲冰缓强度
constexpr float kColdPulseDuration = 3.0f;    // 冰霜脉冲冰缓时长
constexpr float kShockMagnitude = 15.0f;      // 雷盾脉冲感电强度

[[nodiscard]] const data::SkillMechanicsRegistry &Mech() {
  return data::SkillMechanicsRegistry::Get();
}

// 解析形态参数：优先烘焙档案；缺失时用 exec.active_nodes 合成专精后走同一烘焙路径，
// 保证未同步档案（测试/兜底）时仍可测且与生产数值单源。
[[nodiscard]] const PhantomTranceParams &ResolveParams(entt::registry &registry,
                                                       entt::entity owner,
                                                       SkillExecution &exec,
                                                       BakedSkillProfile &scratch) {
  if (const auto *profile = SkillSystem::GetBakedSkillProfile(
          registry, owner, PhantomTrance::kSkillId)) {
    return profile->delivery.trance;
  }
  SpecializedSkill synthesized;
  synthesized.skill_id = PhantomTrance::kSkillId;
  for (uint32_t node_id : kPhantomTranceNodes) {
    if (exec.active_nodes.test(node_id % 100)) {
      synthesized.allocated_points[node_id] = 1;
    }
  }
  SkillSpecializationBaker::Bake(registry, owner, PhantomTrance::kSkillId,
                                 &synthesized, scratch, nullptr);
  return scratch.delivery.trance;
}

[[nodiscard]] std::vector<entt::entity>
CollectEnemiesInRadius(entt::registry &registry, const Position &center,
                       float radius) {
  std::vector<entt::entity> result;
  const float r2 = radius * radius;
  auto view = registry.view<EnemyTag, Position>();
  for (auto entity : view) {
    const auto &p = view.get<Position>(entity);
    const float dx = p.x - center.x;
    const float dy = p.y - center.y;
    if (dx * dx + dy * dy > r2) {
      continue;
    }
    if (const auto *hp = registry.try_get<HealthComponent>(entity);
        hp && hp->current <= 0.0f) {
      continue;
    }
    result.push_back(entity);
  }
  return result;
}

[[nodiscard]] entt::entity FindNearestEnemy(entt::registry &registry,
                                            const Position &center, float range) {
  entt::entity best = entt::null;
  float best_d2 = range * range;
  auto view = registry.view<EnemyTag, Position>();
  for (auto entity : view) {
    const auto &p = view.get<Position>(entity);
    const float dx = p.x - center.x;
    const float dy = p.y - center.y;
    const float d2 = dx * dx + dy * dy;
    if (d2 >= best_d2) {
      continue;
    }
    if (const auto *hp = registry.try_get<HealthComponent>(entity);
        hp && hp->current <= 0.0f) {
      continue;
    }
    best_d2 = d2;
    best = entity;
  }
  return best;
}

void ApplyAilment(entt::registry &registry, entt::entity target,
                  entt::entity source, AilmentType type, float magnitude,
                  float duration) {
  systems::AilmentApplyRequest request;
  request.ailment = type;
  request.source = source;
  request.magnitude = magnitude;
  request.duration = duration;
  request.stacks = 1;
  (void)systems::AilmentApplier::Apply(registry, target, request);
}

// 构造形态增益 Buff：按已点节点裁剪 modifier。
// sword_step_active: 施放瞬间是否已处于御剑步，用于 988 御剑化影叠加形态闪避。
[[nodiscard]] BuffEffect BuildFormBuff(const PhantomTranceParams &params,
                                       entt::entity owner, float duration,
                                       bool sword_step_active) {
  BuffEffect form;
  form.id = std::string(BuffIdToString(BuffId::PhantomTranceForm));
  form.name = "PhantomTranceForm";
  form.type = BuffType::SpeedUp;
  form.duration = duration;
  form.remaining = duration;
  form.source = owner;
  form.source_skill_id = static_cast<int>(PhantomTrance::kSkillId);

  if (params.move_speed_pct > 0.0f) {
    form.modifiers.push_back({.value = params.move_speed_pct,
                              .type = StatType::MoveSpeed,
                              .mode = ModifierMode::PercentAdd});
  }
  // 闪避基数按百分点存储，需用 Flat 累加绝对值（PercentAdd 对 0 基数无效）。
  if (params.dodge_pct > 0.0f) {
    form.modifiers.push_back({.value = params.dodge_pct,
                              .type = StatType::DodgeChance,
                              .mode = ModifierMode::Flat});
  }
  // 988 御剑化影：施放入形态时已处于御剑步 → 叠加形态闪避。
  if (sword_step_active && params.sword_step_dodge_pct > 0.0f) {
    form.modifiers.push_back({.value = params.sword_step_dodge_pct,
                              .type = StatType::DodgeChance,
                              .mode = ModifierMode::Flat});
  }
  if (params.recovery_pct > 0.0f) {
    form.modifiers.push_back({.value = params.recovery_pct,
                              .type = StatType::CooldownReduction,
                              .mode = ModifierMode::Flat});
  }
  // 攻速/施法速度仅在逆脉形态下生效（与节点 934 的定位一致）。
  if (params.atk_cast_speed_pct > 0.0f && params.death_seal) {
    form.modifiers.push_back({.value = params.atk_cast_speed_pct,
                              .type = StatType::AttackSpeed,
                              .mode = ModifierMode::PercentAdd});
    form.modifiers.push_back({.value = params.atk_cast_speed_pct,
                              .type = StatType::CastSpeed,
                              .mode = ModifierMode::PercentAdd});
  }
  if (params.void_gift_dr_pct > 0.0f) {
    form.modifiers.push_back({.value = params.void_gift_dr_pct * 100.0f,
                              .type = StatType::GlobalDamageReduction,
                              .mode = ModifierMode::Flat});
  }
  // 973 过载护盾：转闪电时追加移速/攻速。
  if (params.overload_speed_pct > 0.0f) {
    form.modifiers.push_back({.value = params.overload_speed_pct * 100.0f,
                              .type = StatType::MoveSpeed,
                              .mode = ModifierMode::PercentAdd});
    form.modifiers.push_back({.value = params.overload_speed_pct * 100.0f,
                              .type = StatType::AttackSpeed,
                              .mode = ModifierMode::PercentAdd});
  }
  return form;
}

void ApplyDeathSeal(entt::registry &registry, entt::entity owner,
                    const CombatStats &stats, const PhantomTranceParams &params) {
  if (auto *hp = registry.try_get<HealthComponent>(owner)) {
    hp->max = stats.max_health * params.death_seal_hp_cap_pct;
    if (hp->current > hp->max) {
      hp->current = hp->max;
    }
  }

  BuffEffect seal;
  seal.id = std::string(BuffIdToString(BuffId::PhantomTranceDeathSeal));
  seal.name = "PhantomTranceDeathSeal";
  seal.type = BuffType::PowerBoost;
  seal.duration = params.duration_sec;
  seal.remaining = params.duration_sec;
  seal.source = owner;
  seal.source_skill_id = static_cast<int>(PhantomTrance::kSkillId);
  for (StatType type : kDamageStatTypes) {
    seal.modifiers.push_back({.value = params.death_seal_damage_more_pct,
                              .type = type,
                              .mode = ModifierMode::PercentMult});
  }
  registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(seal);
  registry.get_or_emplace<StatsDirty>(owner);
}

void ApplyVoidBody(entt::registry &registry, entt::entity owner,
                   const PhantomTranceParams &params) {
  if (auto *hp = registry.try_get<HealthComponent>(owner)) {
    hp->current *= (1.0f - params.void_body_hp_cost_pct);
  }
  BuffEffect stealth;
  stealth.id = std::string(BuffIdToString(BuffId::PhantomTranceStealth));
  stealth.name = "PhantomTranceStealth";
  stealth.type = BuffType::SpeedUp;
  stealth.duration = params.duration_sec;
  stealth.remaining = params.duration_sec;
  stealth.source = owner;
  stealth.source_skill_id = static_cast<int>(PhantomTrance::kSkillId);
  registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(stealth);
  // 不可选中由 PhantomTranceStealth 承载；PhaseTag 供物理穿透相位使用，
  // 其生命周期由 SkillSystem 的御剑步相位清理统一接管。
  registry.emplace_or_replace<PhaseTag>(owner);
}

void ApplyWard(entt::registry &registry, entt::entity owner,
               const PhantomTranceParams &params) {
  auto *stats = registry.try_get<CombatStats>(owner);
  if (!stats) {
    return;
  }
  stats->barrier += stats->max_health * params.ward_pct;
  stats->barrier_delay = Mech().GetFloat(PhantomTrance::kSkillId, 0, "ward_duration", 2.0f);
  registry.get_or_emplace<BarrierComponent>(owner);
  registry.get_or_emplace<StatsDirty>(owner);
}

void RegisterEchoRule(entt::registry &registry, entt::entity owner,
                      const PhantomTranceParams &params) {
  if (!params.echo_synergy) {
    return;
  }
  auto &triggers = registry.get_or_emplace<TriggerRuleComponent>(owner);
  triggers.RemoveRule(993);
  TriggerRule echo;
  echo.rule_id = 993;
  echo.listen_event = CombatEventType::OnSkillHit;
  echo.target_mode = TriggerTargetPolicy::Victim;
  echo.required_skill_id = 8;
  echo.cast_skill_id = 8;
  echo.base_chance = Mech().GetFloat(PhantomTrance::kSkillId, 0, "echo_chance", 0.4f);
  echo.use_proc_scaling = false;
  echo.internal_cooldown = Mech().GetFloat(PhantomTrance::kSkillId, 0, "echo_icd", 0.5f);
  echo.effectiveness =
      Mech().GetFloat(PhantomTrance::kSkillId, 0, "echo_effectiveness", 0.5f);
  triggers.AddRule(echo);
}

// 983 死亡螺旋：向最近敌人发射 death_spiral_count 柄高穿透飞剑。
void FireSpiralSwords(entt::registry &registry, entt::entity owner,
                      const PhantomTranceComponent &pt, float damage, int count) {
  const auto *owner_pos = registry.try_get<Position>(owner);
  if (!owner_pos || count <= 0) {
    return;
  }
  const entt::entity target = FindNearestEnemy(registry, *owner_pos, kSpiralSeekRange);
  const auto *target_pos = registry.try_get<Position>(target);
  if (!target_pos) {
    return;
  }
  Vector2 dir = Vector2Normalize(
      Vector2Subtract({target_pos->x, target_pos->y}, {owner_pos->x, owner_pos->y}));
  if (Vector2Length(dir) <= 0.0001f) {
    return;
  }
  const auto *stats = registry.try_get<CombatStats>(owner);
  for (int i = 0; i < count; ++i) {
    // 少量扇形偏置，避免多柄飞剑完全重叠。
    const float offset = static_cast<float>(i - count / 2) * 0.12f;
    const Vector2 spread = Vector2Rotate(dir, offset);
    auto proj_ent = registry.create();
    registry.emplace<LocalLevelTag>(proj_ent);
    registry.emplace<Position>(proj_ent, owner_pos->x, owner_pos->y);
    registry.emplace<Velocity>(proj_ent, spread.x * kSpiralSpeed, spread.y * kSpiralSpeed);
    auto &proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = owner;
    proj.cast_id = pt.cast_id;
    proj.speed = kSpiralSpeed;
    proj.lifeTime = kSpiralLifeTime;
    proj.radius = kSpiralRadius;
    proj.pierce = true;
    proj.pierceCount = 99;
    proj.max_pierce = 99;
    if (stats) {
      proj.snapshot = *stats;
      proj.payload_context = {
          .base_damage_min = damage,
          .base_damage_max = damage,
          .crit_chance = stats->crit_chance / 100.0f,
          .crit_multiplier = stats->crit_damage,
          .more_damage = 1.0f,
          .effective_tags = Tag::Physical,
          .source_skill_id = PhantomTrance::kSkillId};
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    }
    registry.emplace<SkillComponent>(proj_ent, PhantomTrance::kSkillId, owner);
  }
}

// 982 孤注一掷：按缺失生命刷新暴伤 Buff。
void RefreshLastStand(entt::registry &registry, entt::entity owner,
                      PhantomTranceComponent &pt, const CombatStats &stats,
                      const HealthComponent &hp) {
  if (hp.max <= 0.0f) {
    return;
  }
  const float missing_pct = (1.0f - hp.current / hp.max) * 100.0f;
  const float value = missing_pct * pt.params.last_stand_crit_pct;

  BuffEffect last;
  last.id = std::string(BuffIdToString(BuffId::PhantomTranceLastStand));
  last.name = "PhantomTranceLastStand";
  last.type = BuffType::CritDamageUp;
  last.duration = 0.5f;
  last.remaining = 0.5f;
  last.source = owner;
  last.source_skill_id = static_cast<int>(PhantomTrance::kSkillId);
  last.modifiers.push_back({.value = value,
                            .type = StatType::CritDamage,
                            .mode = ModifierMode::PercentAdd});
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  effects.AddOrRefresh(last);
  if (BuffEffect *applied = effects.Get(BuffId::PhantomTranceLastStand); applied) {
    if (!applied->modifiers.empty()) {
      applied->modifiers[0].value = value;
    } else {
      applied->modifiers.push_back({.value = value,
                                    .type = StatType::CritDamage,
                                    .mode = ModifierMode::PercentAdd});
    }
  }
  pt.last_stand_buff_value = value;
  registry.get_or_emplace<StatsDirty>(owner);
}

// 穿行诅咒：对半径内敌人施加六系减伤（负 PercentMult）。
void ApplyPassWeaken(entt::registry &registry, entt::entity owner,
                     const PhantomTranceComponent &pt) {
  const auto *center = registry.try_get<Position>(owner);
  if (!center) {
    return;
  }
  for (entt::entity enemy : CollectEnemiesInRadius(registry, *center, kPulseRadius)) {
    BuffEffect weaken;
    weaken.id = std::string(BuffIdToString(BuffId::PhantomTranceWeaken));
    weaken.name = "PhantomTranceWeaken";
    weaken.type = BuffType::AttackDown;
    weaken.is_debuff = true;
    weaken.duration = kPassWeakenDuration;
    weaken.remaining = kPassWeakenDuration;
    weaken.source = owner;
    weaken.source_skill_id = static_cast<int>(PhantomTrance::kSkillId);
    for (StatType type : kDamageStatTypes) {
      weaken.modifiers.push_back({.value = -pt.params.weaken_on_pass_pct * 100.0f,
                                  .type = type,
                                  .mode = ModifierMode::PercentMult});
    }
    registry.get_or_emplace<ActiveEffectsComponent>(enemy).AddOrRefresh(weaken);
    registry.get_or_emplace<StatsDirty>(enemy);
  }
}

// 989 冰霜脉冲：对半径内敌人施加冰缓。
void ApplyFrostPulse(entt::registry &registry, entt::entity owner) {
  const auto *center = registry.try_get<Position>(owner);
  if (!center) {
    return;
  }
  for (entt::entity enemy : CollectEnemiesInRadius(registry, *center, kPulseRadius)) {
    ApplyAilment(registry, enemy, owner, AilmentType::Chill, kColdPulseMagnitude,
                 kColdPulseDuration);
  }
}

// 972 雷盾脉冲：对半径内敌人施加感电并造成一次低倍闪电伤害。
void ApplyStormPulse(entt::registry &registry, entt::entity owner,
                     const CombatStats &stats, PhantomTranceComponent &pt) {
  const auto *center = registry.try_get<Position>(owner);
  if (!center) {
    return;
  }
  const float base_damage =
      (stats.min_weapon_damage + stats.max_weapon_damage) * 0.5f *
      Mech().GetFloat(PhantomTrance::kSkillId, 0, "storm_pulse_mult", 0.4f);
  for (entt::entity enemy : CollectEnemiesInRadius(registry, *center, kPulseRadius)) {
    ApplyAilment(registry, enemy, owner, AilmentType::Shock, kShockMagnitude, kShockDuration);
    if (base_damage <= 0.0f) {
      continue;
    }
    DamagePool pool;
    pool.Add(Tag::Lightning, base_damage);
    DamageRequest request;
    request.attacker = owner;
    request.defender = enemy;
    request.skill_id = PhantomTrance::kSkillId;
    request.base_pool = pool;
    request.additional_tags = Tag::Area | Tag::Hit;
    const DamageExecutionResult result = ResolveDamage(registry, request, enemy);
    // 984 嗜血结算依赖本形态造成的直接伤害累计。
    pt.damage_dealt_accum += result.final_applied_damage;
  }
}

// 991 意念穿透：附魔窗口内每帧刷新敌方元素减抗（技能9专属）。
void UpdateEnchantPenetration(entt::registry &registry, entt::entity owner,
                              PhantomTranceComponent &pt) {
  if (pt.enchant_tag == Tag::None || pt.params.enchant_pen_per_intent_pct <= 0.0f) {
    return;
  }
  StatType resist_type = StatType::Count;
  if (pt.enchant_tag == Tag::Cold) {
    resist_type = StatType::ResistCold;
  } else if (pt.enchant_tag == Tag::Lightning) {
    resist_type = StatType::ResistLightning;
  } else {
    return;
  }

  int stacks = 0;
  if (const auto *blade = registry.try_get<BladeResourceComponent>(owner)) {
    stacks = blade->current;
  } else if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
    stacks = intent->stacks;
  }
  const float pen_points =
      std::min(pt.params.enchant_pen_cap_pct,
               pt.params.enchant_pen_per_intent_pct * 100.0f * static_cast<float>(stacks));
  if (pen_points <= 0.0f) {
    return;
  }

  const auto *center = registry.try_get<Position>(owner);
  if (!center) {
    return;
  }
  for (entt::entity enemy :
       CollectEnemiesInRadius(registry, *center, kPulseRadius)) {
    BuffEffect shred;
    shred.id = std::string(BuffIdToString(BuffId::PhantomTranceEnchant));
    shred.name = "PhantomTranceEnchant";
    shred.type = BuffType::DefenseDown;
    shred.is_debuff = true;
    shred.duration = 1.0f;
    shred.remaining = 1.0f;
    shred.source = owner;
    // 991 契约为 GlobalWhileBuffActive：减抗作用于附魔窗口内全部伤害，
    // 而非仅技能9自身伤害，故 source_skill_id 置 0 (无技能归属)。
    shred.source_skill_id = 0;
    shred.modifiers.push_back(
        {.value = -pen_points, .type = resist_type, .mode = ModifierMode::Flat});
    registry.get_or_emplace<ActiveEffectsComponent>(enemy).AddOrRefresh(shred);
    registry.get_or_emplace<StatsDirty>(enemy);
  }
}

// 结束爆发：以 transmuter 元素对半径内敌人造成一次范围伤害。
void RunBurst(entt::registry &registry, entt::entity owner,
              const PhantomTranceParams &params, PhantomTranceComponent &pt) {
  const auto *center = registry.try_get<Position>(owner);
  if (!center) {
    return;
  }
  const auto *skill = SkillRegistry::Get().GetSkill(PhantomTrance::kSkillId);
  const float burst_radius =
      (skill ? skill->GetParam("burst_radius", 160.0f) : 160.0f) *
      params.burst_damage_mult;
  const auto *stats = registry.try_get<CombatStats>(owner);
  if (!stats) {
    return;
  }
  const float weapon_mult =
      Mech().GetFloat(PhantomTrance::kSkillId, 0, "burst_weapon_mult", 1.5f) *
      params.burst_damage_mult;
  const float base_damage =
      (stats->min_weapon_damage + stats->max_weapon_damage) * 0.5f * weapon_mult;

  Tag element = Tag::Physical;
  if (params.transmuter_tag == Tag::Cold) {
    element = Tag::Cold;
  } else if (params.transmuter_tag == Tag::Lightning) {
    element = Tag::Lightning;
  }

  for (entt::entity enemy : CollectEnemiesInRadius(registry, *center, burst_radius)) {
    DamagePool pool;
    pool.Add(element, base_damage);
    DamageRequest request;
    request.attacker = owner;
    request.defender = enemy;
    request.skill_id = PhantomTrance::kSkillId;
    request.base_pool = pool;
    request.additional_tags = Tag::Area | Tag::Hit;
    const DamageExecutionResult result = ResolveDamage(registry, request, enemy);
    pt.damage_dealt_accum += result.final_applied_damage;

    if (element == Tag::Cold) {
      if (GetRandomValue(0, 1000) / 1000.0f < kEnchantAilmentChance) {
        ApplyAilment(registry, enemy, owner, AilmentType::Freeze, 1.0f, kFreezeDuration);
      }
    } else if (element == Tag::Lightning) {
      if (GetRandomValue(0, 1000) / 1000.0f < kEnchantAilmentChance) {
        ApplyAilment(registry, enemy, owner, AilmentType::Shock, kShockMagnitude,
                     kShockDuration);
      }
    }
  }
}

// 仅清除本形态写入的物理转元素尾附，避免误删其他来源的转换修饰。
void ClearEnchantGainExtra(entt::registry &registry, entt::entity owner,
                           Tag enchant_tag) {
  if (enchant_tag == Tag::None) {
    return;
  }
  if (auto *modifiers = registry.try_get<SkillModifierComponent>(owner)) {
    std::erase_if(modifiers->damage_modifiers,
                  [enchant_tag](const DamageModifier &mod) {
                    return mod.type == ModifierType::GainExtra &&
                           mod.source_tag == Tag::Physical &&
                           mod.target_tag == enchant_tag;
                  });
  }
}

// 形态结束结算：解除锁血、爆发、冷却返还、回复、开启附魔窗口并清理临时效果。
void ExecuteEndResolution(entt::registry &registry, entt::entity owner,
                          PhantomTranceComponent &pt) {
  pt.ending = true;
  const PhantomTranceParams &params = pt.params;
  auto *stats = registry.try_get<CombatStats>(owner);
  auto *hp = registry.try_get<HealthComponent>(owner);

  // a. 解除逆脉锁血，恢复上限交由 RegenerationSystem 下一帧同步。
  if (hp && stats) {
    hp->max = stats->max_health;
  }

  // b. 结束爆发。
  RunBurst(registry, owner, params, pt);

  // c. 954 时光逆流：返还其他技能槽冷却。
  if (params.time_reversal_sec > 0.0f) {
    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (auto &slot : active->slots) {
        if (slot.id != PhantomTrance::kSkillId && slot.id != 0 && slot.cooldown > 0.0f) {
          slot.cooldown = std::max(0.0f, slot.cooldown - params.time_reversal_sec);
        }
      }
    }
  }

  // d. 984 嗜血本能：按期间累计伤害回复，直接写生命绕过禁疗。
  // 结算后清零累计值，避免附魔窗口内组件驻留时被重复消费。
  if (hp && params.bloodthirst_pct > 0.0f && pt.damage_dealt_accum > 0.0f) {
    hp->current = std::min(hp->max, hp->current + pt.damage_dealt_accum * params.bloodthirst_pct);
    pt.damage_dealt_accum = 0.0f;
  }

  // e. 978 浴血重生：免死触发按已损失生命回复，否则按上限回复。
  if (hp && stats) {
    if (pt.lethal_triggered && params.rebirth_lost_pct > 0.0f) {
      const float lost = stats->max_health - hp->current;
      hp->current = std::min(hp->max, hp->current + lost * params.rebirth_lost_pct);
    } else if (!pt.lethal_triggered && params.rebirth_flat_pct > 0.0f) {
      hp->current = std::min(hp->max, hp->current + stats->max_health * params.rebirth_flat_pct);
    }
  }

  // f. transmuter 附魔窗口：写入元素尾附 GainExtra。
  if (params.transmuter_tag != Tag::None) {
    pt.enchant_tag = params.transmuter_tag;
    pt.enchant_remaining = Mech().GetFloat(PhantomTrance::kSkillId, 0, "enchant_duration", 4.0f);
    auto &modifiers = registry.get_or_emplace<SkillModifierComponent>(owner);
    ClearEnchantGainExtra(registry, owner, pt.enchant_tag);
    modifiers.damage_modifiers.push_back(
        {Tag::Physical, pt.enchant_tag,
         Mech().GetFloat(PhantomTrance::kSkillId, 0, "enchant_added_pct", 0.5f),
         ModifierType::GainExtra});
  }

  // g. 移除 993 规则与形态相关临时 Buff。
  if (auto *triggers = registry.try_get<TriggerRuleComponent>(owner)) {
    triggers->RemoveRule(993);
  }
  if (auto *effects = registry.try_get<ActiveEffectsComponent>(owner)) {
    effects->Remove(BuffId::PhantomTranceForm);
    effects->Remove(BuffId::PhantomTranceDeathSeal);
    effects->Remove(BuffId::PhantomTranceLastStand);
    effects->Remove(BuffId::PhantomTranceStealth);
  }
}

// 形态内逐拍效果：剑意、回蓝、死亡螺旋、元素脉冲、穿行诅咒、孤注一掷。
void TickFormEffects(entt::registry &registry, entt::entity owner,
                     PhantomTranceComponent &pt, float dt) {
  const PhantomTranceParams &params = pt.params;
  auto *stats = registry.try_get<CombatStats>(owner);
  auto *hp = registry.try_get<HealthComponent>(owner);

  // 987 意随神行：每秒获得剑意。
  if (params.intent_per_sec > 0) {
    pt.intent_tick += dt;
    while (pt.intent_tick >= 1.0f) {
      pt.intent_tick -= 1.0f;
      SkillSystem::GainSwordIntent(registry, owner, params.intent_per_sec,
                                   PhantomTrance::kSkillId);
    }
  }

  // 914 虚境馈赠：每秒回蓝。
  if (params.void_gift_mana_per_sec > 0.0f) {
    pt.mana_tick += dt;
    while (pt.mana_tick >= 1.0f) {
      pt.mana_tick -= 1.0f;
      if (auto *live_stats = registry.try_get<CombatStats>(owner)) {
        live_stats->mana =
            std::min(live_stats->max_mana, live_stats->mana + params.void_gift_mana_per_sec);
        registry.get_or_emplace<StatsDirty>(owner);
      }
    }
  }

  // 983 死亡螺旋：逆脉形态下每拍发射高穿透飞剑。
  if (IsDeathSealActive(pt) && params.death_spiral_count > 0 && stats && hp) {
    pt.spiral_tick += dt;
    const float interval =
        std::max(0.1f, Mech().GetFloat(PhantomTrance::kSkillId, 0, "spiral_interval", 0.5f));
    while (pt.spiral_tick >= interval) {
      pt.spiral_tick -= interval;
      const float damage = std::max(
          0.0f, (stats->max_health - hp->max) * params.death_spiral_damage_pct);
      FireSpiralSwords(registry, owner, pt, damage, params.death_spiral_count);
    }
  }

  // 989/972 元素脉冲：每秒一次。
  if (params.transmuter_tag == Tag::Cold || params.transmuter_tag == Tag::Lightning) {
    pt.pulse_tick += dt;
    while (pt.pulse_tick >= 1.0f) {
      pt.pulse_tick -= 1.0f;
      if (params.transmuter_tag == Tag::Cold) {
        ApplyFrostPulse(registry, owner);
      } else if (stats) {
        ApplyStormPulse(registry, owner, *stats, pt);
      }
    }
  }

  // 913 灵流穿透：穿行敌人获得虚弱。
  if (params.void_body && params.weaken_on_pass_pct > 0.0f) {
    pt.weaken_tick += dt;
    while (pt.weaken_tick >= kPassTickInterval) {
      pt.weaken_tick -= kPassTickInterval;
      ApplyPassWeaken(registry, owner, pt);
    }
  }

  // 982 孤注一掷：逆脉形态下按缺失生命刷新暴伤。
  if (IsDeathSealActive(pt) && params.last_stand_crit_pct > 0.0f && stats && hp) {
    RefreshLastStand(registry, owner, pt, *stats, *hp);
  }
}

} // namespace

void PhantomTrance::DoCast(entt::registry &registry, entt::entity owner,
                           SkillExecution &exec) {
  auto *pos = registry.try_get<Position>(owner);
  if (!pos) {
    return;
  }

  BakedSkillProfile scratch;
  const PhantomTranceParams &params = ResolveParams(registry, owner, exec, scratch);

  // 985 破空一闪：瞬移至光标。
  if (params.blink) {
    pos->x = exec.target_pos.x;
    pos->y = exec.target_pos.y;
  }

  // 生成形态组件并复制形态参数。
  auto &pt = registry.emplace_or_replace<PhantomTranceComponent>(owner);
  pt.owner = owner;
  pt.cast_id = exec.cast_id;
  pt.params = params;
  pt.duration = params.duration_sec;
  pt.remaining = params.duration_sec;
  pt.elapsed = 0.0f;
  pt.lethal_triggered = false;
  pt.damage_dealt_accum = 0.0f;
  pt.intent_tick = 0.0f;
  pt.mana_tick = 0.0f;
  pt.spiral_tick = 0.0f;
  pt.pulse_tick = 0.0f;
  pt.weaken_tick = 0.0f;
  pt.last_stand_buff_value = 0.0f;
  pt.enchant_remaining = 0.0f;
  pt.enchant_tag = Tag::None;
  pt.ending = false;

  auto *stats = registry.try_get<CombatStats>(owner);

  // 形态增益 Buff。
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  // 988 御剑化影：御剑步 Buff 或御剑步相位任一存在即视为处于御剑步。
  const BuffEffect *sword_step = effects.Get(BuffId::SwordStep);
  const bool sword_step_active =
      (sword_step != nullptr && sword_step->remaining > 0.0f) ||
      registry.any_of<PhaseTag>(owner);
  effects.AddOrRefresh(
      BuildFormBuff(params, owner, params.duration_sec, sword_step_active));

  // 981 逆脉：锁血 + 六系增伤。
  if (params.death_seal && stats) {
    ApplyDeathSeal(registry, owner, *stats, params);
  }

  // 980 虚灵之躯：扣血 + 潜行。
  if (params.void_body) {
    ApplyVoidBody(registry, owner, params);
  }

  // 979 绝影护甲：上限比例护盾。
  if (params.ward_pct > 0.0f && stats) {
    ApplyWard(registry, owner, params);
  }

  // 993 影剑回响触发规则。
  RegisterEchoRule(registry, owner, params);

  if (stats) {
    registry.get_or_emplace<StatsDirty>(owner);
  }
}

bool PhantomTrance::Update(entt::registry &registry, entt::entity owner,
                           PhantomTranceComponent &pt, float dt) {
  pt.elapsed += dt;

  // 形态持续阶段。
  if (!pt.ending && pt.remaining > 0.0f) {
    pt.remaining -= dt;
    if (pt.remaining > 0.0f) {
      TickFormEffects(registry, owner, pt, dt);
      return false;
    }
    // 形态自然结束：执行结算。
    ExecuteEndResolution(registry, owner, pt);
  } else if (!pt.ending) {
    // 免死提前结束：CombatSystem 已将 remaining 清零，仍需执行一次结束结算，
    // 否则会丢失结束爆发、浴血重生与冷却返还。
    ExecuteEndResolution(registry, owner, pt);
  }

  // 附魔窗口阶段：形态已结束但尾附仍在。
  if (pt.enchant_remaining <= 0.0f) {
    // 窗口未开启或已到期：兜底清除尾附（enchant_duration 配置为 0 时同样清理）。
    ClearEnchantGainExtra(registry, owner, pt.enchant_tag);
    pt.enchant_tag = Tag::None;
    return true;
  }
  pt.enchant_remaining -= dt;
  UpdateEnchantPenetration(registry, owner, pt);
  if (pt.enchant_remaining <= 0.0f) {
    // 清除本形态的附魔尾附，敌方减抗 Buff 依靠自身时长自然过期。
    ClearEnchantGainExtra(registry, owner, pt.enchant_tag);
    pt.enchant_tag = Tag::None;
    return true;
  }
  return false;
}

REGISTER_SKILL_BEHAVIOR(PhantomTrance)
void RegisterPhantomTrance() {}
} // namespace NoMoreDay::skills
