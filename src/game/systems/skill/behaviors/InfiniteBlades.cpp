/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 引导技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/skill/SkillSystem.hpp"

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

    // Talent: Yi Qi Bao Fa (意气爆发) - ID 551
    if (exec.active_nodes.test(InfiniteBladesNodes::IntentBurst % 100)) {
      if (auto *intent = registry.try_get<SwordIntentComponent>(owner)) {
        if (intent->stacks >= 10) {
          intent->stacks = 0; // Consume all
          chan.extra_projectiles = true;
          chan.consume_intent =
              true; // Mark for damage multiplier logic in system
          LOG_INFO("Infinite Blades (551): Consumed all intent for double "
                   "projectiles and damage boost.");
        }
      }
    }

    // Talent: Full Screen Lock (530)
    if (exec.active_nodes.test(InfiniteBladesNodes::MindLock % 100)) {
      chan.full_screen_lock = true;
    }

    // Talent: Burst Finisher (513)
    if (exec.active_nodes.test(InfiniteBladesNodes::BurstFinisher % 100)) {
      chan.burst_finisher = true;
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
