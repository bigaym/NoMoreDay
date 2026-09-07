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
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay::skills {

namespace BladeWardNodes {
constexpr uint32_t GoldenBell = 400;   // 金钟
constexpr uint32_t CloudShift = 401;   // 拨云
constexpr uint32_t FiveGuard = 411;    // 五行御守
constexpr uint32_t Mountain = 412;     // 不动如山
constexpr uint32_t IntentBlock = 430;  // 剑意格挡
constexpr uint32_t BlinkCounter = 451; // 瞬身反击
constexpr uint32_t AgileCounter = 452; // 灵动反击
constexpr uint32_t CounterBlade = 470; // 反制剑气
constexpr uint32_t RainbowQi = 471;    // 剑气如虹
constexpr uint32_t BladeStorm = 473;   // 剑刃风暴
} // namespace BladeWardNodes

struct BladeWard : SkillBehaviorBase<BladeWard> {
  static constexpr uint32_t kSkillId = 4;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }

    bool goldenBell = profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(BladeWardNodes::GoldenBell % 100);
    float phys_dr = 10.0f + (goldenBell ? 5.0f : 0.0f);
    BuffEffect ward_buff{.id = std::string(BuffIdToString(BuffId::BladeWard)), .name = "Blade Ward", .type = BuffType::Shield, .duration = 10.0f, .remaining = 10.0f};
    ward_buff.modifiers.push_back({.value = phys_dr, .type = StatType::ResistPhysical, .mode = ModifierMode::Flat});

    bool fiveGuard = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(BladeWardNodes::FiveGuard % 100);
    if (fiveGuard) {
      ward_buff.modifiers.push_back({.value = 15.0f, .type = StatType::ResistAll, .mode = ModifierMode::Flat});
    }
    bool intentBlock = profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(BladeWardNodes::IntentBlock % 100);
    if (intentBlock) {
      ward_buff.modifiers.push_back({.value = 10.0f, .type = StatType::BlockChance, .mode = ModifierMode::Flat});
    }

    registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(ward_buff);

    auto &sentinel = registry.emplace_or_replace<OrbitingSentinelComponent>(owner);
    sentinel.anchor_entity = owner; sentinel.cast_id = exec.cast_id; sentinel.skill_id = kSkillId;
    sentinel.count = static_cast<uint8_t>(exec.is_empowered ? 6 : 3); sentinel.orbit_radius = 60.0f;
    sentinel.angular_velocity = 180.0f; sentinel.interception_chance = 0.3f;

    auto &reactive = registry.emplace_or_replace<ReactiveWardComponent>(owner);
    reactive.owner = owner; reactive.ward_duration = 10.0f; reactive.counter_window = 0.8f; reactive.counter_skill_id = kSkillId;

    auto &ward = registry.get_or_emplace<BladeWardComponent>(owner);
    ward.duration = ward.remaining = 10.0f; ward.sword_count = sentinel.count; ward.interception_chance = sentinel.interception_chance;
    ward.is_solidified = profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(BladeWardNodes::Mountain % 100);
    ward.has_blink_counter = profile ? ((profile->delivery.feature_flags & 32) != 0) : exec.active_nodes.test(BladeWardNodes::BlinkCounter % 100);
    ward.trigger_counter = ward.has_blink_counter || (profile ? ((profile->delivery.feature_flags & 128) != 0) : exec.active_nodes.test(BladeWardNodes::CounterBlade % 100));
    ward.has_agile_counter = profile ? ((profile->delivery.feature_flags & 64) != 0) : exec.active_nodes.test(BladeWardNodes::AgileCounter % 100);
    ward.has_rainbow_qi = profile ? ((profile->delivery.feature_flags & 256) != 0) : exec.active_nodes.test(BladeWardNodes::RainbowQi % 100);
    ward.counter_spin = profile ? ((profile->delivery.feature_flags & 512) != 0) : exec.active_nodes.test(BladeWardNodes::BladeStorm % 100);

    if (profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(BladeWardNodes::CloudShift % 100)) ward.interception_chance += 0.25f;
    if (ward.has_blink_counter) ward.interception_chance += 0.1f;
    if (ward.has_agile_counter) ward.sword_count += 1;
    if (ward.has_rainbow_qi) ward.interception_chance += 0.15f;
    if (exec.is_empowered) ward.interception_chance *= 2.0f;
    sentinel.interception_chance = ward.interception_chance;
    registry.get_or_emplace<StatsDirty>(owner);
  }
};

REGISTER_SKILL_BEHAVIOR(BladeWard)
void RegisterBladeWard() {}
} // namespace NoMoreDay::skills
