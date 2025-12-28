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
    }
}

void DropSystem::GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim) {
    if (!registry.all_of<DropTableComponent, Position>(victim)) return;

    const auto& table = registry.get<DropTableComponent>(victim);
    const auto& pos = registry.get<Position>(victim);

    // Get Player Stats for MF and Gold Bonus
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

    const LootPool* pool = ItemFactory::getLootPool(table.poolId);
    if (!pool) {
        // Fallback to global pool if specific pool not found
        pool = ItemFactory::getLootPool(0);
    }

    if (!pool || pool->entries.empty()) return;

    // Determine number of rolls
    std::uniform_int_distribution<int> rollDist(table.minRolls, table.maxRolls);
    int rolls = rollDist(g_drop_rng);

    for (int i = 0; i < rolls; ++i) {
        // Check drop chance
        std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
        if (chanceDist(g_drop_rng) > table.dropChance) continue;

        // Roll on the pool
        std::uniform_real_distribution<float> weightDist(0.0f, pool->totalWeight);
        float roll = weightDist(g_drop_rng);
        float currentWeight = 0.0f;

        for (const auto& entry : pool->entries) {
            currentWeight += entry.weight;
            if (roll <= currentWeight) {
                // Generate Loot
                if (entry.type == LootEntryType::Item) {
                    auto item = ItemFactory::createRandomLoot(registry, playerLevel, mf);
                    registry.emplace_or_replace<Position>(item, pos.x, pos.y);
                    LOG_DEBUG("DropSystem: Dropped item at ({}, {})", pos.x, pos.y);
                } else if (entry.type == LootEntryType::Gold) {
                    std::uniform_int_distribution<uint32_t> amountDist(entry.minAmount, entry.maxAmount);
                    uint32_t amount = amountDist(g_drop_rng);
                    amount = (uint32_t)((float)amount * (1.0f + goldBonus));

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
