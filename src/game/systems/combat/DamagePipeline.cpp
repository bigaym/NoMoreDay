#include "game/systems/combat/DamagePipeline.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/foundation/components/AdvancedAffixComponents.hpp" // InvulnerableComponent, SuppressorComponent
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/CombatAntiMeta.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "game/contracts/CombatFormula.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/AilmentEngine.hpp" // AilmentAdapter (990 凛冬附魔异常判定)
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/damage/DamageInterceptors.hpp"
#include "game/systems/combat/damage/DamageConversion.hpp"
#include "game/systems/combat/damage/DamageTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp" // GetTriggerEffectivenessForCast
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <span>
#include <xsimd/xsimd.hpp>

namespace NoMoreDay {

#ifndef COMBAT_DEFENSE_DEBUG
#define COMBAT_DEFENSE_DEBUG 0
#endif

#if COMBAT_DEFENSE_DEBUG
#define COMBAT_DEFENSE_LOG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define COMBAT_DEFENSE_LOG(...) ((void)0)
#endif

namespace {

struct DefenseResolution {
  bool dodged = false;
  bool blocked = false;
  float block_multiplier = 1.0f;
  float effective_dodge = 0.0f;
  float block_effectiveness = 0.0f;
  float block_amount = 0.0f;
};

float ClampMoreToMultiplier(float more) {
  return std::max(0.0f, 1.0f + more);
}

// 990 凛冬附魔: 攻击者处于冰霜附魔窗口 (enchant_remaining>0 且 enchant_tag==Cold)
// 时，对处于冰冻/冰缓状态的目标追加 frost_amp_pct 增伤。返回最终伤害系数，
// 无加成返回 1.0；判定与 CombatSystem 的 992 击杀刷新口径保持一致。
float ResolveFrostAmpMultiplier(entt::registry &registry, entt::entity attacker,
                                entt::entity defender) {
  if (!registry.valid(attacker) || !registry.valid(defender)) {
    return 1.0f;
  }
  const auto *pt = registry.try_get<PhantomTranceComponent>(attacker);
  if (pt == nullptr || pt->enchant_remaining <= 0.0f ||
      pt->enchant_tag != Tag::Cold || pt->params.frost_amp_pct <= 0.0f) {
    return 1.0f;
  }
  const auto *effects = registry.try_get<ActiveEffectsComponent>(defender);
  if (effects == nullptr) {
    return 1.0f;
  }
  for (const auto &effect : effects->effects) {
    if (effect.remaining <= 0.0f) {
      continue;
    }
    const auto ailment = systems::AilmentAdapter::TryMapLegacyBuff(effect);
    if (ailment && (*ailment == AilmentType::Freeze ||
                    *ailment == AilmentType::Chill)) {
      return 1.0f + pt->params.frost_amp_pct;
    }
  }
  return 1.0f;
}

bool ShouldResolveDefenseRolls(Tag hit_tags, bool skip_mitigation,
                               bool is_simulation) {
  if (skip_mitigation || is_simulation) {
    return false;
  }

  if (!HasTag(hit_tags, Tag::Hit)) {
    return false;
  }

  return !HasTag(hit_tags, Tag::DamageOverTime);
}

float ResolveBlockEffectiveness(const CombatStats &defender_stats) {
  float block_effectiveness = defender_stats.effective_block_eff;
  if (block_effectiveness <= 0.0f && defender_stats.block_amount > 0.0f) {
    const int area_level = (std::max)(1, defender_stats.cached_area_level);
    block_effectiveness = CombatFormula::CalculateBlockEffectiveness(
        defender_stats.block_amount, area_level);
  }
  return std::clamp(block_effectiveness, 0.0f, 1.0f);
}

DefenseResolution ResolveDefenseResolution(
    entt::registry &registry, entt::entity attacker, entt::entity defender,
    const CombatStats *attacker_stats, const CombatStats *defender_stats,
    Tag hit_tags, bool skip_mitigation, bool is_simulation,
    bool dispatch_events, float attacker_accuracy_override = -1.0f) {
  DefenseResolution resolution;
  if (!defender_stats || !ShouldResolveDefenseRolls(hit_tags, skip_mitigation,
                                                    is_simulation)) {
    return resolution;
  }

  float extra_block_chance = 0.0f;
  if (const auto *formation = registry.try_get<BladeFormationComponent>(defender)) {
    if (formation->current_swords > 0 && formation->block_chance_per_sword > 0.0f) {
      extra_block_chance = static_cast<float>(formation->current_swords) * formation->block_chance_per_sword;
    }
  }

  if (defender_stats->dodge_chance <= 0.0f &&
      defender_stats->block_chance <= 0.0f && extra_block_chance <= 0.0f) {
    return resolution;
  }

  // 快照路径由管线传入冻结的 accuracy；<0 表示无覆盖，回退实时属性 (B3)。
  const float attacker_accuracy =
      (attacker_accuracy_override >= 0.0f)
          ? attacker_accuracy_override
          : (attacker_stats ? attacker_stats->accuracy : 1.0f);
  resolution.effective_dodge = std::clamp(
      defender_stats->dodge_chance - (attacker_accuracy - 1.0f), 0.0f, 1.0f);
  COMBAT_DEFENSE_LOG(
      "[DefenseChain] step=1 attacker={} defender={} dodgeChance={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      resolution.effective_dodge);

  // 必中/必闪边缘保护：MSVC 的 uniform_real_distribution 偶发返回上界 1.0，
  // 使 100% 概率判定 < 1.0 失败；此处对 >=1.0 的概率直接判定生效。
  if (resolution.effective_dodge > 0.0f &&
      (resolution.effective_dodge >= 1.0f ||
       utils::ThreadSafeRandom::GetFloat01() < resolution.effective_dodge)) {
    resolution.dodged = true;
    COMBAT_DEFENSE_LOG(
        "[DefenseChain] step=1 attacker={} defender={} dodged=true",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender));

    if (dispatch_events) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateOnDodge(defender, attacker));
    }
    return resolution;
  }

  const float block_chance = std::clamp(defender_stats->block_chance + extra_block_chance, 0.0f, 1.0f);
  COMBAT_DEFENSE_LOG(
      "[DefenseChain] step=2 attacker={} defender={} blockChance={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      block_chance);

  if (block_chance > 0.0f &&
      (block_chance >= 1.0f ||
       utils::ThreadSafeRandom::GetFloat01() < block_chance)) {
    resolution.blocked = true;
    resolution.block_amount = defender_stats->block_amount;
    resolution.block_effectiveness = ResolveBlockEffectiveness(*defender_stats);
    resolution.block_multiplier = 1.0f - resolution.block_effectiveness;

    COMBAT_DEFENSE_LOG(
        "[DefenseChain] step=2 attacker={} defender={} blocked=true "
        "blockMultiplier={:.4f}",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
        resolution.block_multiplier);

    if (dispatch_events) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateOnBlock(
                        defender, attacker, resolution.block_amount));
    }
  }

  return resolution;
}

struct SummonAttributionTuple {
  entt::entity owner = entt::null;
  entt::entity summon = entt::null;
  uint32_t source_skill_id = 0;

  [[nodiscard]] bool IsValid() const {
    return owner != entt::null && summon != entt::null && source_skill_id != 0;
  }
};

SummonAttributionTuple ResolveSummonAttribution(const entt::registry &registry,
                                                entt::entity attacker,
                                                entt::entity source_entity,
                                                uint32_t skill_id) {
  auto fromContext = [&](entt::entity entity) -> SummonAttributionTuple {
    if (!registry.valid(entity)) {
      return {};
    }
    if (const auto *ctx = registry.try_get<SummonAttributionContext>(entity)) {
      if (registry.valid(ctx->owner) && registry.valid(ctx->summon) &&
          ctx->source_skill_id != 0) {
        return {ctx->owner, ctx->summon, ctx->source_skill_id};
      }
    }
    if (const auto *summon = registry.try_get<SummonComponent>(entity)) {
      if (registry.valid(summon->owner)) {
        return {summon->owner, entity,
                summon->skill_id != 0 ? summon->skill_id : skill_id};
      }
    }
    return {};
  };

  if (const auto direct = fromContext(attacker); direct.IsValid()) {
    return direct;
  }
  if (const auto sourceDirect = fromContext(source_entity); sourceDirect.IsValid()) {
    return sourceDirect;
  }

  if (registry.valid(source_entity)) {
    if (const auto *proj = registry.try_get<Projectile>(source_entity)) {
      if (const auto ownerContext = fromContext(proj->owner);
          ownerContext.IsValid()) {
        return ownerContext;
      }
    }
  }

  return {};
}

void AttachSummonAttributionIfAny(CombatEvent &event,
                                  const SummonAttributionTuple &attribution) {
  if (attribution.IsValid()) {
    CombatEventFactory::AttachSummonAttribution(event, attribution.owner,
                                                attribution.summon,
                                                attribution.source_skill_id);
  }
}

struct EventAttackerContext {
  entt::entity attacker = entt::null;
  uint64_t cast_id = 0;
};

// 来源实体归因：投射物/剑阵/技能执行三类来源实体的 cast_id 与归属 owner
// 共用同一判定（技能6 剑阵归因单源）。光束通道仍只参与 cast_id 解析，
// 不并入事件归属覆盖，保留既有语义。
struct SourceAttribution {
  uint64_t cast_id = 0;
  entt::entity owner = entt::null;
};

[[nodiscard]] SourceAttribution
ResolveSourceAttribution(const entt::registry &registry,
                         entt::entity source_entity) {
  SourceAttribution attribution;
  if (!registry.valid(source_entity)) {
    return attribution;
  }

  if (const auto *proj = registry.try_get<Projectile>(source_entity)) {
    attribution.cast_id = proj->cast_id;
    attribution.owner = proj->owner;
    return attribution;
  }

  if (const auto *array = registry.try_get<SwordArrayComponent>(source_entity)) {
    attribution.cast_id = array->cast_id;
    attribution.owner = array->owner;
    return attribution;
  }

  if (const auto *exec = registry.try_get<SkillExecution>(source_entity)) {
    attribution.cast_id = exec->cast_id;
    attribution.owner = exec->owner;
  }

  return attribution;
}

EventAttackerContext
ResolveEventAttackerContext(const entt::registry &registry, entt::entity attacker,
                            entt::entity source_entity,
                            const SummonAttributionTuple &summon_attribution) {
  EventAttackerContext context;
  context.attacker = attacker;

  if (summon_attribution.IsValid()) {
    context.attacker = summon_attribution.owner;
  }

  const SourceAttribution source_attr =
      ResolveSourceAttribution(registry, source_entity);
  context.cast_id = source_attr.cast_id;
  if (!summon_attribution.IsValid() && registry.valid(source_attr.owner)) {
    context.attacker = source_attr.owner;
  }

  return context;
}

