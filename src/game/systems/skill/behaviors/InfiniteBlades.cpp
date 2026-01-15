/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 引导技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay::skills {

struct InfiniteBlades : SkillBehaviorBase<InfiniteBlades> {
  static constexpr uint32_t kSkillId = 5;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    LOG_INFO("DEBUG: InfiniteBlades::DoCast TRIGGERED for entity {}",
             (uint32_t)owner);
    auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
    chan.skill_id = 5;
    chan.channel_timer = 0.5f;
    chan.tick_interval = 0.1f;
    chan.tick_timer = -0.01f;
    chan.target_pos = exec.target_pos;
    chan.is_empowered = exec.is_empowered;
    chan.cast_id = exec.cast_id;

    // Talent: Yi Qi Bao Fa (意气爆发) - ID 551 (Was 520 in old code, correcting
    // based on skills.json 551) Note: skills.json ID 551 says "Before channel
    // if 10 intent, consume all for double projectiles" ID 520 seems to be
    // legacy or mixed up. Using 551 as per plan verification.
    if (exec.active_nodes.test(551 % 100)) {
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

    // Talent 551: Sword Intent Consumption (Generic/Buff effect)
    if (exec.active_nodes.test(551 % 100)) {
      // Implementation TODO or check if logic exists
    }

    // Talent: Full Screen Lock (530)
    if (exec.active_nodes.test(530 % 100)) {
      chan.full_screen_lock = true;
    }

    // Talent: Burst Finisher (513)
    if (exec.active_nodes.test(513 % 100)) {
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
