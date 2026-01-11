/**
 * @file MindBlade.cpp
 * @brief 心剑无影 (ID 7) - 引导技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::skills {

struct MindBlade : SkillBehaviorBase<MindBlade> {
    static constexpr uint32_t kSkillId = 7;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& chan = registry.emplace_or_replace<ChannelingComponent>(owner);
        chan.skill_id = 7;
        chan.channel_timer = 2.0f;
        chan.tick_interval = 0.05f;
        chan.tick_timer = 0.0f;
        chan.target_pos = exec.target_pos;
        chan.is_empowered = exec.is_empowered;
        
        LOG_INFO("Mind Blade channeling started for entity {}", (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(MindBlade)

void RegisterMindBlade() {}

} // namespace NoMoreDay::skills