void DispatchSingleDamageEvents(entt::registry &registry, entt::entity attacker,
                                entt::entity defender, uint32_t skill_id,
                                Tag combined_hit_tags,
                                entt::entity source_entity,
                                const SummonAttributionTuple &summon_attribution,
                                float reported_damage,
                                float final_applied_damage, bool is_crit) {
  const EventAttackerContext event_attacker = ResolveEventAttackerContext(
      registry, attacker, source_entity, summon_attribution);

  float parent_skill_cd = 0.0f;
  if (skill_id != 0) {
    if (registry.valid(event_attacker.attacker)) {
      if (const auto *profile = SkillSystem::GetBakedSkillProfile(registry, event_attacker.attacker, skill_id)) {
        parent_skill_cd = profile->effective_cooldown;
      }
    }
    if (parent_skill_cd <= 0.0f) {
      if (const auto *skillData = SkillRegistry::Get().GetSkill(skill_id)) {
        parent_skill_cd = skillData->cooldown;
      }
    }
  }
  uint8_t current_depth = 0;
  if (event_attacker.cast_id != 0) {
    current_depth = SkillSystem::QueryCastDepth(event_attacker.cast_id);
  }

  auto enrichEvent = [&](CombatEvent &evt) {
    evt.parent_skill_cd = parent_skill_cd;
    evt.trigger_depth = current_depth;
  };

  if (!HasTag(combined_hit_tags, Tag::DamageOverTime)) {
    CombatEvent hit_evt = CombatEventFactory::CreateSkillHit(
        event_attacker.attacker, defender, skill_id, combined_hit_tags, is_crit,
        event_attacker.cast_id);
    CombatEventFactory::SetDamagePayload(hit_evt, reported_damage,
                                         final_applied_damage);
    enrichEvent(hit_evt);
    AttachSummonAttributionIfAny(hit_evt, summon_attribution);
    CombatEventDispatcher::Dispatch(registry, hit_evt);
  }

  if (reported_damage <= 0.0f) {
    return;
  }

  if (HasTag(combined_hit_tags, Tag::Melee)) {
    CombatEvent melee_evt = CombatEventFactory::CreateMeleeHit(
        event_attacker.attacker, defender, skill_id, combined_hit_tags,
        reported_damage, is_crit);
    CombatEventFactory::SetFinalAppliedDamage(melee_evt, final_applied_damage);
    enrichEvent(melee_evt);
    AttachSummonAttributionIfAny(melee_evt, summon_attribution);
    CombatEventDispatcher::Dispatch(registry, melee_evt);
  }
  if (HasTag(combined_hit_tags, Tag::Projectile)) {
    CombatEvent projectile_evt = CombatEventFactory::CreateProjectileHit(
        event_attacker.attacker, defender, skill_id, combined_hit_tags,
        reported_damage, is_crit, source_entity);
    CombatEventFactory::SetFinalAppliedDamage(projectile_evt,
                                              final_applied_damage);
    enrichEvent(projectile_evt);
    AttachSummonAttributionIfAny(projectile_evt, summon_attribution);
    CombatEventDispatcher::Dispatch(registry, projectile_evt);
  }
  if (HasTag(combined_hit_tags, Tag::Area)) {
    CombatEvent area_evt = CombatEventFactory::CreateAreaHit(
        event_attacker.attacker, defender, skill_id, combined_hit_tags,
        reported_damage, is_crit);
    CombatEventFactory::SetFinalAppliedDamage(area_evt, final_applied_damage);
    enrichEvent(area_evt);
    AttachSummonAttributionIfAny(area_evt, summon_attribution);
    CombatEventDispatcher::Dispatch(registry, area_evt);
  }

  CombatEvent deal_evt = CombatEventFactory::CreateDealDamage(
      event_attacker.attacker, defender, skill_id, combined_hit_tags,
      reported_damage, is_crit, source_entity);
  CombatEventFactory::SetFinalAppliedDamage(deal_evt, final_applied_damage);
  enrichEvent(deal_evt);
  AttachSummonAttributionIfAny(deal_evt, summon_attribution);
  CombatEventDispatcher::Dispatch(registry, deal_evt);

  CombatEvent take_evt = CombatEventFactory::CreateTakeDamage(
      defender, event_attacker.attacker, skill_id, combined_hit_tags,
      reported_damage, is_crit);
  CombatEventFactory::SetFinalAppliedDamage(take_evt, final_applied_damage);
  enrichEvent(take_evt);
  AttachSummonAttributionIfAny(take_evt, summon_attribution);
  CombatEventDispatcher::Dispatch(registry, take_evt);

  if (is_crit) {
    CombatEvent crit_evt = CombatEventFactory::CreateOnCrit(
        event_attacker.attacker, defender, skill_id, combined_hit_tags,
        reported_damage);
    CombatEventFactory::SetFinalAppliedDamage(crit_evt, final_applied_damage);
    enrichEvent(crit_evt);
    AttachSummonAttributionIfAny(crit_evt, summon_attribution);
    CombatEventDispatcher::Dispatch(registry, crit_evt);
  }
}

} // namespace

class ScopedDamageTelemetryTimer {
public:
  ScopedDamageTelemetryTimer() noexcept {
#if COMBAT_TELEMETRY_ENABLED
    m_enabled = CombatTelemetry::Get().IsRuntimeEnabled();
    if (m_enabled) {
      m_start = Clock::now();
    }
#endif
  }

  ~ScopedDamageTelemetryTimer() {
#if COMBAT_TELEMETRY_ENABLED
    if (!m_enabled) {
      return;
    }
    const auto elapsed_us = std::chrono::duration<double, std::micro>(
        Clock::now() - m_start).count();
    CombatTelemetry::Get().RecordDamagePipelineDurationUs(elapsed_us);
#endif
  }

private:
#if COMBAT_TELEMETRY_ENABLED
  using Clock = std::chrono::steady_clock;
  Clock::time_point m_start{};
  bool m_enabled = false;
#endif
};

uint64_t ResolveCastIdFromSourceEntity(const entt::registry &registry,
                                       entt::entity source_entity) {
  if (!registry.valid(source_entity)) {
    return 0;
  }
  // 技能执行优先级最高，保持与原 cast_id 解析顺序一致；投射物/剑阵交由统一
  // 归因函数处理。光束通道不参与事件归属，仅在此保留原兜底。
  if (const auto *exec = registry.try_get<SkillExecution>(source_entity)) {
    return exec->cast_id;
  }
  const uint64_t source_cast_id =
      ResolveSourceAttribution(registry, source_entity).cast_id;
  if (source_cast_id != 0) {
    return source_cast_id;
  }
  if (const auto *beam = registry.try_get<BeamChannelComponent>(source_entity)) {
    return beam->cast_id;
  }
  return 0;
}

namespace {

// 结算帧重入深度上限：最外层结算帧深度为 1，重入结算帧深度为 2；
// 超出上限的延后动作被丢弃并告警，杜绝无限递归。
constexpr int kMaxReentrancyDepth = 2;

thread_local int s_reentrancy_depth = 0;
// FIFO：按入队（因果）顺序结算反击/受击触发，避免 LIFO 逆序导致后触发的
// 反伤先结算、改变连锁结果。
thread_local std::deque<DamagePipeline::DeferredCombatAction> s_deferred_actions;

// 结算帧 RAII 守卫：仅由 Execute / CalculateBatch 公共入口创建。
// 最外层帧析构时统一弹出执行延后动作（此时深度仍为 1，嵌套 Execute 不会二次冲洗）。
class SettlementFrame {
public:
  explicit SettlementFrame(entt::registry &registry)
      : registry_(&registry), root_(s_reentrancy_depth == 0) {
    ++s_reentrancy_depth;
  }
  SettlementFrame(const SettlementFrame &) = delete;
  SettlementFrame &operator=(const SettlementFrame &) = delete;
  ~SettlementFrame() {
    if (root_) {
      FlushDeferredActions();
    }
    --s_reentrancy_depth;
  }

private:
  void FlushDeferredActions() {
    while (!s_deferred_actions.empty()) {
      DamagePipeline::DeferredCombatAction action =
          std::move(s_deferred_actions.front());
      s_deferred_actions.pop_front();
      if (action.request.defender == entt::null ||
          !registry_->valid(action.request.defender)) {
        continue;
      }
      if (action.apply_attacker == entt::null ||
          !registry_->valid(action.apply_attacker)) {
        continue;
      }
      action.request.is_simulation = false;
      (void)DamagePipeline::Execute(*registry_, action.request,
                                    action.apply_attacker, action.show_vfx);
    }
  }

  entt::registry *registry_;
  bool root_;
};

// 入队延后动作：仅在结算帧内（深度>=1）且未超过重入上限时接受。
// 裸 Calculate 无 Apply 收尾帧，动作无落点，直接丢弃避免跨调用残留。
void QueueDeferredAction(DamagePipeline::DeferredCombatAction action) {
  if (s_reentrancy_depth <= 0) {
    LOG_WARN("Combat: Deferred action queued outside a settlement frame; "
             "dropping.");
    return;
  }
  if (s_reentrancy_depth >= kMaxReentrancyDepth) {
    LOG_WARN("Combat: Maximum re-entrancy depth reached. Dropping deferred "
             "action.");
    return;
  }
  s_deferred_actions.push_back(std::move(action));
}

// 从攻方与技能配置构建动态条件规则 (P3-1 攻守解耦)。
// 条件规则只依赖攻方专精/技能域，不依赖守方；守方实时状态在命中时以
// TargetConditionState 掩码求值。数值与旧内联分支逐一对应，保持等价。
FixedVector<damage::ConditionalDamageOp, 4>
BuildConditionalOps(entt::registry &registry, entt::entity attacker,
                    uint32_t skill_id, Tag combined_hit_tags) {
  FixedVector<damage::ConditionalDamageOp, 4> ops;
  const auto &mech = data::SkillMechanicsRegistry::Get();

  auto points_for = [&](uint32_t source_skill, uint32_t node_id) -> int {
    if (const auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == source_skill) {
          return skills::ReadPoints(spec, node_id);
        }
      }
    }
    return 0;
  };

  const bool is_dot = HasTag(combined_hit_tags, Tag::DamageOverTime);

  if (skill_id == 1 && !is_dot) {
    // 172 凛风: 流云刺命中冻结目标 +50% More (数值读 skill_mechanics.json)
    const float frozen_more = mech.GetFloat(1, 172, "frozen_more_mult", 1.50f);
    if (frozen_more != 1.0f) {
      ops.push_back({damage::TargetCondition::Frozen, damage::OpStage::More,
                     frozen_more - 1.0f, 0.0f, BuffKind::None});
    }
    // 150 弱点: 高生命 (>80%) 或受控目标暴击倍率 +15%/点
    const int wp_points = points_for(1, 150);
    if (wp_points > 0) {
      ops.push_back({damage::TargetCondition::HpAbove80 |
                         damage::TargetCondition::Controlled,
                     damage::OpStage::CritDamage,
                     0.15f * static_cast<float>(wp_points), 0.0f,
                     BuffKind::None});
    }
  }

  if (skill_id == 5) {
    // 512 天降命印: 目标每层命印使受到的万剑归宗伤害 +3%
    const float per_stack =
        mech.GetFloat(5, 512, "damage_taken_per_stack", 0.03f);
    ops.push_back({damage::TargetCondition::HasFateMark, damage::OpStage::More,
                   0.0f, per_stack, BuffKind::FateMark});
    // 573 绝对零度: 对冻结目标的击碎伤害 +15%/点
    const int pts_573 = points_for(5, 573);
    if (pts_573 > 0) {
      const float per_point =
          mech.GetFloat(5, 573, "shatter_damage_pct_per_point", 0.15f);
      ops.push_back({damage::TargetCondition::Frozen, damage::OpStage::More,
                     per_point * static_cast<float>(pts_573), 0.0f,
                     BuffKind::None});
    }
  }

  // 250 剑气烙印: 目标每层烙印使受到的暴击伤害 +4% (最多 5 层)
  ops.push_back({damage::TargetCondition::HasQiBrand,
                 damage::OpStage::CritDamage, 0.0f, 0.04f, BuffKind::QiBrand});

  // 812 撕裂伤口: 对流血目标增加暴击倍率 (仅 skill 8 域)
  if (skill_id == 8) {
    if (const auto *profile =
            SkillSystem::GetBakedSkillProfile(registry, attacker, 8)) {
      if (profile->delivery.crit_mult_vs_bleeding != 0.0f) {
        ops.push_back({damage::TargetCondition::Bleeding,
                       damage::OpStage::CritDamage,
                       profile->delivery.crit_mult_vs_bleeding, 0.0f,
                       BuffKind::None});
      }
    }
  }
  return ops;
}

// 事件元素标签重映：事件类型判定依赖 Melee/Projectile/Hit/DamageOverTime 等
// 非元素位，必须原样保留；伤害元素位替换为本次结算实际参与的元素（转换后的
// final_type），使 SkillHit/MeleeHit/ProjectileHit/AreaHit 的 evt.tags 反映真实
// 伤害元素，避免物转火后事件仍带 Physical 导致下游 Proc/异常判定脱节。
// payload 显式声明的元素（如专精 170 火焰/172 冰霜的 effective_tags）与伤害元素
// 语义正交，必须一并保留，否则 DoHit 的元素异常（Ignite/FrostSlow）会失效。
// resolved 为 None（全被拦截/无实例）时回退原始元素位，保证事件标签不丢语义。
[[nodiscard]] Tag ResolveEventElementTags(Tag combined_hit_tags,
                                          Tag resolved_element_tags,
                                          Tag payload_element_tags) {
  if (resolved_element_tags == Tag::None) {
    return combined_hit_tags;
  }
  const uint64_t non_elemental =
      static_cast<uint64_t>(combined_hit_tags) &
      ~static_cast<uint64_t>(damage::kAllDamageTypeTags);
  const uint64_t payload_elemental =
      static_cast<uint64_t>(payload_element_tags) &
      static_cast<uint64_t>(damage::kAllDamageTypeTags);
  return static_cast<Tag>(non_elemental |
                          static_cast<uint64_t>(resolved_element_tags) |
                          payload_elemental);
}

