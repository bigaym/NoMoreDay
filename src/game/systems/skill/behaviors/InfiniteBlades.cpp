/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 引导技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace InfiniteBladesNodes {
// 基础分支 / Base
constexpr uint32_t IntentResonance = 500; // 剑意共鸣 / Intent Resonance
constexpr uint32_t SwordShield = 501;     // 剑盾 / Sword Shield

// 引导分支 / Channel branch
constexpr uint32_t SpiritResonance = 510; // 灵力共振 / Spirit Resonance
constexpr uint32_t CalmMind = 511;        // 气定神闲 / Calm Mind
constexpr uint32_t FastChannel = 512;     // 神速咏唱 / Fast Channel
constexpr uint32_t BurstFinisher = 513;   // 天剑降世 / Burst Finisher

// 锁定分支 / Lock branch
constexpr uint32_t MindLock = 530;        // 神识锁定 / Mind Lock
constexpr uint32_t NoEscape = 531;        // 无处遁形 / No Escape
constexpr uint32_t WeakMark = 532;        // 弱点标记 / Weak Mark
constexpr uint32_t HeartPierce = 533;     // 万剑穿心 / Heart Pierce

// 剑意分支 / Intent branch
constexpr uint32_t IntentReflow = 550;    // 剑意回流 / Intent Reflow
constexpr uint32_t IntentBurst = 551;     // 意气爆发 / Intent Burst
constexpr uint32_t MindUnify = 552;       // 意念合一 / Mind Unify
constexpr uint32_t SwordGodSmile = 553;   // 剑神一笑 / Sword God Smile

// 元素分支 / Element branch
constexpr uint32_t ElementFall = 570;     // 元素陨落 / Element Fall
constexpr uint32_t ElementPen = 571;      // 元素渗透 / Element Pen
constexpr uint32_t ElementOverload = 572; // 元素过载 / Element Overload
} // namespace InfiniteBladesNodes

struct InfiniteBlades : SkillBehaviorBase<InfiniteBlades> {
  static constexpr uint32_t kSkillId = 5;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    LOG_INFO("DEBUG: InfiniteBlades::DoCast TRIGGERED for entity {}",
             (uint32_t)owner);
    auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
    chan.skill_id = kSkillId;
    chan.channel_timer = 0.5f;
    chan.tick_interval = 0.5f;
    chan.tick_timer = -0.01f;
    chan.target_pos = exec.target_pos;
    chan.is_empowered = exec.is_empowered;
    chan.cast_id = exec.cast_id;
    chan.conversion_tag = Tag::None;
    chan.bonus_damage_mult = 1.0f;
    chan.bonus_crit_chance = 0.0f;
    chan.bonus_armor_pen = 0.0f;
    chan.synergy_lock = false;

    // Talent: Yi Qi Bao Fa (意气爆发) - ID 551
    if (exec.active_nodes.test(InfiniteBladesNodes::IntentBurst % 100)) {
      if (SkillSystem::ConsumeSwordIntent(
              registry, owner, SkillConstants::DEFAULT_MAX_SWORD_INTENT,
              kSkillId)) {
        chan.extra_projectiles = true;
        chan.consume_intent = true; // Mark for damage multiplier logic in system
        LOG_INFO("Infinite Blades (551): Consumed all intent for double "
                 "projectiles and damage boost.");
      }
    }

    // Talent: Full Screen Lock (530)
    if (exec.active_nodes.test(InfiniteBladesNodes::MindLock % 100)) {
      chan.full_screen_lock = true;
      chan.synergy_lock = true;
    }

    // Talent: Burst Finisher (513)
    if (exec.active_nodes.test(InfiniteBladesNodes::BurstFinisher % 100)) {
      chan.burst_finisher = true;
    }

    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id != kSkillId) {
          continue;
        }

        // Contract key node 570: transmuter conversion snapshot for channel ticks.
        if (auto it = spec.allocated_points.find(InfiniteBladesNodes::ElementFall);
            it != spec.allocated_points.end() && it->second > 0) {
          const ElementalConversion conv =
              ResolveElementalConversion(InfiniteBladesNodes::ElementFall, it->second);
          chan.conversion_tag = conv.target_element;
        }

        // Contract key node 571: penetration-like scaling.
        if (auto it = spec.allocated_points.find(InfiniteBladesNodes::ElementPen);
            it != spec.allocated_points.end() && it->second > 0) {
          chan.bonus_armor_pen = static_cast<float>(it->second) * 6.0f;
        }

        // Contract sword-intent key node 552.
        if (auto it = spec.allocated_points.find(InfiniteBladesNodes::MindUnify);
            it != spec.allocated_points.end() && it->second > 0) {
          int stacks = 0;
          if (const auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
            stacks = intent->stacks;
          }
          const float perStack = 0.015f * static_cast<float>(it->second);
          chan.bonus_damage_mult +=
              std::clamp(static_cast<float>(stacks) * perStack, 0.0f, 0.45f);
          chan.bonus_crit_chance += 1.5f * static_cast<float>(it->second);
        }

        // Contract trigger key node 533: reinforce trigger window.
        if (spec.allocated_points.contains(InfiniteBladesNodes::HeartPierce) &&
            spec.allocated_points.at(InfiniteBladesNodes::HeartPierce) > 0) {
          chan.bonus_damage_mult += 0.12f;
          chan.bonus_crit_chance += 6.0f;
        }
        if (spec.allocated_points.contains(InfiniteBladesNodes::FastChannel) &&
            spec.allocated_points.at(InfiniteBladesNodes::FastChannel) > 0) {
          chan.tick_interval *= 0.8f;
        }
        if (spec.allocated_points.contains(InfiniteBladesNodes::SpiritResonance) &&
            spec.allocated_points.at(InfiniteBladesNodes::SpiritResonance) > 0) {
          chan.bonus_damage_mult +=
              0.04f * static_cast<float>(spec.allocated_points.at(
                          InfiniteBladesNodes::SpiritResonance));
        }
        break;
      }
    }

    // Talent: Sword Intent Resonance (500)
    // "Longer channel = higher freq". Handled in System update logic.

    LOG_INFO("Infinite Blades channeling started for entity {}",
             (uint32_t)owner);
  }
};

REGISTER_SKILL_BEHAVIOR(InfiniteBlades)

void RegisterInfiniteBlades() {}

} // namespace NoMoreDay::skills
