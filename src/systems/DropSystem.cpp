#include "DropSystem.hpp"
#include "../components/Common.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../core/ItemFactory.hpp"
#include "../tools/Logger.hpp"
#include <random>

namespace NoMoreDay {

static std::mt19937 g_drop_rng(std::random_device{}());

void DropSystem::update(entt::registry& registry) {
    auto view = registry.view<KilledTag, DropTableComponent, Position>();

    for (auto entity : view) {
        const auto& killedTag = view.get<KilledTag>(entity);
        GenerateDrops(registry, killedTag.killer, entity);
    } // 遍历所有被击杀的实体
}

void DropSystem::GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim) {
    if (!registry.all_of<DropTableComponent, Position>(victim)) return;

    const auto& table = registry.get<DropTableComponent>(victim);
    const auto& pos = registry.get<Position>(victim);

    // 获取玩家的魔法寻宝率和金币加成
    float mf = 0.0f;
    float goldBonus = 0.0f;
    int playerLevel = 1;

    if (registry.valid(killer) && registry.all_of<PlayerTag>(killer)) {
        if (registry.all_of<CombatStats>(killer)) {
            const auto& combat = registry.get<CombatStats>(killer);
            mf = combat.magic_find;
            goldBonus = combat.gold_bonus;
        }
        if (registry.all_of<PlayerLevel>(killer)) {
            playerLevel = registry.get<PlayerLevel>(killer).value;
        }
    }

    // --- 临时测试机制：必掉金币 (100%) ---
    // TODO: 测试完成后移除或降低几率
    {
        std::uniform_int_distribution<uint32_t> testGoldDist(10, 50);
        uint32_t amount = testGoldDist(g_drop_rng);
        amount = (uint32_t)((float)amount * (1.0f + goldBonus));
        
        if (amount > 0) {
            auto gold = registry.create();
            // 稍微偏移一点位置，避免重叠
            registry.emplace<Position>(gold, pos.x + 5.0f, pos.y + 5.0f);
            registry.emplace<GoldComponent>(gold, amount);
            LOG_DEBUG("DropSystem: [TEST] Guaranteed drop {} gold at ({}, {})", amount, pos.x, pos.y);
        }
    }

    const LootPool* pool = ItemFactory::getLootPool(table.poolId);
    if (!pool) {
        // 如果未找到特定掉落池，则回退到全局掉落池
        pool = ItemFactory::getLootPool(0);
    }

    if (!pool || pool->entries.empty()) return;

    // 确定掷骰次数
    std::uniform_int_distribution<int> rollDist(table.minRolls, table.maxRolls);
    int rolls = rollDist(g_drop_rng);

    for (int i = 0; i < rolls; ++i) {
        // 检查掉落几率
        std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
        if (chanceDist(g_drop_rng) > table.dropChance) continue;

        // Roll on the pool
        std::uniform_real_distribution<float> weightDist(0.0f, pool->totalWeight);
        float roll = weightDist(g_drop_rng);
        float currentWeight = 0.0f;
        
        for (const auto& entry : pool->entries) {
            currentWeight += entry.weight;
            if (roll <= currentWeight) {
                // 生成掉落物
                if (entry.type == LootEntryType::Item) {
                    auto item = ItemFactory::createRandomLoot(registry, playerLevel, mf);
                    registry.emplace_or_replace<Position>(item, pos.x, pos.y);
                    LOG_DEBUG("DropSystem: Dropped item at ({}, {})", pos.x, pos.y);
                } else if (entry.type == LootEntryType::Gold) {
                    std::uniform_int_distribution<uint32_t> amountDist(entry.minAmount, entry.maxAmount);
                    uint32_t amount = amountDist(g_drop_rng); // 随机金币数量
                    amount = (uint32_t)((float)amount * (1.0f + goldBonus)); // 应用金币加成

                    if (amount > 0) {
                        auto gold = registry.create();
                        registry.emplace<Position>(gold, pos.x, pos.y);
                        registry.emplace<GoldComponent>(gold, amount);
                        LOG_DEBUG("DropSystem: Dropped {} gold at ({}, {})", amount, pos.x, pos.y);
                    }
                }
                break;
            }
        }
    }
}

} // namespace NoMoreDay