// More 乘区单一口径累加器：把 global_mods / 专精树节点 /
// SkillModifierComponent / cost affix 四类来源统一收集后一次求积，
// 消除分散在实例循环中的内联 final_more 乘法。
// 求值顺序与旧实现逐项一致：prefix → global → 专精节点 → skillMods → cost affix。
class MoreAccumulator {
public:
  void Add(Tag source_tag, float multiplier) {
    entries_.push_back({source_tag, multiplier});
  }

  void AccumulateCostAffix(Tag source_tag, float value) {
    if (value <= 0.0f) {
      return;
    }
    for (size_t idx = 0; idx < cost_buckets_.size; ++idx) {
      if (static_cast<uint64_t>(cost_buckets_[idx].source_tag) ==
          static_cast<uint64_t>(source_tag)) {
        cost_buckets_[idx].actual += value;
        return;
      }
    }
    cost_buckets_.push_back({source_tag, value});
  }

  [[nodiscard]] float Evaluate(Tag instance_tags) const {
    float more = 1.0f;
    for (size_t idx = 0; idx < entries_.size; ++idx) {
      const Entry &entry = entries_[idx];
      if (entry.source_tag == Tag::None ||
          HasTag(instance_tags, entry.source_tag)) {
        more *= entry.multiplier;
      }
    }
    for (size_t idx = 0; idx < cost_buckets_.size; ++idx) {
      more *= (1.0f + CombatAntiMeta::ApplyDiminishingReturns(
                          cost_buckets_[idx].actual));
    }
    return more;
  }

private:
  struct Entry {
    Tag source_tag = Tag::None;
    float multiplier = 1.0f;
  };
  struct CostBucket {
    Tag source_tag = Tag::None;
    float actual = 0.0f;
  };

  FixedVector<Entry, 32> entries_;
  FixedVector<CostBucket, 8> cost_buckets_;
};

} // namespace

DamageResult
DamagePipeline::Calculate(entt::registry &registry, entt::entity attacker,
                          entt::entity defender, uint32_t skill_id,
                          const DamagePool &base_pool, Tag additional_tags,
                          entt::entity source_entity, bool is_simulation) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = skill_id;
  request.base_pool = base_pool;
  request.additional_tags = additional_tags;
  request.source_entity = source_entity;
  request.is_simulation = is_simulation;
  request.dispatch_damage_events = false;
  return Calculate(registry, request);
}

