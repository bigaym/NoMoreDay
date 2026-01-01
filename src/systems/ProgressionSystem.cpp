#include "ProgressionSystem.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Progression.hpp"
#include "../components/Stats.hpp"
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

float ProgressionSystem::CalculateRequiredXP(int level) {
    if (level <= 0) return 100.0f;
    // 指数曲线: 100 * (等级 ^ 1.5)
    // 四舍五入到最近的整数以获得更整洁的数字
    return std::floor(100.0f * std::pow(static_cast<float>(level), 1.5f));
}

float ProgressionSystem::CalculateAwardedXP(int playerLevel, int monsterLevel, float baseXP) {
    float multiplier = 1.0f;
    if (playerLevel > monsterLevel) {
        float levelDifference = static_cast<float>(playerLevel - monsterLevel);
        // 每级差距减少10%，最低为基础经验的10%
        multiplier = std::max(0.1f, 1.0f - levelDifference * 0.1f);
    }
    // 根据规范，不为较低玩家等级（较高怪物等级）提供奖励，重点在于减少。
    return baseXP * multiplier;
}

void ProgressionSystem::AddExperience(entt::registry& registry, entt::entity entity, float amount) {
    if (!registry.all_of<PlayerStats>(entity)) return;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.level >= MAX_LEVEL) return;

    stats.current_xp += amount;

    // 检查升级（循环处理同时升级多级）
    while (stats.level < MAX_LEVEL && stats.current_xp >= stats.required_xp) {
        stats.current_xp -= stats.required_xp;
        LevelUp(registry, entity);
        
        if (stats.level >= MAX_LEVEL) {
            stats.current_xp = 0; // 满级后清空经验
            stats.required_xp = 0; // 满级不再有需求
            break;
        }
        // 刷新新等级所需的经验
        stats.required_xp = CalculateRequiredXP(stats.level);
    }
}

void ProgressionSystem::LevelUp(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<PlayerStats>(entity)) return;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.level >= MAX_LEVEL) return;

    stats.level++;
    
    // 如果存在 PlayerLevel 组件，则更新
    if (auto* levelComp = registry.try_get<PlayerLevel>(entity)) {
        levelComp->value = stats.level;
    }

    // 奖励点数
    stats.available_attribute_points += 5;
    stats.available_skill_points += 1;
    
    // 奖励星盘点数 (Astrolabe points)
    if (auto* astro = registry.try_get<AstrolabeComponent>(entity)) {
        astro->available_points += 1;
    }

    // 基础属性增长
    if (auto* primStats = registry.try_get<PrimaryStats>(entity)) {
        // 移除自动属性增长，改为由玩家手动分配
        // primStats->strength += 2.0f;
        // primStats->dexterity += 1.0f;
        // primStats->intelligence += 1.0f;
        // primStats->vitality += 2.0f;
        
        // 标记属性为脏，以触发 CombatStats 的重新计算
        registry.get_or_emplace<StatsDirty>(entity);
    }
}

bool ProgressionSystem::AllocateAttribute(entt::registry& registry, entt::entity entity, StatType type) {
    if (!registry.all_of<PlayerStats, PrimaryStats>(entity)) return false;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.available_attribute_points <= 0) return false;

    auto& primStats = registry.get<PrimaryStats>(entity);
    
    switch (type) {
        case StatType::Strength:     primStats.strength += 1.0f; break;
        case StatType::Dexterity:    primStats.dexterity += 1.0f; break;
        case StatType::Intelligence: primStats.intelligence += 1.0f; break;
        case StatType::Vitality:     primStats.vitality += 1.0f; break;
        default: return false; // 其他属性不能直接分配
    }

    stats.available_attribute_points--;
    registry.get_or_emplace<StatsDirty>(entity);
    return true;
}

bool ProgressionSystem::AllocateSkillPoint(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<PlayerStats>(entity)) return false;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.available_skill_points <= 0) return false;

    // 目前仅减少点数，因为技能树超出了范围
    stats.available_skill_points--;
    return true;
}

} // namespace NoMoreDay