/**
 * @file BladeWard.cpp
 * @brief 剑气护体 (ID 4) - 模块化投射/响应护盾交付实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "BladeWardRuntime.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/FactionComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/physics/PhysicsUtils.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace NoMoreDay::skills {

namespace BladeWardNodes {
constexpr uint32_t GoldenBell      = 400; // 金钟罩
constexpr uint32_t Deflection      = 401; // 拨云见日
constexpr uint32_t Persist         = 402; // 持久
constexpr uint32_t Repel           = 403; // 剑压外放
constexpr uint32_t IronGuard       = 410; // 厚积薄发
constexpr uint32_t LastStand       = 413; // 破釜沉舟
constexpr uint32_t Vitality        = 415; // 坚韧回生
constexpr uint32_t Unstoppable     = 431; // 势不可挡
constexpr uint32_t BloodBarrier    = 433; // 鲜血壁垒
constexpr uint32_t PerfectParry    = 434; // 无瑕之御
constexpr uint32_t Aftermath       = 453; // 流风余韵
constexpr uint32_t SwordStep       = 454; // 御剑闪步
constexpr uint32_t Permafrost      = 475; // 永冻领域
constexpr uint32_t FiveGuard       = 411; // 五行御守
constexpr uint32_t Mountain        = 412; // 不动如山 (Keystone)
constexpr uint32_t IntentBlock     = 430; // 剑意格挡
constexpr uint32_t ShieldBarrier   = 432; // 剑盾屏障
constexpr uint32_t IntentProc      = 435; // 剑意格御
constexpr uint32_t CounterSpeed    = 451; // 借力打力 (闪避提速)
constexpr uint32_t BlinkCounter    = 452; // 瞬身反打 (Trigger 流云刺)
constexpr uint32_t AttackDefend    = 455; // 以攻代守
constexpr uint32_t CounterBlade    = 470; // 剑气反震 (Keystone)
constexpr uint32_t Vengeance       = 471; // 以眼还眼 (反击增伤)
constexpr uint32_t StaticField     = 472; // 雷霆法环 (Transmuter Lightning)
constexpr uint32_t ThunderCascade  = 473; // 雷贯长虹
constexpr uint32_t FrostArmor      = 474; // 霜铠 (Transmuter Cold)
constexpr uint32_t Exposure        = 476; // 元素曝光（双前置 473/474，OR 语义；布局锚点取首前置 473，仅视觉偏差）
} // namespace BladeWardNodes

struct BladeWard : SkillBehaviorBase<BladeWard> {
  static constexpr uint32_t kSkillId = 4;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    const auto &mechanics = data::SkillMechanicsRegistry::Get();

    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    const SpecializedSkill *specPtr = nullptr;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) {
          specPtr = &spec;
          SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr);
          profile = &localProfile;
          break;
        }
      }
    } else if (registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) {
          specPtr = &spec;
          break;
        }
      }
    }

    auto getPoints = [&](uint32_t node_id) -> int {
      if (specPtr) {
        // 读点 helper：已分配节点点数恒 ≥1，正数即已点亮。
        const int points = skills::ReadPoints(*specPtr, node_id);
        if (points > 0) return points;
      }
      return exec.active_nodes.test(node_id % 100) ? 1 : 0;
    };

    // 1. 减伤与属性 Buff 配置 (L1 优化: 直接使用静态 std::string，避免重复堆分配)
    static const std::string kBladeWardBuffId{BuffIdToString(BuffId::BladeWard)};
    static const std::string kBladeWardBuffName{"Blade Ward"};
    const float base_dr = mechanics.GetFloat(kSkillId, BladeWardNodes::GoldenBell, "phys_dr_base", 12.0f);
    BuffEffect ward_buff{
        .id = kBladeWardBuffId,
        .name = kBladeWardBuffName,
        .type = BuffType::Shield,
        .duration = 10.0f,
        .remaining = 10.0f
    };
    ward_buff.modifiers.push_back({.value = base_dr, .type = StatType::ResistPhysical, .mode = ModifierMode::Flat});

    const int fiveGuardPts = getPoints(BladeWardNodes::FiveGuard);
    if (fiveGuardPts > 0) {
      const float resistVal = mechanics.GetFloat(kSkillId, BladeWardNodes::FiveGuard, "all_resist_per_point", 6.0f) * static_cast<float>(fiveGuardPts);
      ward_buff.modifiers.push_back({.value = resistVal, .type = StatType::ResistAll, .mode = ModifierMode::Flat});
    }

    const int intentBlockPts = getPoints(BladeWardNodes::IntentBlock);
    if (intentBlockPts > 0) {
      const float blockVal = mechanics.GetFloat(kSkillId, BladeWardNodes::IntentBlock, "block_chance_per_point", 4.0f) * static_cast<float>(intentBlockPts);
      ward_buff.modifiers.push_back({.value = blockVal, .type = StatType::BlockChance, .mode = ModifierMode::Flat});
    }

    // 402 持久：维持法力消耗降低（ResourceCostReduction 以百分比点数计量）。
    const int persistPts = getPoints(BladeWardNodes::Persist);
    const float persistManaReduction =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Persist, "mana_cost_reduction_per_point", 0.15f) *
        static_cast<float>(persistPts);
    if (persistManaReduction > 0.0f) {
      ward_buff.modifiers.push_back({.value = persistManaReduction * 100.0f,
                                     .type = StatType::ResourceCostReduction,
                                     .mode = ModifierMode::Flat});
    }

    // 431 势不可挡：格挡效果提升（BlockRating 百分比乘算）。
    const int unstoppablePts = getPoints(BladeWardNodes::Unstoppable);
    const float unstoppableBlockEffect =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Unstoppable, "block_eff_pct_per_point", 0.15f) *
        static_cast<float>(unstoppablePts);
    if (unstoppableBlockEffect > 0.0f) {
      ward_buff.modifiers.push_back({.value = unstoppableBlockEffect * 100.0f,
                                     .type = StatType::BlockRating,
                                     .mode = ModifierMode::PercentMult});
    }
    registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(ward_buff);

    // 2. 环绕灵剑视觉与物理表现实体 (OrbitingSentinel)
    // 拦截判定由 ProjectileSystem（投射物）与 DamageInterceptors（非投射物源）
    // 统一承担，环绕灵剑不再重复拦截，此处仅保留环绕与周期攻击
    auto &sentinel = registry.emplace_or_replace<OrbitingSentinelComponent>(owner);
    sentinel.anchor_entity = owner;
    sentinel.cast_id = exec.cast_id;
    sentinel.skill_id = kSkillId;
    sentinel.count = static_cast<uint8_t>(exec.is_empowered ? 6 : 3);
    sentinel.orbit_radius = 60.0f;
    sentinel.angular_velocity = 180.0f;

    // 3. 剑气护体逻辑核心 (BladeWardComponent)
    auto &ward = registry.get_or_emplace<BladeWardComponent>(owner);
    ward.duration = ward.remaining = 10.0f;
    ward.sword_count = sentinel.count;

    // 偏转几率计算 (解 H8, M12): 基础 10% + 拨云见日每点 4%
    const float base_defl = mechanics.GetFloat(kSkillId, BladeWardNodes::Deflection, "base_deflection", 0.10f);
    const float defl_per_pt = mechanics.GetFloat(kSkillId, BladeWardNodes::Deflection, "deflection_pct_per_point", 0.04f);
    ward.interception_chance = base_defl + defl_per_pt * static_cast<float>(getPoints(BladeWardNodes::Deflection));
    if (exec.is_empowered) {
      ward.interception_chance = std::min(1.0f, ward.interception_chance * 1.5f);
    }

    // 专精状态映射
    ward.is_solidified = profile ? ((profile->delivery.feature_flags & 8) != 0) : (getPoints(BladeWardNodes::Mountain) > 0);
    ward.trigger_counter = profile ? ((profile->delivery.feature_flags & 128) != 0) : (getPoints(BladeWardNodes::CounterBlade) > 0);
    const int vengeancePts = getPoints(BladeWardNodes::Vengeance);
    ward.counter_damage_more = mechanics.GetFloat(kSkillId, BladeWardNodes::Vengeance, "counter_more_damage_per_point", 0.20f) * static_cast<float>(vengeancePts);

    ward.is_lightning_ward = profile ? ((profile->delivery.feature_flags & 512) != 0) : (getPoints(BladeWardNodes::StaticField) > 0);
    ward.is_cold_ward = profile ? ((profile->delivery.feature_flags & 1024) != 0) : (getPoints(BladeWardNodes::FrostArmor) > 0);
    ward.counter_spin = profile ? ((profile->delivery.feature_flags & 2048) != 0) : (getPoints(BladeWardNodes::ThunderCascade) > 0);

    const int speedPts = getPoints(BladeWardNodes::CounterSpeed);
    ward.dodge_speed_points = static_cast<float>(speedPts);
    ward.dodge_power_boost = (getPoints(BladeWardNodes::AttackDefend) > 0);

    const int intentProcPts = getPoints(BladeWardNodes::IntentProc);
    ward.block_intent_chance = mechanics.GetFloat(kSkillId, BladeWardNodes::IntentProc, "intent_chance_per_point", 0.15f) * static_cast<float>(intentProcPts);

    const int barrierPts = getPoints(BladeWardNodes::ShieldBarrier);
    ward.block_ward_amount = mechanics.GetFloat(kSkillId, BladeWardNodes::ShieldBarrier, "ward_per_block_per_point", 10.0f) * static_cast<float>(barrierPts);

    // 4. 专精节点运行时数值烘焙（机制表驱动，供运行时、命中结算与反击站点消费）
    // 402/431 已随护盾 Buff 生效，此处镜像到组件字段供断言与后续读取。
    ward.mana_cost_reduction = persistManaReduction;
    ward.block_effectiveness = unstoppableBlockEffect;

    // 403 剑压外放：反击几率加成并入偏转几率（偏转是剑气护体的几率型反击路径），
    // 射程加成消费于反击剑气实体的飞行射程。
    const int repelPts = getPoints(BladeWardNodes::Repel);
    ward.counter_chance_bonus =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Repel, "counter_chance_per_point", 0.10f) *
        static_cast<float>(repelPts);
    ward.counter_range_bonus =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Repel, "counter_range_per_point", 0.10f) *
        static_cast<float>(repelPts);
    if (ward.counter_chance_bonus > 0.0f) {
      ward.interception_chance = std::min(1.0f, ward.interception_chance + ward.counter_chance_bonus);
    }

    // 410 厚积薄发：护甲减伤提升 + 每千护甲额外减伤（动态部分在 Update 中按当前护甲折算）。
    const int ironGuardPts = getPoints(BladeWardNodes::IronGuard);
    ward.armor_dr_bonus =
        mechanics.GetFloat(kSkillId, BladeWardNodes::IronGuard, "dr_bonus_pct_per_point", 0.10f) *
        static_cast<float>(ironGuardPts);
    ward.armor_dr_per_1000 =
        mechanics.GetFloat(kSkillId, BladeWardNodes::IronGuard, "dr_per_1000_armor", 0.01f) *
        static_cast<float>(ironGuardPts);

    // 413 破釜沉舟：低血时基础减伤与护甲倍率提升。
    const int lastStandPts = getPoints(BladeWardNodes::LastStand);
    if (lastStandPts > 0) {
      ward.last_stand_threshold = mechanics.GetFloat(kSkillId, BladeWardNodes::LastStand, "low_health_threshold", 0.35f);
      ward.last_stand_base_dr = mechanics.GetFloat(kSkillId, BladeWardNodes::LastStand, "base_dr", 0.24f);
      ward.last_stand_armor_mult = mechanics.GetFloat(kSkillId, BladeWardNodes::LastStand, "armor_multiplier", 2.0f);
    }

    // 415 坚韧回生：按已损生命的每秒回复比例。
    const int vitalityPts = getPoints(BladeWardNodes::Vitality);
    ward.missing_hp_regen_pct =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Vitality, "missing_hp_regen_pct_per_point", 0.005f) *
        static_cast<float>(vitalityPts);

    // 433 鲜血壁垒：数据仅提供启用开关；生命转护甲比例无对应键，
    // 运行时采用命名常量，已在实现报告中标注该歧义。
    const float bloodBarrierActive =
        mechanics.GetFloat(kSkillId, BladeWardNodes::BloodBarrier, "blood_barrier_active", 1.0f);
    ward.has_blood_barrier = (getPoints(BladeWardNodes::BloodBarrier) > 0) && bloodBarrierActive > 0.0f;

    // 434 无瑕之御：周期性获得完全招架充能（施放即就绪）。
    const int perfectParryPts = getPoints(BladeWardNodes::PerfectParry);
    if (perfectParryPts > 0) {
      ward.perfect_parry_interval =
          mechanics.GetFloat(kSkillId, BladeWardNodes::PerfectParry, "perfect_parry_interval", 5.0f);
      ward.perfect_parry_timer = 0.0f;
      ward.perfect_parry_ready = true;
    }

    // 435 剑意格御：获得剑意时附带暴击加成。
    ward.block_intent_crit =
        mechanics.GetFloat(kSkillId, BladeWardNodes::IntentProc, "crit_bonus", 0.05f) *
        static_cast<float>(intentProcPts);

    // 453 流风余韵：瞬身反打触发后按已损生命回复。
    const int aftermathPts = getPoints(BladeWardNodes::Aftermath);
    ward.aftermath_heal_pct =
        mechanics.GetFloat(kSkillId, BladeWardNodes::Aftermath, "heal_missing_hp_pct_per_point", 0.05f) *
        static_cast<float>(aftermathPts);

    // 454 御剑闪步：闪避后获得闪避等级。
    const int swordStepPts = getPoints(BladeWardNodes::SwordStep);
    ward.sword_step_dodge_rating =
        mechanics.GetFloat(kSkillId, BladeWardNodes::SwordStep, "dodge_rating_flat_per_point", 50.0f) *
        static_cast<float>(swordStepPts);

    // 472 雷霆法环 / 473 雷贯长虹：雷电系周期脉冲与感电强化。
    const int staticPts = getPoints(BladeWardNodes::StaticField);
    const int cascadePts = getPoints(BladeWardNodes::ThunderCascade);
    if (ward.is_lightning_ward || staticPts > 0 || cascadePts > 0) {
      ward.static_interval = mechanics.GetFloat(kSkillId, BladeWardNodes::StaticField, "static_interval", 0.5f);
      ward.static_radius = mechanics.GetFloat(kSkillId, BladeWardNodes::StaticField, "static_radius", 50.0f);
    }
    ward.thunder_frequency_bonus =
        mechanics.GetFloat(kSkillId, BladeWardNodes::ThunderCascade, "frequency_pct_per_point", 0.30f) *
        static_cast<float>(cascadePts);
    ward.shock_slow =
        mechanics.GetFloat(kSkillId, BladeWardNodes::ThunderCascade, "shock_slow_pct_per_point", 0.10f) *
        static_cast<float>(cascadePts);
    ward.shock_damage_bonus =
        mechanics.GetFloat(kSkillId, BladeWardNodes::ThunderCascade, "shock_damage_bonus", 0.15f) *
        static_cast<float>(cascadePts);

    // 474 霜铠 / 475 永冻领域：冰霜风暴半径与击退，475 提升半径。
    if (ward.is_cold_ward) {
      const float permafrostBonus =
          mechanics.GetFloat(kSkillId, BladeWardNodes::Permafrost, "radius_pct_per_point", 0.20f) *
          static_cast<float>(getPoints(BladeWardNodes::Permafrost));
      ward.permafrost_radius_bonus = permafrostBonus;
      ward.frost_radius = mechanics.GetFloat(kSkillId, BladeWardNodes::FrostArmor, "frost_radius", 80.0f) *
                          (1.0f + permafrostBonus);
      ward.frost_knockback = mechanics.GetFloat(kSkillId, BladeWardNodes::FrostArmor, "knockback", 100.0f);
    }

    // 476 元素曝光：命中施加对应元素易伤，节点可叠点提升比例。
    const int exposurePts = getPoints(BladeWardNodes::Exposure);
    if (exposurePts > 0) {
      ward.exposure_pct =
          mechanics.GetFloat(kSkillId, BladeWardNodes::Exposure, "exposure_pct_per_point", 0.04f) *
          static_cast<float>(exposurePts);
      ward.exposure_duration = mechanics.GetFloat(kSkillId, BladeWardNodes::Exposure, "exposure_duration", 3.0f);
    }

    registry.get_or_emplace<StatsDirty>(owner);
  }
};

namespace {

// 433 鲜血壁垒：缺失生命转化为护甲的比例无机制键，采用命名常量。
constexpr float kBloodBarrierArmorPerMissingHp = 0.10f;
// 472 雷霆法环：脉冲基础伤害无机制键（机制表仅给出间隔与半径），采用命名常量。
constexpr float kStaticAuraBaseDamage = 10.0f;

// 476 元素曝光：削减目标对应元素抗性，持续 exposure_duration，重复命中仅刷新不叠加。
void ApplyElementalExposure(entt::registry &registry, entt::entity target,
                            StatType resist_type, float pct, float duration) {
  if (pct <= 0.0f || duration <= 0.0f || !registry.valid(target)) {
    return;
  }
  BuffEffect exposure;
  exposure.id = "blade_ward_exposure";
  exposure.name = "Elemental Exposure";
  exposure.type = BuffType::DefenseDown;
  exposure.duration = duration;
  exposure.remaining = duration;
  exposure.stacks = 1;
  exposure.max_stacks = 1;
  exposure.is_debuff = true;
  exposure.source_skill_id = 4;
  // 机制表数值为比例（0.04 即每点 4%），抗性修饰以百分比点数计量。
  exposure.modifiers.push_back({.value = -pct * 100.0f,
                                .type = resist_type,
                                .mode = ModifierMode::Flat});
  registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(exposure);
  registry.get_or_emplace<StatsDirty>(target);
}

// 473 雷贯长虹：对目标施加减速与雷元素易伤，并补挂雷异常以驱动既有异常系统。
void ApplyThunderShock(entt::registry &registry, entt::entity owner,
                       entt::entity target, const BladeWardComponent &ward) {
  if (!registry.valid(target)) {
    return;
  }
  if (ward.shock_slow > 0.0f || ward.shock_damage_bonus > 0.0f) {
    BuffEffect shock;
    shock.id = "blade_ward_thunder_shock";
    shock.name = "Thunder Shock";
    shock.type = BuffType::Shock;
    shock.kind = BuffKind::Slow;
    shock.duration = 3.0f;
    shock.remaining = 3.0f;
    shock.is_debuff = true;
    shock.source_skill_id = 4;
    if (ward.shock_slow > 0.0f) {
      shock.modifiers.push_back({.value = -ward.shock_slow * 100.0f,
                                 .type = StatType::MoveSpeed,
                                 .mode = ModifierMode::PercentAdd});
    }
    if (ward.shock_damage_bonus > 0.0f) {
      shock.modifiers.push_back({.value = -ward.shock_damage_bonus * 100.0f,
                                 .type = StatType::ResistLightning,
                                 .mode = ModifierMode::Flat});
    }
    registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(shock);
    registry.get_or_emplace<StatsDirty>(target);
  }
  // 感电强度由机制表 473 段驱动（默认值与原硬编码一致）；持续时长与层数
  // 沿用异常契约既有口径。
  const float shockMagnitude =
      skills::GetMech(4u, BladeWardNodes::ThunderCascade, "shock_magnitude", 1.0f);
  (void)systems::AilmentApplier::Apply(
      registry, target,
      systems::AilmentApplyRequest{AilmentType::Shock, owner, shockMagnitude, 3.0f, 1});
}

} // namespace

void UpdateBladeWardRuntime(entt::registry &registry,
                            systems::SpatialHashGrid &grid, entt::entity entity,
                            BladeWardComponent &ward, float dt) {
  if (!registry.valid(entity)) {
    return;
  }
  auto *health = registry.try_get<HealthComponent>(entity);
  auto *stats = registry.try_get<CombatStats>(entity);
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(entity);

  // 410 厚积薄发：护甲减伤基础加成 + 每千护甲额外减伤（随当前护甲动态折算）。
  if (ward.armor_dr_bonus > 0.0f || ward.armor_dr_per_1000 > 0.0f) {
    const float armor = stats != nullptr ? stats->armor : 0.0f;
    const float dr = ward.armor_dr_bonus + (armor / 1000.0f) * ward.armor_dr_per_1000;
    // 模板仅构造一次；效果已存在时走无分配的就地刷新，避免每帧堆分配。
    static const BuffEffect kGuardTemplate = [] {
      BuffEffect buff;
      buff.id = "blade_ward_iron_guard";
      buff.name = "Accumulated Guard";
      buff.type = BuffType::DefenseUp;
      buff.duration = 0.5f;
      buff.remaining = 0.5f;
      buff.source_skill_id = 4;
      buff.modifiers.push_back({.value = 0.0f,
                                .type = StatType::GlobalDamageReduction,
                                .mode = ModifierMode::Flat});
      return buff;
    }();
    if (!effects.UpdateModifierValue("blade_ward_iron_guard", dr * 100.0f, 0.5f)) {
      BuffEffect guard = kGuardTemplate;
      guard.modifiers[0].value = dr * 100.0f;
      effects.AddOrRefresh(guard);
    }
  }

  // 413 破釜沉舟：生命低于阈值时提升基础减伤与护甲倍率，回血脱离后移除。
  if (ward.last_stand_base_dr > 0.0f) {
    const bool lowHealth = health != nullptr && health->max > 0.0f &&
                           (health->current / health->max) < ward.last_stand_threshold;
    if (lowHealth) {
      // 模板含两个 modifier。首次插入写入完整数值，之后每帧仅就地刷新
      // modifiers[0]（减伤值），护甲倍率 modifiers[1] 保持首插值，
      // 避免热路径构造 BuffEffect 的字符串/向量堆分配。
      static const BuffEffect kLastStandTemplate = [] {
        BuffEffect buff;
        buff.id = "blade_ward_last_stand";
        buff.name = "Last Stand";
        buff.type = BuffType::DefenseUp;
        buff.duration = 0.5f;
        buff.remaining = 0.5f;
        buff.source_skill_id = 4;
        buff.modifiers.push_back({.value = 0.0f,
                                  .type = StatType::GlobalDamageReduction,
                                  .mode = ModifierMode::Flat});
        buff.modifiers.push_back({.value = 0.0f,
                                  .type = StatType::Armor,
                                  .mode = ModifierMode::PercentMult});
        return buff;
      }();
      if (!effects.UpdateModifierValue("blade_ward_last_stand",
                                       ward.last_stand_base_dr * 100.0f, 0.5f)) {
        BuffEffect lastStand = kLastStandTemplate;
        lastStand.modifiers[0].value = ward.last_stand_base_dr * 100.0f;
        lastStand.modifiers[1].value =
            (ward.last_stand_armor_mult - 1.0f) * 100.0f;
        effects.AddOrRefresh(lastStand);
      }
    } else {
      effects.Remove("blade_ward_last_stand");
    }
  }

  // 415 坚韧回生：按已损生命的固定比例持续回复。
  if (ward.missing_hp_regen_pct > 0.0f && health != nullptr && health->current < health->max) {
    const float missing = health->max - health->current;
    health->current = std::min(health->max, health->current + missing * ward.missing_hp_regen_pct * dt);
  }

  // 433 鲜血壁垒：缺失生命按命名常量折算为护甲。
  if (ward.has_blood_barrier && health != nullptr && stats != nullptr) {
    const float missing = std::max(0.0f, health->max - health->current);
    static const BuffEffect kBloodBarrierTemplate = [] {
      BuffEffect buff;
      buff.id = "blade_ward_blood_barrier";
      buff.name = "Blood Barrier";
      buff.type = BuffType::DefenseUp;
      buff.duration = 0.5f;
      buff.remaining = 0.5f;
      buff.source_skill_id = 4;
      buff.modifiers.push_back({.value = 0.0f,
                                .type = StatType::Armor,
                                .mode = ModifierMode::Flat});
      return buff;
    }();
    const float armorFromHp = missing * kBloodBarrierArmorPerMissingHp;
    if (!effects.UpdateModifierValue("blade_ward_blood_barrier", armorFromHp, 0.5f)) {
      BuffEffect bloodBarrier = kBloodBarrierTemplate;
      bloodBarrier.modifiers[0].value = armorFromHp;
      effects.AddOrRefresh(bloodBarrier);
    }
  }

  // 434 无瑕之御：完全招架充能计时，就绪后由拦截层消费。
  if (ward.perfect_parry_interval > 0.0f && !ward.perfect_parry_ready) {
    ward.perfect_parry_timer += dt;
    if (ward.perfect_parry_timer >= ward.perfect_parry_interval) {
      ward.perfect_parry_timer = 0.0f;
      ward.perfect_parry_ready = true;
    }
  }

  // 472 雷霆法环 / 473 雷贯长虹：周期雷击范围内敌人并施加感电与元素曝光。
  if (ward.is_lightning_ward && ward.static_interval > 0.0f && ward.static_radius > 0.0f) {
    ward.static_timer += dt;
    const float interval =
        ward.static_interval / (1.0f + std::max(0.0f, ward.thunder_frequency_bonus));
    if (interval > 0.0f && ward.static_timer >= interval) {
      ward.static_timer = 0.0f;
      const Position *selfPos = registry.try_get<Position>(entity);
      if (selfPos != nullptr) {
        const Position center = *selfPos;
        // 先收集目标再结算，避免在网格回调中改动组件池。
        std::vector<entt::entity> targets;
        grid.query(center, ward.static_radius,
                   [&](entt::entity target, const Position &) {
                     if (target == entity) {
                       return;
                     }
                     if (!registry.all_of<FactionComponent, HealthComponent, EnemyTag>(target)) {
                       return;
                     }
                     targets.push_back(target);
                   });
        for (const entt::entity target : targets) {
          if (!registry.valid(target)) {
            continue;
          }
          DamageRequest pulse;
          pulse.origin = DamageOrigin::HazardEnvironment; // 持续场：不注入武器点伤
          pulse.attacker = entity;
          pulse.defender = target;
          pulse.skill_id = 4;
          pulse.base_pool.Add(Tag::Lightning, kStaticAuraBaseDamage);
          pulse.additional_tags = Tag::Hit | Tag::Lightning;
          pulse.source_entity = entity;
          // 环境脉冲关闭事件派发，避免与命中链路的反击/触发形成回环。
          pulse.dispatch_damage_events = false;
          (void)ResolveDamage(registry, pulse, entity);
          ApplyThunderShock(registry, entity, target, ward);
          ApplyElementalExposure(registry, target, StatType::ResistLightning,
                                 ward.exposure_pct, ward.exposure_duration);
        }
      }
    }
  }
}

void SpawnBladeWardCounterSwords(entt::registry &registry, entt::entity owner,
                                 uint64_t cast_id) {
  if (!registry.valid(owner) || !registry.all_of<Position>(owner)) {
    return;
  }
  const auto &mechanics = data::SkillMechanicsRegistry::Get();
  const int swordCount = static_cast<int>(
      mechanics.GetFloat(4, BladeWardNodes::CounterBlade, "counter_swords", 5.0f));
  if (swordCount <= 0) {
    return;
  }
  const auto *ward = registry.try_get<BladeWardComponent>(owner);
  const float rangeScale = ward != nullptr ? (1.0f + ward->counter_range_bonus) : 1.0f;
  const Position origin = registry.get<Position>(owner);
  // 速度恒定，射程仅由 lifeTime 随 counter_range_bonus 线性缩放（避免二次方增长）。
  const float speed = 520.0f;
  for (int i = 0; i < swordCount; ++i) {
    const float angle =
        (6.2831853f / static_cast<float>(swordCount)) * static_cast<float>(i);
    entt::entity sword = registry.create();
    auto &pos = registry.emplace<Position>(sword);
    pos.x = origin.x;
    pos.y = origin.y;
    auto &vel = registry.emplace<Velocity>(sword);
    vel.vx = std::cos(angle) * speed;
    vel.vy = std::sin(angle) * speed;

    Projectile projectile;
    projectile.owner = owner;
    projectile.cast_id = cast_id;
    projectile.speed = speed;
    projectile.radius = 6.0f;
    projectile.lifeTime = 0.5f * rangeScale;
    projectile.pierce = false;
    projectile.visual_only = true; // 仅表现：命中不产生伤害结算
    projectile.visualType = 0;
    projectile.snapshot = registry.all_of<CombatStats>(owner)
                              ? registry.get<CombatStats>(owner)
                              : CombatStats{};
    // 不设 payload_context：反击伤害由 ResolveSkill4Counter 单源结算，实体仅承担表现。
    registry.emplace<Projectile>(sword, std::move(projectile));
    registry.emplace<CombatStats>(sword, registry.get<Projectile>(sword).snapshot);
    registry.emplace<SkillComponent>(sword, 4u, owner);
  }
}

void ApplyBladeWardCounterOnHit(entt::registry &registry, entt::entity owner,
                                entt::entity target,
                                const BladeWardComponent &ward) {
  if (!registry.valid(owner)) {
    return;
  }
  // 476 元素曝光：被反击者的直接命中目标获得对应元素易伤。
  if (registry.valid(target) && ward.exposure_pct > 0.0f) {
    const StatType resist = ward.is_lightning_ward
                                ? StatType::ResistLightning
                                : (ward.is_cold_ward ? StatType::ResistCold
                                                     : StatType::ResistAll);
    ApplyElementalExposure(registry, target, resist, ward.exposure_pct,
                           ward.exposure_duration);
  }
  // 474 霜铠：寒冰系反击时以自身为中心爆发冰霜风暴（冰缓 + 击退）。
  if (!ward.is_cold_ward || ward.frost_radius <= 0.0f) {
    return;
  }
  const Position *selfPos = registry.try_get<Position>(owner);
  if (selfPos == nullptr) {
    return;
  }
  const Position center = *selfPos;
  const float radiusSq = ward.frost_radius * ward.frost_radius;
  std::vector<entt::entity> targets;
  // EnemyTag 为空标签类型，EnTT 的 each 仅在筛选时使用、不会作为回调参数传入。
  auto view = registry.view<FactionComponent, HealthComponent, EnemyTag, Position>();
  view.each([&](entt::entity entity, const FactionComponent &, HealthComponent &,
                const Position &position) {
    if (entity == owner) {
      return;
    }
    const float dx = position.x - center.x;
    const float dy = position.y - center.y;
    if (dx * dx + dy * dy > radiusSq) {
      return;
    }
    targets.push_back(entity);
  });
  for (const entt::entity entity : targets) {
    if (!registry.valid(entity)) {
      continue;
    }
    // 冰缓强度/时长/层数由机制表 474 段驱动（默认值与原硬编码一致）。
    const float chillMagnitude =
        skills::GetMech(4u, BladeWardNodes::FrostArmor, "chill_magnitude", 1.0f);
    const float chillDuration =
        skills::GetMech(4u, BladeWardNodes::FrostArmor, "chill_duration", 2.0f);
    const uint8_t chillStacks = static_cast<uint8_t>(
        skills::GetMech(4u, BladeWardNodes::FrostArmor, "chill_stacks", 1.0f));
    (void)systems::AilmentApplier::Apply(
        registry, entity,
        systems::AilmentApplyRequest{AilmentType::Chill, owner, chillMagnitude,
                                     chillDuration, chillStacks});
    Utils::ApplyKnockback(registry, entity, {center.x, center.y},
                          ward.frost_knockback);
    ApplyElementalExposure(registry, entity, StatType::ResistCold,
                           ward.exposure_pct, ward.exposure_duration);
  }
}

REGISTER_SKILL_BEHAVIOR(BladeWard)
void RegisterBladeWard() {}
} // namespace NoMoreDay::skills