DamageResult DamagePipeline::Calculate(entt::registry &registry,
                                       const DamageRequest &request) {
  ScopedDamageTelemetryTimer telemetryTimer;

  const entt::entity attacker = request.attacker;
  const entt::entity defender = request.defender;
  const uint32_t skill_id = request.skill_id;
  const DamagePool &base_pool = request.base_pool;
  const Tag additional_tags = request.additional_tags;
  const entt::entity source_entity = request.source_entity;
  const bool is_simulation = request.is_simulation;
  const bool dispatch_damage_events = request.dispatch_damage_events;
  const bool thorns_like_damage = request.thorns_like_damage;
  const bool skip_mitigation =
      request.skip_mitigation || request.thorns_like_damage;

  // === PRE-CALCULATION INTERCEPTORS ===

  // 1. Invulnerable Check (Shielding, Clone Invulnerability)
  if (damage::EvaluateInvulnerability(registry, defender, is_simulation)
          .blocked) {
    // Defender is invulnerable, negate all damage
    DamageResult result;
    result.total_damage = 0.0f;
    result.is_crit = false;
    return result;
  }

  // 2. Suppressor Check (Distance-based damage reduction)
  float suppressor_multiplier = 1.0f;
  if (registry.valid(attacker) && registry.valid(defender)) {
    if (auto *suppressor = registry.try_get<SuppressorComponent>(defender)) {
      // Calculate distance between attacker and defender
      auto *attPos = registry.try_get<Position>(attacker);
      auto *defPos = registry.try_get<Position>(defender);

      if (attPos && defPos) {
        float dx = attPos->x - defPos->x;
        float dy = attPos->y - defPos->y;
        float distance = std::sqrt(dx * dx + dy * dy);

        // If attacker is beyond threshold, apply damage reduction
        if (distance > suppressor->threshold) {
          suppressor_multiplier = 1.0f - suppressor->damageReduction;
        }
      }
    }
  }

  const auto *skill_data = SkillRegistry::Get().GetSkill(skill_id);
  if (!skill_data) {
    bool empty_pool = true;
    for (float v : base_pool.values)
      if (v > 0.0f) {
        empty_pool = false;
        break;
      }

    if (empty_pool) {
      LOG_WARN("DamagePipeline: Calculating damage for invalid skill ID {} "
                   "with empty base pool. Result will be 0.",
                   skill_id);
    }
  }
  const float skill_added_effectiveness =
      skill_data ? (std::max)(0.0f, skill_data->added_damage_effectiveness)
                 : 1.0f;
  const float added_effectiveness =
      (std::max)(0.0f, request.added_effectiveness) * skill_added_effectiveness;
  float trigger_effectiveness = (std::max)(0.0f, request.trigger_effectiveness);
  const uint64_t source_cast_id =
      ResolveCastIdFromSourceEntity(registry, source_entity);
  if (source_cast_id != 0) {
    trigger_effectiveness *=
        SkillSystem::GetTriggerEffectivenessForCast(source_cast_id);
  } else if (registry.valid(source_entity)) {
    if (const auto *exec = registry.try_get<SkillExecution>(source_entity)) {
      trigger_effectiveness *= (std::max)(0.0f, exec->trigger_effectiveness);
    }
  }
  Tag skill_tags = skill_data ? skill_data->tags : Tag::None;
  if (request.payload_context.has_value() && request.payload_context->effective_tags != Tag::None) {
    skill_tags = skill_tags | request.payload_context->effective_tags;
  }
  Tag combined_hit_tags = skill_tags | additional_tags;

  // 技能4 节点455「以攻代守」：闪避后挂起的「下一次攻击全局 More」标记在此消费。
  // 命中与否都算一次攻击，故消费点位于闪避判定之前；快照模拟与 DoT 跳伤不消耗。
  // 仅真正攻击（DirectSkillCast/SecondaryProc 等）消耗：荆棘反伤、地面危险区/
  // 持续场以及异常 DoT 跳伤都不是攻击，不得消耗该标记。
  // 一次「攻击行为」（一次施法或一次普攻挥击）产生的全部伤害实例共享同一份
  // +20%：首个实例置为已消费并记录攻击标识，同标识的后续实例（AoE 全目标、
  // 同施法多段）继续享受加成；换攻击行为则清除标记且本次不加成。
  float offensive_guard_multiplier = 1.0f;
  if (!is_simulation && !HasTag(combined_hit_tags, Tag::DamageOverTime) &&
      request.origin != DamageOrigin::ThornsReflect &&
      request.origin != DamageOrigin::HazardEnvironment &&
      request.origin != DamageOrigin::AilmentTick && registry.valid(attacker)) {
    if (auto *attacker_effects =
            registry.try_get<ActiveEffectsComponent>(attacker)) {
      if (auto *marker = attacker_effects->Get("blade_ward_dodge_power")) {
        const uint64_t attack_key =
            (source_cast_id != 0) ? source_cast_id : request.attack_key;
        if (!marker->offensive_guard_consumed) {
          marker->offensive_guard_consumed = true;
          marker->offensive_guard_attack_key = attack_key;
          offensive_guard_multiplier =
              1.0f +
              (std::max)(0.0f,
                         skills::GetMech(4u, 455u, "more_damage_pct", 0.20f));
          // 无攻击标识：退回逐实例消费语义（保持既有测试路径行为）。
          if (attack_key == 0) {
            attacker_effects->Remove("blade_ward_dodge_power");
          }
        } else if (attack_key != 0 &&
                   marker->offensive_guard_attack_key == attack_key) {
          // 同一攻击行为的后续伤害实例：继续享受本份加成。
          offensive_guard_multiplier =
              1.0f +
              (std::max)(0.0f,
                         skills::GetMech(4u, 455u, "more_damage_pct", 0.20f));
        } else {
          // 新攻击行为：清理标记且本次不加成。
          attacker_effects->Remove("blade_ward_dodge_power");
        }
      }
    }
  }

  const SummonAttributionTuple summon_attribution =
      ResolveSummonAttribution(registry, attacker, source_entity, skill_id);
  auto can_apply_scope = [&](ScopePolicy scope, uint32_t source_skill_id) {
    switch (scope) {
    case ScopePolicy::SkillOnly:
      return source_skill_id == skill_id;
    case ScopePolicy::GlobalAlways:
      return true;
    case ScopePolicy::GlobalWhileBuffActive:
      if (const auto *beam = registry.try_get<BeamChannelComponent>(attacker)) {
        if (beam->skill_id == source_skill_id) {
          return true;
        }
      }
      return false;
    default:
      return false;
    }
  };
  auto is_keystone_excluded =
      [&](const SpecializedSkill &specialized, uint32_t source_skill_id,
          uint32_t node_id, const NodeContractData *node_contract) -> bool {
    if (!node_contract || node_contract->keystone_exclusion_group == 0) {
      return false;
    }
    uint32_t selected_node = 0;
    for (const auto &[candidate_node_id, points] : specialized.allocated_points) {
      if (points <= 0) {
        continue;
      }
      const auto *candidate_contract = SkillRegistry::Get().GetNodeContract(
          source_skill_id, candidate_node_id);
      if (!candidate_contract ||
          candidate_contract->keystone_exclusion_group !=
              node_contract->keystone_exclusion_group) {
        continue;
      }
      if (selected_node == 0 || candidate_node_id < selected_node) {
        selected_node = candidate_node_id;
      }
    }
    return selected_node != 0 && selected_node != node_id;
  };

  // Optimization: Access modifiers directly instead of copying to a vector
  auto *global_mods = registry.try_get<GlobalModifierComponent>(attacker);

  auto *attacker_stats = registry.try_get<CombatStats>(attacker);
  auto *defender_stats = registry.try_get<CombatStats>(defender);

  // P3-2: 投射物/DoT 命中优先消费冻结攻方快照。检测前置到基础点伤与转换
  // 收集之前 (S3)，命中快照时直接跳过武器/技能点伤、专精树遍历与转换计算。
  // 查找顺序：source_entity (DoT 载体/投射物) → attacker。这样即便 caster
  // 已销毁，只要载体实体仍存活即可按冻结数值结算。
  const damage::DamageSnapshotComponent *snapshot_component =
      registry.try_get<damage::DamageSnapshotComponent>(source_entity);
  if (snapshot_component == nullptr) {
    snapshot_component =
        registry.try_get<damage::DamageSnapshotComponent>(attacker);
  }
  const bool use_snapshot =
      snapshot_component != nullptr && !thorns_like_damage;

  // 快照命中时命中判定改用冻结 accuracy，避免施法者销毁后命中率漂移 (B3)。
  const float snapshot_accuracy =
      use_snapshot ? snapshot_component->snapshot.accuracy : -1.0f;
  const DefenseResolution defense_resolution = ResolveDefenseResolution(
      registry, attacker, defender, attacker_stats, defender_stats,
      combined_hit_tags, skip_mitigation, is_simulation, !is_simulation,
      snapshot_accuracy);
  auto &endgameRegistry = systems::EndgameModifierRegistry::Get();
  (void)endgameRegistry.EnsureLoaded();
  const auto endgameResolution =
      endgameRegistry.ResolveForEntities(registry, attacker, defender);
  const auto &endgame = endgameResolution.aggregate;
  const float endgameDamageMoreMultiplier =
      ClampMoreToMultiplier(endgame.outgoing_damage_more);

  if (defense_resolution.dodged) {
    DamageResult dodged_result;
    dodged_result.was_dodged = true;
    dodged_result.block_multiplier = defense_resolution.block_multiplier;
    COMBAT_DEFENSE_LOG(
        "[DefenseChain] attacker={} defender={} step=6 hpDamage=0.0000 "
        "(dodged)",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender));
    return dodged_result;
  }

  // Initial Instances from Base Pool
  struct Instance {
    float amount;
    Tag tags;
    Tag final_type;
  };

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  FixedVector<Instance, MAX_INSTANCES> instances;

  // 快照命中：直接按池索引填充已烘焙数值与标签，跳过以下来源收集 (S3)。
  if (use_snapshot) {
    for (int i = 0; i < damage::kElementCount; ++i) {
      const float amount =
          snapshot_component->snapshot.base_damage[static_cast<size_t>(i)];
      if (amount > 0.0f) {
        const Tag type_tag = static_cast<Tag>(1ULL << i);
        // 合并当前请求的技能/载荷标签，避免快照只带基础命中标签而丢失减伤
        // 所需的标签语义。
        instances.push_back(
            {amount, type_tag | snapshot_component->snapshot.hit_tags |
                         combined_hit_tags,
             type_tag});
      }
    }
  }

  // 1. Add instances from provided base_pool（仅非快照路径）
  if (!use_snapshot) {
    using namespace NoMoreDay::Constants::Combat::Pipeline;
    for (int i = 0; i < DAMAGE_POOL_SIZE; ++i) {
      if (base_pool.values[i] > 0.0f) {
        Tag type_tag = static_cast<Tag>(1ULL << i);
        instances.push_back(
            {base_pool.values[i], type_tag | combined_hit_tags, type_tag});
      }
    }
  }

  // 2. Add Skill Base Damage（设计 D6）
  // 唯 DirectSkillCast 允许注入武器点伤与技能配置点伤；次级打击、DoT、环境、
  // 反伤、词缀特效一律不得读取武器或加 skill_data->base_damage，避免武器双算
  // 与次级打击被技能固有伤害污染。payload 基础伤害属于调用方显式输入：
  //   - DirectSkillCast：payload 覆盖武器 min/max 后仍按 weapon_damage_mult
  //     缩放并叠加 skill_data->base_damage（既有语义）；
  //   - 其余来源：payload 基础伤害按纯点伤实例进入流水线，不再乘 wmult、
  //     不再加 skill_data->base_damage (S4)。
  const bool payload_supplies_base =
      request.payload_context.has_value() &&
      (request.payload_context->base_damage_min > 0.0f ||
       request.payload_context->base_damage_max > 0.0f);
  if (!use_snapshot && skill_data &&
      request.origin == DamageOrigin::DirectSkillCast) {
    // Calculate Weapon Damage part
    float min_w = attacker_stats ? attacker_stats->min_weapon_damage : 0.0f;
    float max_w = attacker_stats ? attacker_stats->max_weapon_damage : 0.0f;
    if (payload_supplies_base) {
      min_w = request.payload_context->base_damage_min;
      max_w = request.payload_context->base_damage_max;
    }
    float weapon_avg = (min_w + max_w) * 0.5f;

    float base_dmg =
        skill_data->base_damage + (weapon_avg * skill_data->weapon_damage_mult);

    // Find the primary damage type of the skill
    using namespace NoMoreDay::Constants::Combat::Pipeline;
    Tag primary_type = Tag::Physical;
    for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i) {
      Tag t = static_cast<Tag>(1ULL << i);
      if (HasTag(skill_data->tags, t)) {
        primary_type = t;
        break;
      }
    }

    if (base_dmg > 0.0f) {
      instances.push_back(
          {base_dmg, primary_type | combined_hit_tags, primary_type});
    }
  } else if (!use_snapshot && payload_supplies_base &&
             request.origin != DamageOrigin::DirectSkillCast) {
    // 次级打击等来源：payload 基础伤害为纯点伤，元素取 payload 有效标签，
    // 缺失时回退技能固有标签推导。
    const float base_dmg =
        (request.payload_context->base_damage_min +
         request.payload_context->base_damage_max) *
        0.5f;
    const Tag type_source =
        request.payload_context->effective_tags != Tag::None
            ? request.payload_context->effective_tags
            : (skill_data ? skill_data->tags : Tag::None);
    using namespace NoMoreDay::Constants::Combat::Pipeline;
    Tag primary_type = Tag::Physical;
    for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i) {
      Tag t = static_cast<Tag>(1ULL << i);
      if (HasTag(type_source, t)) {
        primary_type = t;
        break;
      }
    }
    if (base_dmg > 0.0f) {
      instances.push_back(
          {base_dmg, primary_type | combined_hit_tags, primary_type});
    }
  }

  // 3. Add flat damage from stats with Added Effectiveness scaling.
  if (!use_snapshot && attacker_stats && added_effectiveness > 0.0f) {
    for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i) {
      const float scaled_added = attacker_stats->flat_damage[i] * added_effectiveness;
      if (scaled_added <= 0.0f) {
        continue;
      }
      const Tag type_tag = static_cast<Tag>(1ULL << i);
      instances.push_back({scaled_added, type_tag | combined_hit_tags, type_tag});
    }
  }

  // 4. Conversion and Gain Logic (Snapshot, Single Pass)
  // 将 global_mods / SkillModifierComponent / 专精树三处规则统一收集为
  // ConversionRule，再用快照纯函数一次性结算：单遍、无级联、守恒。
  // Convert 总和 > 100% 时按比例缩放，GainExtra 不封顶、不扣减源元素。
  // 规则字段为真实 DamageType：tag 位先还原为池索引校验，再经
  // PoolIndexToDamageType 转回枚举序（4/5 位 Shadow/Poison 互换）。
  // 不再做方向合法性校验：逆向转换（火转冰等）同样合法 (S2)。
  FixedVector<damage::ConversionRule, 64> conversion_rules;

  auto AddConversionRule = [&](const DamageModifier &mod) {
    if (mod.source_tag == Tag::None || mod.target_tag == Tag::None) {
      return;
    }
    if (mod.type != ModifierType::Convert &&
        mod.type != ModifierType::GainExtra) {
      return;
    }
    const int source_pool =
        std::countr_zero(static_cast<uint64_t>(mod.source_tag));
    const int target_pool =
        std::countr_zero(static_cast<uint64_t>(mod.target_tag));
    if (source_pool >= damage::kElementCount ||
        target_pool >= damage::kElementCount) {
      return;
    }
    // 自转为无操作，静默跳过。
    if (source_pool == target_pool) {
      return;
    }
    conversion_rules.push_back(
        {damage::PoolIndexToDamageType(static_cast<size_t>(source_pool)),
         damage::PoolIndexToDamageType(static_cast<size_t>(target_pool)),
         mod.value, mod.type == ModifierType::GainExtra});
  };

  // 规则来源一：全局修饰符
  if (!use_snapshot && global_mods)
    for (const auto &mod : global_mods->modifiers)
      AddConversionRule(mod);
  // 规则来源二：技能修饰符（source_entity）
  if (!use_snapshot && registry.valid(source_entity)) {
    if (auto *sm = registry.try_get<SkillModifierComponent>(source_entity))
      for (const auto &mod : sm->damage_modifiers)
        AddConversionRule(mod);
  }

  // 规则来源三：专精树节点
  if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker);
      !use_snapshot && active != nullptr) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == INVALID_SKILL_ID) {
        continue;
      }
      const uint32_t source_skill_id = spec.skill_id;
      if (const auto *tree = SkillRegistry::Get().GetSkillTree(source_skill_id)) {
        for (auto [node_id, pts] : spec.allocated_points) {
          if (pts <= 0 || !tree->nodes.contains(node_id)) {
            continue;
          }
          ScopePolicy scope = ScopePolicy::SkillOnly;
          const NodeContractData *node_contract =
              SkillRegistry::Get().GetNodeContract(source_skill_id, node_id);
          if (node_contract) {
            scope = node_contract->scope_policy;
          }
          if (!can_apply_scope(scope, source_skill_id)) {
            continue;
          }
          if (node_contract &&
              node_contract->role == SpecNodeRole::Transmuter) {
            const uint32_t active_transmuter =
                SkillSystem::GetActiveTransmuterNode(registry, attacker,
                                                     source_skill_id);
            if (active_transmuter != 0 && active_transmuter != node_id) {
              continue;
            }
          }
          if (is_keystone_excluded(spec, source_skill_id, node_id,
                                   node_contract)) {
            continue;
          }
          for (const auto &mod : tree->nodes.at(node_id).damage_modifiers) {
            AddConversionRule(mod);
          }
        }
      }
    }
  }

  if (!use_snapshot && !conversion_rules.empty()) {
    // 聚合现有实例为按元素池索引对齐的快照输入
    std::array<float, damage::kElementCount> source_values{};
    std::array<Tag, damage::kElementCount> source_tags{};
    for (int i = 0; i < damage::kElementCount; ++i) {
      source_tags[static_cast<size_t>(i)] =
          static_cast<Tag>(1ULL << i) | combined_hit_tags;
    }
    for (size_t i = 0; i < instances.size; ++i) {
      const int elem =
          std::countr_zero(static_cast<uint64_t>(instances[i].final_type));
      if (elem >= 0 && elem < damage::kElementCount) {
        source_values[static_cast<size_t>(elem)] += instances[i].amount;
      }
    }

    const damage::ConversionOutput converted = damage::ApplyConversion(
        source_values, source_tags,
        std::span<const damage::ConversionRule>(conversion_rules.data.data(),
                                                conversion_rules.size));

    // 重建实例：同元素合并为单一实例，产物元素标签已在转换内剥离重赋
    instances.clear();
    for (int i = 0; i < damage::kElementCount; ++i) {
      if (converted.values[static_cast<size_t>(i)] > 0.0f) {
        instances.push_back({converted.values[static_cast<size_t>(i)],
                             converted.tags[static_cast<size_t>(i)],
                             static_cast<Tag>(1ULL << i)});
      }
    }
  }

  // 快照检测与填充已前移到基础点伤/转换之前 (S3)，此处不再重复。

  // 3 & 4. Apply Multipliers (Dynamic via StatsSystem)
  DamageResult result;
  result.was_blocked = defense_resolution.blocked;
  result.block_multiplier = defense_resolution.block_multiplier;
  float total_final_damage = 0.0f;

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  float shadow_multiplier = 1.0f;

  if (!thorns_like_damage) {
    if (auto *sc = registry.try_get<ShadowComponent>(attacker)) {
      shadow_multiplier = sc->damage_scale;
    } else if (registry.all_of<ShadowCloneComponent>(attacker)) {
      shadow_multiplier = SHADOW_MULTIPLIER;
    }
  }

  // P3-1: 守方动态条件掩码与攻方条件规则一次性求值，供全场实例复用。
  const damage::TargetConditionState target_conditions =
      damage::EvaluateTargetConditionState(registry, defender);
  // 快照路径复用创建时冻结的条件规则；常规路径按当前攻方重建。
  FixedVector<damage::ConditionalDamageOp, 4> conditional_ops;
  if (!use_snapshot) {
    conditional_ops =
        BuildConditionalOps(registry, attacker, skill_id, combined_hit_tags);
  }
  const std::span<const damage::ConditionalDamageOp> condition_span =
      use_snapshot ? snapshot_component->snapshot.conditional_ops.span()
                   : conditional_ops.span();
  const float conditional_more =
      damage::ApplyConditionalMore(condition_span, target_conditions);
  const float conditional_crit =
      damage::ApplyConditionalCritDamage(condition_span, target_conditions);

  for (size_t i = 0; i < instances.size; ++i) {
    auto &inst = instances[i];
    if (inst.amount <= 0.0f)
      continue;

    if (!thorns_like_damage) {
      if (use_snapshot) {
        // 快照已含攻方乘区与转换，仅补守方条件 More (创建时守方未知)。
        inst.amount *= conditional_more;
      } else {
      inst.amount *= shadow_multiplier;

      StatType dmg_stat = StatType::PhysicalDamage;
      switch (inst.final_type) {
      case Tag::Physical:
        dmg_stat = StatType::PhysicalDamage;
        break;
      case Tag::Fire:
        dmg_stat = StatType::FireDamage;
        break;
      case Tag::Cold:
        dmg_stat = StatType::ColdDamage;
        break;
      case Tag::Lightning:
        dmg_stat = StatType::LightningDamage;
        break;
      case Tag::Poison:
        dmg_stat = StatType::PoisonDamage;
        break;
      case Tag::Shadow:
        dmg_stat = StatType::ShadowDamage;
        break;
      default:
        break;
      }

      // 无攻方实体（环境/陷阱/落石/亡魂等）或攻方无 CombatStats 时，属性乘区
      // 缺省为 100% 基准，不得把 base_pool 基础点伤归零 (B4)。
      float multiplier_pct = 100.0f;
      if (registry.valid(attacker) && registry.all_of<CombatStats>(attacker)) {
        multiplier_pct = StatsSystem::GetStatWithTags(
            registry, attacker, dmg_stat, inst.tags, skill_id, source_entity);
      }
      if (request.payload_context.has_value() &&
          request.payload_context->increased_damage != 0.0f) {
        multiplier_pct += request.payload_context->increased_damage * 100.0f;
      }
      inst.amount *= (multiplier_pct / 100.0f);

      // 条件 More 乘区 (P3-1)：由 ConditionalDamageOp 统一求值，覆盖旧内联的
      // 172 凛风 / 512 命印 / 573 绝对零度，数值来源不变。
      float final_more = conditional_more;
      if (request.payload_context.has_value()) {
        final_more *= request.payload_context->more_damage;
      }

      // More 乘区单一口径累加：prefix(条件/载荷) 作为首项保持乘法顺序，
      // 后续 global_mods / 专精节点 / skillMods / cost affix 统一收集后一次求值。
      MoreAccumulator more_acc;
      more_acc.Add(Tag::None, final_more);

      if (global_mods) {
        for (const auto &dmod : global_mods->modifiers) {
          if (dmod.type == ModifierType::More &&
              (dmod.source_tag == Tag::None ||
               HasTag(inst.tags, dmod.source_tag))) {
            more_acc.Add(dmod.source_tag, 1.0f + dmod.value);
          }
        }
      }
      if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
        for (const auto &specialized : active->specialized_slots) {
          if (specialized.skill_id == INVALID_SKILL_ID) {
            continue;
          }
          const uint32_t source_skill_id = specialized.skill_id;
          if (const auto *tree =
                  SkillRegistry::Get().GetSkillTree(source_skill_id)) {
            for (auto [node_id, pts] : specialized.allocated_points) {
              if (pts <= 0 || !tree->nodes.contains(node_id)) {
                continue;
              }
              ScopePolicy scope = ScopePolicy::SkillOnly;
              const NodeContractData *node_contract =
                  SkillRegistry::Get().GetNodeContract(source_skill_id,
                                                       node_id);
              if (node_contract) {
                scope = node_contract->scope_policy;
              }
              if (!can_apply_scope(scope, source_skill_id)) {
                continue;
              }
              if (node_contract &&
                  node_contract->role == SpecNodeRole::Transmuter) {
                const uint32_t active_transmuter =
                    SkillSystem::GetActiveTransmuterNode(registry, attacker,
                                                         source_skill_id);
                if (active_transmuter != 0 && active_transmuter != node_id) {
                  continue;
                }
              }
              if (is_keystone_excluded(specialized, source_skill_id, node_id,
                                       node_contract)) {
                continue;
              }
              for (const auto &dmod :
                   tree->nodes.at(node_id).damage_modifiers) {
                if (dmod.type == ModifierType::More &&
                    (dmod.source_tag == Tag::None ||
                     HasTag(inst.tags, dmod.source_tag))) {
                  // 专精树节点的 More 数值以百分比存储（如 12.0 表示 12%），
                  // 与 skills.json 转换类的小数协议不同，此处统一归一到小数后再取幂。
                  const float more_fraction = dmod.value * 0.01f;
                  more_acc.Add(dmod.source_tag,
                               std::pow(1.0f + more_fraction,
                                        static_cast<float>(pts)));
                }
              }
              if (node_contract &&
                  node_contract->cost_affix != CostAffixPreset::None) {
                const auto &cost_affix = CombatAntiMeta::GetCostAffixConfig(
                    node_contract->cost_affix);
                if (cost_affix.damage_more_value > 0.0f &&
                    (cost_affix.damage_more_source_tag == Tag::None ||
                     HasTag(inst.tags, cost_affix.damage_more_source_tag))) {
                  more_acc.AccumulateCostAffix(
                      cost_affix.damage_more_source_tag,
                      cost_affix.damage_more_value * static_cast<float>(pts));
                }
              }
            }
          }
        }
      }
      if (registry.valid(source_entity)) {
        if (auto *skillMods =
                registry.try_get<SkillModifierComponent>(source_entity)) {
          for (const auto &dmod : skillMods->damage_modifiers) {
            if (dmod.type == ModifierType::More &&
                (dmod.source_tag == Tag::None ||
                 HasTag(inst.tags, dmod.source_tag))) {
              more_acc.Add(dmod.source_tag, 1.0f + dmod.value);
            }
          }
        }
      }
      final_more = more_acc.Evaluate(inst.tags);
      inst.amount *= final_more;
      inst.amount *= endgameDamageMoreMultiplier;
      inst.amount *= trigger_effectiveness;
      } // else: !use_snapshot
    }

    // 5. Final Settlement (Crit & Defense)
    float crit_mult = 1.0f;
    if (!thorns_like_damage && HasTag(inst.tags, Tag::Hit) &&
        !HasTag(inst.tags, Tag::DamageOverTime)) {
      bool is_crit = HasTag(additional_tags, Tag::Critical);
      // 条件暴击倍率增量 (P3-1)：150 弱点 / 250 剑气烙印 / 812 撕裂伤口，
      // 由 ConditionalDamageOp 统一按守方条件掩码求值。
      float extra_crit_mult = conditional_crit;

      // 暴击口径优先级 (B3)：快照 → payload → 实时属性。快照路径使用创建时
      // 冻结的 crit_chance/crit_damage，施法者销毁后仍可暴击，且避免与
      // CreateSnapshot 的期望暴击反算口径漂移。
      const bool crit_from_snapshot = use_snapshot;
      float crit_chance = 0.0f;
      if (crit_from_snapshot) {
        crit_chance = snapshot_component->snapshot.crit_chance;
      } else if (request.payload_context.has_value()) {
        // payload_context->crit_chance 与 CombatStats/GetStatWithTags 统一为
        // 分数制 [0.0, 1.0]（1.0f = 100%），此处直接使用，不再 ×100 换算。
        crit_chance = request.payload_context->crit_chance;
      } else if (attacker_stats) {
        crit_chance = StatsSystem::GetStatWithTags(
            registry, attacker, StatType::CritChance, inst.tags, skill_id,
            source_entity);
      }
      auto resolve_crit_damage = [&]() -> float {
        if (crit_from_snapshot) {
          return snapshot_component->snapshot.crit_damage;
        }
        if (request.payload_context.has_value() &&
            request.payload_context->crit_multiplier > 0.0f) {
          return request.payload_context->crit_multiplier;
        }
        return attacker_stats ? attacker_stats->crit_damage : DEFAULT_CRIT_MULT;
      };
      const bool crit_available = crit_from_snapshot ||
                                  request.payload_context.has_value() ||
                                  attacker_stats != nullptr;

      // Dynamic Crit Check if not already marked as critical
      if (!is_crit && crit_available) {
        if (is_simulation) {
          // Calculate expected damage multiplier
          using namespace NoMoreDay::Constants::Combat::Pipeline;
          const float chance =
              std::clamp(crit_chance, 0.0f,
                         NoMoreDay::Constants::Combat::Cap::CRIT_CHANCE);
          const float dmg_mult = resolve_crit_damage() + extra_crit_mult;
          // Expected = 1 * (1-P) + Mult * P = 1 + P * (Mult - 1)
          crit_mult = 1.0f + chance * (dmg_mult - 1.0f);
          if (chance >= 1.0f) {
            is_crit = true;
          }
        } else {
          const float chance =
              std::clamp(crit_chance, 0.0f,
                         NoMoreDay::Constants::Combat::Cap::CRIT_CHANCE);
          if ((utils::ThreadSafeRandom::GetFloat01()) < chance) {
            is_crit = true;
          }
        }
      }

      if (is_crit) {
        crit_mult = resolve_crit_damage() + extra_crit_mult;
        result.is_crit = true;
      }
    }
    inst.amount *= crit_mult;

    using namespace NoMoreDay::Constants::Combat::Pipeline;
    const int type_idx = std::countr_zero(static_cast<uint64_t>(inst.final_type));
    // 选择在减免结算前累计：减免到 0 的实例仍属于"实际进入结算"的元素，
    // 事件应据此重映；resolved 不参与数值，只描述元素归因。
    if (inst.amount > 0.0f) {
      result.resolved_element_tags = result.resolved_element_tags | inst.final_type;
    }
    // 快照路径传入冻结 armor_pen；非快照路径 -1 表示回退实时查询 (B3)。
    const float snapshot_armor_pen =
        use_snapshot ? snapshot_component->snapshot.armor_pen : -1.0f;
    const float damage_after_res = DamageMitigationService::Apply(
        registry, attacker, defender, skill_id, inst.tags, inst.final_type,
        inst.amount,
        defender_stats, endgame, skip_mitigation,
        defense_resolution.blocked, defense_resolution.block_multiplier,
        source_entity, snapshot_armor_pen);
    if (!skip_mitigation) {
      COMBAT_DEFENSE_LOG(
          "[DefenseChain] attacker={} defender={} step=3/4 typeIdx={} "
          "postMitigation={:.4f}",
          static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
          type_idx, damage_after_res);
    }

    total_final_damage += damage_after_res;

    if (type_idx < 16) {
      result.final_pool.values[type_idx] += damage_after_res;
    }
  }

  // === FINAL MULTIPLIERS (Suppressor, etc) ===
  total_final_damage *= suppressor_multiplier;
  // 覆盖整个伤害池（含 Void 位）：仅缩放 total 而漏缩放 final_pool 会破坏
  // total_damage == sum(final_pool) 的守恒关系。
  for (int i = 0; i < DAMAGE_POOL_SIZE; ++i) {
    result.final_pool.values[i] *= suppressor_multiplier;
  }

  // 455「以攻代守」：下一次攻击全局 More（一次性消费，已在上方清除标记）。
  if (offensive_guard_multiplier != 1.0f) {
    total_final_damage *= offensive_guard_multiplier;
    for (int i = 0; i < DAMAGE_POOL_SIZE; ++i) {
      result.final_pool.values[i] *= offensive_guard_multiplier;
    }
  }

  // 990 凛冬附魔: 冰霜附魔窗口内对冰冻/冰缓目标最终伤害增伤 (仅结算一次)。
  const float frost_amp_multiplier =
      ResolveFrostAmpMultiplier(registry, attacker, defender);
  if (frost_amp_multiplier != 1.0f) {
    total_final_damage *= frost_amp_multiplier;
    for (int i = 0; i < DAMAGE_POOL_SIZE; ++i) {
      result.final_pool.values[i] *= frost_amp_multiplier;
    }
  }

  COMBAT_DEFENSE_LOG(
      "[DefenseChain] attacker={} defender={} step=5 barrier=delegated "
      "step=6 hpDamage={:.4f} blocked={} blockMultiplier={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      total_final_damage, result.was_blocked ? "true" : "false",
      result.block_multiplier);

  // --- Blade Ward Defensive Logic (Single Target) ---
  if (!is_simulation && registry.valid(defender)) {
    // Layer 3 (Post-Mitigation) 拦截：Blade Ward 投射物偏转（含消耗飞剑）。
    const damage::BladeWardIntercept ward_intercept =
        damage::EvaluateBladeWardInterception(registry, defender, source_entity,
                                              combined_hit_tags, is_simulation);
    const bool intercepted = ward_intercept.intercepted;
    if (intercepted) {
      total_final_damage = 0.0f; // Negate damage
      result.final_pool.Clear();
    }

    // --- Blade Ward Counter Logic (Talent 470: 偏转/格挡/近战受击触发反击) ---
    // 反击不在结算中途同步派发，而是压入延迟动作队列，由最外层 Execute 收尾统一执行。
    {
      if (const auto *ward = registry.try_get<BladeWardComponent>(defender)) {
        if (ward->trigger_counter && registry.valid(attacker) &&
            attacker != defender && registry.all_of<CombatStats>(attacker) &&
            skill_id != 4 && !HasTag(combined_hit_tags, Tag::SecondaryHit)) {
          const bool isMelee = HasTag(combined_hit_tags, Tag::Melee);
          const bool isBlocked = defense_resolution.blocked;
          if (intercepted || isMelee || isBlocked) {
            DamagePipeline::DeferredCombatAction action;
            action.request = damage::ResolveSkill4CounterEffects(
                registry, defender, attacker, *ward, !request.is_simulation);
            action.apply_attacker = defender;
            action.show_vfx = true;
            QueueDeferredAction(std::move(action));
          }
        }
      }
    }
  }

  result.total_damage = total_final_damage;

  // --- Event System: Dispatch combat events ---
  if (!is_simulation && dispatch_damage_events) {
    const Tag payload_tags = request.payload_context.has_value()
                                 ? request.payload_context->effective_tags
                                 : Tag::None;
    const Tag event_tags = ResolveEventElementTags(
        combined_hit_tags, result.resolved_element_tags, payload_tags);
    DispatchSingleDamageEvents(registry, attacker, defender, skill_id,
                               event_tags, source_entity,
                               summon_attribution, total_final_damage,
                               total_final_damage, result.is_crit);
  }

  return result;
}

