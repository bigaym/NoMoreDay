#include "XPAwardingSystem.hpp"
#include "../components/Common.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Stats.hpp"
#include "../components/EnemyComponent.hpp"
#include "ProgressionSystem.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay {

void XPAwardingSystem::update(entt::registry& registry) {
    auto view = registry.view<KilledTag>();

    for (auto entity : view) {
        const auto& killedTag = view.get<KilledTag>(entity);
        entt::entity killer = killedTag.killer;

        // 确保击杀者有效且是玩家
        if (registry.valid(killer) && registry.all_of<PlayerTag>(killer)) {
            // 确保玩家有等级组件
            if (registry.all_of<PlayerLevel>(killer)) {
                float xpAmount = 0.0f;
                int targetLevel = 1;

                // 情况 A: 击杀的是怪物
                if (registry.all_of<EnemyStateComponent>(entity)) {
                    const auto& state = registry.get<EnemyStateComponent>(entity);
                    targetLevel = state.level;
                    EnemyRace race(state.raceType);
                    xpAmount = race.baseXP;

                    // 等级加成: 每级 +10%
                    if (targetLevel > 1) xpAmount *= (1.0f + (targetLevel - 1) * 0.1f);

                    // 稀有度加成
                    if (registry.all_of<EnemyRarityComponent>(entity)) {
                        const auto& rarity = registry.get<EnemyRarityComponent>(entity);
                        if (rarity.rarity == EnemyRarityComponent::ELITE) xpAmount *= 2.5f;
                        else if (rarity.rarity == EnemyRarityComponent::BOSS) xpAmount *= 10.0f;
                    }
                }
                // 情况 B: 击杀的是其他玩家 (PvP) 或拥有等级的实体
                else if (registry.all_of<PlayerLevel>(entity)) {
                    targetLevel = registry.get<PlayerLevel>(entity).value;
                    xpAmount = 50.0f; // 基础 PvP 经验
                }
                
                // 获取玩家的 CombatStats 以获得经验值加成（例如，来自魔法寻宝率）
                float xpBonus = 0.0f;
                if (registry.all_of<CombatStats>(killer)) {
                    xpBonus = registry.get<CombatStats>(killer).experience_gain_mult;
                }

                if (xpAmount > 0.0f) {
                    xpAmount *= (1.0f + xpBonus);
                    ProgressionSystem::AddExperience(registry, killer, xpAmount);
                    LOG_INFO("XP System: Player {} gained {:.1f} XP from target {} (Lvl {})", 
                        (uint32_t)killer, xpAmount, (uint32_t)entity, targetLevel);
                }
            }
        }
        
        // 移除 KilledTag，因为它已被处理
        registry.remove<KilledTag>(entity);
        // 在经验值和其他死亡后处理完成后销毁实体
        registry.destroy(entity);
    }
}

} // namespace NoMoreDay
