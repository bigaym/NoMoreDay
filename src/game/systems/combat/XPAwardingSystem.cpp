#include "game/systems/combat/XPAwardingSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/combat/ProgressionSystem.hpp"
#include "core/logging/Logger.hpp"
#include <vector>
#include <queue>

namespace NoMoreDay {

// 真正待清理的物理实体队列，完全脱离 EnTT 标签系统以避免重复扫描
static std::queue<entt::entity> g_gcQueue;

void XPAwardingSystem::update(entt::registry& registry) {
    // 1. 处理新产生的死亡 (逻辑结算)
    // 仅查找带有 KilledTag 但尚未处理过经验 (没有 XPProcessedTag) 的实体
    auto killedView = registry.view<KilledTag>(entt::exclude<XPProcessedTag>);
    if (killedView.begin() != killedView.end()) {
        float totalXP = 0.0f;
        entt::entity playerKiller = entt::null;
        std::vector<entt::entity> to_queue;

        for (auto entity : killedView) {
            const auto& killedTag = killedView.get<KilledTag>(entity);
            entt::entity killer = killedTag.killer;

            // --- 经验结算 ---
            if (registry.valid(killer) && registry.all_of<PlayerTag>(killer)) {
                playerKiller = killer;
                if (auto* state = registry.try_get<EnemyStateComponent>(entity)) {
                    const auto& raceData = kRaceData[static_cast<size_t>(state->raceType)];
                    float xp = raceData.baseXP * (1.0f + (state->level - 1) * 0.1f);
                    if (auto* rarity = registry.try_get<EnemyRarityComponent>(entity)) {
                        if (rarity->rarity == EnemyRarityComponent::ELITE) xp *= 2.5f;
                        else if (rarity->rarity == EnemyRarityComponent::BOSS) xp *= 10.0f;
                    }
                    totalXP += xp;
                }
            }
            
            to_queue.push_back(entity);
        }

        if (totalXP > 0.0f && playerKiller != entt::null) {
            float xpMult = 1.0f;
            if (auto* cs = registry.try_get<CombatStats>(playerKiller)) xpMult = 1.0f + cs->experience_gain_mult;
            ProgressionSystem::AddExperience(registry, playerKiller, totalXP * xpMult);
        }

        // 关键：打上“已处理”标签，并进入销毁队列
        // 绝对不要移除 KilledTag，直到真正物理销毁！
        for (auto entity : to_queue) {
            registry.emplace<XPProcessedTag>(entity);
            g_gcQueue.push(entity);
        }
    }

    // 2. 垃圾回收：分帧物理销毁 (真正的负载均衡)
    static constexpr int GC_BUDGET = 15; 
    int count = 0;
    while (!g_gcQueue.empty() && count < GC_BUDGET) {
        entt::entity e = g_gcQueue.front();
        if (registry.valid(e)) {
            registry.destroy(e);
        }
        g_gcQueue.pop();
        count++;
    }
}

} // namespace NoMoreDay