DamageExecutionResult DamagePipeline::Execute(entt::registry &registry,
                                              const DamageRequest &request,
                                              entt::entity apply_attacker,
                                              bool show_vfx) {
  // 结算帧：最外层 Execute 收尾时统一弹出执行延后动作（如 Blade Ward 反击）。
  SettlementFrame settlement_frame(registry);
  DamageExecutionResult execution;
  DamageRequest calculate_request = request;
  calculate_request.dispatch_damage_events = false;
  execution.damage = Calculate(registry, calculate_request);

  if (request.is_simulation || execution.damage.was_dodged) {
    if (request.is_simulation) {
      execution.final_applied_damage = execution.damage.total_damage;
    }
    return execution;
  }

  CombatSystem::DamageApplyResult apply_result;
  if (execution.damage.total_damage > 0.0f) {
    const entt::entity effective_apply_attacker =
        (apply_attacker != entt::null) ? apply_attacker : request.attacker;
    execution.target_killed = CombatSystem::ApplyDamage(
        registry, request.defender, execution.damage.total_damage,
        effective_apply_attacker, execution.damage.is_crit, show_vfx,
        &apply_result, request.skill_id);
  }
  execution.final_applied_damage = apply_result.health_applied;
  execution.barrier_absorbed = apply_result.barrier_absorbed;
  execution.was_prevented = apply_result.was_prevented;

  // 173 碎裂基数: 记录该目标最近受到的暴击伤害 (流云刺碎裂溅射读取)
  if (execution.damage.is_crit && registry.valid(request.defender)) {
    auto &lc = registry.get_or_emplace<LastCritDamageComponent>(request.defender);
    lc.amount = execution.final_applied_damage;
    lc.source = request.attacker;
  }

  if (request.dispatch_damage_events) {
    const auto *skill_data = SkillRegistry::Get().GetSkill(request.skill_id);
    Tag combined_hit_tags =
        (skill_data ? skill_data->tags : Tag::None) | request.additional_tags;
    // 事件标签需与 Calculate 的伤害标签一致：并入 payload 的元素 tags，
    // 否则元素转换（170/172）只影响伤害数值，DoHit 拿不到元素、异常不触发
    if (request.payload_context.has_value() &&
        request.payload_context->effective_tags != Tag::None) {
      combined_hit_tags =
          combined_hit_tags | request.payload_context->effective_tags;
    }
    const SummonAttributionTuple summon_attribution = ResolveSummonAttribution(
        registry, request.attacker, request.source_entity, request.skill_id);

    // 元素转换（170/172）后事件标签需按实际结算元素重映，保留动作/机制位与
    // payload 显式元素（171/172 的 Ignite/FrostSlow 依赖该元素位）。
    const Tag payload_tags = request.payload_context.has_value()
                                 ? request.payload_context->effective_tags
                                 : Tag::None;
    const Tag event_tags = ResolveEventElementTags(
        combined_hit_tags, execution.damage.resolved_element_tags, payload_tags);
    DispatchSingleDamageEvents(
        registry, request.attacker, request.defender, request.skill_id,
        event_tags, request.source_entity, summon_attribution,
        execution.damage.total_damage, execution.final_applied_damage,
        execution.damage.is_crit);
  }

  return execution;
}

