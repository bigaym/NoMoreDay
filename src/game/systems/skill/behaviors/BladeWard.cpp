/**
 * @file BladeWard.cpp
 * @brief 剑气护体 (ID 4) - 模块化投射/响应护盾交付实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace BladeWardNodes {
constexpr uint32_t GoldenBell      = 400; // 金钟罩
constexpr uint32_t Deflection      = 401; // 拨云见日
constexpr uint32_t Repel           = 403; // 剑压外放
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

    registry.get_or_emplace<StatsDirty>(owner);
  }
};

REGISTER_SKILL_BEHAVIOR(BladeWard)
void RegisterBladeWard() {}
} // namespace NoMoreDay::skills
