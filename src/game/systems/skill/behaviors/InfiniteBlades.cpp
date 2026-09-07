/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 模块化弹幕光束交付实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace InfiniteBladesNodes {
constexpr uint32_t SpiritResonance = 510;
constexpr uint32_t FastChannel = 512;
constexpr uint32_t BurstFinisher = 513;
constexpr uint32_t MindLock = 530;
constexpr uint32_t HeartPierce = 533;
constexpr uint32_t IntentBurst = 551;
constexpr uint32_t MindUnify = 552;
constexpr uint32_t ElementFall = 570;
constexpr uint32_t ElementPen = 571;
} // namespace InfiniteBladesNodes

struct InfiniteBlades : SkillBehaviorBase<InfiniteBlades> {
  static constexpr uint32_t kSkillId = 5;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
    chan.skill_id = kSkillId; chan.channel_timer = 0.5f; chan.tick_interval = 0.5f; chan.tick_timer = -0.01f;
    chan.target_pos = exec.target_pos; chan.is_empowered = exec.is_empowered; chan.cast_id = exec.cast_id;
    chan.conversion_tag = Tag::None; chan.bonus_damage_mult = 1.0f; chan.bonus_crit_chance = 0.0f;
    chan.bonus_armor_pen = 0.0f; chan.synergy_lock = false;

    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }

    if (profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(InfiniteBladesNodes::IntentBurst % 100)) {
      if (SkillSystem::ConsumeSwordIntent(registry, owner, SkillConstants::DEFAULT_MAX_SWORD_INTENT, kSkillId)) chan.extra_projectiles = true;
    }
    if (profile ? ((profile->delivery.feature_flags & 32) != 0) : exec.active_nodes.test(InfiniteBladesNodes::MindUnify % 100)) {
      int stacks = 0; if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) stacks = intent->stacks;
      chan.bonus_damage_mult += std::clamp(static_cast<float>(stacks) * 0.03f, 0.0f, 0.45f);
      chan.bonus_crit_chance += profile ? profile->delivery.bonus_crit : 3.0f;
    }
    if (profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(InfiniteBladesNodes::FastChannel % 100)) chan.tick_interval *= 0.7f;
    if (profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(InfiniteBladesNodes::BurstFinisher % 100)) chan.burst_finisher = true;
    if (profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(InfiniteBladesNodes::HeartPierce % 100)) chan.bonus_crit_chance += 15.0f;
    if (profile ? ((profile->delivery.feature_flags & 128) != 0) : exec.active_nodes.test(InfiniteBladesNodes::ElementFall % 100)) {
      chan.conversion_tag = profile ? (profile->effective_tags & (Tag::Fire | Tag::Cold | Tag::Lightning)) : Tag::Lightning;
    }
    if (profile ? ((profile->delivery.feature_flags & 64) != 0) : exec.active_nodes.test(InfiniteBladesNodes::ElementPen % 100)) {
      chan.bonus_armor_pen += profile ? profile->delivery.armor_pen : 20.0f;
    }
    if (profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(InfiniteBladesNodes::MindLock % 100)) chan.synergy_lock = true;

    if (chan.conversion_tag == Tag::None) chan.conversion_tag = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
    chan.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);

    auto &beam = registry.emplace_or_replace<BeamChannelComponent>(owner);
    beam.owner = owner; beam.cast_id = exec.cast_id; beam.skill_id = kSkillId;
    beam.mode = BeamChannelMode::BarrageEmitter; beam.max_channel_time = chan.channel_timer;
    beam.tick_interval = chan.tick_interval; beam.target_pos = exec.target_pos;
    beam.is_empowered = exec.is_empowered; beam.bonus_damage_mult = chan.bonus_damage_mult;
  }

  static void DoHit(entt::registry &, entt::entity, entt::entity, Tag, bool) {}
};

REGISTER_SKILL_BEHAVIOR(InfiniteBlades)
void RegisterInfiniteBlades() {}
} // namespace NoMoreDay::skills