damage::AttackerSnapshot
DamagePipeline::CreateSnapshot(entt::registry &registry, entt::entity attacker,
                               uint32_t skill_id, const DamagePool &base_pool,
                               Tag hit_tags, entt::entity source_entity,
                               const DamagePayloadContext *payload,
                               DamageOrigin origin) {
  using namespace NoMoreDay::Constants::Combat::Pipeline;
  damage::AttackerSnapshot snap;
  snap.hit_tags = hit_tags;

  // We run a "simulation" calculation on a dummy target to get the attacker's
  // final output per type This is a bit of a hack but it reuse the existing
  // complex logic of Calculate()
  // 载荷必须一并传入：投射物/DoT 的 base_pool 可能为空，伤害由 payload 提供，
  // 否则仿真会得到零基线。同时让 E 的反算口径与单目标路径完全一致。
  DamageRequest sim_request;
  sim_request.attacker = attacker;
  sim_request.defender = entt::null;
  sim_request.origin = origin;
  sim_request.skill_id = skill_id;
  sim_request.base_pool = base_pool;
  sim_request.additional_tags = hit_tags;
  sim_request.source_entity = source_entity;
  sim_request.is_simulation = true;
  sim_request.dispatch_damage_events = false;
  if (payload != nullptr) {
    sim_request.payload_context = *payload;
  }
  DamageResult res = Calculate(registry, sim_request);

  auto *stats = registry.try_get<CombatStats>(attacker);
  // 与单目标路径口径一致：载荷存在时暴击率/倍率以载荷为准。
  if (payload != nullptr) {
    snap.crit_chance = payload->crit_chance;
  } else {
    snap.crit_chance = stats ? stats->crit_chance : 0.0f;
  }
  snap.crit_damage = stats ? stats->crit_damage : DEFAULT_CRIT_MULT;
  if (payload != nullptr && payload->crit_multiplier > 0.0f) {
    snap.crit_damage = payload->crit_multiplier;
  }

  // 上述 is_simulation=true 的 Calculate 已把期望暴击系数
  // E = 1 + P*(M-1) 烘焙进 res.final_pool；批量路径随后会按目标逐一掷暴击并乘
  // crit_damage。为避免暴击双计（历史缺陷），此处先除回 E 得到暴击前基线，
  // 从而保证同一输入下单目标 Calculate 与 CalculateBatch 结果一致。
  // 注意：模拟目标为 entt::null，动态条件(冻结/命印等)在仿真中恒不满足，
  // 故基线不含条件乘区，批量内再按各目标实时条件求值补齐。
  const bool crit_applies =
      HasTag(hit_tags, Tag::Hit) && !HasTag(hit_tags, Tag::DamageOverTime);
  float crit_baseline_inv = 1.0f;
  if (crit_applies) {
    const float p = std::clamp(snap.crit_chance, 0.0f,
                               NoMoreDay::Constants::Combat::Cap::CRIT_CHANCE);
    const float expected_crit = 1.0f + p * (snap.crit_damage - 1.0f);
    if (expected_crit > 0.0f) {
      crit_baseline_inv = 1.0f / expected_crit;
    }
  }

  for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i)
    snap.base_damage[i] = res.final_pool.values[i] * crit_baseline_inv;

  snap.armor_pen =
      stats
          ? stats->armor_pen
          : 0.0f; // Simplified for now, should use GetStatWithTags if possible
  snap.accuracy = stats ? stats->accuracy : 1.0f;
  // 批量事件按快照演算的真实结算元素重映（转换后），与单目标路径一致。
  snap.resolved_element_tags = res.resolved_element_tags;

  // P3-1/P3-2: 条件规则随快照下沉到投射物/DoT，命中时按守方实时掩码求值。
  snap.conditional_ops = BuildConditionalOps(registry, attacker, skill_id,
                                             hit_tags);

  return snap;
}

void DamagePipeline::AttachSnapshotComponent(
    entt::registry &registry, entt::entity holder, entt::entity attacker,
    uint32_t skill_id, const DamagePool &base_pool, Tag hit_tags,
    entt::entity source_entity, const DamagePayloadContext *payload,
    DamageOrigin origin) {
  if (!registry.valid(holder) || !registry.valid(attacker)) {
    return;
  }
  damage::DamageSnapshotComponent component;
  component.snapshot = CreateSnapshot(registry, attacker, skill_id, base_pool,
                                      hit_tags, source_entity, payload, origin);
  component.origin = origin;
  component.skill_id = skill_id;
  component.caster = attacker;
  registry.emplace_or_replace<damage::DamageSnapshotComponent>(holder,
                                                               component);
}

namespace {

// P3-3: 批量守方减伤内核的可观测计数与测试强制开关。
// relaxed 原子：计数仅用于诊断/测试，不参与结算或线程同步。
std::atomic<uint64_t> g_batch_scalar_targets{0};
std::atomic<uint64_t> g_batch_simd_blocks{0};
std::atomic<uint64_t> g_batch_simd_targets{0};
std::atomic<bool> g_batch_force_scalar{false};

} // namespace

DamagePipeline::BatchKernelStats DamagePipeline::GetBatchKernelStats() {
  BatchKernelStats stats;
  stats.scalar_targets =
      g_batch_scalar_targets.load(std::memory_order_relaxed);
  stats.simd_blocks = g_batch_simd_blocks.load(std::memory_order_relaxed);
  stats.simd_targets = g_batch_simd_targets.load(std::memory_order_relaxed);
  return stats;
}

void DamagePipeline::ResetBatchKernelStats() {
  g_batch_scalar_targets.store(0, std::memory_order_relaxed);
  g_batch_simd_blocks.store(0, std::memory_order_relaxed);
  g_batch_simd_targets.store(0, std::memory_order_relaxed);
}

void DamagePipeline::SetForceScalarKernelForTests(bool force) {
  g_batch_force_scalar.store(force, std::memory_order_relaxed);
}

