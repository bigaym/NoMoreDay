#include "DropSystem.hpp"
#include "../components/Common.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../components/EnemyComponent.hpp"
#include "../core/ItemFactory.hpp"
#include "../core/LootFilter.hpp"
#include "../tools/Logger.hpp"
#include <random>

namespace NoMoreDay {

static std::mt19937 g_drop_rng(std::random_device{}());

void DropSystem::update(entt::registry& registry, int areaLevel) {
    auto view = registry.view<KilledTag, DropTableComponent, Position>();

    for (auto entity : view) {
        const auto& killedTag = view.get<KilledTag>(entity);
        GenerateDrops(registry, killedTag.killer, entity, areaLevel);
    } // 遍历所有被击杀的实体
}

void DropSystem::GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim, int areaLevel) {
    if (!registry.all_of<DropTableComponent, Position>(victim)) return;

    const auto& table = registry.get<DropTableComponent>(victim);
    const auto& pos = registry.get<Position>(victim);

    // 获取玩家的魔法寻宝率和金币加成
    float mf = 0.0f;
    float goldBonus = 0.0f;
    int dropLevel = areaLevel;

    // 1. 获取基础掉落等级 (如果敌人等级更高，则使用敌人等级)
    if (registry.all_of<EnemyStateComponent>(victim)) {
        int enemyLevel = registry.get<EnemyStateComponent>(victim).level;
        if (enemyLevel > dropLevel) dropLevel = enemyLevel;
    }

    // 2. 获取玩家加成 (MF, 金币)
    if (registry.valid(killer) && registry.all_of<PlayerTag>(killer)) {
        if (registry.all_of<CombatStats>(killer)) {
            const auto& combat = registry.get<CombatStats>(killer);
            mf = combat.magic_find;
            goldBonus = combat.gold_bonus;
        }
    }

    // 3. 稀有度对掉落质量的额外影响
    float rarityMFBoost = 0.0f;
    int extraRolls = 0;
    if (registry.all_of<EnemyRarityComponent>(victim)) {
        auto rarity = registry.get<EnemyRarityComponent>(victim).rarity;
        if (rarity == EnemyRarityComponent::ELITE) {
            rarityMFBoost = 50.0f; // +50 MF
            extraRolls = 1;
        } else if (rarity == EnemyRarityComponent::BOSS) {
            rarityMFBoost = 200.0f; // +200 MF
            extraRolls = 3;
        }
    }

    const LootPool* pool = ItemFactory::getLootPool(table.poolId);
    if (!pool) {
        // 如果未找到特定掉落池，则回退到全局掉落池
        pool = ItemFactory::getLootPool(0);
    }

    if (!pool || pool->entries.empty()) return;

    // 确定掷骰次数
    std::uniform_int_distribution<int> rollDist(table.minRolls, table.maxRolls + extraRolls);
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
                    auto item = ItemFactory::createRandomLoot(registry, dropLevel, mf + rarityMFBoost);
                    registry.emplace_or_replace<Position>(item, pos.x, pos.y);
                    
                    // Spawn Visual Effect
                    auto effect = registry.create();
                    registry.emplace<Position>(effect, pos.x, pos.y);
                    VisualEffect vEffect;
                    vEffect.type = VisualEffectType::DropPillar;
                    vEffect.lifeTime = 0.5f;
                    vEffect.startScale = 0.2f;
                    vEffect.endScale = 1.0f;
                    vEffect.color = WHITE;
                    
                    if (registry.all_of<ItemComponent>(item)) {
                        const auto& ic = registry.get<ItemComponent>(item);
                        switch(ic.rarity) {
                            case Rarity::Magic: vEffect.color = SKYBLUE; break;
                            case Rarity::Rare: vEffect.color = YELLOW; break;
                            case Rarity::Legendary: vEffect.color = ORANGE; vEffect.lifeTime = 1.0f; vEffect.endScale = 1.5f; break;
                            default: vEffect.color = WHITE; break;
                        }
                    }
                    registry.emplace<VisualEffect>(effect, vEffect);

                    // Apply Loot Filter
                    if (registry.all_of<ItemComponent>(item)) {
                        const auto& itemComp = registry.get<ItemComponent>(item);
                        auto action = LootFilter::evaluate(itemComp, dropLevel);
                        
                        auto& result = registry.emplace<LootFilterResultComponent>(item);
                        
                        if (action.type == FilterActionType::HIDE) {
                            result.visible = false;
                        } else if (action.type == FilterActionType::EMPHASIZE) {
                            if (action.colorOverride.has_value()) {
                                result.color = action.colorOverride.value();
                            } else {
                                result.color = RED;
                            }
                            result.scale = action.scale;
                            result.showOnMinimap = action.minimapIcon;
                            
                            // 过滤器高亮时，特效颜色也跟随
                            registry.get<VisualEffect>(effect).color = result.color;
                            registry.get<VisualEffect>(effect).lifeTime = 1.2f; // 更持久
                        }
                    }

                    LOG_DEBUG("DropSystem: Dropped item level {} at ({}, {})", dropLevel, pos.x, pos.y);
                } else if (entry.type == LootEntryType::Gold) {
                    std::uniform_int_distribution<uint32_t> amountDist(entry.minAmount, entry.maxAmount);
                    uint32_t amount = amountDist(g_drop_rng); // 随机金币数量
                    amount = (uint32_t)((float)amount * (1.0f + goldBonus)); // 应用金币加成

                    if (amount > 0) {
                        auto gold = registry.create();
                        registry.emplace<Position>(gold, pos.x, pos.y);
                        registry.emplace<GoldComponent>(gold, amount);
                        
                        // Spawn Gold Effect
                        auto effect = registry.create();
                        registry.emplace<Position>(effect, pos.x, pos.y);
                        VisualEffect vEffect;
                        vEffect.type = VisualEffectType::GoldSparkle;
                        vEffect.lifeTime = 0.4f;
                        vEffect.color = GOLD;
                        registry.emplace<VisualEffect>(effect, vEffect);

                        LOG_DEBUG("DropSystem: Dropped {} gold at ({}, {})", amount, pos.x, pos.y);
                    }
                }
                break;
            }
        }
    }
}

} // namespace NoMoreDay
