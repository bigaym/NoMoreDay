/**
 * @file InfiniteBlades.cpp
 * @brief 万剑归宗 (ID 5) - 引导技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::skills {

struct InfiniteBlades : SkillBehaviorBase<InfiniteBlades> {
    static constexpr uint32_t kSkillId = 5;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        LOG_INFO("DEBUG: InfiniteBlades::DoCast TRIGGERED for entity {}", (uint32_t)owner);
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 5;
        chan.channel_timer = 0.5f;
        chan.tick_interval = 0.1f;
        chan.tick_timer = -0.01f;
        chan.target_pos = exec.target_pos;
        chan.is_empowered = exec.is_empowered;

        // Talent: Yi Qi Bao Fa (意气爆发) - ID 520
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 5 && spec.allocated_points.contains(520) && spec.allocated_points.at(520) > 0) {
                    if (auto* intent = registry.try_get<SwordIntentComponent>(owner)) {
                        if (intent->stacks >= 5) {
                            intent->stacks -= 5;
                            chan.extra_projectiles = true;
                            LOG_INFO("Yi Qi Bao Fa: Consumed 5 intent for double projectiles.");
                        }
                    }
                    break;
                }
            }
        }
        LOG_INFO("Infinite Blades channeling started for entity {}", (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(InfiniteBlades)

} // namespace NoMoreDay::skills