void DamagePipeline::CalculateBatch(
    entt::registry &registry, entt::entity attacker,
    const std::vector<entt::entity> &defenders, uint32_t skill_id,
    const DamagePool &base_pool, Tag additional_tags,
    entt::entity source_entity, tf::Executor *executor) {
  ScopedDamageTelemetryTimer telemetryTimer;

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  if (defenders.empty())
    return;

  // 结算帧：批量入口收尾时统一弹出执行延后动作（反击在提交阶段入队）。
  SettlementFrame settlement_frame(registry);

  const auto *skill_data = SkillRegistry::Get().GetSkill(skill_id);
  Tag combined_tags =
      (skill_data ? skill_data->tags : Tag::None) | additional_tags;
  auto &endgameRegistry = systems::EndgameModifierRegistry::Get();
  (void)endgameRegistry.EnsureLoaded();
  const SummonAttributionTuple summon_attribution =
      ResolveSummonAttribution(registry, attacker, source_entity, skill_id);

  // 1. Snapshot Attacker
  damage::AttackerSnapshot snap = CreateSnapshot(
      registry, attacker, skill_id, base_pool, combined_tags, source_entity);

  struct BatchResult {
    entt::entity target = entt::null;
    float damage = 0.0f;
    bool is_crit = false;
    bool was_dodged = false;
    bool was_blocked = false;
    float block_multiplier = 1.0f;
  };
  std::vector<BatchResult> results(defenders.size());

  // 减抗来源过滤 (SkillOnly scope)：ElementalErosion 等带来源技能归属 (source_skill_id!=0)
  // 的 Flat 减抗 debuff 仅对该技能的伤害生效。抗性烘焙值 (CombatStats.resistances[]) 不含
  // debuff 修饰符，此处聚合对当前伤害生效的 debuff 减抗并参与结算：
  //   - source_skill_id == 0 的减抗对全体伤害生效（含 skill_id == 0 的无归属伤害）；
  //   - source_skill_id != 0 的减抗仅当 == 当前 skill_id 时生效，否则跳过。
  // 实现复用 DamageMitigationService::ApplySkillScopedResistEffects，与单实体路径 (Apply) 保持一致。
  auto debuff_resist_aggregate = [&](entt::entity defender,
                                     DamageType type) -> float {
    return DamageMitigationService::ApplySkillScopedResistEffects(
        registry, defender, skill_id, type);
  };

  // Type E 抗性上限压制聚合 (技能7 心念灭抗 775)，与单实体路径 (Apply) 共用实现：
  // 返回需从 RESISTANCE_MAX 扣除的压制量 (绝对值小数)。
  auto debuff_resist_cap_suppression = [&](entt::entity defender,
                                           DamageType type) -> float {
    return AggregateSkillScopedResistCapSuppression(registry, defender, skill_id,
                                                    type);
  };

  // P3-3 两段式自适应分流。第一段（攻方乘区）由上方 CreateSnapshot 统一执行
  // 一次；此处只负责第二段守方减伤：
  //   - N < 4（含单目标）：标量内核内联，不经 xsimd 包装与批数组暂存；
  //   - N >= 4：进入 xsimd 内核，先以原生批宽铺满，再用 4 宽补齐，末尾不足 4
  //     的目标用标量补齐，保证任意 N 数值正确。
  constexpr size_t kScalarMaxTargets = 4;
  constexpr size_t kNarrowWidth = 4;

  auto record_scalar_targets = [](size_t count) {
    g_batch_scalar_targets.fetch_add(count, std::memory_order_relaxed);
  };

  // 标量内核：单目标减伤全流程。N<4 直通路径与 SIMD 批宽余数补齐共用。
  auto process_scalar = [&](size_t idx) {
    auto defender = defenders[idx];
    if (!registry.valid(defender)) {
      return;
    }
    auto *def_stats = registry.try_get<CombatStats>(defender);
    const auto endgame =
        endgameRegistry.ResolveForEntities(registry, attacker, defender)
            .aggregate;
    const float endgameResDelta = endgame.incoming_resistance_bonus -
                                  endgame.outgoing_resistance_reduction;
    const float endgameArmorDelta =
        endgame.incoming_armor_bonus - endgame.outgoing_armor_reduction;
    const float endgameDrDelta =
        endgame.incoming_global_damage_reduction_bonus -
        endgame.outgoing_global_damage_reduction_reduction;
    const float endgameDamageTakenMultiplier =
        ClampMoreToMultiplier(endgame.incoming_damage_taken_more);
    // Use defaults if stats are missing to avoid "invincible" bugs
    float final_damage = 0.0f;
    float dr = def_stats ? def_stats->damage_reduction : 0.0f;
    float armor = (def_stats ? def_stats->armor : 0.0f) + endgameArmorDelta;

    using namespace NoMoreDay::Constants::Combat::Pipeline;
    for (int j = 0; j < ELEMENTAL_TYPE_COUNT; ++j) {
      float amt = snap.base_damage[j];
      if (amt <= 0.0f)
        continue;
      // 池索引 j 为 Tag 位序，与 DamageType / resistances[] 索引仅 4/5 位
      // (Shadow/Poison) 互换，必须经 damage::PoolIndexTo* 映射后再消费。
      const DamageType pooled_type =
          damage::PoolIndexToDamageType(static_cast<size_t>(j));
      const size_t resist_idx =
          damage::PoolIndexToResistIndex(static_cast<size_t>(j));
      float res = def_stats ? def_stats->resistances[resist_idx] : 0.0f;
      res += debuff_resist_aggregate(defender, pooled_type);
      res += endgameResDelta;
      // Type E：标量路径同样扣除抗性上限压制
      const float cap_suppression =
          debuff_resist_cap_suppression(defender, pooled_type);
      const float effective_max =
          std::max(RESISTANCE_MIN, RESISTANCE_MAX - cap_suppression);
      res = std::clamp(res, RESISTANCE_MIN, effective_max);
      float after_res = amt * (1.0f - res);
      if (j == 0) {
        float effective_armor = armor - snap.armor_pen;
        int area_level = def_stats ? def_stats->cached_area_level : 1;
        float armor_mult = NoMoreDay::CombatFormula::CalculateArmorMultiplier(
            effective_armor, area_level);
        after_res *= armor_mult;
      }
      final_damage += after_res;
    }
    const float effectiveDr = std::clamp(dr + endgameDrDelta, 0.0f, DR_MAX);
    final_damage *= (1.0f - effectiveDr);
    final_damage *= endgameDamageTakenMultiplier;

    // P3-1: 守方实时条件乘区与暴击增量 (标量路径)。
    float cond_more = 1.0f;
    float cond_crit = 0.0f;
    if (!snap.conditional_ops.empty()) {
      const damage::TargetConditionState cond_state =
          damage::EvaluateTargetConditionState(registry, defender);
      cond_more = damage::ApplyConditionalMore(snap.conditional_ops.span(),
                                               cond_state);
      cond_crit = damage::ApplyConditionalCritDamage(snap.conditional_ops.span(),
                                                     cond_state);
    }
    final_damage *= cond_more;

    // === Suppressor Check (Scalar) ===
    if (auto *suppressor = registry.try_get<SuppressorComponent>(defender)) {
      auto *attPos = registry.try_get<Position>(attacker);
      auto *defPos = registry.try_get<Position>(defender);
      if (attPos && defPos) {
        float dx = attPos->x - defPos->x;
        float dy = attPos->y - defPos->y;
        float distanceSq = dx * dx + dy * dy;
        if (distanceSq > suppressor->threshold * suppressor->threshold) {
          final_damage *= (1.0f - suppressor->damageReduction);
        }
      }
    }

    // crit_chance 快照与 CombatStats/单目标路径统一为分数制 [0,1]
    bool is_crit =
        snap.crit_chance >= 1.0f ||
        (snap.crit_chance > 0.0f &&
         (utils::ThreadSafeRandom::GetFloat01() < snap.crit_chance));
    results[idx] = {defender,
                    is_crit ? (final_damage * (snap.crit_damage + cond_crit))
                            : final_damage,
                    is_crit};
  };

  // SIMD 内核：对 [start, start+W) 的 W 个目标一次性向量化结算。
  auto process_simd_block = [&]<size_t W>(size_t start) {
    // xsimd::batch<T, A> 的第二参数是架构而非宽度，按宽度取具体向量类型需用 make_sized_batch_t。
    using batch_type = xsimd::make_sized_batch_t<float, W>;
    constexpr size_t kWidth = W;
    // AVX-512 原生批宽 W=16 时 load_aligned 需 64B 对齐；64B 同时满足
    // 所有 32B 及以下向量宽度，故统一提升到 64。
    alignas(64) std::array<float, kWidth> res_batch_data;
    alignas(64) std::array<float, kWidth> armor_batch_data;
    alignas(64) std::array<float, kWidth> level_batch_data;
    alignas(64) std::array<float, kWidth> endgame_res_delta_data;
    alignas(64) std::array<float, kWidth> endgame_armor_delta_data;
    alignas(64) std::array<float, kWidth> endgame_dr_delta_data;
    alignas(64) std::array<float, kWidth> endgame_damage_taken_mult_data;
    alignas(64) std::array<float, kWidth> cap_suppression_batch_data;
    alignas(64) std::array<float, kWidth> final_dmg_sum;
    // 有效目标掩码：无效目标不得进入任何 registry 查询，避免 entt 失效句柄访问。
    std::array<bool, kWidth> valid_mask{};
    final_dmg_sum.fill(0.0f);

    for (size_t k = 0; k < kWidth; ++k) {
      const auto defender = defenders[start + k];
      if (!registry.valid(defender)) {
        endgame_res_delta_data[k] = 0.0f;
        endgame_armor_delta_data[k] = 0.0f;
        endgame_dr_delta_data[k] = 0.0f;
        endgame_damage_taken_mult_data[k] = 1.0f;
        cap_suppression_batch_data[k] = 0.0f;
        armor_batch_data[k] = 0.0f;
        level_batch_data[k] = 1.0f;
        res_batch_data[k] = 0.0f;
        valid_mask[k] = false;
        continue;
      }
      valid_mask[k] = true;
      const auto endgame =
          endgameRegistry.ResolveForEntities(registry, attacker, defender)
              .aggregate;
      endgame_res_delta_data[k] = endgame.incoming_resistance_bonus -
                                  endgame.outgoing_resistance_reduction;
      endgame_armor_delta_data[k] =
          endgame.incoming_armor_bonus - endgame.outgoing_armor_reduction;
      endgame_dr_delta_data[k] =
          endgame.incoming_global_damage_reduction_bonus -
          endgame.outgoing_global_damage_reduction_reduction;
      endgame_damage_taken_mult_data[k] =
          ClampMoreToMultiplier(endgame.incoming_damage_taken_more);
    }

    using namespace NoMoreDay::Constants::Combat::Pipeline;
    for (int j = 0; j < ELEMENTAL_TYPE_COUNT; ++j) {
      float base_amt = snap.base_damage[j];
      if (base_amt <= 0.0f)
        continue;

      // 与标量内核共用同一映射：池索引 j(Tag 位序) -> DamageType / 抗性索引。
      const DamageType pooled_type =
          damage::PoolIndexToDamageType(static_cast<size_t>(j));
      const size_t resist_idx =
          damage::PoolIndexToResistIndex(static_cast<size_t>(j));

      for (size_t k = 0; k < kWidth; ++k) {
        if (!valid_mask[k]) {
          res_batch_data[k] = 0.0f;
          continue;
        }
        auto *ds = registry.try_get<CombatStats>(defenders[start + k]);
        res_batch_data[k] =
            (ds ? ds->resistances[resist_idx] : 0.0f) + endgame_res_delta_data[k] +
            debuff_resist_aggregate(defenders[start + k], pooled_type);
        // Type E：逐目标聚合抗性上限压制量 (元素按当前伤害类型过滤)
        cap_suppression_batch_data[k] = debuff_resist_cap_suppression(
            defenders[start + k], pooled_type);
        // 护甲项与标量内核口径一致：仅物理(池索引 0)参与，且缺 CombatStats
        // 时仍取 endgame 护甲增量，避免两内核在无属性目标上产生分歧。
        armor_batch_data[k] =
            (j == 0) ? ((ds ? ds->armor : 0.0f) + endgame_armor_delta_data[k])
                     : 0.0f;
        level_batch_data[k] =
            (j == 0 && ds) ? (float)ds->cached_area_level : 1.0f;
      }

      using namespace NoMoreDay::Constants::Combat::Pipeline;
      auto amt_v = batch_type(base_amt);
      auto raw_res_v = batch_type::load_aligned(res_batch_data.data());
      // Type E：有效抗性上限 = max(RESISTANCE_MIN, RESISTANCE_MAX - cap)。
      // 仍以 select 实现 clamp，避免引入 xsimd::min/max 的命名空间歧义。
      auto cap_v = batch_type::load_aligned(cap_suppression_batch_data.data());
      auto effective_max_v = batch_type(RESISTANCE_MAX) - cap_v;
      effective_max_v =
          xsimd::select(effective_max_v < batch_type(RESISTANCE_MIN),
                        batch_type(RESISTANCE_MIN), effective_max_v);
      auto res_v = xsimd::select(
          raw_res_v > effective_max_v, effective_max_v,
          xsimd::select(raw_res_v < batch_type(RESISTANCE_MIN),
                        batch_type(RESISTANCE_MIN), raw_res_v));
      auto current_v = amt_v * (batch_type(1.0f) - res_v);

      if (j == 0) {
        using namespace NoMoreDay::Constants::Combat::Scaling;
        auto pen_v = batch_type(snap.armor_pen);
        auto eff_armor_v =
            batch_type::load_aligned(armor_batch_data.data()) - pen_v;

        // Level Factor: LEVEL_BASE + LEVEL_LINEAR*L + LEVEL_QUADRATIC*L^2
        // 与 CombatFormula::LevelFactor 共用同一组常量，避免 SIMD 路径内联漂移。
        auto L_v = batch_type::load_aligned(level_batch_data.data());
        auto LF_v = batch_type(LEVEL_BASE) + batch_type(LEVEL_LINEAR) * L_v +
                    batch_type(LEVEL_QUADRATIC) * L_v * L_v;

        auto abs_armor_v = xsimd::abs(eff_armor_v);
        auto denom_v = abs_armor_v + LF_v;

        // Positive: LF / (Armor + LF) = LF / denom
        auto pos_mult = LF_v / denom_v;

        // Negative: 1 + |Armor| / (|Armor| + LF) = 1 + abs_armor / denom
        auto neg_mult = batch_type(1.0f) + (abs_armor_v / denom_v);

        auto positive_mask = eff_armor_v >= batch_type(0.0f);
        current_v *= xsimd::select(positive_mask, pos_mult, neg_mult);
      }
      auto sum_v = batch_type::load_aligned(final_dmg_sum.data()) + current_v;
      sum_v.store_aligned(final_dmg_sum.data());
    }

    for (size_t k = 0; k < kWidth; ++k) {
      using namespace NoMoreDay::Constants::Combat::Pipeline;
      auto defender = defenders[start + k];
      // 无效目标保持默认 (target=entt::null)，与标量内核一致，避免提交阶段
      // 对失效实体落地伤害。
      if (!registry.valid(defender)) {
        continue;
      }
      auto *ds = registry.try_get<CombatStats>(defender);
      float dr = ds ? ds->damage_reduction : 0.0f;
      const float effectiveDr =
          std::clamp(dr + endgame_dr_delta_data[k], 0.0f, DR_MAX);
      float damage = final_dmg_sum[k] * (1.0f - effectiveDr);
      damage *= endgame_damage_taken_mult_data[k];

      // P3-1: 守方实时条件乘区与暴击增量，与单目标 Calculate 口径一致。
      float cond_more = 1.0f;
      float cond_crit = 0.0f;
      if (!snap.conditional_ops.empty()) {
        const damage::TargetConditionState cond_state =
            damage::EvaluateTargetConditionState(registry, defender);
        cond_more = damage::ApplyConditionalMore(snap.conditional_ops.span(),
                                                 cond_state);
        cond_crit = damage::ApplyConditionalCritDamage(
            snap.conditional_ops.span(), cond_state);
      }
      damage *= cond_more;

      // === Suppressor Check (Batch) ===
      if (auto *suppressor = registry.try_get<SuppressorComponent>(defender)) {
        auto *attPos = registry.try_get<Position>(attacker);
        auto *defPos = registry.try_get<Position>(defender);
        if (attPos && defPos) {
          float dx = attPos->x - defPos->x;
          float dy = attPos->y - defPos->y;
          float distanceSq = dx * dx + dy * dy;
          if (distanceSq > suppressor->threshold * suppressor->threshold) {
            damage *= (1.0f - suppressor->damageReduction);
          }
        }
      }

      // crit_chance 快照与 CombatStats/单目标路径统一为分数制 [0,1]
      bool is_crit =
          snap.crit_chance >= 1.0f ||
          (snap.crit_chance > 0.0f &&
           (utils::ThreadSafeRandom::GetFloat01() < snap.crit_chance));
      results[start + k] = {defender,
                            is_crit ? (damage * (snap.crit_damage + cond_crit))
                                    : damage,
                            is_crit};
    }

    g_batch_simd_blocks.fetch_add(1, std::memory_order_relaxed);
    g_batch_simd_targets.fetch_add(kWidth, std::memory_order_relaxed);
  };

  auto process_range = [&](size_t start, size_t end) {
    const size_t count = end - start;
    const bool force_scalar =
        g_batch_force_scalar.load(std::memory_order_relaxed);
    if (force_scalar || count < kScalarMaxTargets) {
      record_scalar_targets(count);
      for (size_t i = start; i < end; ++i) {
        process_scalar(i);
      }
      return;
    }

    constexpr size_t kNativeWidth = xsimd::batch<float>::size;
    size_t i = start;
    // 原生批宽铺满（AVX2=8 / SSE=4）。
    while (i + kNativeWidth <= end) {
      process_simd_block.template operator()<kNativeWidth>(i);
      i += kNativeWidth;
    }
    // 批宽余数中仍 >=4 的部分用 4 宽 SIMD 向量化（原生宽度 > 4 时才有意义）。
    if constexpr (kNativeWidth > kNarrowWidth) {
      if (i + kNarrowWidth <= end) {
        process_simd_block.template operator()<kNarrowWidth>(i);
        i += kNarrowWidth;
      }
    }
    // 末尾不足 4 的目标用标量补齐。
    if (i < end) {
      record_scalar_targets(end - i);
      for (; i < end; ++i) {
        process_scalar(i);
      }
    }
  };

  // 2. Execution
  if (executor && defenders.size() >= (size_t)BATCH_GRAIN_SIZE) {
    // Parallel Math
    tf::Taskflow taskflow;
    size_t grainSize = BATCH_GRAIN_SIZE;
    for (size_t i = 0; i < defenders.size(); i += grainSize) {
      size_t start = i;
      size_t end = std::min(i + grainSize, defenders.size());
      taskflow.emplace([=]() { process_range(start, end); });
    }
    executor->run(taskflow).wait();
  } else {
    process_range(0, defenders.size());
  }

  // 3. Serial Commit (Main Thread Safe)
  const auto *batch_attacker_stats = registry.try_get<CombatStats>(attacker);
  for (const auto &res : results) {
    if (res.target != entt::null) {
      float final_damage = res.damage;
      const auto *batch_defender_stats = registry.try_get<CombatStats>(res.target);
      const DefenseResolution defense_resolution = ResolveDefenseResolution(
          registry, attacker, res.target, batch_attacker_stats,
          batch_defender_stats, combined_tags, false, false, true);

      if (defense_resolution.dodged) {
        continue;
      }

      if (defense_resolution.blocked) {
        final_damage *= defense_resolution.block_multiplier;
      }

      // Layer 3 (Post-Mitigation) 拦截：Blade Ward 投射物偏转（含消耗飞剑）。
      const damage::BladeWardIntercept ward_intercept =
          damage::EvaluateBladeWardInterception(registry, res.target,
                                                source_entity, combined_tags,
                                                false);
      const bool intercepted = ward_intercept.intercepted;
      if (intercepted) {
        final_damage = 0.0f;
      }

      // --- Blade Ward Counter Logic (Talent 470: 偏转/格挡/近战受击触发反击) ---
      // 反击入延迟队列，由批量结算帧收尾统一执行，避免提交阶段重入。
      {
        if (const auto *ward = registry.try_get<BladeWardComponent>(res.target)) {
          if (ward->trigger_counter && registry.valid(attacker) &&
              attacker != res.target &&
              registry.all_of<CombatStats>(attacker) && skill_id != 4 &&
              !HasTag(combined_tags, Tag::SecondaryHit)) {
            const bool isMelee = HasTag(combined_tags, Tag::Melee);
            const bool isBlocked = defense_resolution.blocked;
            if (intercepted || isMelee || isBlocked) {
              DamagePipeline::DeferredCombatAction action;
              action.request = damage::ResolveSkill4CounterEffects(
                  registry, res.target, attacker, *ward, true);
              action.apply_attacker = res.target;
              action.show_vfx = true;
              QueueDeferredAction(std::move(action));
            }
          }
        }
      }

      CombatSystem::DamageApplyResult apply_result;
      CombatSystem::ApplyDamage(registry, res.target, final_damage, attacker,
                                res.is_crit, true, &apply_result, skill_id);
      const float final_applied_damage = apply_result.health_applied;
      const EventAttackerContext event_attacker = ResolveEventAttackerContext(
          registry, attacker, source_entity, summon_attribution);

      // --- Event System: Dispatch combat events ---
      // 元素转换后元素位按快照真实结算元素重映，动作/机制位（Melee 等）原样保留。
      // 批量接口无 payload 上下文，显式元素位传 None。
      const Tag event_tags = ResolveEventElementTags(
          combined_tags, snap.resolved_element_tags, Tag::None);
      if (!HasTag(combined_tags, Tag::DamageOverTime)) {
        CombatEvent skillHitEvent = CombatEventFactory::CreateSkillHit(
            event_attacker.attacker, res.target, skill_id, event_tags,
            res.is_crit, event_attacker.cast_id);
        CombatEventFactory::SetDamagePayload(skillHitEvent, final_damage,
                                             final_applied_damage);
        AttachSummonAttributionIfAny(skillHitEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, skillHitEvent);
      }

      // Dispatch specific hit types
      if (HasTag(combined_tags, Tag::Melee)) {
        CombatEvent meleeEvent =
            CombatEventFactory::CreateMeleeHit(event_attacker.attacker,
                                               res.target, skill_id, event_tags,
                                               final_damage, res.is_crit);
        CombatEventFactory::SetFinalAppliedDamage(meleeEvent,
                                                  final_applied_damage);
        AttachSummonAttributionIfAny(meleeEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, meleeEvent);
      }
      if (HasTag(combined_tags, Tag::Projectile)) {
        CombatEvent projectileEvent = CombatEventFactory::CreateProjectileHit(
            event_attacker.attacker, res.target, skill_id, event_tags, final_damage,
            res.is_crit, source_entity);
        CombatEventFactory::SetFinalAppliedDamage(projectileEvent,
                                                  final_applied_damage);
        AttachSummonAttributionIfAny(projectileEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, projectileEvent);
      }
      if (HasTag(combined_tags, Tag::Area)) {
        CombatEvent areaEvent = CombatEventFactory::CreateAreaHit(
            event_attacker.attacker, res.target, skill_id, event_tags, final_damage,
            res.is_crit);
        CombatEventFactory::SetFinalAppliedDamage(areaEvent,
                                                  final_applied_damage);
        AttachSummonAttributionIfAny(areaEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, areaEvent);
      }

      // Standard damage events
      CombatEvent dealEvent = CombatEventFactory::CreateDealDamage(
          event_attacker.attacker, res.target, skill_id, event_tags, final_damage,
          res.is_crit, source_entity);
      CombatEventFactory::SetFinalAppliedDamage(dealEvent, final_applied_damage);
      AttachSummonAttributionIfAny(dealEvent, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, dealEvent);

      CombatEvent takeEvent = CombatEventFactory::CreateTakeDamage(
          res.target, event_attacker.attacker, skill_id, event_tags, final_damage,
          res.is_crit);
      CombatEventFactory::SetFinalAppliedDamage(takeEvent, final_applied_damage);
      AttachSummonAttributionIfAny(takeEvent, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, takeEvent);

      if (res.is_crit) {
        // 173 碎裂基数: 记录该目标最近受到的暴击伤害 (流云刺碎裂溅射读取)
        auto &lc = registry.get_or_emplace<LastCritDamageComponent>(res.target);
        lc.amount = final_applied_damage;
        lc.source = event_attacker.attacker;

        CombatEvent critEvent = CombatEventFactory::CreateOnCrit(
            event_attacker.attacker, res.target, skill_id, event_tags,
            final_damage);
        CombatEventFactory::SetFinalAppliedDamage(critEvent,
                                                  final_applied_damage);
        AttachSummonAttributionIfAny(critEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, critEvent);
      }
    }
  }
}

std::vector<DamageResult>
DamagePipeline::CalculateBatchResults(entt::registry &registry,
                                      const DamageRequest &request) {
  std::vector<DamageResult> results;
  if (request.defender == entt::null) {
    return results;
  }
  // 批量钩子按契约返回逐目标计算结果；当前 DamageRequest 仅携带单一防御方，
  // 因此结果与逐目标 Calculate 完全一致，且不落地伤害（Apply 由调用方负责）。
  results.push_back(Calculate(registry, request));
  return results;
}

// ---------------------------------------------------------------------------
// Hook registration (static-init, before main): gameplay domains resolve
// damage through ResolveDamage / ResolveDamageBatch without a compile-time
// dependency on the combat domain.
// ---------------------------------------------------------------------------
namespace {

class DamageResolutionHookRegistrar {
public:
  DamageResolutionHookRegistrar() {
    DamageResolutionHooks hooks;
    hooks.execute = [](entt::registry &registry, const DamageRequest &request,
                       entt::entity target) {
      return DamagePipeline::Execute(registry, request, target, true);
    };
    hooks.calculateBatch = [](entt::registry &registry,
                              const DamageRequest &request) {
      return DamagePipeline::CalculateBatchResults(registry, request);
    };
    RegisterDamageResolutionHooks(hooks);
  }
};

DamageResolutionHookRegistrar g_damageResolutionHookRegistrar;

} // namespace

} // namespace NoMoreDay
